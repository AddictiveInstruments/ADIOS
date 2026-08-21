# defines rules building the programming model

# where is FreeRTOS located

FREE_RTOS      =    $(ADIOS_PATH)/FreeRTOS

# Make-level mirror of the ADIOS_APP_USE_FREERTOS opt-in switch
# (include/adios/adios_sys.h) - decides whether the FreeRTOS kernel
# sources below are even compiled/linked. Needed here, at the Make level,
# because the C-side #ifdef alone isn't enough: tasks.c/port.c/etc are
# always-referenced-by-each-other units with their own undefined external
# symbols (e.g. tasks.c unconditionally needs vApplicationStackOverflowHook,
# port.c defines SysTick_Handler) - the linker can't --gc-sections its way
# out of an unresolved reference or a symbol collision, only genuinely dead
# code. Same tiered default as adios_sys.h (RAM<=8K or FLASH<=32K -> off) -
# keep both lists in sync if you add a processor to either one. A project
# can override this Make variable directly (on the command line or its own
# Makefile) - but if it does, it must also override the matching
# ADIOS_APP_USE_FREERTOS #define in its adios_config.h to match, there's
# no automatic link between the two.
ifndef ADIOS_APP_USE_FREERTOS
ifeq ($(PROCESSOR),STM32G030K6)
ADIOS_APP_USE_FREERTOS = 0
endif
ifeq ($(PROCESSOR),STM32G031K8)
ADIOS_APP_USE_FREERTOS = 0
endif
ADIOS_APP_USE_FREERTOS ?= 1
endif

# extend include path
C_INCLUDE += 	-I $(ADIOS_PATH)/core \
		-I $(FREE_RTOS)/Source/include \
		-I $(FREE_RTOS)/Source/portable/GCC/ARM_CM3 \
		-I $(FREE_RTOS)/Source/portable/MemMang \

# Every PROCESSOR value in this codebase follows ST's fixed 11-character
# format: 9-char line prefix (STM32 + 2-char family + 3-char line code, e.g.
# STM32G041 or STM32F407) + 1-char package letter + 1-char density code
# (e.g. STM32G041K8 = G041 line, K package, 8 density). This lets us derive
# the right reference .ld/startup file for ANY chip mechanically instead of
# hand-listing every SKU here and having to revisit this file each time a
# new one is added to etc/ld or etc/startup.
#
# .ld naming (etc/ld/$(FAMILY)/) is NOT uniform though: the handful of
# chips actively used by a real project so far keep their exact PROCESSOR
# name (e.g. STM32G070CB.ld) - a .ld's
# content never depends on package anyway, so the much larger reference
# library added 2026-08-04 (etc/gen_bsl_boundary.sh's completion pass, ST's
# own CubeIDE MCU database as source) uses a package-less name instead
# (STM32<line>x<density>.ld, e.g. STM32G041x8.ld - "x" is ST's own
# placeholder for "any package", same convention as their CMSIS headers) to
# avoid dozens of byte-identical duplicate files. LD_FILE below tries the
# exact name first (covers the chips already wired to real Makefiles) and
# falls back to the derived package-less name for everything else.
#
# Startup file naming (etc/startup/$(FAMILY)/) has no such split - every
# chip in a subfamily (same first 9 PROCESSOR characters, regardless of
# package/density) shares the exact same vector table, so the file is
# always startup_<lowercased 9-char line prefix>.c with no exceptions to
# special-case.
# ONE preprocessed template per family (2026-08-10) instead of one linker
# script per chip: only the contiguous RAM size and the total flash ever
# differed, so those live in a table inside the template and the body is
# shared (etc/ld/adios_body.ld.inc). Both candidate keys are passed to the
# preprocessor - the exact part number and the package-less form - and the
# table matches whichever it carries, reproducing the old exact-then-
# fallback filename lookup with no logic on this side. Verified that no chip
# has both of its forms in the table, so the two can never collide.
LD_TEMPLATE_S    = $(ADIOS_PATH)/etc/ld/$(FAMILY).ld.S
LD_CHIP_DEFS     = -DADIOS_LD_CHIP_$(PROCESSOR) -DADIOS_LD_CHIP_$(shell echo $(PROCESSOR) | sed -E 's/^(.{9}).(.)$$/\1x\2/')
LD_PREPROCESS    = $(CC) -E -x assembler-with-cpp -P -I $(ADIOS_PATH)/etc/ld $(LD_CHIP_DEFS)
STARTUP_FILE     = $(ADIOS_PATH)/etc/startup/$(FAMILY)/startup_$(shell echo $(PROCESSOR) | cut -c1-9 | tr '[:upper:]' '[:lower:]').c

