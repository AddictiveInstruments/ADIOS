# BSL-update tool ("the updater") - a SECOND bootloader, linked in its own
# dedicated window ABOVE the normal app origin (layout derived per family by
# etc/gen_bsl_boundary.sh in updater mode - see UPDATER_ORIGIN_OFF there).
#
# Uploaded through the EXISTING bootloader of a device like any application,
# it then receives the NEW bootloader image over the same SysEx upload
# protocol and writes it DIRECTLY into the BSL region (no staging - see the
# rationale in ../src/bsl_sysex.c), preserving the persistent info block
# (device ID, fastboot...) across the update, and resets into the fresh
# bootloader on ADIOS Studio's final 0x7f. See the BSL_UPDATER sections in
# ../src/bsl_sysex.c and ../src/main.c - this build reuses the bootloader's
# own sources, only adios_config.h (in THIS directory) and the -DBSL_UPDATER
# switch differ.
#
# The output hex carries BOTH stages in disjoint address ranges: the tool
# itself (its own window above the app origin) and the new bootloader image
# (0x08000000..boundary). ADIOS Studio runs the whole sequence from it.
#
#   make [PROCESSOR=STM32G070CB]     -> project_build/updater_cpu.hex
#
# Default PROCESSOR = the deployed 5x6_505 fleet chip.

PROCESSOR ?= STM32G070CB

# self-locating repo root (this directory is 2 levels below it) - an exported
# self-locating repo root (this directory is 3 levels below it) - an exported
ADIOS_PATH ?= ../../..

# This tool carries no MIDI wiring of its own: it talks on the same connector
# as the bootloader it installs, which is a fact about the INSTRUMENT, not
# about the tool. The three values below normally arrive from an application
# build (see its common.mk); this project is built on its own, so it names
# the application it serves.
BSL_RELAY_SRC ?= $(ADIOS_PATH)/apps/Bruno/5x6_display
ADIOS_DEVICE_ID_PERSIST ?= 1
ADIOS_USERDATA_PAGES ?= 4

# same family/board/startup derivations as ../src/Makefile.bsl - one default
# board per family, never per-chip conditionals
ifeq ($(findstring STM32F4,$(PROCESSOR)),STM32F4)
FAMILY    = 	STM32F4xx
BOARD	  = 	MBHP_DIPCOREF4
STARTUP_FILE = $(ADIOS_PATH)/etc/startup/STM32F4xx/startup_stm32f4xx.c
else
FAMILY    = 	STM32G0xx
BOARD	  = 	STM32G0GENERIC
STARTUP_FILE = $(ADIOS_PATH)/etc/startup/STM32G0xx/startup_$(shell echo $(PROCESSOR) | cut -c1-9 | tr '[:upper:]' '[:lower:]').c
endif

LCD       =     dummy


################################################################################
# Source Files, include paths and libraries
################################################################################

# the bootloader's own sources, reused as-is (BSL_UPDATER sections included
# via the define below). Spelled with $(ADIOS_PATH)/ so common.mk's object
# rule rebases them into project_build/ (a literal "../src/" would walk out).
THUMB_SOURCE    = $(STARTUP_FILE) \
				main.c \
				bsl_sysex.c \
				tr5x6_fixup.c

THUMB_AS_SOURCE =
ARM_SOURCE      =
ARM_AS_SOURCE   =

# THIS directory first: its adios_config.h (core type "UPDATER") must win
# over the bootloader's own copy in ../src
C_INCLUDE = 	-I . -I $(ADIOS_PATH)/core
A_INCLUDE = 	-I .

LIBS =


################################################################################
# Remaining variables
################################################################################

LD_FILE   = 	project_build/cpu_updater.ld
PROJECT   = 	project

DEBUG     =	-g
OPTIMIZE  =	-Os

# Target machine, 5 = TR-505 (the deployed beta boards) or 6 = TR-626. The
# desktop application pings the instrument BEFORE starting anything and gets
# an unambiguous answer, so it sends the image built for that machine - the
# tool itself deduces nothing.
TR5X6_UNIT ?= 5
UNIT_NAME := $(if $(filter 6,$(TR5X6_UNIT)),626,505)

