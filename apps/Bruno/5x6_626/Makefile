
################################################################################
# following setup taken from environment variables
################################################################################

PROCESSOR =	$(ADIOS_PROCESSOR)
FAMILY    = 	$(ADIOS_FAMILY)
BOARD	  = 	$(ADIOS_BOARD)
LCD       =     $(ADIOS_LCD)


################################################################################
# Source Files, include paths and libraries
################################################################################

THUMB_SOURCE    = app.c \
				tr5x6_decod.c \
				tr5x6_rom.c \
				midio_sysex.c

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
