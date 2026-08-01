#!/bin/sh
# gen_bsl_boundary.sh
#
# Computes the real bootloader/app flash boundary for a project instead of
# using a hand-picked hardcoded constant, and makes the BOOTLOADER ITSELF
# consistent with that boundary (not just the app) - see the 2026-07-31
# real-hardware test that faulted because only the app side was updated:
# the bootloader embedded in flash still jumped to the old address.
#
# Steps:
#   1) build the bootloader for the given chip (Makefile.bsl_<CHIP>) - pass 1,
#      to measure its size (whatever boundary value it currently holds does
#      not affect its own compiled size)
#   2) measure the real compiled size of project_build/project.bin
#   3) round it up to the next flash page boundary (default 0x800 = 2K on G0),
#      clamped to a safety MINIMUM (default 0x2800) because STM32G0xx keeps a
#      small persistent config block (device ID, fast-boot flag, etc, see
#      MIOS32_SYS_ADDR_BSL_INFO_BEGIN in include/mios32/mios32_sys.h) at a
#      FIXED offset (0x2700) that must stay inside the protected BSL region -
#      never shrink the boundary below this without also moving that block.
#   4) writes mios32_bsl_boundary.h into bootloader/src AND the project dir
#   5) rebuilds the bootloader - pass 2, now with the final boundary baked in
#      (bsl_sysex.c's protection check AND main.c's jump-to-app address both
#      read MIOS32_APP_FLASH_START_ADDR from the generated header)
#   6) regenerates the embedded bootloader blob (mios32_bsl_<CHIP>.inc, in the
#      shared mios32/STM32G0xx folder - included by mios32_bsl.c) from this
#      final pass-2 binary, sized to match the final boundary exactly
#   7) writes the project's own linker script copy (<CHIP>_generated.ld) with
#      FLASH_BSL/FLASH origins and lengths matching the same final boundary
#
# Also relays an optional project-level BSL_HOLD pin override
# (MIOS32_BSL_HOLD_PORT_OVERRIDE / MIOS32_BSL_HOLD_PIN_OVERRIDE, defined by
# the project's own mios32_config.h) into the bootloader's copy of the
# generated header only, via the same channel as MIOS32_APP_FLASH_START_ADDR -
# see step 4/6 above.
#
# Usage:
#   gen_bsl_boundary.sh <CHIP> <TOTAL_FLASH_BYTES> <LD_TEMPLATE> <PROJECT_DIR> [PAGE_SIZE] [PADDING_BYTES] [MIN_BOUNDARY]
#
# PADDING_BYTES (default 0) is added to the measured bootloader size before
# rounding - safety margin for future bootloader growth, or for testing that
# the boundary genuinely propagates end-to-end with a different value.
#
# Requires MIOS32_PATH and MIOS32_GCC_PREFIX to already be exported (same as
# for a normal MIOS32 build).

set -e

CHIP="$1"
TOTAL_FLASH="$2"
LD_TEMPLATE="$3"
PROJECT_DIR="$4"
PAGE_SIZE="${5:-2048}"
PADDING_BYTES="${6:-0}"
MIN_BOUNDARY="${7:-10240}" # 0x2800 - see MIOS32_SYS_ADDR_BSL_INFO_BEGIN note above

if [ -z "$PROJECT_DIR" ]; then
    echo "Usage: gen_bsl_boundary.sh <CHIP> <TOTAL_FLASH_BYTES> <LD_TEMPLATE> <PROJECT_DIR> [PAGE_SIZE] [PADDING_BYTES] [MIN_BOUNDARY]"
    exit 1
fi

if [ -z "$MIOS32_PATH" ]; then
    echo "MIOS32_PATH must be exported first"
    exit 1
fi

BSL_DIR="$MIOS32_PATH/bootloader/src"
BSL_MAKEFILE="Makefile.bsl_$CHIP"
BIN_FILE="$BSL_DIR/project_build/project.bin"

