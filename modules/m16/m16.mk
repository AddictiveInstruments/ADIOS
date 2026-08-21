# defines additional rules for the M16 FPGA interface

# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/m16

# add module to thumb sources
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/m16/m16.c

# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/m16
