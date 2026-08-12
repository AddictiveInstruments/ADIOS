# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# g030k6_test - real dynamic bootloader/app boundary test on a Nucleo-G030K6
# (onboard user LED on PC6, see MIOS32_SOL_PORT/PIN override in
# mios32_config.h). Deliberately the SMALLEST chip in the reference library
# (32K flash, 8K RAM, "small tier" FreeRTOS-off) - the tightest, most
# revealing test of etc/gen_bsl_boundary.sh: the bootloader's own minimum
# boundary (10240 bytes) already eats a third of total flash here, unlike
# STM32G070CB (5x6_505's chip, 128K flash) where that same minimum is
# comfortable margin. Permanently pinned (no env-var indirection) like
# 5x6_505 - this project exists specifically to exercise the mechanism on
# real hardware, not to be repointed at a different chip.
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
