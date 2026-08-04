#!/bin/sh
# gen_bsl_boundary.sh
#
# Computes the real bootloader/app flash boundary for a project instead of
# using a hand-picked hardcoded constant, and makes the BOOTLOADER ITSELF
# consistent with that boundary (not just the app) - see the 2026-07-31
# real-hardware test that faulted because only the app side was updated:
# the bootloader embedded in flash still jumped to the old address.
#
# 2026-08 update: also generates the BOOTLOADER's own linker script (was
# previously a hand-maintained, separately-drifting copy per chip under
# bootloader/src/*_bsl.ld) and extends the whole mechanism to STM32F4xx
# (was G0xx-only) - F4xx erases flash at 16K SECTOR granularity (fixed by
# silicon, unlike G0xx's uniform 2K PAGE granularity), so the boundary
# rounds to whole sectors there instead of pages. Both generated files use
# a FIXED name (cpu_app.ld / cpu_bsl.ld, no chip name embedded) so switching
# target chip mid-project overwrites the previous one instead of
# accumulating stale copies.
#
# Steps:
#   0) if bootloader/src/cpu_bsl.ld doesn't exist yet (fresh checkout), seed
#      one from the template using the safety-minimum boundary, so pass 1
#      below has something to link against
#   1) build the bootloader for the given chip (Makefile.bsl_<CHIP>) - pass 1,
#      to measure its size (whatever boundary value cpu_bsl.ld currently
#      holds does not affect the bootloader's own compiled code size)
#   2) measure the real compiled size of project_build/project.bin
#   3) round it up to the next flash erase-granularity boundary (page for
#      G0xx, sector for F4xx), clamped to a safety MINIMUM because STM32G0xx
#      keeps a small persistent config block (device ID, fast-boot flag,
#      etc, see MIOS32_SYS_ADDR_BSL_INFO_BEGIN in include/mios32/mios32_sys.h)
#      at a FIXED OFFSET BEFORE the boundary that must stay inside the
#      protected BSL region - never shrink the boundary below this without
#      also moving that block. F4xx has the same block, but since its
#      boundary is already sector-granular (min 1 sector = 16K) it's always
#      comfortably larger than the 256-byte block needs.
#   4) writes mios32_bsl_boundary.h into bootloader/src AND the project dir
#   5) rebuilds the bootloader - pass 2, now with the final boundary baked in
#      (bsl_sysex.c's protection check AND main.c's jump-to-app address both
#      read MIOS32_APP_FLASH_START_ADDR from the generated header)
#   6) regenerates the embedded bootloader blob (mios32_bsl_<CHIP>.inc, in
#      the chip's own mios32/<FAMILY> folder - included by mios32_bsl.c via
#      MIOS32_BSL_INC_FILE) from this final pass-2 binary, sized to match
#      the final boundary exactly
#   7) writes bootloader/src/cpu_bsl.ld: the same per-chip template, with
#      the FLASH_BSL line removed entirely (the bootloader doesn't reserve
#      a separate region for itself - it IS the first region) and FLASH
#      rewritten to ORIGIN=0x08000000, LENGTH=<final boundary>
#   8) writes the project's own linker script, $PROJECT_DIR/cpu_app.ld, from
#      the same template: FLASH_BSL/FLASH origins and lengths matching the
#      same final boundary
#
# Also relays an optional project-level BSL_HOLD pin override
# (MIOS32_BSL_HOLD_PORT_OVERRIDE / MIOS32_BSL_HOLD_PIN_OVERRIDE, defined by
# the project's own mios32_config.h) into the bootloader's copy of the
# generated header only, via the same channel as MIOS32_APP_FLASH_START_ADDR -
# see step 4/6 above.
#
# Usage:
#   gen_bsl_boundary.sh <CHIP> <LD_TEMPLATE> <PROJECT_DIR> [PAGE_SIZE] [PADDING_BYTES] [MIN_BOUNDARY]
#
# TOTAL_FLASH_BYTES is not a caller-supplied argument (2026-08-05) - it is
# derived from $LD_TEMPLATE's own MEMORY block instead (FLASH_BSL length +
# FLASH length), so a project's Makefile never has to restate its chip's
# total flash size by hand. Every .ld template in etc/ld/ - both the handful
# of exact-named ones actively wired to real projects, and the package-less
# reference library covering every other G0/F4 SKU - consistently express
# these lengths in K-suffixed form (e.g. "10K", "118K"), so a single parser
# handles both sets uniformly.
#
# PAGE_SIZE/MIN_BOUNDARY default to family-appropriate values inferred from
# the CHIP name (STM32F4* -> 16384/16384 sector granularity, everything else
# -> 2048/10240 G0xx page granularity) - pass them explicitly to override.
#
# PADDING_BYTES (default 0) is added to the measured bootloader size before
# rounding - safety margin for future bootloader growth, or for testing that
# the boundary genuinely propagates end-to-end with a different value.
#
# Requires MIOS32_PATH and MIOS32_GCC_PREFIX to already be exported (same as
# for a normal MIOS32 build).

