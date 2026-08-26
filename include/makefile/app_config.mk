# Reads the application's configuration out of its adios_config.h - THE
# single source of truth. The Makefile declares nothing about the target any
# more: it asks.
#
# HOW TO USE IT (from an application Makefile, before core.mk):
#   ADIOS_PATH ?= ../../..
#   include $(ADIOS_PATH)/include/makefile/app_config.mk
#
# and adios_config.h carries the answers:
#   #define ADIOS_PROCESSOR       STM32G070CB
#   #define ADIOS_LCD_DRIVER      ili9488_5x6      (omit for "dummy")
#   #define ADIOS_DEVICE_ID_PERSIST 1              (optional)
#   #define ADIOS_USE_DYNAMIC_BSL_BOUNDARY 1       (optional)
#   #define ADIOS_USERDATA_PAGES  4                (optional, may sit in #if
#                                                   chains - it is EVALUATED)
#
# Two stages, on purpose:
#   1. ADIOS_PROCESSOR is read as plain text (sed): it is the anchor -
#      unconditional by nature, and nothing can be preprocessed before the
#      target is known.
#   2. everything else goes through the real preprocessor, run over
#      include/adios/adios_config_defaults.h - the SAME header the C build
#      includes. #if chains, derivations and the OS defaulting rules land
#      here already resolved, so a value the config derives never has to be
#      derived a second time in make.
# If the compiler is not on the PATH, stage 2 fails exactly where the build
# would have failed anyway - with a message instead of a silent empty value.

APP_CFG ?= adios_config.h

# ---- stage 1: the anchor -----------------------------------------------
ADIOS_PROCESSOR := $(shell sed -n 's/^\#define[ \t]*ADIOS_PROCESSOR[ \t]*\([A-Za-z0-9_]*\).*/\1/p' $(APP_CFG))
ifeq ($(ADIOS_PROCESSOR),)
$(error $(APP_CFG) does not define ADIOS_PROCESSOR - without it the build cannot pick a family, a startup file or a linker script)
endif
PROCESSOR := $(ADIOS_PROCESSOR)

# family derived from the part number, board defaulted per family (one
# default per family - the 2026-08-16 rule, boards are not code switches)
ifneq (,$(filter STM32G0%,$(PROCESSOR)))
FAMILY := STM32G0xx
BOARD  ?= STM32G0GENERIC
else ifneq (,$(filter STM32F4%,$(PROCESSOR)))
FAMILY := STM32F4xx
BOARD  ?= MBHP_DIPCOREF4
else
$(error no known family for processor $(PROCESSOR))
endif

# ---- stage 2: the evaluated configuration ------------------------------
# one preprocessor run, filtered per key; values are single tokens, and the
# character class keeps a CRLF config from poisoning them with a stray \r
app_cfg_extract = $(shell arm-none-eabi-gcc -E -dM -I. -I$(ADIOS_PATH)/include/adios $(ADIOS_PATH)/include/adios/adios_config_defaults.h 2>/dev/null | sed -n 's/^\#define[ \t]*$(1)[ \t]*\([A-Za-z0-9_]*\).*/\1/p')

ADIOS_USERDATA_PAGES := $(call app_cfg_extract,ADIOS_USERDATA_PAGES)
ifeq ($(ADIOS_USERDATA_PAGES),)
$(error could not preprocess $(APP_CFG) - is arm-none-eabi-gcc on the PATH?)
endif

LCD := $(call app_cfg_extract,ADIOS_LCD_DRIVER)
ifeq ($(LCD),)
LCD := dummy
endif

ADIOS_DEVICE_ID_PERSIST := $(call app_cfg_extract,ADIOS_DEVICE_ID_PERSIST)
ADIOS_USE_DYNAMIC_BSL_BOUNDARY := $(call app_cfg_extract,ADIOS_USE_DYNAMIC_BSL_BOUNDARY)
