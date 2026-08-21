# defines additional rules for integrating MINFS

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/minfs


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/minfs/minfs.c \
	$(ADIOS_PATH)/modules/minfs/minfs_fram.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/minfs