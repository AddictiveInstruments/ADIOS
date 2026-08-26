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
#   1) build the bootloader for the given chip (the single generic
#      bootloader/src/Makefile.bsl, with PROCESSOR passed on its command
#      line) - pass 1, to measure its size (whatever boundary value
#      cpu_bsl.ld currently holds does not affect the bootloader's own
#      compiled code size)
#   2) measure the real compiled size of project_build/project.bin
#   3) round it up to the next flash erase-granularity boundary (page for
#      G0xx, sector for F4xx), then clamp to a MINIMUM.
#
#      That minimum is a DELIBERATE FLEET CHOICE, not a technical limit: the
#      rounding alone would let the boundary follow the bootloader's compiled
#      size, so one added feature - or merely a different compiler, which is
#      worth tens of bytes here - would move it and oblige every deployed
#      board to migrate. Pinning it buys a stable address with a known margin,
#      and the build stops loudly (#error, further down) if the bootloader
#      ever outgrows it, instead of silently relocating the application.
#
#      Choose it as: the boundary you intend the fleet to keep, at least one
#      erase unit above the largest bootloader that boundary must hold. On
#      F4xx the erase unit IS the minimum (one 16K sector). On G0xx, 0x2000
#      with roughly 600 bytes to spare at the time of writing.
#   4) writes adios_bsl_boundary.h into bootloader/src AND the project dir
#   5) rebuilds the bootloader - pass 2, now with the final boundary baked in
#      (bsl_sysex.c's protection check AND main.c's jump-to-app address both
#      read ADIOS_APP_FLASH_START_ADDR from the generated header)
#   6) regenerates the embedded bootloader blob (adios_bsl_<CHIP>.inc, in
#      the chip's own adios/<FAMILY> folder - included by adios_bsl.c via
#      ADIOS_BSL_INC_FILE) from this final pass-2 binary, sized to match
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
# (ADIOS_BSL_HOLD_PORT_OVERRIDE / ADIOS_BSL_HOLD_PIN_OVERRIDE, defined by
# the project's own adios_config.h) into the bootloader's copy of the
# generated header only, via the same channel as ADIOS_APP_FLASH_START_ADDR -
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
# Requires ADIOS_PATH and ADIOS_GCC_PREFIX to already be exported (same as
# for a normal ADIOS build).

set -e

CHIP="$1"
LD_TEMPLATE="$2"
PROJECT_DIR="$3"

# family-appropriate erase-granularity defaults, overridable via $4/$6.
# UPDATER_ORIGIN_OFF: where the BSL-update tool is linked when
# BSL_BOUNDARY_MODE=updater (see below) - the updater is linked ABOVE the
# normal app origin. It writes the incoming bootloader image DIRECTLY into
# the BSL region (no staging - see bootloader/src/bsl_sysex.c).
# It is given everything from there to the top of usable flash. There used
# to be a fixed-size window as well, and it was a leftover: the only thing
# that ever needed protecting up there is the application's own data area,
# and ADIOS_USERDATA_PAGES already carves that out - for the updater as for
# the application, both being relayed the same page count. A hand-picked
# length on top of that protected nothing and merely capped the tool, which
# is how a tool that grew a USB stack stopped fitting.
# ORIGIN = the FLEET BOUNDARY CEILING (the highest boundary any device can
# have). The updater is compiled code with absolute addresses - its link
# address is fixed at build time, before the target device's OLD boundary is
# known, and neither ADIOS Studio nor anyone can relocate compiled ARM code.
# Two constraints force it >= max(old, new) boundary: (1) it is uploaded
# THROUGH the old bootloader, which physically refuses writes below its own
# boundary; (2) it must survive the BSL-region rewrite (writes below the new
# boundary). Since old is unknown at build time, we link at the ceiling,
# which is >= every possible boundary in both directions. No +1 page is
# needed: the fresh bootloader is kept resident by the TAMP flag, not by
# erasing the app entry, so the updater never erases its own page (see
# BSL_SYSEX_ReleaseHaltState). G0 ceiling = 0x3000 (a ~10.4K debug-enabled
# BSL); F4 = one 16K sector (0x4000), updater at sector #2 (0x8000).
# A BSL that pushes the boundary above the ceiling trips a build #error
# below - raise the ceiling then.
case "$CHIP" in
    STM32F4*)
        FAMILY_DIR="STM32F4xx"
        DEFAULT_PAGE_SIZE=16384   # sector-erase, fixed by silicon
        DEFAULT_MIN_BOUNDARY=16384
        UPDATER_ORIGIN_OFF=32768  # 0x8000, 16K sector #2 (ceiling = sector #1 = 0x4000)
        ;;
    *)
        FAMILY_DIR="STM32G0xx"
        DEFAULT_PAGE_SIZE=2048    # uniform page-erase
        DEFAULT_MIN_BOUNDARY=8192  # 0x2000 - a CHOSEN floor, see note below
        UPDATER_ORIGIN_OFF=12288  # 0x3000 - the fleet boundary ceiling
        ;;
