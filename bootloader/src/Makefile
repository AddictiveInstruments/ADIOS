# $Id: Makefile 194 2008-12-18 01:47:21Z tk $

################################################################################
# Standalone bootloader build - compiles the code and generates the
# mios32_bsl_<CHIP>.inc file for one chip.
#
# NOTE: this is the MANUAL entry point, kept for building a bootloader image
# on its own. The normal path is etc/gen_bsl_boundary.sh, driven automatically
# from an app's own build (MIOS32_USE_DYNAMIC_BSL_BOUNDARY = 1) - that one
# measures the real compiled size and sizes the boundary/.inc accordingly,
# which this manual path cannot do.
#
# One generic Makefile.bsl handles every chip (2026-08-05, replaced the
# per-chip Makefile.bsl_STM32xxx copies) - FAMILY/BOARD/startup are all
# derived from PROCESSOR, so a new chip needs no new file, just a target
# below (or simply: make CHIP=STM32G031K8).
################################################################################

CHIP ?= STM32G031K8

all:
	$(MAKE) -f Makefile.bsl PROCESSOR=$(CHIP) cleanall
	$(MAKE) -f Makefile.bsl PROCESSOR=$(CHIP)
	mv project_build/project.bin project_$(CHIP).bin
	perl gen_inc_file.pl project_$(CHIP).bin $(INC_DIR)/mios32_bsl_$(CHIP).inc mios32_bsl_image mios32_bsl $(INC_FLAGS)
	perl gen_inc_file.pl project_$(CHIP).bin ../updater/mios32_bsl_$(CHIP).inc bsl_image 0 $(INC_FLAGS)

# F4xx keeps the full bootloader image; G0xx uses the reduced variant
ifeq ($(findstring STM32F4,$(CHIP)),STM32F4)
INC_DIR   = ../../mios32/STM32F4xx
INC_FLAGS =
else
INC_DIR   = ../../mios32/STM32G0xx
INC_FLAGS = -reduced_bsl
endif

# convenience targets - "make STM32G030K6" is the same as "make CHIP=STM32G030K6"
STM32F405RG STM32F407VE STM32G030K6 STM32G031K8 STM32G050K8 STM32G070CB:
	$(MAKE) CHIP=$@

clean:
	$(MAKE) -f Makefile.bsl PROCESSOR=$(CHIP) clean

cleanall:
	$(MAKE) -f Makefile.bsl PROCESSOR=$(CHIP) cleanall
