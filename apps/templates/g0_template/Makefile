
################################################################################
# g0_template - STARTING POINT for an STM32G0 application. Copy this directory,
# rename it, and edit. The deliverable filenames follow the directory name on
# their own (DYNAMIC_BSL_APP_NAME), so there is nothing to rename in here.
#
# Set up here for the SMALLEST part of the family - a Nucleo-G030K6, 32K flash
# and 8K RAM, FreeRTOS off - because a template that fits there fits anywhere
# on the family. Larger G0 parts only need PROCESSOR and the .ld changed.
#
# What to change first:
#   - PROCESSOR below, and the .ld in ld/ if your part differs
#   - MIOS32_APP_NAME1/2 in mios32_config.h
#   - the BSL_RELAY block in mios32_config.h: the MIDI connector this board is
#     actually reached on, which the bootloader and its updater inherit
#
# NOTE: no USB on this family below G0B1/G0C1 - the transport here is DIN MIDI.
################################################################################
PROCESSOR =	STM32G030K6
FAMILY    =	STM32G0xx
BOARD	  =	STM32G0GENERIC

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

THUMB_SOURCE    = app.c

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

# no application LCD driver: MIOS32_DONT_USE_LCD is set in mios32_config.h,
# which guards out every APP_LCD_* call inside mios32_lcd.c (see the
# #if !defined(MIOS32_DONT_USE_LCD) wrapping its whole body) - so no
# app_lcd module is ever actually called here. Compiling one in anyway
# (there's no minimal/dummy option in modules/app_lcd/, only real,
# sizeable physical-controller drivers) was pure dead weight - on this
# chip specifically, enough to overflow the available app flash after the
# bootloader's own reservation (found 2026-08-04 converting this project
# to the real dynamic-boundary mechanism).

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(MIOS32_PATH)/include/makefile/common.mk
