//! \defgroup APP_LCD
//!
//! Null display driver - see app_lcd.h. Exactly the entry points ADIOS_LCD
//! calls (adios/common/adios_lcd.c), each a no-op reporting success, so a
//! project without a display still links and runs.
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ========================================================================== */

#include <adios.h>
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
s32 APP_LCD_BitmapPixelSet(adios_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour) { return 0; }
s32 APP_LCD_BitmapPrint(adios_lcd_bitmap_t bitmap)                          { return 0; }

//! \}
