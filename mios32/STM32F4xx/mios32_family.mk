# $Id: mios32_family.mk 2029 2014-08-09 20:13:05Z tk $
# defines additional rules for MIOS32 family

# select driver library
#DRIVER_LIB =	$(MIOS32_PATH)/drivers/$(FAMILY)/v1.1.0
DRIVER_LIB =	$(MIOS32_PATH)/drivers/$(FAMILY)/LL_HAL
DRIVER_USB_LIB =	$(MIOS32_PATH)/drivers/$(FAMILY)/v1.1.0
# enhance include path
#C_INCLUDE +=	-I $(MIOS32_PATH)/mios32/$(FAMILY) -I $(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/inc -I $(DRIVER_LIB)/STM32_USB_Device_Library/Core/inc  -I $(DRIVER_LIB)/STM32_USB_HOST_Library/Core/inc -I $(DRIVER_LIB)/STM32_USB_OTG_Driver/inc -I $(DRIVER_LIB)/CMSIS/Include -I $(DRIVER_LIB)/CMSIS/ST/STM32F4xx/Include -I $(DRIVER_LIB)/CMSIS/ST/STM32F4xx/Include
C_INCLUDE +=	-I $(DRIVER_LIB)/CMSIS/Include -I $(DRIVER_LIB)/CMSIS/Device/ST/STM32F4xx/Include
C_INCLUDE +=	-I $(MIOS32_PATH)/mios32/$(FAMILY) -I $(DRIVER_LIB)/STM32F4xx_HAL_Driver/inc 
C_INCLUDE +=	-I $(DRIVER_USB_LIB)/STM32_USB_Device_Library/Core/inc  -I $(DRIVER_USB_LIB)/STM32_USB_HOST_Library/Core/inc -I $(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/inc 
 


CFLAGS += -DUSE_FULL_LL_DRIVER -DUSB_SUPPORT_USER_STRING_DESC


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
	$(DRIVER_USB_LIB)/STM32_USB_Device_Library/Core/src/usbd_core.c \
	$(DRIVER_USB_LIB)/STM32_USB_Device_Library/Core/src/usbd_ioreq.c \
	$(DRIVER_USB_LIB)/STM32_USB_Device_Library/Core/src/usbd_req.c \
	$(DRIVER_USB_LIB)/STM32_USB_HOST_Library/Core/src/usbh_core.c \
	$(DRIVER_USB_LIB)/STM32_USB_HOST_Library/Core/src/usbh_hcs.c \
	$(DRIVER_USB_LIB)/STM32_USB_HOST_Library/Core/src/usbh_ioreq.c \
	$(DRIVER_USB_LIB)/STM32_USB_HOST_Library/Core/src/usbh_stdreq.c \
	$(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/src/usb_core.c \
	$(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/src/usb_dcd.c \
	$(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/src/usb_dcd_int.c \
	$(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/src/usb_hcd.c \
	$(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/src/usb_hcd_int.c \
	$(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/src/usb_otg.c

	#$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/misc.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_adc.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_can.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_crc.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_cryp.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_cryp_aes.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_cryp_des.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_cryp_tdes.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_dac.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_dbgmcu.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_dcmi.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_dma.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_exti.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_flash.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_fsmc.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_hash.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_hash_md5.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_hash_sha1.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_i2c.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_iwdg.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_pwr.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rcc.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rng.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rtc.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_sdio.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_spi.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_syscfg.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_tim.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_usart.c \
	$(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_wwdg.c \


THUMB_AS_SOURCE += 

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/mios32/$(FAMILY)
