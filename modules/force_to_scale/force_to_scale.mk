# defines additional rules for integrating the module

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/force_to_scale


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/force_to_scale/force_to_scale.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/force_to_scale