set -e

CHIP="$1"
LD_TEMPLATE="$2"
PROJECT_DIR="$3"

# family-appropriate erase-granularity defaults, overridable via $4/$6
case "$CHIP" in
    STM32F4*)
        FAMILY_DIR="STM32F4xx"
        DEFAULT_PAGE_SIZE=16384   # sector-erase, fixed by silicon
        DEFAULT_MIN_BOUNDARY=16384
        ;;
    *)
        FAMILY_DIR="STM32G0xx"
        DEFAULT_PAGE_SIZE=2048    # uniform page-erase
        DEFAULT_MIN_BOUNDARY=10240 # 0x2800 - see MIOS32_SYS_ADDR_BSL_INFO_BEGIN note above
        ;;
esac

PAGE_SIZE="${4:-$DEFAULT_PAGE_SIZE}"
PADDING_BYTES="${5:-0}"
MIN_BOUNDARY="${6:-$DEFAULT_MIN_BOUNDARY}"

if [ -z "$PROJECT_DIR" ]; then
    echo "Usage: gen_bsl_boundary.sh <CHIP> <LD_TEMPLATE> <PROJECT_DIR> [PAGE_SIZE] [PADDING_BYTES] [MIN_BOUNDARY]"
    exit 1
fi

if [ -z "$MIOS32_PATH" ]; then
    echo "MIOS32_PATH must be exported first"
    exit 1
fi

if [ ! -f "$LD_TEMPLATE" ]; then
    echo "LD_TEMPLATE not found: $LD_TEMPLATE"
    exit 1
fi

# --- derive TOTAL_FLASH_BYTES from the template's own MEMORY block - see the
#     usage comment above. The FLASH_BSL pattern is checked first and is
#     specific enough (requires the literal "_BSL" right after "FLASH") that
#     it can never accidentally match the plain "FLASH (rx)" line below it.
FLASH_BSL_K=$(sed -n -E 's/^[[:space:]]*FLASH_BSL[[:space:]]*\(rx\)[[:space:]]*:.*LENGTH[[:space:]]*=[[:space:]]*([0-9]+)K.*/\1/p' "$LD_TEMPLATE" | head -1)
FLASH_K=$(sed -n -E 's/^[[:space:]]*FLASH[[:space:]]*\(rx\)[[:space:]]*:.*LENGTH[[:space:]]*=[[:space:]]*([0-9]+)K.*/\1/p' "$LD_TEMPLATE" | head -1)
if [ -z "$FLASH_BSL_K" ] || [ -z "$FLASH_K" ]; then
    echo "Could not parse K-suffixed FLASH_BSL/FLASH LENGTH out of $LD_TEMPLATE"
    exit 1
