# defines additional rules for integrating the button/LED matrix

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/microvga


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/microvga/microvga.c \
	$(ADIOS_PATH)/modules/microvga/conio.c \
	$(ADIOS_PATH)/modules/microvga/ui.c \


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/microvga