ifeq ($(FAMILY),STM32F4xx)
CFLAGS    +=    -DGCC_ARMCM3
# add modules to thumb sources
THUMB_SOURCE += \
		$(ADIOS_PATH)/core/main.c

ifneq ($(ADIOS_APP_USE_FREERTOS),0)
THUMB_SOURCE += \
		$(FREE_RTOS)/Source/tasks.c \
		$(FREE_RTOS)/Source/list.c \
		$(FREE_RTOS)/Source/queue.c \
		$(FREE_RTOS)/Source/timers.c \
		$(FREE_RTOS)/Source/portable/GCC/ARM_CM3/port.c \
		$(FREE_RTOS)/Source/portable/MemMang/heap_4.c
endif

# single canonical startup shared by the whole F4xx family (unlike G0xx
# below) - the Cortex-M4 vector table layout doesn't vary by subfamily the
# way G0's does, confirmed against ST's own CMSIS gcc templates (only one
# GCC startup .s exists anywhere in ST's F4xx device pack).
THUMB_SOURCE += $(ADIOS_PATH)/etc/startup/STM32F4xx/startup_stm32f4xx.c
endif
ifeq ($(FAMILY),STM32G0xx)
CFLAGS    +=    -DGCC_ARMCM0
# add modules to thumb sources
THUMB_SOURCE += \
		$(ADIOS_PATH)/core/main.c

ifneq ($(ADIOS_APP_USE_FREERTOS),0)
THUMB_SOURCE += \
		$(FREE_RTOS)/Source/tasks.c \
		$(FREE_RTOS)/Source/list.c \
		$(FREE_RTOS)/Source/queue.c \
		$(FREE_RTOS)/Source/timers.c \
		$(FREE_RTOS)/Source/portable/GCC/ARM_CM0/port.c \
		$(FREE_RTOS)/Source/portable/MemMang/heap_4.c
endif

THUMB_SOURCE += $(STARTUP_FILE)

endif

################################################################################
# Automatic bootloader/app flash boundary (opt-in): a project sets
#   ADIOS_USE_DYNAMIC_BSL_BOUNDARY = 1
# in its own Makefile BEFORE including this file to get a real, measured
# bootloader/app split (etc/gen_bsl_boundary.sh) instead of a project-local
# noboot .ld. Builds the bootloader for $(PROCESSOR), measures its real size,
# and generates a project-local linker script (project_build/cpu_app.ld) + C
# header (adios_bsl_boundary.h, picked up automatically by adios_sys.h) with
# the boundary rounded to a flash page/sector - instead of a hand-picked
# hardcoded constant. This also regenerates the bootloader's own linker
# script and embedded-image .inc file, so the bootloader and app always agree
# on the same boundary. Runs unconditionally on every make invocation - the
# boundary can shift between builds as the bootloader's own code changes, so
# a cached/stale generated .ld can't be trusted.
#
# Both paths start from the same per-family template (LD_TEMPLATE_S above);
# only who resolves it differs:
#   dynamic  -> gen_bsl_boundary.sh preprocesses it, reads the chip's total
#               flash out of it, and writes cpu_app.ld / cpu_bsl.ld with the
#               measured boundary. LD_FILE becomes the generated cpu_app.ld.
#   static   -> a plain rule below preprocesses it into project_build/cpu.ld,
#               the bootloader region reserved at a FIXED size.
# A third mode sits on the other axis: ADIOS_USE_BOOTLOADER = 0 means there
# is no bootloader at all - the application is linked at the base of flash and
# owns all of it. The boundary question then has no meaning, hence the guard
# just below.
# PROJECT_DIR is $(CURDIR) - the directory make was invoked from, i.e. the
# app's own folder - so this generalizes to any project without hardcoding it.
################################################################################
ADIOS_USE_BOOTLOADER ?= 1
ifeq ($(ADIOS_USE_BOOTLOADER),0)
ifeq ($(ADIOS_USE_DYNAMIC_BSL_BOUNDARY),1)
$(error ADIOS_USE_BOOTLOADER = 0 and ADIOS_USE_DYNAMIC_BSL_BOUNDARY = 1 contradict each other: a boundary cannot be measured against a bootloader this project does not have. Drop one of the two.)
endif
# no bootloader: no reserved region in the linker script, no embedded image,
# and the application starts at the base of flash. The C side needs to agree,
# hence ADIOS_APP_FLASH_START_ADDR = 0 - it is what the SysEx query 0x0a
# answers, and it also keeps ADIOS_SYS_ADDR_BSL_INFO_BEGIN out of existence
# (see include/adios/adios_sys.h).
LD_CHIP_DEFS += -DADIOS_LD_NO_BOOTLOADER
C_DEFINES    += -DADIOS_USE_BOOTLOADER=0 -DADIOS_DONT_INCLUDE_BSL -DADIOS_APP_FLASH_START_ADDR=0
endif


