# $Id: tia.mk 1686 2013-02-07 22:11:42Z tk $

# enhance include path
C_INCLUDE += -I $(MIOS32_PATH)/modules/tia


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(MIOS32_PATH)/modules/tia/tia.c




# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/modules/tia
