# defines additional rules for integrating FATFS

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/fatfs/src


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/fatfs/src/diskio.c \
	$(ADIOS_PATH)/modules/fatfs/src/option/ccsbcs.c \
	$(ADIOS_PATH)/modules/fatfs/src/ff.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/fatfs