esac

PAGE_SIZE="${4:-$DEFAULT_PAGE_SIZE}"
# Also settable from the project's Makefile as ADIOS_BSL_PADDING, relayed
# through the environment like the other project facts. It MUST reach every
# pass that computes a boundary - the application's and the updater's - or
# the two would size the same bootloader differently and the tool would
# install an image cut for a boundary the application does not use.
PADDING_BYTES="${5:-${ADIOS_BSL_PADDING:-0}}"
MIN_BOUNDARY="${6:-$DEFAULT_MIN_BOUNDARY}"

if [ -z "$PROJECT_DIR" ]; then
    echo "Usage: gen_bsl_boundary.sh <CHIP> <LD_TEMPLATE> <PROJECT_DIR> [PAGE_SIZE] [PADDING_BYTES] [MIN_BOUNDARY]"
    exit 1
fi

if [ -z "$ADIOS_PATH" ]; then
    echo "ADIOS_PATH must be exported first"
    exit 1
fi

if [ ! -f "$LD_TEMPLATE" ]; then
    echo "LD_TEMPLATE not found: $LD_TEMPLATE"
    exit 1
fi

# --- a per-family template (.ld.S) has to be resolved for THIS chip before
#     anything below can read it: the linker scripts are no longer one file
#     per chip, they are one per family plus a table of the only two values
#     that ever differed (contiguous RAM, total flash). Both candidate keys
#     are passed - the exact part number and the package-less form - and the
#     table matches whichever it carries, reproducing the old exact-then-
#     fallback filename lookup. Everything downstream (the TOTAL_FLASH parse
#     just below, write_ld) then works on the resolved file unchanged, which
#     is why the template keeps literal K-suffixed lengths.
case "$LD_TEMPLATE" in
    *.ld.S)
        LD_TEMPLATE_SRC="$LD_TEMPLATE"
        LD_TEMPLATE="${TMPDIR:-/tmp}/adios_ld_${CHIP}_$$.ld"
        LD_CHIP_FALLBACK=$(echo "$CHIP" | sed -E 's/^(.{9}).(.)$/\1x\2/')
        "${ADIOS_GCC_PREFIX:-arm-none-eabi}-gcc" -E -x assembler-with-cpp -P \
            -I "$(dirname "$LD_TEMPLATE_SRC")" \
            -DADIOS_LD_CHIP_"$CHIP" -DADIOS_LD_CHIP_"$LD_CHIP_FALLBACK" \
            "$LD_TEMPLATE_SRC" -o "$LD_TEMPLATE" || {
            echo "Could not resolve $LD_TEMPLATE_SRC for $CHIP (keys tried: $CHIP, $LD_CHIP_FALLBACK)"
            exit 1
        }
        trap 'rm -f "$LD_TEMPLATE"' EXIT
        ;;
esac

# --- the chip's total flash, stated explicitly by the template (see the
#     _flash_size line in etc/ld/adios_body.ld.inc). Read from one dedicated
#     line rather than summing the two FLASH region lengths: those are a
#     LAYOUT, they change with the boundary and their written form can drift,
#     while the chip's flash size is a fixed fact about the silicon.
TOTAL_FLASH_K=$(sed -n -E 's/^[[:space:]]*PROVIDE[[:space:]]*\([[:space:]]*_flash_size[[:space:]]*=[[:space:]]*([0-9]+)K.*/\1/p' "$LD_TEMPLATE" | head -1)
if [ -z "$TOTAL_FLASH_K" ]; then
    echo "Could not read the chip's total flash from $LD_TEMPLATE (expected a 'PROVIDE ( _flash_size = <n>K ) ;' line)"
    exit 1
fi
TOTAL_FLASH=$(( TOTAL_FLASH_K * 1024 ))