################################################################################
# Persistent SysEx device ID (opt-in).
#
# The record lives in the LAST TWO BYTES of flash - a 0x42 confirm marker
# followed by the value. That position is deliberate: it is the one place the
# application and the bootloader both find WITHOUT being told, so nothing has
# to be relayed between them and the two stay independent.
#
# Storing something means owning the page it sits in, so asking for the
# feature reserves a page when the project has reserved none. A project that
# already keeps data up there declares the real count instead and the record
# lands in its last page, needing no extra page at all.
#
# NOTE: this block MUST come before the ADIOS_USERDATA_PAGES one below - the
# ?= default has to be in place before that block tests whether the variable is
# defined, or the page would be silently missing from the linker script and the
# record would be written into the application's own code.
################################################################################
ADIOS_DEVICE_ID_PERSIST ?= 0

ifeq ($(ADIOS_DEVICE_ID_PERSIST),1)
ADIOS_USERDATA_PAGES ?= 1
ifeq ($(ADIOS_USERDATA_PAGES),0)
$(error ADIOS_DEVICE_ID_PERSIST = 1 needs somewhere to write, but ADIOS_USERDATA_PAGES is explicitly 0. Reserve at least one page, or drop the persistence.)
endif
C_DEFINES += -DADIOS_DEVICE_ID_PERSIST=1
endif


################################################################################
# ADIOS_USERDATA_PAGES: flash pages at the TOP of memory that belong to the
# application but not to its image - sound banks, settings, the device-ID
# record above.
#
# Relayed to the linker template as well as to the C side, and that is the
# whole point of it: the template shortens its FLASH region by exactly this
# much, so an application growing into its own data fails to LINK instead of
# overwriting it at the first write. Nothing prevented that until now - a
# project's FLASH region ran to the very last byte of flash, stored data
# included.
#
# Pages, not bytes: uniform 2K erase granularity on G0. F4 sectors are unequal
# and its last one is 128K, useless for a few bytes, so that family needs the
# region placed elsewhere entirely - its linker template deliberately declares
# no page size and the build stops if this is set (see adios_body.ld.inc).
################################################################################
ifdef ADIOS_USERDATA_PAGES
C_DEFINES    += -DADIOS_USERDATA_PAGES=$(ADIOS_USERDATA_PAGES)
LD_CHIP_DEFS += -DADIOS_LD_USERDATA_PAGES=$(ADIOS_USERDATA_PAGES)
endif

