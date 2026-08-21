# defines additional rules for integrating the module_x

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/module_x


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/module_x/module_x.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/module_x
