# defines the rule for creating the glcd_font_*.o objects,

# enhance include path
C_INCLUDE +=	-I $(ADIOS_PATH)/modules/glcd_font

# add modules to thumb sources
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_16bit_pix.c \


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/glcd_font
