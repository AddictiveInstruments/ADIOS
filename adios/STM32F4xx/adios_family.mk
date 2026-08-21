# defines additional rules for ADIOS family

# select driver library
DRIVER_LIB =	$(ADIOS_PATH)/drivers/$(FAMILY)/LL_HAL
TINYUSB =	$(ADIOS_PATH)/drivers/tinyusb
# enhance include path
#C_INCLUDE +=	-I $(ADIOS_PATH)/adios/$(FAMILY) -I $(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/inc -I $(DRIVER_LIB)/STM32_USB_Device_Library/Core/inc  -I $(DRIVER_LIB)/STM32_USB_HOST_Library/Core/inc -I $(DRIVER_LIB)/STM32_USB_OTG_Driver/inc -I $(DRIVER_LIB)/CMSIS/Include -I $(DRIVER_LIB)/CMSIS/ST/STM32F4xx/Include -I $(DRIVER_LIB)/CMSIS/ST/STM32F4xx/Include
C_INCLUDE +=	-I $(DRIVER_LIB)/CMSIS/Include -I $(DRIVER_LIB)/CMSIS/Device/ST/STM32F4xx/Include
C_INCLUDE +=	-I $(ADIOS_PATH)/adios/$(FAMILY) -I $(DRIVER_LIB)/STM32F4xx_HAL_Driver/inc 
C_INCLUDE +=	-I $(TINYUSB)/src
 


# The USB peripheral of this family is a Synopsys DWC2 core, so TinyUSB serves
# it with its dwc2 controller driver. This is the one place that fact is
# stated - everything above adios_usb_ll.c is family-independent.
CFLAGS += -DUSE_FULL_LL_DRIVER -DCFG_TUSB_MCU=OPT_MCU_STM32F4


# add modules to thumb sources
THUMB_SOURCE += \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_rcc.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_pwr.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_rtc.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_gpio.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_dac.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_tim.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_usart.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_exti.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_utils.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_flash.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_spi.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_dma.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_i2c.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_fmpi2c.c \
	$(DRIVER_LIB)/STM32F4xx_HAL_Driver/src/stm32f4xx_ll_adc.c


# USB: the OS layer, then TinyUSB. The linker drops the lot when no project
# asks for a USB class, so listing it here costs nothing to those that don't.
THUMB_SOURCE += \
	$(ADIOS_PATH)/adios/common/adios_usb.c \
	$(ADIOS_PATH)/adios/common/adios_usb_midi.c \
	$(ADIOS_PATH)/adios/common/adios_usb_desc.c \
	$(ADIOS_PATH)/adios/common/adios_usb_hid.c \
	$(ADIOS_PATH)/adios/common/adios_usb_msc.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_usb_ll.c \
	$(TINYUSB)/src/tusb.c \
	$(TINYUSB)/src/common/tusb_fifo.c \
	$(TINYUSB)/src/device/usbd.c \
	$(TINYUSB)/src/class/midi/midi_device.c \
	$(TINYUSB)/src/portable/synopsys/dwc2/dcd_dwc2.c \
	$(TINYUSB)/src/portable/synopsys/dwc2/dwc2_common.c \
	$(TINYUSB)/src/portable/synopsys/dwc2/hcd_dwc2.c \
	$(TINYUSB)/src/host/usbh.c \
	$(TINYUSB)/src/host/hub.c \
	$(TINYUSB)/src/class/midi/midi_host.c \
	$(TINYUSB)/src/class/hid/hid_host.c \
	$(TINYUSB)/src/class/msc/msc_host.c

THUMB_AS_SOURCE +=

# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/adios/$(FAMILY)