fi
TOTAL_FLASH=$(( (FLASH_BSL_K + FLASH_K) * 1024 ))

# canonicalize to an absolute path for OUR OWN file operations (write_header,
# log redirection, etc) - a deeply relative BSL_DIR (e.g.
# "../../../bootloader/src", the case for a project 3 levels below
# MIOS32_PATH) has been observed to fail redirection ("> $BSL_DIR/....log")
# under some sh.exe builds, even though the same relative path works fine
# for plain `cd`/`make -f`.
BSL_DIR="$(cd "$MIOS32_PATH/bootloader/src" && pwd)"
BSL_MAKEFILE="Makefile.bsl_$CHIP"
BIN_FILE="$BSL_DIR/project_build/project.bin"
# generated .ld files live inside each side's own project_build/ (the
# gitignored make-target build dir, not the source directory) - same reasoning
# as the bootloader/src stray-directory fix earlier: keep every regenerated-
# every-build artifact contained in one place instead of scattered next to
# real source files. Created here (not left for `make`'s own `dirs:` target)
# because this script runs via $(shell ...) at Makefile PARSE time, before
# any recipe - including `dirs:` - has had a chance to run.
mkdir -p "$BSL_DIR/project_build" "$PROJECT_DIR/project_build"
BSL_LD_FILE="$BSL_DIR/project_build/cpu_bsl.ld"
APP_LD_FILE="$PROJECT_DIR/project_build/cpu_app.ld"

# --- write one variant of the generated linker script from the shared
#     per-chip template. $1 = output path, $2 = "bsl" or "app".
write_ld () {
    OUT="$1"
    VARIANT="$2"
    if [ "$VARIANT" = "bsl" ]; then
        # the bootloader doesn't reserve a separate FLASH_BSL region for
        # itself - drop that line entirely, and FLASH becomes exactly the
        # bootloader's own reserved window starting at the base of flash.
        # The .mios32_bsl OUTPUT SECTION (SECTIONS block) is retargeted from
        # >FLASH_BSL to >FLASH too - it stays empty here (the bootloader
        # build defines MIOS32_DONT_INCLUDE_BSL, it never embeds a copy of
        # itself), but with FLASH_BSL gone as a declared region, leaving the
        # old mapping causes ld to warn "memory region not declared" and mis-size
        # the surrounding sections.
        sed \
            -e '/^[[:space:]]*FLASH_BSL[[:space:]]*(rx)/d' \
            -e "s/^\([[:space:]]*\)FLASH[[:space:]]*(rx)[[:space:]]*:.*/\1FLASH (rx)     : ORIGIN = 0x08000000, LENGTH = $BOUNDARY/" \
            -e 's/}[[:space:]]*>FLASH_BSL/} >FLASH/' \
            "$LD_TEMPLATE" > "$OUT"
    else
        sed \
            -e "s/^\([[:space:]]*\)FLASH_BSL[[:space:]]*(rx)[[:space:]]*:.*/\1FLASH_BSL (rx) : ORIGIN = 0x08000000, LENGTH = $BOUNDARY/" \
            -e "s/^\([[:space:]]*\)FLASH[[:space:]]*(rx)[[:space:]]*:.*/\1FLASH (rx)     : ORIGIN = $APP_FLASH_ORIGIN_HEX, LENGTH = $APP_FLASH_LENGTH/" \
            "$LD_TEMPLATE" > "$OUT"
    fi
}

