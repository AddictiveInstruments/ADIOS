# defines additional rules for integrating the random module

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/random


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/random/jsw_rand.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/random
