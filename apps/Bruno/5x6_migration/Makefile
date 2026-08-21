
################################################################################
# 5x6 ROM one-shot migration tool - see app.c for what it does and why it is a
# separate application rather than a check inside the firmware.
#
# Pinned to the same hardware as 5x6_505, and for the same reason: it is loaded
# on that instrument and nothing else. No environment-variable indirection, so
# a stale CubeIDE environment cannot build it for the wrong target.
################################################################################

PROCESSOR =	STM32G070CB
FAMILY    =	STM32G0xx
BOARD	  =	STM32G0GENERIC
LCD       =	dummy

# self-locating repo root: apps live at a fixed depth inside the repo, so a
# plain `make` works with no environment setup at all (any exported
# ADIOS_PATH - e.g. from a CubeIDE workspace - still overrides this default)
ADIOS_PATH ?= ../../..

# same boundary mechanism as the firmware: this tool is uploaded through the
# very bootloader the update just installed, so it has to be linked where that
# bootloader expects an application to start
ADIOS_USE_DYNAMIC_BSL_BOUNDARY = 1

# It WRITES the device-ID record, so it needs the same view of flash as the
# firmware: the record's address, and the four sound-ROM pages kept out of the
# linker's reach. Same two values as 5x6_505's Makefile - they describe the
# instrument, not the program.
ADIOS_DEVICE_ID_PERSIST = 1
ADIOS_USERDATA_PAGES = 4


################################################################################
# Source Files, include paths and libraries
################################################################################

THUMB_SOURCE    = app.c

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
OPTIMIZE  =	-Os

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
