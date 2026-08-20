# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# f4_template - STARTING POINT for an STM32F4 application. Copy this directory,
# rename it, and edit. The deliverable filenames follow the directory name on
# their own (DYNAMIC_BSL_APP_NAME), so there is nothing to rename in here.
#
# What to change first:
#   - PROCESSOR below, and the .ld in ld/ if your part differs
#   - MIOS32_APP_NAME1/2 and MIOS32_USB_PRODUCT_STR in mios32_config.h
#   - the BSL_RELAY block in mios32_config.h: the connector this board is
#     actually reached on, which the bootloader and its updater inherit
#
# Working examples of what can be built on top: apps/examples/f4_dual_usb (two
# sockets, device + host) and apps/examples/f4_dual_role_usb (one socket, role
# decided by the ID contact).
################################################################################
PROCESSOR =	STM32F407VE
FAMILY    =	STM32F4xx
BOARD	  =	MBHP_DIPCOREF4
LCD       =	dummy

# Padding can be added to the measured bootloader size before it is rounded to
# an erase unit, which is how you make a build behave as a bootloader that
# outgrew its sector without changing a line of its code:
#
#   MIOS32_BSL_PADDING = 1024      -> boundary 0x8000 instead of 0x4000
#
# Used on 2026-08-19 to exercise a boundary MIGRATION on this family for the
# first time: a device sitting at 0x4000 was sent a bootloader wanting 0x8000,
# which also forced the entry handover, the tool then being linked above the
# boundary the device declared. Both worked. Left at 0 for normal builds.

# self-locating repo root: apps live at a fixed depth inside the repo, so a
# plain `make` works with no environment setup at all (any exported
# MIOS32_PATH - e.g. from a CubeIDE workspace - still overrides this default)
MIOS32_PATH ?= ../../..

# opt into the real dynamic bootloader/app flash boundary mechanism - see
# programming_model.mk and etc/gen_bsl_boundary.sh for the full explanation.
# STM32F407VE has no exact-named .ld template (only the package-less
# STM32F407xE.ld reference-library one) - programming_model.mk's own
# LD_FILE_EXACT/LD_FILE_FALLBACK derivation already resolves to that file
# automatically, no override needed here.
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

# application specific LCD driver (selected via makefile variable)
include $(MIOS32_PATH)/modules/app_lcd/$(LCD)/app_lcd.mk

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(MIOS32_PATH)/include/makefile/common.mk
