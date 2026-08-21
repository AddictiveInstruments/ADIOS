# defines additional rules for integrating the sequencer modules

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/sequencer


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/sequencer/seq_bpm.c \
	$(ADIOS_PATH)/modules/sequencer/seq_midi_out.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/sequencer
