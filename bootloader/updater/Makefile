# BSL-update tool ("the updater") - a SECOND bootloader, linked in its own
# dedicated window ABOVE the normal app origin (layout derived per family by
# etc/gen_bsl_boundary.sh in updater mode - see UPDATER_ORIGIN_OFF there).
#
# Uploaded through the EXISTING bootloader of a device like any application,
# it then receives the NEW bootloader image over the same SysEx upload
# protocol and writes it DIRECTLY into the BSL region (no staging - see the
# rationale in ../src/bsl_sysex.c), preserving the persistent info block
# (device ID, fastboot...) across the update, and resets into the fresh
# bootloader on MIOS Studio's final 0x7f. See the BSL_UPDATER sections in
# ../src/bsl_sysex.c and ../src/main.c - this build reuses the bootloader's
# own sources, only mios32_config.h (in THIS directory) and the -DBSL_UPDATER
# switch differ.
#
# The output hex carries BOTH stages in disjoint address ranges: the tool
# itself (its own window above the app origin) and the new bootloader image
# (0x08000000..boundary). MIOS Studio runs the whole sequence from it.
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
# Stale-object guard: THIS project_build is shared by every chip (one
# Makefile, PROCESSOR passed per invocation) - the same trap documented for
# bootloader/src in etc/gen_bsl_boundary.sh: make tracks source mtimes only,
# not -D changes, so switching PROCESSOR silently reuses objects compiled for
# the PREVIOUS chip. Found the hard way (2026-08-09): an updater linked for
# G070CB with a leftover G030-flavored LL_USART object configured USART3 but
# never programmed its baudrate - the tool ran, MIDI-mute, the one-file BSL
# update stalled before stage 2. Wipe the objects whenever the chip changed
# since the last build (a per-chip stamp file marks who built last).
################################################################################
PROC_STAMP := project_build/.built_for_$(PROCESSOR)
ifeq (,$(wildcard $(PROC_STAMP)))
$(shell rm -rf project_build && mkdir -p project_build && touch $(PROC_STAMP))
endif

################################################################################
# Updater layout generation - the same two-pass boundary mechanism as every
# app build, switched to updater mode: measures the CURRENT bootloader,
# derives the boundary, and generates project_build/cpu_updater.ld (this
# build's own window) + mios32_bsl_boundary.h (boundary + updater origin).
################################################################################
LD_TEMPLATE = $(MIOS32_PATH)/etc/ld/$(FAMILY).ld.S

# BSL_RELAY_SRC (passed by the app's common.mk) names the directory whose
# mios32_config.h carries this board's MIDI wiring - THIS directory holds
# none, deliberately: the updater talks on the same connector as the
# bootloader it installs, which is a fact about the instrument, not about
# the tool. Without it the build stops on the "no board MIDI wiring" guard
# in mios32_config.h rather than producing a tool that never answers.
GEN_BSL_BOUNDARY_STATUS := $(shell BSL_BOUNDARY_MODE=updater MIOS32_PATH=$(MIOS32_PATH) BSL_RELAY_SRC=$(BSL_RELAY_SRC) $(MIOS32_PATH)/etc/gen_bsl_boundary.sh $(PROCESSOR) $(LD_TEMPLATE) $(CURDIR) >&2; echo $$?)
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
# Deliverable: ONE hex per chip carrying BOTH stages of a BSL update - the
# update tool (its own window above the app origin) AND the new bootloader
# image (0x08000000..boundary, straight from the pass-2 bootloader this very
# build measured). The two occupy disjoint address ranges, so a plain ihex
# concatenation (EOF record of the first part stripped) is a valid file.
# MIOS Studio splits it by the boundary: stage 1 uploads the tool through
# the device's existing bootloader, then - recognizing the freshly booted
# core as "UPDATER" (query 0x0b) - automatically uploads the image part,
# which the updater writes into the BSL region directly, then triggers the
# finalize. One file, one click.
################################################################################
all: updater_$(PROCESSOR).hex

updater_$(PROCESSOR).hex: project_build/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@.tool
	$(OBJCOPY) -O ihex $(MIOS32_PATH)/bootloader/src/project_build/$(PROJECT).elf $@.payload
	grep -v '^:00000001FF' $@.tool > $@
	cat $@.payload >> $@
	rm -f $@.tool $@.payload
