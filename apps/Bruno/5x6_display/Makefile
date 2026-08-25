
################################################################################
# This app is permanently pinned to one specific board (2026-08-01): unlike a
# generic ADIOS app meant to be rebuilt for whatever ADIOS_PROCESSOR/FAMILY/
# BOARD/LCD the environment happens to export, 5x6_505 always targets the same
# hardware, so these are fixed constants here instead of env-var indirection -
# no more risk of building against the wrong target because of a stale
# `source_me_*`/CubeIDE environment.
################################################################################

PROCESSOR =	STM32G070CB
FAMILY    =	STM32G0xx
BOARD	  =	STM32G0GENERIC
LCD       =	ili9488_5x6

# self-locating repo root: apps live at a fixed depth inside the repo, so a
# plain `make` works with no environment setup at all (any exported
# ADIOS_PATH - e.g. from a CubeIDE workspace - still overrides this default)
ADIOS_PATH ?= ../../..

# opt into the real dynamic bootloader/app flash boundary mechanism - see
# core.mk and etc/gen_bsl_boundary.sh for the full explanation.
ADIOS_USE_DYNAMIC_BSL_BOUNDARY = 1

# The SysEx device ID survives a power cycle: ADIOS keeps it in the last two
# bytes of flash, this application writes it there from its settings menu
# (TR5X6_ROM_DeviceIDStore) and the bootloader reads the same two bytes.
ADIOS_DEVICE_ID_PERSIST = 1

# Flash pages at the top of memory that belong to the application but not to
# its image - they leave the linker's FLASH region, so a growing application
# fails to LINK instead of destroying them at the first write.
# How many depends on the board revision, and the revision is decided in
# adios_config.h (APP_HARD_REV, where the C side reads it) - so it is
# extracted from there rather than repeated here, where the two copies
# would drift apart:
#   1 - no EEPROM: the bank records live in the last 4 pages
#   2 - AT24C64:   only the system page (magic, device ID, settings) stays
APP_HARD_REV := $(shell sed -n 's/^\#define APP_HARD_REV[ \t]*\([0-9]*\).*/\1/p' adios_config.h)
ifeq ($(APP_HARD_REV),2)
ADIOS_USERDATA_PAGES = 1
else
ADIOS_USERDATA_PAGES = 4
endif


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
include $(ADIOS_PATH)/core/core.mk

# application specific LCD driver (selected via makefile variable)
include $(ADIOS_PATH)/modules/app_lcd/$(LCD)/app_lcd.mk

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(ADIOS_PATH)/include/makefile/common.mk
