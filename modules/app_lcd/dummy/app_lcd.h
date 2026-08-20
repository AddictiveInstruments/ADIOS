//! \defgroup APP_LCD
//!
//! Null display driver - satisfies the app_lcd contract without driving any
//! hardware. Select it with "LCD = dummy" in a project that has no display,
//! or that drives its own screen directly and only needs the build to link.
//!
//! Every entry point below is a no-op returning 0 ("done"), so a project can
//! be built with or without MIOS32_DONT_USE_LCD and behave identically: with
//! the LCD layer compiled in, calls simply go nowhere.
//!
//! \{
#ifndef _APP_LCD_H
#define _APP_LCD_H

#include <app_lcd_bitmap.h>

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// a nominal 2x16 character geometry: MIOS32_LCD's own cursor/bitmap
// arithmetic runs on these, so they must be non-zero even though nothing is
// ever displayed.
#ifndef APP_LCD_NUM_X
#define APP_LCD_NUM_X 1
#endif
#ifndef APP_LCD_NUM_Y
#define APP_LCD_NUM_Y 1
#endif
#ifndef APP_LCD_WIDTH
#define APP_LCD_WIDTH 16
#endif
#ifndef APP_LCD_HEIGHT
#define APP_LCD_HEIGHT 2
#endif
#ifndef APP_LCD_COLOUR_DEPTH
#define APP_LCD_COLOUR_DEPTH 1
#endif

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 APP_LCD_Init(u32 mode);
extern s32 APP_LCD_Data(u8 data);
extern s32 APP_LCD_Cmd(u8 cmd);
extern s32 APP_LCD_Clear(void);
extern s32 APP_LCD_CursorSet(u16 column, u16 line);
extern s32 APP_LCD_GCursorSet(u16 x, u16 y);
extern s32 APP_LCD_SpecialCharInit(u8 num, u8 table[8]);
extern s32 APP_LCD_BColourSet(u32 rgb);
extern s32 APP_LCD_FColourSet(u32 rgb);
extern s32 APP_LCD_BitmapPixelSet(mios32_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour);
extern s32 APP_LCD_BitmapPrint(mios32_lcd_bitmap_t bitmap);

#endif /* _APP_LCD_H */
//! \}
