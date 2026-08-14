# $Id: mios32.mk 1921 2014-01-10 21:22:37Z tk $
# defines additional rules for MIOS32

# enhance include path
C_INCLUDE +=	-I $(MIOS32_PATH)/include/mios32

# forward MIOS32 environment variables to preprocessor
CFLAGS    +=    -DMIOS32_PROCESSOR_$(PROCESSOR) \
		-DMIOS32_PROCESSOR_STR=\"$(PROCESSOR)\" \
		-DMIOS32_FAMILY_$(FAMILY) \
		-DMIOS32_FAMILY_STR=\"$(FAMILY)\" \
		-DMIOS32_BOARD_$(BOARD) \
		-DMIOS32_BOARD_STR=\"$(BOARD)\" \
		-DMIOS32_LCD_$(LCD) \
		-DMIOS32_LCD_STR=\"$(LCD)\"


# CMSIS device macro (STM32G070xx, STM32F407xx, ...) - the thing that tells
# ST's stm32<family>.h WHICH device header to pull in, and therefore which
# peripheral set the whole build sees.
#
# DERIVED from PROCESSOR, never hand-listed. Until 2026-08-13 this was a
# hand-written #if/#elif table inside each stm32<family>.h, knowing four G0
# and three F4 parts and stopping the build with an #error for anything else
# - while the .ld and startup files, derived mechanically right next door in
# programming_model.mk, already covered every chip in the tree. That table
# was also where a silent miscompilation had lived (see the comment it left
# behind in stm32g0xx.h): the wrong device header selected for every G0
# build, caught only because a G030K6 referenced a timer it does not have.
#
# ST's format is fixed at 11 characters: STM32 + 4-char line + 1-char
# package + 1-char density (STM32G0B1CB = G0B1 line, C package, B density).
# Their device headers are named after the line, with "x" standing in for
# whatever the peripheral layout does NOT depend on:
#
#   default        STM32<line>xx     STM32G0B1CB -> STM32G0B1xx
#   F401, F411     STM32<line>x<D>   split by DENSITY  (STM32F401RC -> STM32F401xC)
#   F410, F412     STM32<line><P>x   split by PACKAGE  (STM32F410RB -> STM32F410Rx)
#
# A wrong or empty PROCESSOR does not slip through: with no device macro
# defined, ST's own header stops with "Please select first the target
# STM32xxx device used in your application".
PROCESSOR_LINE    := $(shell echo $(PROCESSOR) | cut -c1-9)
PROCESSOR_PACKAGE := $(shell echo $(PROCESSOR) | cut -c10)
PROCESSOR_DENSITY := $(shell echo $(PROCESSOR) | cut -c11)

ifneq ($(filter STM32F401 STM32F411,$(PROCESSOR_LINE)),)
CMSIS_DEVICE := $(PROCESSOR_LINE)x$(PROCESSOR_DENSITY)
else
ifneq ($(filter STM32F410 STM32F412,$(PROCESSOR_LINE)),)
CMSIS_DEVICE := $(PROCESSOR_LINE)$(PROCESSOR_PACKAGE)x
else
CMSIS_DEVICE := $(PROCESSOR_LINE)xx
endif
endif

CFLAGS    +=    -D$(CMSIS_DEVICE)


# add modules to thumb sources
# TODO: provide makefile option to add code to ARM sources
THUMB_SOURCE += \
	$(MIOS32_PATH)/mios32/common/mios32_srio.c \
	$(MIOS32_PATH)/mios32/common/mios32_srin.c \
	$(MIOS32_PATH)/mios32/common/mios32_srout.c \
	$(MIOS32_PATH)/mios32/common/mios32_enc.c \
	$(MIOS32_PATH)/mios32/common/mios32_lcd.c \
	$(MIOS32_PATH)/mios32/common/mios32_midi.c \
	$(MIOS32_PATH)/mios32/common/mios32_osc.c \
	$(MIOS32_PATH)/mios32/common/mios32_din_midi.c \
	$(MIOS32_PATH)/mios32/common/mios32_spi_midi.c \
	$(MIOS32_PATH)/mios32/common/mios32_can_midi.c \
	$(MIOS32_PATH)/mios32/common/mios32_sdcard.c \
	$(MIOS32_PATH)/mios32/common/mios32_timestamp.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_bsl.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_sys.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_irq.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_spi.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_i2s.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_utils.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_ain.c \
	$(MIOS32_PATH)/mios32/common/printf-stdarg.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_uart.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_i2c.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_can.c 
	
ifneq ($(FAMILY),STM32G0xx)
THUMB_SOURCE += \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_usb.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_usb_midi.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_usb_com.c \
	$(MIOS32_PATH)/mios32/$(FAMILY)/mios32_usb_hid.c 
endif

# MEMO: the gcc linker is clever enough to exclude functions from the final memory image
# if they are not references from the main routine - accordingly we can savely include
# the USB drivers without the danger that this increases the project size of applications,
# which don't use the USB peripheral at all :-)


# add family specific files
include $(MIOS32_PATH)/mios32/$(FAMILY)/mios32_family.mk


# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/mios32/common \
        $(MIOS32_PATH)/mios32/mios32.mk \
        $(MIOS32_PATH)/include/mios32 \
        $(MIOS32_PATH)/doc/mios32