# the bootloader's own sub-make needs MIOS32_PATH relative to ITS OWN
# directory (bootloader/src is always exactly 2 levels below the repo
# root) - NOT the inherited value, which is relative to whatever depth the
# CALLING project sits at (e.g. "../../.." for a project 3 levels deep) and
# would resolve one directory too far up once this script cd's into
# bootloader/src. An absolute MIOS32_PATH would avoid that mismatch too, but
# GNU Make's rule parser chokes on a Windows drive-letter colon ("E:/...")
# embedded in the absolute source paths mios32.mk builds from it.
#
# A literal relative value ("../..") doesn't work either though: THUMB_SOURCE
# paths built from it (e.g. "../../mios32/STM32G0xx/mios32_sys.c") carry
# real ".." segments, and common.mk's object rule just prefixes $(PROJECT_OUT)
# onto whatever source path it's given - it doesn't textually contain that
# ".."; mkdir -p/gcc -o resolve it as a real upward directory walk, scattering
# mios32/etc/drivers directories one level above bootloader/src on every
# single build (found & fixed 2026-08-04, after being found once already
# earlier this project and mistakenly assumed fixed).
#
# Fix: a same-directory symlink, bootstrapped once if missing, standing in
# for the repo root - "REPO_ROOT_LINK/mios32/..." has the same "path depth"
# as "../../mios32/..." to the OS, but contains no ".." token for Make/mkdir
# to walk out on, so the mirrored object tree stays correctly inside
# $(PROJECT_OUT). Falls back to a literal ".." value on platforms where a
# symlink can't be created (e.g. Windows without Developer Mode/admin) -
# the stray directories reappearing there is a cosmetic wart, not a build
# failure, and preferable to a hard error on every build.
REPO_ROOT_LINK="$BSL_DIR/.repo_root"
if [ ! -e "$REPO_ROOT_LINK" ]; then
    # MSYS's default symlink mode silently produces something $BSL_MAKEFILE's
    # sub-make can't actually use as a directory (confirmed 2026-08-04: `ln -s`
    # "succeeds" - exit 0, no error - but the result fails `test -L` and
    # doesn't behave as a real directory link). winsymlinks:nativestrict
    # forces a real Windows symlink (needs Developer Mode or an elevated
    # shell - same requirement as any Windows symlink, not specific to this
    # script) - falls through to the ".." fallback below if that's not
    # available either.
    MSYS=winsymlinks:nativestrict ln -s "$MIOS32_PATH" "$REPO_ROOT_LINK" 2>/dev/null || true
fi
if [ -L "$REPO_ROOT_LINK" ]; then
    BSL_SUBMAKE_MIOS32_PATH=".repo_root"
else
    BSL_SUBMAKE_MIOS32_PATH="../.."
fi

build_bootloader () {
    ( cd "$BSL_DIR" && MIOS32_PATH=$BSL_SUBMAKE_MIOS32_PATH make -f "$BSL_MAKEFILE" > "$BSL_DIR/gen_bsl_boundary_build.log" 2>&1 ) || {
        echo "Bootloader build failed, see $BSL_DIR/gen_bsl_boundary_build.log"
        exit 1
    }
    if [ ! -f "$BIN_FILE" ]; then
        echo "Expected bootloader binary not found: $BIN_FILE"
        exit 1
    fi
}

