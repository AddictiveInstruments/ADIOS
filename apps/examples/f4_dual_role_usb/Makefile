
################################################################################
# f4_dual_role_usb - EXAMPLE. ONE micro-AB socket that is a device socket and a
# host socket at different moments, decided by the receptacle's ID contact
# rather than by a build. This is the single-port dual-role case: a processor
# with one connector must be able to be either.
#
# Shows: role detection on the ID pin, the VBUS power switch a host needs, the
# over-current flag, and a debug MIDI output on a wire - because the socket
# itself is the thing under test and has no computer on the other end while it
# hosts. Built and verified on a Waveshare Core407V.
#
# Board wiring it depends on, all declared in mios32_config.h:
#   PA10  ID contact of the micro-AB      (grounded by an A plug = host)
#   PA3   enable of the MIC2075-2 power switch, active low, pulled up
#   PA5   that switch's over-current flag, open drain, active low
#   PA2   debug MIDI out - readable in BOTH roles, unlike the socket itself
#   PD6   the peripheral's RX, moved off PA3 which the switch now owns
# The 5 V selector must be on EXTERNAL supply whenever this board hosts.
#
# For the simpler TWO-socket case, see apps/examples/f4_dual_usb.
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
# The mechanism it exercises - a device sitting at one boundary being sent a
# bootloader that wants another - was validated on this family in both
# directions. Left at 0 for normal builds.

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
