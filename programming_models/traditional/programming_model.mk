# $Id: programming_model.mk 2424 2016-11-03 00:44:05Z tk $
# defines rules building the programming model

# philetaylor - changed to use umm_malloc and added MemMang to the include dirs.

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

# required by FreeRTOS to select the port
ifeq ($(FAMILY),STM32F4xx)
CFLAGS    +=    -DGCC_ARMCM3
LD_FILE   = 	$(MIOS32_PATH)/etc/ld/$(FAMILY)/STM32F405RG.ld
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

THUMB_SOURCE += $(MIOS32_PATH)/etc/startup/STM32F4xx/startup_stm32f4xx.c
endif
ifeq ($(FAMILY),STM32G0xx)
ifeq ($(PROCESSOR),STM32G030K6)
LD_FILE   = 	$(MIOS32_PATH)/etc/ld/$(FAMILY)/STM32G030K6.ld 
endif
ifeq ($(PROCESSOR),STM32G031K8)
LD_FILE   = 	$(MIOS32_PATH)/etc/ld/$(FAMILY)/STM32G031K8.ld 
endif
ifeq ($(PROCESSOR),STM32G050K8)
LD_FILE   = 	$(MIOS32_PATH)/etc/ld/$(FAMILY)/STM32G050K8.ld 
endif
ifeq ($(PROCESSOR),STM32G070CB)
LD_FILE   = 	$(MIOS32_PATH)/etc/ld/$(FAMILY)/STM32G070CB.ld 
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

ifeq ($(PROCESSOR),STM32G030K6)
THUMB_SOURCE += $(MIOS32_PATH)/etc/startup/STM32G0xx/startup_stm32g030.c
endif
ifeq ($(PROCESSOR),STM32G031K8)
THUMB_SOURCE += $(MIOS32_PATH)/etc/startup/STM32G0xx/startup_stm32g031.c
endif
ifeq ($(PROCESSOR),STM32G050K8)
THUMB_SOURCE += $(MIOS32_PATH)/etc/startup/STM32G0xx/startup_stm32g050.c
endif
ifeq ($(PROCESSOR),STM32G070CB)
THUMB_SOURCE += $(MIOS32_PATH)/etc/startup/STM32G0xx/startup_stm32g070.c
endif

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
