
################################################################################
# Everything that CONFIGURES this application lives in adios_config.h - the
# single source of truth, for the C build and for make alike. This Makefile
# only lists the sources and asks app_config.mk to read the configuration
# (processor, LCD driver, persistence, reservations) out of the header.
################################################################################

# self-locating repo root: apps live at a fixed depth inside the repo, so a
# plain `make` works with no environment setup at all (any exported
# ADIOS_PATH - e.g. from a CubeIDE workspace - still overrides this default)
ADIOS_PATH ?= ../../..

include $(ADIOS_PATH)/include/makefile/app_config.mk


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
