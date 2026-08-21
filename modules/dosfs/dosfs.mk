# defines additional rules for integrating DOSFS

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/dosfs


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/dosfs/dosfs.c \
	$(ADIOS_PATH)/modules/dosfs/dfs_sdcard.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/dosfs