# canonicalize to an absolute path for OUR OWN file operations (write_header,
# log redirection, etc) - a deeply relative BSL_DIR (e.g.
# "../../../bootloader/src", the case for a project 3 levels below
# ADIOS_PATH) has been observed to fail redirection ("> $BSL_DIR/....log")
# under some sh.exe builds, even though the same relative path works fine
# for plain `cd`/`make -f`.
BSL_DIR="$(cd "$ADIOS_PATH/bootloader/src" && pwd)"
# ONE generic bootloader Makefile for every chip/family (2026-08-05, replaced
# the per-chip Makefile.bsl_STM32xxx copies) - the chip is passed to it as a
# make variable instead of being baked into a separate file per SKU, so a new
# chip needs no new file here.
BSL_MAKEFILE="Makefile.bsl"
# perl helpers live under etc/perl/ with the rest of the tooling, not next to
# the bootloader sources they happen to serve. Canonicalized for the same
# reason as BSL_DIR above.
PERL_DIR="$(cd "$ADIOS_PATH/etc/perl" && pwd)"
BIN_FILE="$BSL_DIR/project_build/project.bin"
# the sub-make build log lives inside project_build/ like every other
# regenerated artifact, so `clean` disposes of it instead of leaving it to
# accumulate at the top of the source directory forever. See the two clean
# sites below for why they CAPTURE it rather than redirect into it.
BSL_LOG="$BSL_DIR/project_build/gen_bsl_boundary_build.log"
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
# the two GENERATED-AND-INCLUDED files join the .ld scripts inside
# project_build/: they are rewritten at every build and belong to the build
# output, not next to real source files. Nothing at a project root ever got
# cleaned - clean is `rm -rf project_build`, so it could not see them.
# The C side finds them because common.mk puts -I$(PROJECT_OUT) on CFLAGS.
BSL_HDR_FILE="$BSL_DIR/project_build/adios_bsl_boundary.h"
APP_HDR_FILE="$PROJECT_DIR/project_build/adios_bsl_boundary.h"
APP_INC_FILE="$PROJECT_DIR/project_build/adios_bsl_image.inc"

# --- serialize concurrent invocations (2026-08-09). bootloader/src (and its
#     project_build/) is SHARED by every project build - two overlapping
#     runs of this script trample each other's build dir: one side's cleanall
#     "rm -rf project_build" races the other side's still-running compiler
#     ("Device or resource busy", then ".su for writing: No such file or
#     directory" once the dir vanishes under it). Never happens with
#     sequential command-line builds, but CubeIDE triggers it naturally: a
#     CANCELED IDE build kills the top-level make yet leaves the script's
#     bootloader sub-make running as an orphan, and the user's very next
#     build collides with it (observed 2026-08-09 on the real 5x6_505
#     CubeIDE console). mkdir is the portable atomic test-and-set; the
#     holder's PID is stored inside so a lock left behind by a killed/dead
#     process is detected (kill -0) and stolen instead of deadlocking.
LOCK_DIR="$BSL_DIR/.gen_bsl_boundary.lock"
LOCK_WAITED=0
while ! mkdir "$LOCK_DIR" 2>/dev/null; do
    HOLDER_PID=$(cat "$LOCK_DIR/pid" 2>/dev/null)
    if [ -n "$HOLDER_PID" ] && ! kill -0 "$HOLDER_PID" 2>/dev/null; then
        echo "Removing stale gen_bsl_boundary.sh lock left by dead process $HOLDER_PID"
        rm -rf "$LOCK_DIR"
        continue
    fi
    if [ "$LOCK_WAITED" -ge 600 ]; then
        echo "ERROR: timed out after ${LOCK_WAITED}s waiting for a concurrent gen_bsl_boundary.sh run (pid $HOLDER_PID) - if no build is actually running, remove $LOCK_DIR by hand."
        exit 1
    fi
    if [ "$LOCK_WAITED" -eq 0 ]; then
        echo "Waiting for a concurrent gen_bsl_boundary.sh run (pid $HOLDER_PID) to finish..."
    fi
    sleep 2
    LOCK_WAITED=$(( LOCK_WAITED + 2 ))
done
echo $$ > "$LOCK_DIR/pid"
# released on ANY exit (success, set -e failure, or signal) - INT/TERM/HUP
# re-raise through exit so the EXIT trap runs under every sh flavor
trap 'rm -rf "$LOCK_DIR"' EXIT
trap 'exit 1' INT TERM HUP

