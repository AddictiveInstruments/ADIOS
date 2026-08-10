// $Id: $
//! \defgroup APP_LCD
//!
//! Null display driver - see app_lcd.h. Exactly the entry points MIOS32_LCD
//! calls (mios32/common/mios32_lcd.c), each a no-op reporting success, so a
//! project without a display still links and runs.
//!
//! \{
/* ==========================================================================
 *  Licensed for personal non-commercial use only.
 * ========================================================================== */

#include <mios32.h>
#include "app_lcd.h"

s32 APP_LCD_Init(u32 mode)                                                   { return 0; }
s32 APP_LCD_Data(u8 data)                                                    { return 0; }
s32 APP_LCD_Cmd(u8 cmd)                                                      { return 0; }
s32 APP_LCD_Clear(void)                                                      { return 0; }
s32 APP_LCD_CursorSet(u16 column, u16 line)                                  { return 0; }
s32 APP_LCD_GCursorSet(u16 x, u16 y)                                         { return 0; }
s32 APP_LCD_SpecialCharInit(u8 num, u8 table[8])                             { return 0; }
s32 APP_LCD_BColourSet(u32 rgb)                                              { return 0; }
s32 APP_LCD_FColourSet(u32 rgb)                                              { return 0; }
s32 APP_LCD_BitmapPixelSet(mios32_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour) { return 0; }
s32 APP_LCD_BitmapPrint(mios32_lcd_bitmap_t bitmap)                          { return 0; }

//! \}
