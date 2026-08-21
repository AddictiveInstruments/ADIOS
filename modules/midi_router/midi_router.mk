# defines additional rules for integrating the midi_router modules

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/midi_router


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/midi_router/midi_router.c \
	$(ADIOS_PATH)/modules/midi_router/midi_port.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/midi_router