CFLAGS =	$(DEBUG) $(OPTIMIZE) -DBSL_UPDATER -DTR5X6_FIXUP_UNIT=$(TR5X6_UNIT)


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
# build's own window) + adios_bsl_boundary.h (boundary + updater origin).
################################################################################
LD_TEMPLATE = $(ADIOS_PATH)/etc/ld/$(FAMILY).ld.S

# BSL_RELAY_SRC (passed by the app's common.mk) names the directory whose
# adios_config.h carries this board's MIDI wiring - THIS directory holds
# none, deliberately: the updater talks on the same connector as the
# bootloader it installs, which is a fact about the instrument, not about
# the tool. Without it the build stops on the "no board MIDI wiring" guard
# in adios_config.h rather than producing a tool that never answers.
# ADIOS_DEVICE_ID_PERSIST arrives from the same place and for a sharper
# reason: this script rebuilds bootloader/src and regenerates the embedded
# .inc the application links, so a bootloader built here WITHOUT the switch
# overwrites the one the application's own pass built WITH it.
# ADIOS_USERDATA_PAGES arrives the same way: this tool is linked over
# application space up to the top of flash, so it has to know which pages up
# there are the application's own data and must stay out of its region -
# the same reservation the application's linker script already honours.
GEN_BSL_BOUNDARY_STATUS := $(shell BSL_BOUNDARY_MODE=updater ADIOS_PATH=$(ADIOS_PATH) ADIOS_BSL_PADDING=$(ADIOS_BSL_PADDING) BSL_RELAY_SRC=$(BSL_RELAY_SRC) ADIOS_DEVICE_ID_PERSIST=$(ADIOS_DEVICE_ID_PERSIST) ADIOS_USERDATA_PAGES=$(ADIOS_USERDATA_PAGES) $(ADIOS_PATH)/etc/gen_bsl_boundary.sh $(PROCESSOR) $(LD_TEMPLATE) $(CURDIR) >&2; echo $$?)
ifneq ($(GEN_BSL_BOUNDARY_STATUS),0)
$(error gen_bsl_boundary.sh failed (exit $(GEN_BSL_BOUNDARY_STATUS)) - see its messages above)
endif


################################################################################
# Include source modules via additional makefiles
################################################################################

# add ADIOS sources
include $(ADIOS_PATH)/adios/adios.mk

# common make rules
include $(ADIOS_PATH)/include/makefile/common.mk


################################################################################
# Deliverable: ONE hex per chip carrying BOTH stages of a BSL update - the
# update tool (its own window above the app origin) AND the new bootloader
# image (0x08000000..boundary, straight from the pass-2 bootloader this very
# build measured). The two occupy disjoint address ranges, so a plain ihex
# concatenation (EOF record of the first part stripped) is a valid file.
# ADIOS Studio splits it by the boundary: stage 1 uploads the tool through
# the device's existing bootloader, then - recognizing the freshly booted
# core as "UPDATER" (query 0x0b) - automatically uploads the image part,
# which the updater writes into the BSL region directly, then triggers the
# finalize. One file, one click.
################################################################################
# A GENERIC name, inside project_build/ - not updater_<CHIP>.hex at the top
# of this directory. This file is rewritten at every build and immediately
# copied out under the application own name (see common.mk), so it is an
# intermediate, never a deliverable. Keying it on the chip only accumulated
# one stale copy per chip ever built, which nothing ever read again - the
# same mistake the generated linker scripts made before 2026-08-04.
all: 5x6_migration_$(UNIT_NAME).hex

5x6_migration_$(UNIT_NAME).hex: project_build/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@.tool
	$(OBJCOPY) -O ihex $(ADIOS_PATH)/bootloader/src/project_build/$(PROJECT).elf $@.payload
	grep -v '^:00000001FF' $@.tool > $@
	cat $@.payload >> $@
	rm -f $@.tool $@.payload
