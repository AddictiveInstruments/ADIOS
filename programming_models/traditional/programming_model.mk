# $Id: programming_model.mk 2424 2016-11-03 00:44:05Z tk $
# defines rules building the programming model

# where is FreeRTOS located

FREE_RTOS      =    $(MIOS32_PATH)/FreeRTOS

# Make-level mirror of the MIOS32_APP_USE_FREERTOS opt-in switch
# (include/mios32/mios32_sys.h) - decides whether the FreeRTOS kernel
# sources below are even compiled/linked. Needed here, at the Make level,
# because the C-side #ifdef alone isn't enough: tasks.c/port.c/etc are
# always-referenced-by-each-other units with their own undefined external
# symbols (e.g. tasks.c unconditionally needs vApplicationStackOverflowHook,
# port.c defines SysTick_Handler) - the linker can't --gc-sections its way
# out of an unresolved reference or a symbol collision, only genuinely dead
# code. Same tiered default as mios32_sys.h (RAM<=8K or FLASH<=32K -> off) -
# keep both lists in sync if you add a processor to either one. A project
# can override this Make variable directly (on the command line or its own
# Makefile) - but if it does, it must also override the matching
# MIOS32_APP_USE_FREERTOS #define in its mios32_config.h to match, there's
# no automatic link between the two.
ifndef MIOS32_APP_USE_FREERTOS
ifeq ($(PROCESSOR),STM32G030K6)
MIOS32_APP_USE_FREERTOS = 0
endif
ifeq ($(PROCESSOR),STM32G031K8)
MIOS32_APP_USE_FREERTOS = 0
endif
MIOS32_APP_USE_FREERTOS ?= 1
endif

# extend include path
C_INCLUDE += 	-I $(MIOS32_PATH)/programming_models/traditional \
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
# chips actively used by a real project so far (5x6_505, the bootloader
# targets) keep their exact PROCESSOR name (e.g. STM32G070CB.ld) - a .ld's
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
LD_FILE_EXACT    = $(MIOS32_PATH)/etc/ld/$(FAMILY)/$(PROCESSOR).ld
LD_FILE_FALLBACK = $(MIOS32_PATH)/etc/ld/$(FAMILY)/$(shell echo $(PROCESSOR) | sed -E 's/^(.{9}).(.)$$/\1x\2/').ld
STARTUP_FILE     = $(MIOS32_PATH)/etc/startup/$(FAMILY)/startup_$(shell echo $(PROCESSOR) | cut -c1-9 | tr '[:upper:]' '[:lower:]').c

ifeq ($(FAMILY),STM32F4xx)
CFLAGS    +=    -DGCC_ARMCM3
ifneq (,$(wildcard $(LD_FILE_EXACT)))
LD_FILE = $(LD_FILE_EXACT)
else
LD_FILE = $(LD_FILE_FALLBACK)
endif
# add modules to thumb sources
THUMB_SOURCE += \
		$(MIOS32_PATH)/programming_models/traditional/main.c

ifneq ($(MIOS32_APP_USE_FREERTOS),0)
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
THUMB_SOURCE += $(MIOS32_PATH)/etc/startup/STM32F4xx/startup_stm32f4xx.c
endif
ifeq ($(FAMILY),STM32G0xx)
ifneq (,$(wildcard $(LD_FILE_EXACT)))
LD_FILE = $(LD_FILE_EXACT)
else
LD_FILE = $(LD_FILE_FALLBACK)
endif
CFLAGS    +=    -DGCC_ARMCM0
# add modules to thumb sources
THUMB_SOURCE += \
		$(MIOS32_PATH)/programming_models/traditional/main.c

ifneq ($(MIOS32_APP_USE_FREERTOS),0)
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
#   MIOS32_USE_DYNAMIC_BSL_BOUNDARY = 1
# in its own Makefile BEFORE including this file to get a real, measured
# bootloader/app split (etc/gen_bsl_boundary.sh) instead of a project-local
# noboot .ld. Builds the bootloader for $(PROCESSOR), measures its real size,
# and generates a project-local linker script (project_build/cpu_app.ld) + C
# header (mios32_bsl_boundary.h, picked up automatically by mios32_sys.h) with
# the boundary rounded to a flash page/sector - instead of a hand-picked
# hardcoded constant. This also regenerates the bootloader's own linker
# script and embedded-image .inc file, so the bootloader and app always agree
# on the same boundary. Runs unconditionally on every make invocation - the
# boundary can shift between builds as the bootloader's own code changes, so
# a cached/stale generated .ld can't be trusted.
#
# LD_TEMPLATE reuses whatever $(LD_FILE) was already resolved to above
# (LD_FILE_EXACT if this PROCESSOR has one, else LD_FILE_FALLBACK) - the same
# file gen_bsl_boundary.sh reads to derive both TOTAL_FLASH_BYTES and the
# generated cpu_app.ld/cpu_bsl.ld MEMORY layout. LD_FILE is then overridden to
# the generated, per-build project_build/cpu_app.ld. PROJECT_DIR is
# $(CURDIR) - the directory make was invoked from, i.e. the app's own folder -
# so this generalizes to any project without hardcoding its path.
################################################################################
ifeq ($(MIOS32_USE_DYNAMIC_BSL_BOUNDARY),1)
LD_TEMPLATE := $(LD_FILE)
$(shell $(MIOS32_PATH)/etc/gen_bsl_boundary.sh $(PROCESSOR) $(LD_TEMPLATE) $(CURDIR) >&2)
LD_FILE = $(CURDIR)/$(PROJECT_OUT)/cpu_app.ld
endif

THUMB_CPP_SOURCE += $(MIOS32_PATH)/programming_models/traditional/mini_cpp.cpp
ifneq ($(MIOS32_APP_USE_FREERTOS),0)
# overrides malloc()/calloc()/realloc()/free() to redirect to FreeRTOS's
# pvPortMalloc()/vPortFree() - meaningless without the kernel; without this,
# mini_cpp.cpp's operator new/delete above fall back to newlib's own default
# malloc(), which is the correct behaviour for a bare-metal build anyway.
THUMB_CPP_SOURCE += $(MIOS32_PATH)/programming_models/traditional/freertos_heap.cpp
endif

# add MIOS32 sources
include $(MIOS32_PATH)/mios32/mios32.mk

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/programming_models/traditional
