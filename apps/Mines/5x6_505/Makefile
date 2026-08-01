# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# This app is permanently pinned to one specific board (2026-08-01): unlike a
# generic MIOS32 app meant to be rebuilt for whatever MIOS32_PROCESSOR/FAMILY/
# BOARD/LCD the environment happens to export, 5x6_505 always targets the same
# hardware, so these are fixed constants here instead of env-var indirection -
# no more risk of building against the wrong target because of a stale
# `source_me_*`/CubeIDE environment.
################################################################################

APP_NAME  =	5x6_505
PROCESSOR =	STM32G070CB
FAMILY    =	STM32G0xx
BOARD	  =	STM32G0GENERIC
LCD       =	ili9488


################################################################################
# Source Files, include paths and libraries
################################################################################

THUMB_SOURCE    = app.c \
				tr5x6_decod.c \
				tr5x6_rom.c \
				midio_sysex.c\
				tr5x6_pict.c

# (following source stubs not relevant for Cortex M3 derivatives)
THUMB_AS_SOURCE =
ARM_SOURCE      =
ARM_AS_SOURCE   =

C_INCLUDE = 	-I .
A_INCLUDE = 	-I .

LIBS =


################################################################################
# Remaining variables
################################################################################


PROJECT   = 	project

DEBUG     =	-g
OPTIMIZE  =	-O0

CFLAGS =	$(DEBUG) $(OPTIMIZE)


################################################################################
# Include source modules via additional makefiles
################################################################################

# sources of programming model
include $(MIOS32_PATH)/programming_models/traditional/programming_model.mk

# application specific LCD driver (selected via makefile variable)
include $(MIOS32_PATH)/modules/app_lcd/$(LCD)/app_lcd.mk

################################################################################
# Automatic bootloader/app flash boundary
# Builds the bootloader for $(PROCESSOR), measures its real size, and generates
# a project-local linker script (<PROCESSOR>_generated.ld) + C header
# (mios32_bsl_boundary.h) with the boundary rounded to a flash page - instead
# of a hand-picked hardcoded constant. See etc/gen_bsl_boundary.sh.
# NOTE: TOTAL_FLASH_BYTES (128K) is specific to STM32G070CB - adjust if this
# Makefile is ever repointed at a different chip (see PROCESSOR above).
################################################################################
TOTAL_FLASH_BYTES = 131072
$(shell $(MIOS32_PATH)/etc/gen_bsl_boundary.sh $(PROCESSOR) $(TOTAL_FLASH_BYTES) $(MIOS32_PATH)/etc/ld/$(FAMILY)/$(PROCESSOR).ld $(MIOS32_PATH)/apps/Mines/5x6_505 2048 >&2)
LD_FILE = $(MIOS32_PATH)/apps/Mines/5x6_505/$(PROCESSOR)_generated.ld

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(MIOS32_PATH)/include/makefile/common.mk

################################################################################
# Production deliverables (2026-08-01), named after this app (not the CPU) and
# placed directly in this app's own folder - never inside project_build/:
#   1) a single combined image (bootloader+app) for one-shot SWD flashing of
#      fresh production boards, no MIDI upload needed for testing every unit.
#      project_build/project.bin already IS this: mios32_bsl.c embeds an exact
#      copy of the bootloader at LMA 0x08000000 ahead of the real app code at
#      0x08003000 (confirmed byte-identical to the standalone bootloader
#      build) - flash the whole file at 0x08000000 and both regions land
#      correctly. Copied here under an explicit name so it's never confused
#      with the app-only artifacts below. The bootloader itself never gets
#      its own separate deliverable - like its source, it only ever appears
#      embedded inside this combined image.
#   2) a clean app-only .hex (the embedded bootloader copy above is excluded)
#      for injecting via MIOS Studio without touching SWD - the normal
#      end-user/field update path.
################################################################################
all: $(APP_NAME)_full_bsl_app.bin $(APP_NAME)_app_only.hex

$(APP_NAME)_full_bsl_app.bin: project_build/$(PROJECT).bin
	cp $< $@

$(APP_NAME)_app_only.hex: project_build/$(PROJECT).elf
	$(OBJCOPY) --remove-section=.mios32_bsl -O ihex $< $@
