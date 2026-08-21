# defines additional rules for application specific LCD driver

# enhance include path
C_INCLUDE +=	-I $(ADIOS_PATH)/modules/app_lcd
C_INCLUDE +=	-I $(ADIOS_PATH)/modules/app_lcd/ili9488_5x6

# add modules to thumb sources
THUMB_SOURCE += \
	$(ADIOS_PATH)/modules/app_lcd/ili9488_5x6/app_lcd.c
   
# include fonts
include $(ADIOS_PATH)/modules/glcd_font/glcd_font.mk

# directories and files that should be part of the distribution (release) package
DIST += $(ADIOS_PATH)/modules/app_lcd/ili9488_5x6

