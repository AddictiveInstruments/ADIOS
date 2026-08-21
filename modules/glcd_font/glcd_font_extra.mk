# defines the rule for creating the glcd_font_*.o objects,

# enhance include path
C_INCLUDE +=	-I $(ADIOS_PATH)/modules/glcd_font

# add modules to thumb sources
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_normal.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_normal_inv.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_big.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_small.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_tiny.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_tiny_wide.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_tiny_inv.c \
  $(ADIOS_PATH)/modules/glcd_font/glcd_font_bitlow.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_knob_icons.c \
	$(ADIOS_PATH)/modules/glcd_font/glcd_font_meter_icons_h.c \
  $(ADIOS_PATH)/modules/glcd_font/glcd_font_meter_icons_v.c\
  $(ADIOS_PATH)/modules/glcd_font/glcd_font_minitogsel_icons.c


# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/glcd_font
