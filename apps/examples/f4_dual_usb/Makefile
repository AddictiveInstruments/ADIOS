
################################################################################
# f4_dual_usb - EXAMPLE. A board with TWO USB sockets: the type-B receptacle is
# the device port (OTG_FS) and the type-A one drives whatever is plugged into it
# (OTG_HS). The mechanics decide the roles, so nothing has to be detected - which
# is what makes this the simpler of the two USB examples to read first.
#
# Shows: USB device MIDI, USB host with the three classes (MIDI, HID, MSC),
# and the dynamic bootloader/app boundary on real F4 silicon.
# Built and verified on an MBHP_DIPCOREF4 with an STM32F405.
#
# For the ONE-socket case, where the connector's ID contact decides the role at
# runtime, see apps/examples/f4_dual_role_usb.
################################################################################
PROCESSOR =	STM32F407VE
FAMILY    =	STM32F4xx
BOARD	  =	MBHP_DIPCOREF4
LCD       =	dummy

# Padding can be added to the measured bootloader size before it is rounded to
# an erase unit, which is how you make a build behave as a bootloader that
# outgrew its sector without changing a line of its code:
#
#   ADIOS_BSL_PADDING = 1024      -> boundary 0x8000 instead of 0x4000
#
# Used on 2026-08-19 to exercise a boundary MIGRATION on this family for the
# first time: a device sitting at 0x4000 was sent a bootloader wanting 0x8000,
# which also forced the entry handover, the tool then being linked above the
# boundary the device declared. Both worked. Left at 0 for normal builds.

# self-locating repo root: apps live at a fixed depth inside the repo, so a
# plain `make` works with no environment setup at all (any exported
# ADIOS_PATH - e.g. from a CubeIDE workspace - still overrides this default)
ADIOS_PATH ?= ../../..

# opt into the real dynamic bootloader/app flash boundary mechanism - see
# core.mk and etc/gen_bsl_boundary.sh for the full explanation.
# STM32F407VE has no exact-named .ld template (only the package-less
# STM32F407xE.ld reference-library one) - core.mk's own
# LD_FILE_EXACT/LD_FILE_FALLBACK derivation already resolves to that file
# automatically, no override needed here.
ADIOS_USE_DYNAMIC_BSL_BOUNDARY = 1


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
include $(ADIOS_PATH)/core/core.mk

# application specific LCD driver (selected via makefile variable)
include $(ADIOS_PATH)/modules/app_lcd/$(LCD)/app_lcd.mk

# common make rules
# Please keep this include statement at the end of this Makefile. Add new modules above.
include $(ADIOS_PATH)/include/makefile/common.mk
