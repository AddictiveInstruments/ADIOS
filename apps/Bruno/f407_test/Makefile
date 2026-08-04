# $Id: Makefile 1820 2013-09-01 15:44:33Z tk $

################################################################################
# f407_test - real dynamic bootloader/app boundary test on a Waveshare
# STM32F407VE board, on the ST-Link JTAG/SWD. First-ever real F4 hardware
# validation of etc/gen_bsl_boundary.sh's F4 support (extended 2026-08-04) -
# same rationale as apps/Bruno/g030k6_test on the G0 side: bring-up alone
# (bare, no bootloader) only proves the chip/board works, not that the
# dynamic boundary mechanism itself works on real F4 silicon. Permanently
# pinned (no env-var indirection) like 5x6_505. BOARD stays MBHP_DIPCOREF4:
# F4xx never gets its own per-board #ifdef chain in mios32_board.c, one
# fixed default for all STM32F4xx hardware (see
# mios32/STM32F4xx/mios32_board.c).
################################################################################
PROCESSOR =	STM32F407VE
FAMILY    =	STM32F4xx
BOARD	  =	MBHP_DIPCOREF4
LCD       =	universal

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