build_bootloader () {
    ( cd "$BSL_DIR" && make -f "$BSL_MAKEFILE" > /tmp/gen_bsl_boundary_build.log 2>&1 ) || {
        echo "Bootloader build failed, see /tmp/gen_bsl_boundary_build.log"
        exit 1
    }
    if [ ! -f "$BIN_FILE" ]; then
        echo "Expected bootloader binary not found: $BIN_FILE"
        exit 1
    fi
}

echo "=== Pass 1: building bootloader for $CHIP to measure its real size ==="
build_bootloader
BSL_SIZE=$(stat -c%s "$BIN_FILE")
BSL_SIZE_PADDED=$(( BSL_SIZE + PADDING_BYTES ))

# round up to next page boundary, then clamp to the safety minimum
PAGES=$(( (BSL_SIZE_PADDED + PAGE_SIZE - 1) / PAGE_SIZE ))
BOUNDARY=$(( PAGES * PAGE_SIZE ))
if [ "$BOUNDARY" -lt "$MIN_BOUNDARY" ]; then
    BOUNDARY=$MIN_BOUNDARY
fi
APP_FLASH_LENGTH=$(( TOTAL_FLASH - BOUNDARY ))

BOUNDARY_HEX=$(printf '0x%X' "$BOUNDARY")
APP_FLASH_ORIGIN_HEX=$(printf '0x%08X' $((0x08000000 + BOUNDARY)))

echo "Bootloader actual size : $BSL_SIZE bytes (+ $PADDING_BYTES padding = $BSL_SIZE_PADDED)"
echo "Final boundary (page-rounded, min $MIN_BOUNDARY) : $BOUNDARY_HEX ($BOUNDARY bytes)"

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
        echo "// rounded up to a $PAGE_SIZE byte flash page boundary (min $MIN_BOUNDARY)."
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

echo "=== Pass 2: rebuilding bootloader with the final boundary baked in ==="
build_bootloader
BSL_SIZE_FINAL=$(stat -c%s "$BIN_FILE")
if [ "$BSL_SIZE_FINAL" -gt "$BOUNDARY" ]; then
    echo "ERROR: bootloader grew to $BSL_SIZE_FINAL bytes on pass 2, exceeding the computed boundary $BOUNDARY - increase PADDING_BYTES and retry."
    exit 1
fi

# --- regenerate the embedded bootloader blob (mios32_bsl.c includes this by name) ---
INC_FILE="$MIOS32_PATH/mios32/STM32G0xx/mios32_bsl_${CHIP}.inc"
perl "$BSL_DIR/gen_inc_file.pl" "$BIN_FILE" "$INC_FILE" mios32_bsl_image mios32_bsl -size="$BOUNDARY"
echo "Regenerated $INC_FILE ($BOUNDARY bytes, matches final boundary)"

# --- generated header consumed by the project's own mios32_config.h ---
# (no pin override relayed here: the project already defines it itself,
# relaying it back would be a duplicate #define in the same file)
write_header "$PROJECT_DIR/mios32_bsl_boundary.h" "no"
echo "Wrote $PROJECT_DIR/mios32_bsl_boundary.h"

# --- generated linker script, derived from the shared per-chip template ---
LD_FILE="$PROJECT_DIR/${CHIP}_generated.ld"
sed \
    -e "s/FLASH_BSL (rx) :    ORIGIN = 0x08000000, LENGTH = [0-9]*K/FLASH_BSL (rx) :    ORIGIN = 0x08000000, LENGTH = $BOUNDARY/" \
    -e "s/FLASH (rx) :    ORIGIN = 0x[0-9A-Fa-f]*, LENGTH = [0-9]*K/FLASH (rx) :    ORIGIN = $APP_FLASH_ORIGIN_HEX, LENGTH = $APP_FLASH_LENGTH/" \
    "$LD_TEMPLATE" > "$LD_FILE"
echo "Wrote $LD_FILE (FLASH_BSL=$BOUNDARY bytes, FLASH origin=$APP_FLASH_ORIGIN_HEX length=$APP_FLASH_LENGTH bytes)"
