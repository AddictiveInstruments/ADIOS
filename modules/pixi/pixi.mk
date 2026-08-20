# defines additional rules for integrating the PIXI modules

# enhance include path
C_INCLUDE += -I $(ANTICHAMBRE_PATH)/modules/pixi


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ANTICHAMBRE_PATH)/modules/pixi/max11300.c


# directories and files that should be part of the distribution (release) package
DIST += $(ANTICHAMBRE_PATH)/modules/pixi