# --- step 0: unconditionally seed a starting cpu_bsl.ld from THIS chip's own
#     template before pass 1, so pass 1 below has something correct to link
#     the bootloader against. This is a link-only placeholder, fully
#     decoupled from MIN_BOUNDARY (which sizes the FINAL boundary, clamping
#     it upward from the real measured size) - a bootloader can legitimately
#     compile larger than MIN_BOUNDARY (that's exactly why pass 1 measures it
#     instead of trusting a hardcoded constant), so seeding at MIN_BOUNDARY
#     itself is not reliably enough room to link pass 1 in the first place.
#     32K comfortably fits any MIOS32 bootloader variant built so far
#     (including the heavier F4+USB ones) and is discarded immediately once
#     the real size is known.
#     MUST be unconditional (not "only if missing"): a leftover cpu_bsl.ld
#     from a PREVIOUS run for a DIFFERENT chip (different RAM size, register
#     map, even different family) would otherwise silently get reused for
#     pass 1 here, at best wasting a build, at worst linking pass 1 against
#     the wrong MEMORY block entirely.
# bootloader/src/project_build is SHARED by every Makefile.bsl_<CHIP> (there's
# only one bootloader/src directory for all chips/families) - make's default
# dependency tracking follows source file mtimes only, not CFLAGS/-D changes,
# so a stale .o compiled for a DIFFERENT chip/family in a PREVIOUS invocation
# would otherwise be silently reused here even though its FAMILY defines no
# longer match. Always start from a clean slate - and BEFORE the bootstrap
# seed below, not after: cleanall's `clean:` target is `rm -rf $(PROJECT_OUT)`,
# which would otherwise delete the cpu_bsl.ld the seed step just wrote, now
# that it lives inside project_build/ too (2026-08-04).
( cd "$BSL_DIR" && MIOS32_PATH=$BSL_SUBMAKE_MIOS32_PATH make -f "$BSL_MAKEFILE" cleanall > "$BSL_DIR/gen_bsl_boundary_build.log" 2>&1 ) || {
    echo "Bootloader cleanall failed, see $BSL_DIR/gen_bsl_boundary_build.log"
    exit 1
}
mkdir -p "$BSL_DIR/project_build"

BOOTSTRAP_SIZE=32768
echo "Seeding $BSL_LD_FILE from $LD_TEMPLATE at $BOOTSTRAP_SIZE bytes (link-only placeholder for pass 1)"
BOUNDARY=$BOOTSTRAP_SIZE
write_ld "$BSL_LD_FILE" "bsl"

echo "=== Pass 1: building bootloader for $CHIP to measure its real size ==="
build_bootloader
BSL_SIZE=$(stat -c%s "$BIN_FILE")
BSL_SIZE_PADDED=$(( BSL_SIZE + PADDING_BYTES ))

# round up to next page/sector boundary, then clamp to the safety minimum
PAGES=$(( (BSL_SIZE_PADDED + PAGE_SIZE - 1) / PAGE_SIZE ))
BOUNDARY=$(( PAGES * PAGE_SIZE ))
if [ "$BOUNDARY" -lt "$MIN_BOUNDARY" ]; then
    BOUNDARY=$MIN_BOUNDARY
fi
APP_FLASH_LENGTH=$(( TOTAL_FLASH - BOUNDARY ))

BOUNDARY_HEX=$(printf '0x%X' "$BOUNDARY")
APP_FLASH_ORIGIN_HEX=$(printf '0x%08X' $((0x08000000 + BOUNDARY)))

echo "Bootloader actual size : $BSL_SIZE bytes (+ $PADDING_BYTES padding = $BSL_SIZE_PADDED)"
echo "Final boundary (rounded to $PAGE_SIZE, min $MIN_BOUNDARY) : $BOUNDARY_HEX ($BOUNDARY bytes)"

# --- BSL_HOLD pin override (optional): if the project's own mios32_config.h
#     defines MIOS32_BSL_HOLD_PORT_OVERRIDE / MIOS32_BSL_HOLD_PIN_OVERRIDE,
#     relay them into the BOOTLOADER's copy of the generated header only -
#     never into the project's own copy, since the project's mios32_config.h
#     already defines them itself and including them there too would just be
#     a duplicate #define in the same translation unit. This is the same
#     two-pass-build channel already used for MIOS32_APP_FLASH_START_ADDR,
#     just carrying one more project-level compile-time constant. Not a
#     runtime/flash mechanism - both binaries are compiled together, so a
#     plain preprocessor relay is enough (see 2026-08-01 plan review).
HOLD_PIN_OVERRIDES=""
if [ -f "$PROJECT_DIR/mios32_config.h" ]; then
    HOLD_PIN_OVERRIDES=$(grep -E '^[[:space:]]*#define[[:space:]]+MIOS32_BSL_HOLD_(PORT|PIN)_OVERRIDE\b' "$PROJECT_DIR/mios32_config.h" || true)
