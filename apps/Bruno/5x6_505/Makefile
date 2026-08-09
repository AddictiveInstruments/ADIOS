# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# This app is permanently pinned to one specific board (2026-08-01): unlike a
# generic MIOS32 app meant to be rebuilt for whatever MIOS32_PROCESSOR/FAMILY/
# BOARD/LCD the environment happens to export, 5x6_505 always targets the same
# hardware, so these are fixed constants here instead of env-var indirection -
# no more risk of building against the wrong target because of a stale
# `source_me_*`/CubeIDE environment.
################################################################################

PROCESSOR =	STM32G070CB
FAMILY    =	STM32G0xx
BOARD	  =	STM32G0GENERIC
LCD       =	ili9488

# self-locating repo root: apps live at a fixed depth inside the repo, so a
# plain `make` works with no environment setup at all (any exported
# MIOS32_PATH - e.g. from a CubeIDE workspace - still overrides this default)
MIOS32_PATH ?= ../../..

# opt into the real dynamic bootloader/app flash boundary mechanism - see
# programming_model.mk and etc/gen_bsl_boundary.sh for the full explanation.
MIOS32_USE_DYNAMIC_BSL_BOUNDARY = 1


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

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(MIOS32_PATH)/include/makefile/common.mk
