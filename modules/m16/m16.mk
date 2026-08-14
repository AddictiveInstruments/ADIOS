# $Id$
# defines additional rules for the M16 FPGA interface

# enhance include path
C_INCLUDE += -I $(MIOS32_PATH)/modules/m16

# add module to thumb sources
THUMB_SOURCE += \
	$(MIOS32_PATH)/modules/m16/m16.c

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/modules/m16
