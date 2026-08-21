
# enhance include path
C_INCLUDE += -I $(ADIOS_PATH)/modules/ws2812


# add modules to thumb sources (TODO: provide makefile option to add code to ARM sources)
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/ws2812/ws2812.c




# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/ws2812
