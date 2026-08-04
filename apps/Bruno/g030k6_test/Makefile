# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# following setup taken from environment variables
################################################################################

# g030k6_test - hardware smoke test for the MIOS32_CORE_USE_FREERTOS bare-
# metal path on a Nucleo-G030K6 (onboard user LED on PC6, see
# MIOS32_SOF_LED_PORT/PIN override in mios32_config.h)
PROCESSOR ?=	STM32G030K6
FAMILY    ?= 	STM32G0xx
BOARD	  ?= 	STM32G0GENERIC
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
# at the fixed physical boot address 0x08000000 - see ld/STM32G030K6_noboot.ld
LD_FILE = ld/STM32G030K6_noboot.ld

# application specific LCD driver (selected via makefile variable)
include $(MIOS32_PATH)/modules/app_lcd/$(LCD)/app_lcd.mk

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(MIOS32_PATH)/include/makefile/common.mk