ifeq ($(ADIOS_USE_DYNAMIC_BSL_BOUNDARY),1)
LD_TEMPLATE := $(LD_TEMPLATE_S)
# capture the script's exit status and STOP make right here if it failed -
# $(shell ...) alone silently discards the status, letting the build plow on
# for minutes and die at link time on the missing generated cpu_app.ld with a
# misleading "cannot open linker script" error instead of the script's own
# diagnostic (observed 2026-08-09 in a CubeIDE build console).
# ADIOS_PATH is passed explicitly into the script's environment: when the
# self-locating "ADIOS_PATH ?= ../../.." Makefile default applies (nothing
# exported by the caller), $(shell ...) would not see it otherwise - GNU make
# only forwards `export`ed Makefile variables to $(shell) since 4.4, and
# macOS still ships 3.81.
# ADIOS_USERDATA_PAGES travels the same way and for a related reason: this
# script REWRITES the two FLASH lines of the template with the measured
# boundary, recomputing the application length from the chip's total flash -
# so the reservation the template carved out would be silently undone. It
# subtracts the same pages itself; the page size it already knows.
GEN_BSL_BOUNDARY_STATUS := $(shell ADIOS_PATH=$(ADIOS_PATH) ADIOS_USERDATA_PAGES=$(ADIOS_USERDATA_PAGES) ADIOS_BSL_PADDING=$(ADIOS_BSL_PADDING) ADIOS_DEVICE_ID_PERSIST=$(ADIOS_DEVICE_ID_PERSIST) $(ADIOS_PATH)/etc/gen_bsl_boundary.sh $(PROCESSOR) $(LD_TEMPLATE) $(CURDIR) >&2; echo $$?)
ifneq ($(GEN_BSL_BOUNDARY_STATUS),0)
$(error gen_bsl_boundary.sh failed (exit $(GEN_BSL_BOUNDARY_STATUS)) - see its messages above, and bootloader/src/gen_bsl_boundary_build.log for the bootloader sub-make output)
endif
LD_FILE = $(CURDIR)/$(PROJECT_OUT)/cpu_app.ld
else
# STATIC path: there is still a bootloader, its boundary is just FIXED rather
# than measured - the historical ADIOS layout, FLASH_BSL reserved from
# 0x08000000 and the application starting right after it. The boundary is a
# single define, ADIOS_LD_BSL_BOUNDARY_K, defaulted per family to the same
# value the old per-chip scripts carried (10K on G0, 16K on F4) and
# overridable by a project that reserves more.
# (Building with NO bootloader at all is the separate ADIOS_USE_BOOTLOADER
# switch handled above, not a boundary of 0.)
# The target is spelled literally rather than through $(PROJECT_OUT): that
# variable comes from common.mk, included AFTER this file, and a rule's
# target is fixed when the rule is read - it would resolve to an empty path.
ADIOS_LD_BSL_BOUNDARY_K ?= $(if $(filter STM32F4xx,$(FAMILY)),16,10)
LD_CHIP_DEFS += -DADIOS_LD_BSL_BOUNDARY_K=$(ADIOS_LD_BSL_BOUNDARY_K)
# ...and tell the C side the SAME boundary. Only the dynamic path used to
# define ADIOS_APP_FLASH_START_ADDR (through the generated header), so on a
# static build it was simply absent - leaving ADIOS_SYS_ADDR_BSL_INFO_BEGIN
# as an expression referring to an undefined macro, valid only as long as
# nothing used it. Now all three modes state it.
ifneq ($(ADIOS_USE_BOOTLOADER),0)
C_DEFINES += -DADIOS_APP_FLASH_START_ADDR=$(shell echo $$(( $(ADIOS_LD_BSL_BOUNDARY_K) * 1024 )))
endif
LD_FILE = $(CURDIR)/project_build/cpu.ld
# the RULE that builds it lives in include/makefile/common.mk, not here: this
# file is included FIRST, so a target defined at this point would become
# make's default goal and the build would stop after producing the script.
endif

THUMB_CPP_SOURCE += $(ADIOS_PATH)/core/mini_cpp.cpp
ifneq ($(ADIOS_APP_USE_FREERTOS),0)
# overrides malloc()/calloc()/realloc()/free() to redirect to FreeRTOS's
# pvPortMalloc()/vPortFree() - meaningless without the kernel; without this,
# mini_cpp.cpp's operator new/delete above fall back to newlib's own default
# malloc(), which is the correct behaviour for a bare-metal build anyway.
THUMB_CPP_SOURCE += $(ADIOS_PATH)/core/freertos_heap.cpp
endif

# add ADIOS sources
include $(ADIOS_PATH)/adios/adios.mk

# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/core
