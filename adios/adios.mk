# defines additional rules for ADIOS

# enhance include path
C_INCLUDE +=	-I $(ADIOS_PATH)/include/adios

# forward ADIOS environment variables to preprocessor
CFLAGS    +=    -DADIOS_PROCESSOR_$(PROCESSOR) \
		-DADIOS_PROCESSOR_STR=\"$(PROCESSOR)\" \
		-DADIOS_FAMILY_$(FAMILY) \
		-DADIOS_FAMILY_STR=\"$(FAMILY)\" \
		-DADIOS_LCD_$(LCD) \


# CMSIS device macro (STM32G070xx, STM32F407xx, ...) - the thing that tells
# ST's stm32<family>.h WHICH device header to pull in, and therefore which
# peripheral set the whole build sees.
#
# DERIVED from PROCESSOR, never hand-listed. Until 2026-08-13 this was a
# hand-written #if/#elif table inside each stm32<family>.h, knowing four G0
# and three F4 parts and stopping the build with an #error for anything else
# - while the .ld and startup files, derived mechanically right next door in
# core.mk, already covered every chip in the tree. That table
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
	$(ADIOS_PATH)/adios/common/adios_srio.c \
	$(ADIOS_PATH)/adios/common/adios_srin.c \
	$(ADIOS_PATH)/adios/common/adios_srout.c \
	$(ADIOS_PATH)/adios/common/adios_enc.c \
	$(ADIOS_PATH)/adios/common/adios_midi.c \
	$(ADIOS_PATH)/adios/common/adios_din_midi.c \
	$(ADIOS_PATH)/adios/common/adios_spi_midi.c \
	$(ADIOS_PATH)/adios/common/adios_sdcard.c \
	$(ADIOS_PATH)/adios/common/adios_timestamp.c \
	$(ADIOS_PATH)/adios/common/adios_bsl.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_sys.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_irq.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_spi.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_i2s.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_utils.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_adc.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_dac.c \
	$(ADIOS_PATH)/adios/common/printf-stdarg.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_uart.c \
	$(ADIOS_PATH)/adios/$(FAMILY)/adios_i2c.c \
	
# USB is declared by the family, not here: whether a family has a USB
# peripheral at all, and which TinyUSB controller driver serves it, are facts
# of the silicon. See adios/$(FAMILY)/adios_family.mk.

# MEMO: the gcc linker is clever enough to exclude functions from the final memory image
# if they are not references from the main routine - accordingly we can savely include
# the USB drivers without the danger that this increases the project size of applications,
# which don't use the USB peripheral at all :-)


# add family specific files
include $(ADIOS_PATH)/adios/$(FAMILY)/adios_family.mk


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/adios/common \
        $(ADIOS_PATH)/adios/adios.mk \
        $(ADIOS_PATH)/include/adios \
        $(ADIOS_PATH)/doc/adios
