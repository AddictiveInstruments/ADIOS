# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# f407_test - first hardware bring-up test for a Waveshare STM32F407VE board
# on the ST-Link JTAG/SWD. No bootloader involved (flashed directly via SWD
# at 0x08000000, see ld/STM32F407VE_noboot.ld) - same pattern as
# apps/Bruno/g030k6_test for STM32G0xx. BOARD stays MBHP_DIPCOREF4: F4xx
# never gets its own per-board #ifdef chain in mios32_board.c, one fixed
# default for all STM32F4xx hardware (see mios32/STM32F4xx/mios32_board.c).
################################################################################
PROCESSOR ?=	STM32F407VE
FAMILY    ?= 	STM32F4xx
BOARD	  ?= 	MBHP_DIPCOREF4
LCD       ?=   universal


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

# override: no bootloader in this test project, vector table needs to sit
# at the fixed physical boot address 0x08000000 - see ld/STM32F407VE_noboot.ld
LD_FILE = ld/STM32F407VE_noboot.ld

# application specific LCD driver (selected via makefile variable)
include $(MIOS32_PATH)/modules/app_lcd/$(LCD)/app_lcd.mk

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(MIOS32_PATH)/include/makefile/common.mk
