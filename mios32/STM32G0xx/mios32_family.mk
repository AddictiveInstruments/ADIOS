# defines additional rules for MIOS32 family

# select driver library
DRIVER_LIB =	$(MIOS32_PATH)/drivers/$(FAMILY)/LL_HAL
# enhance include path
#C_INCLUDE +=	-I $(MIOS32_PATH)/mios32/$(FAMILY) -I $(DRIVER_LIB)/STM32F4xx_StdPeriph_Driver/inc -I $(DRIVER_LIB)/STM32_USB_Device_Library/Core/inc  -I $(DRIVER_LIB)/STM32_USB_HOST_Library/Core/inc -I $(DRIVER_LIB)/STM32_USB_OTG_Driver/inc -I $(DRIVER_LIB)/CMSIS/Include -I $(DRIVER_LIB)/CMSIS/ST/STM32F4xx/Include -I $(DRIVER_LIB)/CMSIS/ST/STM32F4xx/Include
C_INCLUDE +=	-I $(DRIVER_LIB)/CMSIS/Include -I $(DRIVER_LIB)/CMSIS/Device/ST/STM32G0xx/Include
C_INCLUDE +=	-I $(MIOS32_PATH)/mios32/$(FAMILY) -I $(DRIVER_LIB)/STM32G0xx_HAL_Driver/inc 
#C_INCLUDE +=	-I $(DRIVER_USB_LIB)/STM32_USB_Device_Library/Core/inc  -I $(DRIVER_USB_LIB)/STM32_USB_HOST_Library/Core/inc -I $(DRIVER_USB_LIB)/STM32_USB_OTG_Driver/inc 
 


CFLAGS += -DUSE_FULL_LL_DRIVER 


# add modules to thumb sources
THUMB_SOURCE += \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_rcc.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_pwr.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_rtc.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_gpio.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_dac.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_tim.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_usart.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_lpuart.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_exti.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_utils.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_spi.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_dma.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_i2c.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_adc.c \
	$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_flash.c
	


	#$(DRIVER_LIB)/STM32G0xx_HAL_Driver/src/stm32g0xx_ll_flash.c \
	#$(DRIVER_USB_LIB)/STM32_USB_Device_Library/Core/src/usbd_core.c \
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


THUMB_AS_SOURCE += 

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/mios32/$(FAMILY)
