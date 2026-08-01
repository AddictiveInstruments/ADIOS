# $Id: programming_model.mk 2424 2016-11-03 00:44:05Z tk $
# defines rules building the programming model

# philetaylor - changed to use umm_malloc and added MemMang to the include dirs.

# where is FreeRTOS located

FREE_RTOS      =    $(MIOS32_PATH)/FreeRTOS

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
#ARM_AS_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32f405rgtx.s
THUMB_SOURCE += \
		$(MIOS32_PATH)/programming_models/traditional/main.c \
		$(FREE_RTOS)/Source/tasks.c \
		$(FREE_RTOS)/Source/list.c \
		$(FREE_RTOS)/Source/queue.c \
		$(FREE_RTOS)/Source/timers.c \
		$(FREE_RTOS)/Source/portable/GCC/ARM_CM3/port.c \
		$(FREE_RTOS)/Source/portable/MemMang/heap_4.c

THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32f4xx.c
#THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/system_stm32f4xx.c 
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
#ARM_AS_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32f405rgtx.s
THUMB_SOURCE += \
		$(MIOS32_PATH)/programming_models/traditional/main.c \
		$(FREE_RTOS)/Source/tasks.c \
		$(FREE_RTOS)/Source/list.c \
		$(FREE_RTOS)/Source/queue.c \
		$(FREE_RTOS)/Source/timers.c \
		$(FREE_RTOS)/Source/portable/GCC/ARM_CM0/port.c \
		$(FREE_RTOS)/Source/portable/MemMang/heap_4.c

ifeq ($(PROCESSOR),STM32G030K6)
THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32g030.c
endif
ifeq ($(PROCESSOR),STM32G031K8)
THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32g031.c
endif
ifeq ($(PROCESSOR),STM32G050K8)
THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32g051.c
endif
ifeq ($(PROCESSOR),STM32G070CB)
THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/startup_stm32g070.c
endif

#THUMB_SOURCE += $(MIOS32_PATH)/programming_models/traditional/system_stm32f4xx.c 
endif


THUMB_CPP_SOURCE += $(MIOS32_PATH)/programming_models/traditional/mini_cpp.cpp \
		    $(MIOS32_PATH)/programming_models/traditional/freertos_heap.cpp

# add MIOS32 sources
include $(MIOS32_PATH)/mios32/mios32.mk

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/programming_models/traditional
