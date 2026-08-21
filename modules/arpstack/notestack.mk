
# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/notestack


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/arpstack/notestack.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/arpstack
