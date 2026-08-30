################################################################################
# terminal_test - a minimal ADIOS example: a text terminal over the ADIOS debug
# protocol. Type a command in ADIOS Studio's "Terminal (Debug)", the core
# answers via DEBUG_MSG. Everything that configures the build (processor, MIDI
# connector) lives in adios_config.h - the single source of truth.
################################################################################

# self-locating repo root: apps live at a fixed depth, so a plain `make` works
# with no environment setup (an exported ADIOS_PATH still overrides this).
ADIOS_PATH ?= ../../..

# read processor / reservations / dynamic-BSL boundary out of adios_config.h
include $(ADIOS_PATH)/include/makefile/app_config.mk


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
OPTIMIZE  =	-O0

CFLAGS =	$(DEBUG) $(OPTIMIZE)


################################################################################
# Include source modules via additional makefiles
################################################################################

# sources of programming model (APP_Init/APP_* hooks + the ADIOS core/MIDI)
include $(ADIOS_PATH)/core/core.mk

# no app LCD driver: ADIOS_DONT_USE_LCD in adios_config.h guards out every
# APP_LCD_* call, so no app_lcd module is needed here.

# common make rules - keep this include last.
include $(ADIOS_PATH)/include/makefile/common.mk
