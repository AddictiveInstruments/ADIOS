# defines additional rules for integrating the Mass Storage Device Driver


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
ifeq ($(FAMILY),STM32F10x)
C_INCLUDE += -I $(ADIOS_PATH)/modules/msd/STM32F10x
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/msd/STM32F10x/msd.c \
	$(ADIOS_PATH)/modules/msd/STM32F10x/msd_desc.c \
	$(ADIOS_PATH)/modules/msd/STM32F10x/msd_bot.c \
	$(ADIOS_PATH)/modules/msd/STM32F10x/msd_scsi.c \
	$(ADIOS_PATH)/modules/msd/STM32F10x/msd_scsi_data.c \
	$(ADIOS_PATH)/modules/msd/STM32F10x/msd_memory.c
endif

ifeq ($(FAMILY),STM32F4xx)
C_INCLUDE += -I $(ADIOS_PATH)/modules/msd/STM32F4xx
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/msd/STM32F4xx/msd.c
endif

ifeq ($(FAMILY),LPC17xx)
C_INCLUDE += -I $(ADIOS_PATH)/modules/msd/LPC17xx
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/msd/LPC17xx/msd.c \
	$(ADIOS_PATH)/modules/msd/LPC17xx/msc_bot.c \
	$(ADIOS_PATH)/modules/msd/LPC17xx/msc_scsi.c \
	$(ADIOS_PATH)/modules/msd/LPC17xx/blockdev_sd.c
endif

# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/msd