# --- write one variant of the generated linker script from the shared
#     per-chip template. $1 = output path, $2 = "bsl" or "app".
write_ld () {
    OUT="$1"
    VARIANT="$2"
    if [ "$VARIANT" = "updater" ]; then
        # the BSL-update tool: linked in its own dedicated window above the
        # normal app origin (see UPDATER_ORIGIN_OFF above) - FLASH_BSL region
        # dropped and .adios_bsl section remapped for the same reason as the
        # bsl variant below (the updater build defines ADIOS_DONT_INCLUDE_BSL)
        sed \
            -e '/^[[:space:]]*FLASH_BSL[[:space:]]*(rx)/d' \
            -e "s/^\([[:space:]]*\)FLASH[[:space:]]*(rx)[[:space:]]*:.*/\1FLASH (rx)     : ORIGIN = $UPDATER_ORIGIN_HEX, LENGTH = $UPDATER_REGION_LEN/" \
            -e 's/}[[:space:]]*>FLASH_BSL/} >FLASH/' \
            "$LD_TEMPLATE" > "$OUT"
    elif [ "$VARIANT" = "bsl" ]; then
        # the bootloader doesn't reserve a separate FLASH_BSL region for
        # itself - drop that line entirely, and FLASH becomes exactly the
        # bootloader's own reserved window starting at the base of flash.
        # The .adios_bsl OUTPUT SECTION (SECTIONS block) is retargeted from
        # >FLASH_BSL to >FLASH too - it stays empty here (the bootloader
        # build defines ADIOS_DONT_INCLUDE_BSL, it never embeds a copy of
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

# the bootloader's own sub-make needs ADIOS_PATH relative to ITS OWN
# directory (bootloader/src is always exactly 2 levels below the repo
# root) - NOT the inherited value, which is relative to whatever depth the
# CALLING project sits at (e.g. "../../.." for a project 3 levels deep) and
# would resolve one directory too far up once this script cd's into
# bootloader/src. An absolute ADIOS_PATH would avoid that mismatch too, but
# GNU Make's rule parser chokes on a Windows drive-letter colon ("E:/...")
# embedded in the absolute source paths adios.mk builds from it.
#
# A literal relative value ("../..") doesn't work either though: THUMB_SOURCE
# paths built from it (e.g. "../../adios/STM32G0xx/adios_sys.c") carry
# real ".." segments, and common.mk's object rule just prefixes $(PROJECT_OUT)
# onto whatever source path it's given - it doesn't textually contain that
# ".."; mkdir -p/gcc -o resolve it as a real upward directory walk, scattering
# adios/etc/drivers directories one level above bootloader/src on every
# single build (found & fixed 2026-08-04, after being found once already
# earlier this project and mistakenly assumed fixed).
#
# Fix: a same-directory symlink, bootstrapped once if missing, standing in
# for the repo root - "REPO_ROOT_LINK/adios/..." has the same "path depth"
# as "../../adios/..." to the OS, but contains no ".." token for Make/mkdir
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
    # the link target must be resolved to an ABSOLUTE path first: a relative
    # ADIOS_PATH (e.g. "../../..", the app Makefiles' self-locating default
    # since 2026-08-09) is relative to THIS script's cwd (the app dir), but a
    # relative symlink target is resolved against the link's OWN directory
    # (bootloader/src) - storing it as-is would point outside the repo.
    MSYS=winsymlinks:nativestrict ln -s "$(cd "$ADIOS_PATH" && pwd)" "$REPO_ROOT_LINK" 2>/dev/null || true
fi
if [ -L "$REPO_ROOT_LINK" ]; then
    BSL_SUBMAKE_ADIOS_PATH=".repo_root"
else
    BSL_SUBMAKE_ADIOS_PATH="../.."
fi

build_bootloader () {
    # safe to redirect here: project_build/ exists (created at parse time and
    # re-created after each clean below) and nothing removes it during a build.
    ( cd "$BSL_DIR" && ADIOS_PATH=$BSL_SUBMAKE_ADIOS_PATH make -f "$BSL_MAKEFILE" PROCESSOR="$CHIP" > "$BSL_LOG" 2>&1 ) || {
        echo "Bootloader build failed, see $BSL_LOG"
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
#     32K comfortably fits any ADIOS bootloader variant built so far
#     (including the heavier F4+USB ones) and is discarded immediately once
#     the real size is known.
#     MUST be unconditional (not "only if missing"): a leftover cpu_bsl.ld
#     from a PREVIOUS run for a DIFFERENT chip (different RAM size, register
#     map, even different family) would otherwise silently get reused for
#     pass 1 here, at best wasting a build, at worst linking pass 1 against
#     the wrong MEMORY block entirely.
# bootloader/src/project_build is SHARED by every chip (one generic
# Makefile.bsl, one bootloader/src directory) - make's default
# dependency tracking follows source file mtimes only, not CFLAGS/-D changes,
# so a stale .o compiled for a DIFFERENT chip/family in a PREVIOUS invocation
# would otherwise be silently reused here even though its FAMILY defines no
# longer match. Always start from a clean slate - and BEFORE the bootstrap
# seed below, not after: cleanall's `clean:` target is `rm -rf $(PROJECT_OUT)`,
# which would otherwise delete the cpu_bsl.ld the seed step just wrote, now
# --- BSL_HOLD pin override (optional): if the project's own adios_config.h
#     defines ADIOS_BSL_HOLD_PORT_OVERRIDE / ADIOS_BSL_HOLD_PIN_OVERRIDE,
#     relay them into the BOOTLOADER's copy of the generated header only -
#     never into the project's own copy, since the project's adios_config.h
#     already defines them itself and including them there too would just be
#     a duplicate #define in the same translation unit. This is the same
#     two-pass-build channel already used for ADIOS_APP_FLASH_START_ADDR,
#     just carrying one more project-level compile-time constant. Not a
#     runtime/flash mechanism - both binaries are compiled together, so a
#     plain preprocessor relay is enough (see 2026-08-01 plan review).
#     Read from RELAY_SRC_DIR, not PROJECT_DIR: the updater builds from its
#     own directory, which holds no board facts (see the relay block below).
RELAY_SRC_DIR="${BSL_RELAY_SRC:-$PROJECT_DIR}"
HOLD_PIN_OVERRIDES=""
if [ -f "$RELAY_SRC_DIR/adios_config.h" ]; then
    HOLD_PIN_OVERRIDES=$(grep -E '^[[:space:]]*#define[[:space:]]+ADIOS_BSL_HOLD_(PORT|PIN)_OVERRIDE\b' "$RELAY_SRC_DIR/adios_config.h" || true)
fi

# --- board MIDI wiring relay (same channel, larger payload): a bootloader and
#     its application share one physical MIDI connector, so which port the
#     bootloader talks on, its TX polarity and its pin drive mode are facts
#     about the BOARD, not about the bootloader. Hardcoding them in
#     bootloader/src/adios_config.h made that file describe exactly one
#     instrument - any other board got a bootloader driving the wrong pins,
#     or a peripheral its chip doesn't even have (a G030K6 has no USART3).
#     A project marks the relevant lines of its own adios_config.h with
#     BSL_RELAY_BEGIN/END and they are copied VERBATIM into the bootloader's
#     and the updater's generated header. Verbatim on purpose: no whitelist
#     of macro names to keep in sync, and the project stays the single place
#     where its wiring is written - the very same lines serve its own build.
#     Exactly ONE block is allowed, deliberately: relaying by macro name
#     would pick up every family branch of a config that has several and
#     quietly emit contradictory #defines.
#     BSL_RELAY_SRC names the directory whose adios_config.h carries the
#     block. It is PROJECT_DIR for a normal app build, but the updater builds
#     from bootloader/updater/ - its own directory holds no board wiring, so
#     common.mk points it back at the application being built.
BSL_RELAY_BLOCK=""
if [ -f "$RELAY_SRC_DIR/adios_config.h" ]; then
    RELAY_BEGINS=$(grep -c '^[[:space:]]*//[[:space:]]*BSL_RELAY_BEGIN' "$RELAY_SRC_DIR/adios_config.h" || true)
    RELAY_ENDS=$(grep -c '^[[:space:]]*//[[:space:]]*BSL_RELAY_END' "$RELAY_SRC_DIR/adios_config.h" || true)
    if [ "$RELAY_BEGINS" != "$RELAY_ENDS" ]; then
        echo "ERROR: $RELAY_SRC_DIR/adios_config.h has $RELAY_BEGINS BSL_RELAY_BEGIN marker(s) for $RELAY_ENDS BSL_RELAY_END - they must be balanced."
        exit 1
    fi
    if [ "$RELAY_BEGINS" -gt 1 ]; then
        echo "ERROR: $RELAY_SRC_DIR/adios_config.h has $RELAY_BEGINS BSL_RELAY blocks - exactly one is allowed, or the bootloader would receive contradictory wiring."
        exit 1
    fi
    if [ "$RELAY_BEGINS" = "1" ]; then
        BSL_RELAY_BLOCK=$(sed -n '/^[[:space:]]*\/\/[[:space:]]*BSL_RELAY_BEGIN/,/^[[:space:]]*\/\/[[:space:]]*BSL_RELAY_END/p' "$RELAY_SRC_DIR/adios_config.h" | sed '1d;$d')
    fi
fi

write_header () {
    TARGET="$1"
    WITH_OVERRIDES="$2"
    {
        echo "// AUTO-GENERATED by etc/gen_bsl_boundary.sh - do not edit by hand."
        echo "// Computed from the real compiled size of the $CHIP bootloader,"
        echo "// rounded up to a $PAGE_SIZE byte flash erase-granularity boundary (min $MIN_BOUNDARY)."
        echo "#ifndef _ADIOS_BSL_BOUNDARY_H"
        echo "#define _ADIOS_BSL_BOUNDARY_H"
        echo "#define ADIOS_APP_FLASH_START_ADDR $BOUNDARY_HEX"
        # names the embedded-bootloader image written beside this header by
        # the same pass. A fixed name: there is only ever one, and it is
        # rewritten whenever this project is built.
        echo "#define ADIOS_BSL_INC_FILE \"adios_bsl_image.inc\""
        # the persistent device-ID opt-in, relayed from the project's Makefile:
        # WITHOUT it the bootloader looks for no ID at all and answers on the
        # compile-time default. It has to arrive this way rather than through
        # the BSL_RELAY block because the reservation it depends on is a
        # Makefile matter (the linker script needs it at preprocessing time).
        if [ "${ADIOS_DEVICE_ID_PERSIST:-0}" = "1" ]; then
            echo "#define ADIOS_DEVICE_ID_PERSIST 1"
        fi
        if [ "$WITH_OVERRIDES" = "yes" ] && [ -n "$HOLD_PIN_OVERRIDES" ]; then
            echo "// BSL_HOLD pin override relayed from the project's adios_config.h:"
            echo "$HOLD_PIN_OVERRIDES"
        fi
        if [ "$WITH_OVERRIDES" = "yes" ] && [ -n "$BSL_RELAY_BLOCK" ]; then
            echo "// Board MIDI wiring, copied verbatim from the BSL_RELAY block of"
            echo "// the project's adios_config.h - the bootloader talks on the same"
            echo "// connector as its application, so the project owns these facts:"
            echo "$BSL_RELAY_BLOCK"
        fi
        echo "#endif"
    } > "$TARGET"
}

# that it lives inside project_build/ too (2026-08-04).
# CAPTURED, never redirected - and the difference is not cosmetic. The log
# lives inside project_build/, and this very command removes project_build/.
# A `> "$BSL_LOG"` would hold that file open across the removal: Windows then
# refuses to delete the directory (sharing violation) and the build stops,
# while Unix deletes it and keeps writing into an unlinked inode - the log
# vanishes in silence and the error message points at a file that is not
# there. Capturing holds nothing open, and the output is written only on
# failure, which is exactly when it gets read. A successful clean has nothing
# to say anyway: it is the output of an `rm -rf`.
# `local` must never be used for this assignment - it would swallow the exit
# status and `set -e` would let a failed clean through.
cleanall_out=$( cd "$BSL_DIR" && ADIOS_PATH=$BSL_SUBMAKE_ADIOS_PATH make -f "$BSL_MAKEFILE" PROCESSOR="$CHIP" cleanall 2>&1 ) || {
    mkdir -p "$BSL_DIR/project_build"
    printf %s "$cleanall_out" > "$BSL_LOG"
    echo "Bootloader cleanall failed, see $BSL_LOG"
    exit 1
}
mkdir -p "$BSL_DIR/project_build"

BOOTSTRAP_SIZE=32768
echo "Seeding $BSL_LD_FILE from $LD_TEMPLATE at $BOOTSTRAP_SIZE bytes (link-only placeholder for pass 1)"
BOUNDARY=$BOOTSTRAP_SIZE
BOUNDARY_HEX=$(printf '0x%X' "$BOUNDARY")
write_ld "$BSL_LD_FILE" "bsl"
# ...and seed the generated header too, for the same reason: pass 1 must
# compile the bootloader with its REAL board wiring, which now arrives
# through this header (BSL_RELAY block). Without it pass 1 would either
# fail on the "no MIDI wiring" guard, or - worse, if that guard were absent -
# measure a bootloader missing its whole UART transport and derive a boundary
# too small for the real pass-2 build. The boundary written here is the
# placeholder; the real one overwrites this file after the measurement.
write_header "$BSL_HDR_FILE" "yes"

echo "=== Pass 1: building bootloader for $CHIP to measure its real size ==="
build_bootloader
# wc -c instead of stat: `stat -c%s` is GNU coreutils only - macOS/BSD stat
# spells it -f%z, wc -c is the one portable way to get a file size in bytes
BSL_SIZE=$(wc -c < "$BIN_FILE")
BSL_SIZE_PADDED=$(( BSL_SIZE + PADDING_BYTES ))

# round up to next page/sector boundary, then clamp to the safety minimum
PAGES=$(( (BSL_SIZE_PADDED + PAGE_SIZE - 1) / PAGE_SIZE ))
BOUNDARY=$(( PAGES * PAGE_SIZE ))
if [ "$BOUNDARY" -lt "$MIN_BOUNDARY" ]; then
    BOUNDARY=$MIN_BOUNDARY
fi
# Pages the application reserves for its own data at the TOP of flash
# (ADIOS_USERDATA_PAGES, relayed through the environment by
# core.mk). The linker template already carves them out of its
# FLASH region - but the rewrite further down recomputes that length from the
# chip's TOTAL flash, so the same pages have to come off here too or the
# reservation would be silently undone on every dynamic-boundary build.
# PAGE_SIZE is this family's erase granularity, already resolved above.
USERDATA_PAGES="${ADIOS_USERDATA_PAGES:-0}"
USERDATA_LEN=$(( USERDATA_PAGES * PAGE_SIZE ))

APP_FLASH_LENGTH=$(( TOTAL_FLASH - BOUNDARY - USERDATA_LEN ))

BOUNDARY_HEX=$(printf '0x%X' "$BOUNDARY")
APP_FLASH_ORIGIN_HEX=$(printf '0x%08X' $((0x08000000 + BOUNDARY)))

echo "Bootloader actual size : $BSL_SIZE bytes (+ $PADDING_BYTES padding = $BSL_SIZE_PADDED)"
echo "Final boundary (rounded to $PAGE_SIZE, min $MIN_BOUNDARY) : $BOUNDARY_HEX ($BOUNDARY bytes)"
if [ "$USERDATA_PAGES" != "0" ]; then
    echo "Application data reserved : $USERDATA_PAGES page(s) = $USERDATA_LEN bytes at the top of flash"
fi


# --- generated header for the BOOTLOADER's own build (so bsl_sysex.c's
#     protection check and main.c's jump-to-app address use the same value) ---
if [ -n "$HOLD_PIN_OVERRIDES" ]; then
    echo "Relayed BSL_HOLD pin override into bootloader build:"
    echo "$HOLD_PIN_OVERRIDES"
fi
if [ -n "$BSL_RELAY_BLOCK" ]; then
    echo "Relayed board MIDI wiring into bootloader build:"
    echo "$BSL_RELAY_BLOCK" | sed 's/^/    /'
else
    echo "NOTE: no BSL_RELAY block in $RELAY_SRC_DIR/adios_config.h - the bootloader gets no board wiring from this project."
fi

# Pass 2 must recompile EVERYTHING, not just relink. The boundary the
# bootloader was just told about is a compile-time constant baked into
# several translation units - bsl_sysex.c decides from it which sector to
# erase, main.c where to jump - and make cannot see that a generated header
# changed under objects whose .c files did not. Reusing them mixes two
# boundaries in one binary: measured on 2026-08-18, an F4 bootloader
# carrying BOTH 0x08004000 and 0x08008000, answering with the new boundary
# while erasing at the old one - which is its own tail once it outgrows the
# first sector. It bricked the board on every upload attempt.
#
# Before write_ld below, never after: clean wipes project_build/, and that is
# where the linker script it writes lives.
# captured, not redirected - same reason as the cleanall site above: this
# command removes the directory the log lives in.
clean_out=$( cd "$BSL_DIR" && ADIOS_PATH=$BSL_SUBMAKE_ADIOS_PATH make -f "$BSL_MAKEFILE" PROCESSOR="$CHIP" clean 2>&1 ) || {
    mkdir -p "$BSL_DIR/project_build"
    printf %s "$clean_out" > "$BSL_LOG"
    echo "Bootloader clean before pass 2 failed, see $BSL_LOG"
    exit 1
}
# clean removed project_build/ entirely, and BOTH the linker script written
# below and the generated boundary header live in it
mkdir -p "$BSL_DIR/project_build"

# Written HERE, after the clean, for exactly the reason the linker script is:
# it lives in project_build/, which the clean above just removed. Pass 2 has
# to see it - it carries the final boundary and the relayed board wiring, and
# a bootloader compiled without it does not know where the application starts.
write_header "$BSL_HDR_FILE" "yes"
echo "Wrote $BSL_HDR_FILE"

# --- regenerate cpu_bsl.ld with the final boundary BEFORE pass 2, so the
#     bootloader links against its real, final flash window ---
write_ld "$BSL_LD_FILE" "bsl"
echo "Wrote $BSL_LD_FILE (FLASH=$BOUNDARY bytes)"
echo "=== Pass 2: rebuilding bootloader with the final boundary baked in ==="
build_bootloader
BSL_SIZE_FINAL=$(wc -c < "$BIN_FILE")
if [ "$BSL_SIZE_FINAL" -gt "$BOUNDARY" ]; then
    echo "ERROR: bootloader grew to $BSL_SIZE_FINAL bytes on pass 2, exceeding the computed boundary $BOUNDARY - increase PADDING_BYTES and retry."
    exit 1
fi

# --- regenerate the embedded bootloader blob (adios_bsl.c includes this via
#     ADIOS_BSL_INC_FILE) ---
#
# It lands in the PROJECT's own directory, beside the adios_bsl_boundary.h
# written by the same pass, and carries a FIXED name. The bootloader is built
# per project - it takes that project's board wiring, and its boundary is
# measured from the result - so this image belongs to the project, not to the
# chip, and it is rewritten at every build of that project. A name keyed on
# the chip preserved nothing and scattered copies through the OS sources.
INC_FILE="$APP_INC_FILE"
perl "$PERL_DIR/gen_inc_file.pl" "$BIN_FILE" "$INC_FILE" adios_bsl_image adios_bsl -size="$BOUNDARY"
echo "Regenerated $INC_FILE ($BOUNDARY bytes, matches final boundary)"

if [ "$BSL_BOUNDARY_MODE" = "updater" ]; then
    # --- BSL-update tool build (BSL_BOUNDARY_MODE=updater, set by
    #     bootloader/updater/Makefile): instead of the app-side outputs,
    #     generate the updater's own linker script (dedicated window above
    #     the normal app origin) and its boundary header. Sanity-check first:
    #     the updater must sit clear above the boundary, and its own window
    #     must fit in flash.
    if [ "$BOUNDARY" -gt "$UPDATER_ORIGIN_OFF" ]; then
        echo "ERROR: boundary $BOUNDARY exceeds the updater origin $UPDATER_ORIGIN_OFF - the updater window layout (see UPDATER_ORIGIN_OFF in this script) must be moved up for this bootloader size."
        exit 1
    fi
    # The tool gets everything from its origin to the top of usable flash -
    # the application's reserved data pages excluded, exactly as the
    # application's own region excludes them. A tool too big to fit now fails
    # at the LINK, naming the overflow, instead of being silently capped.
    UPDATER_REGION_LEN=$(( TOTAL_FLASH - UPDATER_ORIGIN_OFF - USERDATA_LEN ))
    if [ "$UPDATER_REGION_LEN" -le 0 ]; then
        echo "ERROR: nothing left for the updater between its origin $UPDATER_ORIGIN_OFF and this chip's $TOTAL_FLASH bytes of flash minus $USERDATA_LEN reserved bytes."
        exit 1
    fi
    UPDATER_ORIGIN_HEX=$(printf '0x%08X' $(( 0x08000000 + UPDATER_ORIGIN_OFF )))

    UPDATER_LD_FILE="$PROJECT_DIR/project_build/cpu_updater.ld"
    write_ld "$UPDATER_LD_FILE" "updater"
    echo "Wrote $UPDATER_LD_FILE (FLASH origin=$UPDATER_ORIGIN_HEX length=$UPDATER_REGION_LEN bytes)"

    {
        echo "// AUTO-GENERATED by etc/gen_bsl_boundary.sh (updater mode) - do not edit by hand."
        echo "#ifndef _ADIOS_BSL_BOUNDARY_H"
        echo "#define _ADIOS_BSL_BOUNDARY_H"
        echo "// the CURRENT boundary of the bootloader being shipped - the ceiling of"
        echo "// the region this updater writes, and its answer to query 0x0a"
        echo "#define ADIOS_APP_FLASH_START_ADDR $BOUNDARY_HEX"
        echo "// where this updater itself is linked (entry override target for Studio,"
        echo "// and the upper bound of the old-info-block scan)"
        echo "#define ADIOS_UPDATER_ORIGIN_ADDR $UPDATER_ORIGIN_HEX"
        # The persistent device-ID opt-in, relayed exactly as it is to the
        # bootloader (see write_header): this tool answers the host on the
        # instrument's own SysEx ID, so it has to know where that ID is kept.
        # Without it the tool comes up on the compile-time default and a host
        # addressing the instrument cannot reach it.
        if [ "${ADIOS_DEVICE_ID_PERSIST:-0}" = "1" ]; then
            echo "#define ADIOS_DEVICE_ID_PERSIST 1"
        fi
        if [ -n "$BSL_RELAY_BLOCK" ]; then
            echo "// Board MIDI wiring, copied verbatim from the BSL_RELAY block of the"
            echo "// application's adios_config.h (BSL_RELAY_SRC): this tool must come"
            echo "// up on the same connector as the bootloader it installs."
            echo "$BSL_RELAY_BLOCK"
        fi
        echo "#endif"
    } > "$APP_HDR_FILE"
    echo "Wrote $APP_HDR_FILE (updater origin $UPDATER_ORIGIN_HEX)"
else
    # --- generated header consumed by the project's own adios_config.h ---
    # (no pin override relayed here: the project already defines it itself,
    # relaying it back would be a duplicate #define in the same file)
    write_header "$APP_HDR_FILE" "no"
    echo "Wrote $APP_HDR_FILE"

    # --- generated linker script for the project's own app build ---
    write_ld "$APP_LD_FILE" "app"
    echo "Wrote $APP_LD_FILE (FLASH_BSL=$BOUNDARY bytes, FLASH origin=$APP_FLASH_ORIGIN_HEX length=$APP_FLASH_LENGTH bytes)"
fi