fi

write_header () {
    TARGET="$1"
    WITH_OVERRIDES="$2"
    {
        echo "// AUTO-GENERATED by etc/gen_bsl_boundary.sh - do not edit by hand."
        echo "// Computed from the real compiled size of the $CHIP bootloader,"
        echo "// rounded up to a $PAGE_SIZE byte flash erase-granularity boundary (min $MIN_BOUNDARY)."
        echo "#ifndef _MIOS32_BSL_BOUNDARY_H"
        echo "#define _MIOS32_BSL_BOUNDARY_H"
        echo "#define MIOS32_APP_FLASH_START_ADDR $BOUNDARY_HEX"
        # names the exact embedded-bootloader .inc file for this project's own
        # chip - lets mios32_bsl.c include it directly instead of re-deriving
        # it from a MIOS32_BOARD_xxx define (removed 2026-08-01)
        echo "#define MIOS32_BSL_INC_FILE \"mios32_bsl_${CHIP}.inc\""
        if [ "$WITH_OVERRIDES" = "yes" ] && [ -n "$HOLD_PIN_OVERRIDES" ]; then
            echo "// BSL_HOLD pin override relayed from the project's mios32_config.h:"
            echo "$HOLD_PIN_OVERRIDES"
        fi
        echo "#endif"
    } > "$TARGET"
}

# --- generated header for the BOOTLOADER's own build (so bsl_sysex.c's
#     protection check and main.c's jump-to-app address use the same value) ---
write_header "$BSL_DIR/mios32_bsl_boundary.h" "yes"
echo "Wrote $BSL_DIR/mios32_bsl_boundary.h"
if [ -n "$HOLD_PIN_OVERRIDES" ]; then
    echo "Relayed BSL_HOLD pin override into bootloader build:"
    echo "$HOLD_PIN_OVERRIDES"
fi

# --- regenerate cpu_bsl.ld with the final boundary BEFORE pass 2, so the
#     bootloader links against its real, final flash window ---
write_ld "$BSL_LD_FILE" "bsl"
echo "Wrote $BSL_LD_FILE (FLASH=$BOUNDARY bytes)"

echo "=== Pass 2: rebuilding bootloader with the final boundary baked in ==="
build_bootloader
BSL_SIZE_FINAL=$(stat -c%s "$BIN_FILE")
if [ "$BSL_SIZE_FINAL" -gt "$BOUNDARY" ]; then
    echo "ERROR: bootloader grew to $BSL_SIZE_FINAL bytes on pass 2, exceeding the computed boundary $BOUNDARY - increase PADDING_BYTES and retry."
    exit 1
fi

# --- regenerate the embedded bootloader blob (mios32_bsl.c includes this via
#     MIOS32_BSL_INC_FILE) - lives under the chip's own mios32/<FAMILY> dir ---
INC_FILE="$MIOS32_PATH/mios32/$FAMILY_DIR/mios32_bsl_${CHIP}.inc"
perl "$BSL_DIR/gen_inc_file.pl" "$BIN_FILE" "$INC_FILE" mios32_bsl_image mios32_bsl -size="$BOUNDARY"
echo "Regenerated $INC_FILE ($BOUNDARY bytes, matches final boundary)"

# --- generated header consumed by the project's own mios32_config.h ---
# (no pin override relayed here: the project already defines it itself,
# relaying it back would be a duplicate #define in the same file)
write_header "$PROJECT_DIR/mios32_bsl_boundary.h" "no"
echo "Wrote $PROJECT_DIR/mios32_bsl_boundary.h"

# --- generated linker script for the project's own app build ---
write_ld "$APP_LD_FILE" "app"
echo "Wrote $APP_LD_FILE (FLASH_BSL=$BOUNDARY bytes, FLASH origin=$APP_FLASH_ORIGIN_HEX length=$APP_FLASH_LENGTH bytes)"
