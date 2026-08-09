# BSL-update tool ("the updater") - a SECOND bootloader, linked in its own
# dedicated window ABOVE the normal app origin (layout derived per family by
# etc/gen_bsl_boundary.sh in updater mode - see UPDATER_ORIGIN_OFF there).
#
# Uploaded through the EXISTING bootloader of a device like any application,
# it then receives the NEW bootloader image over the same SysEx upload
# protocol, relocated into a staging area (the live BSL region is untouched
# during the whole transfer), and applies it in one short copy window on
# MIOS Studio's final 0x7f query - preserving the persistent info block
# (device ID, fastboot...) across the update. See BSL_UPDATER sections in
# ../src/bsl_sysex.c and ../src/main.c - this build reuses the bootloader's
# own sources, only mios32_config.h (in THIS directory) and the -DBSL_UPDATER
# switch differ.
#
#   make [PROCESSOR=STM32G070CB]     -> updater_<PROCESSOR>.hex
#
# Default PROCESSOR = the deployed 5x6_505 fleet chip.

PROCESSOR ?= STM32G070CB

# self-locating repo root (this directory is 2 levels below it) - an exported
# MIOS32_PATH still overrides, same convention as the app Makefiles
MIOS32_PATH ?= ../..

# same family/board/startup derivations as ../src/Makefile.bsl - one default
# board per family, never per-chip conditionals
ifeq ($(findstring STM32F4,$(PROCESSOR)),STM32F4)
FAMILY    = 	STM32F4xx
BOARD	  = 	MBHP_DIPCOREF4
STARTUP_FILE = $(MIOS32_PATH)/etc/startup/STM32F4xx/startup_stm32f4xx.c
else
FAMILY    = 	STM32G0xx
BOARD	  = 	STM32G0GENERIC
STARTUP_FILE = $(MIOS32_PATH)/etc/startup/STM32G0xx/startup_$(shell echo $(PROCESSOR) | cut -c1-9 | tr '[:upper:]' '[:lower:]').c
endif

LCD       =     dummy


################################################################################
# Source Files, include paths and libraries
################################################################################

# the bootloader's own sources, reused as-is (BSL_UPDATER sections included
# via the define below). Spelled with $(MIOS32_PATH)/ so common.mk's object
# rule rebases them into project_build/ (a literal "../src/" would walk out).
THUMB_SOURCE    = $(STARTUP_FILE) \
				$(MIOS32_PATH)/bootloader/src/main.c \
				$(MIOS32_PATH)/bootloader/src/bsl_sysex.c

THUMB_AS_SOURCE =
ARM_SOURCE      =
ARM_AS_SOURCE   =

# THIS directory first: its mios32_config.h (core type "UPDATER") must win
# over the bootloader's own copy in ../src
C_INCLUDE = 	-I . -I $(MIOS32_PATH)/bootloader/src -I $(MIOS32_PATH)/programming_models/traditional
A_INCLUDE = 	-I .

LIBS =


################################################################################
# Remaining variables
################################################################################

LD_FILE   = 	project_build/cpu_updater.ld
PROJECT   = 	project

DEBUG     =	-g
OPTIMIZE  =	-Os

CFLAGS =	$(DEBUG) $(OPTIMIZE) -DBSL_UPDATER


################################################################################
# Updater layout generation - the same two-pass boundary mechanism as every
# app build, switched to updater mode: measures the CURRENT bootloader,
# derives the boundary, and generates project_build/cpu_updater.ld (this
# build's own window) + mios32_bsl_boundary.h (boundary + staging layout).
################################################################################
LD_TEMPLATE_EXACT    = $(MIOS32_PATH)/etc/ld/$(FAMILY)/$(PROCESSOR).ld
LD_TEMPLATE_FALLBACK = $(MIOS32_PATH)/etc/ld/$(FAMILY)/$(shell echo $(PROCESSOR) | sed -E 's/^(.{9}).(.)$$/\1x\2/').ld
ifneq (,$(wildcard $(LD_TEMPLATE_EXACT)))
LD_TEMPLATE = $(LD_TEMPLATE_EXACT)
else
LD_TEMPLATE = $(LD_TEMPLATE_FALLBACK)
endif

GEN_BSL_BOUNDARY_STATUS := $(shell BSL_BOUNDARY_MODE=updater MIOS32_PATH=$(MIOS32_PATH) $(MIOS32_PATH)/etc/gen_bsl_boundary.sh $(PROCESSOR) $(LD_TEMPLATE) $(CURDIR) >&2; echo $$?)
ifneq ($(GEN_BSL_BOUNDARY_STATUS),0)
$(error gen_bsl_boundary.sh failed (exit $(GEN_BSL_BOUNDARY_STATUS)) - see its messages above)
endif


################################################################################
# Include source modules via additional makefiles
################################################################################

# add MIOS32 sources
include $(MIOS32_PATH)/mios32/mios32.mk

# common make rules
include $(MIOS32_PATH)/include/makefile/common.mk


################################################################################
# Deliverable: one self-contained hex per chip, named accordingly
################################################################################
all: updater_$(PROCESSOR).hex

updater_$(PROCESSOR).hex: project_build/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@
