# $Id: $
# defines additional rules for the null display driver

# enhance include path
C_INCLUDE +=	-I $(MIOS32_PATH)/modules/app_lcd
C_INCLUDE +=	-I $(MIOS32_PATH)/modules/app_lcd/dummy

# add modules to thumb sources
THUMB_SOURCE += \
	$(MIOS32_PATH)/modules/app_lcd/dummy/app_lcd.c

# NOTE: deliberately does NOT include modules/glcd_font/glcd_font.mk, unlike
# every real driver. There is nothing to draw glyphs on here, and a project
# that wants the fonts anyway can include that .mk itself.

# directories and files that should be part of the distribution (release) package
DIST += $(MIOS32_PATH)/modules/app_lcd/dummy
