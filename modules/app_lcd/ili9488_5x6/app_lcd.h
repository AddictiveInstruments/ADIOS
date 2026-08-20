/*
 * Header file for application specific LCD Driver
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _APP_LCD_H
#define _APP_LCD_H

#include <app_lcd_bitmap.h>

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// supported: 1 (more toDo)
#ifndef APP_LCD_NUM_X
#define APP_LCD_NUM_X 1
#endif

// supported: 1 (more toDo)
#ifndef APP_LCD_NUM_Y
#define APP_LCD_NUM_Y 1
#endif

// don't change these values for this GLCD type
#define APP_LCD_WIDTH (u16)480
#define APP_LCD_HEIGHT (u16)320
#define APP_LCD_COLOUR_DEPTH 16
// We need 480*320 pixel, for 16bit bitmaps a pixel is 2 bytes but we use 1bit resolution for memory save
// size of u8 array is 480*320/8 = 19200,
// for stm32g050 (18K RAM) here this is too much big we will never use full Bitmap.
#define APP_LCD_1BITMAP_SIZE ((APP_LCD_NUM_X*APP_LCD_WIDTH * APP_LCD_NUM_Y*APP_LCD_HEIGHT * 1) / 8)
#define APP_LCD_DONT_USE_FULL_BMP
// We need 480*320 pixel, ili9488 will be configured for 16bit pixel, 565 format
// size of u8 array is 480*320*2 = 307200 Ohhhhhh, this will never be used !!!
#define APP_LCD_4BITMAP_SIZE ((APP_LCD_NUM_X*APP_LCD_WIDTH * APP_LCD_NUM_Y*APP_LCD_HEIGHT * APP_LCD_COLOUR_DEPTH) / 8)

#define APP_LCD_STRING_ALIGN_LEFT     0
#define APP_LCD_STRING_ALIGN_CENTER   1
#define APP_LCD_STRING_ALIGN_RIGHT    2

// Color definitions
#define APP_LCD_BLACK      			0x0000      /*   0,   0,   0 */
#define APP_LCD_NAVY				0x000F      /*   0,   0, 128 */
#define APP_LCD_DARKGREEN   		0x03E0      /*   0, 128,   0 */
#define APP_LCD_DARKCYAN    		0x03EF      /*   0, 128, 128 */
#define APP_LCD_MAROON      		0x7800      /* 128,   0,   0 */
#define APP_LCD_PURPLE      		0x780F      /* 128,   0, 128 */
#define APP_LCD_OLIVE       		0x7BE0      /* 128, 128,   0 */
#define APP_LCD_LIGHTGREY   		0xC618      /* 192, 192, 192 */
#define APP_LCD_DARKGREY    		0x7BEF      /* 128, 128, 128 */
#define APP_LCD_BLUE        		0x001F      /*   0,   0, 255 */
#define APP_LCD_GREEN       		0x07E0      /*   0, 255,   0 */
#define APP_LCD_CYAN        		0x07FF      /*   0, 255, 255 */
#define APP_LCD_RED         		0xF800      /* 255,   0,   0 */
#define APP_LCD_MAGENTA     		0xF81F      /* 255,   0, 255 */
#define APP_LCD_YELLOW      		0xFFE0      /* 255, 255,   0 */
#define APP_LCD_WHITE       		0xFFFF      /* 255, 255, 255 */
#define APP_LCD_ORANGE      		0xFD20      /* 255, 165,   0 */
#define APP_LCD_GREENYELLOW 		0xAFE5      /* 173, 255,  47 */
#define APP_LCD_PINK        		0xF81F
#define APP_LCD_ROLAND_BLUE 		0x04fc
#define APP_LCD_ROLAND_ORANGE 		0xd303
/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////

// Graphics, bitmaps fusion mode
typedef enum{
  REPLACE = 0,
  NOBLACK,
  OR,
  AND,
  XOR
}app_lcd_fusion_t;

// bitmap color depth
typedef enum{
  Is1BIT = 1,
  Is16BIT = 16
}app_lcd_color_depth_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

// hooks to MIOS32_LCD
extern s32 APP_LCD_Init(u32 mode);
extern s32 APP_LCD_SPI_TransferModeInit(void);
extern s32 APP_LCD_DelayedInit(u32 mode);
extern s32 APP_LCD_IsReady(void);
extern s32 APP_LCD_Data(u8 data);
extern s32 APP_LCD_Cmd(u8 cmd);
extern s32 APP_LCD_SetRotation(u8 r);
extern s32 APP_LCD_Lite(u8 enable);
extern s32 APP_LCD_Clear(void);

extern s32 APP_LCD_Data_Multi(u8 *buff, size_t buff_size);
extern s32 APP_LCD_SendFastPixels(u32 n, u16 color);
extern s32 APP_LCD_SetAddrWindow(u16 x0, u16 y0, u16 x1, u16 y1);
extern s32 APP_LCD_DrawPixel(s16 x, s16 y, u16 color);
extern s32 APP_LCD_DrawFastVLine(s16 x, s16 y, s16 h, u16 color);
extern s32 APP_LCD_DrawFastHLine(s16 x, s16 y, s16 w, u16 color);
extern s32 APP_LCD_DrawFastBeat(u16 x, u16 y, u16 color);
extern s32 APP_LCD_DrawFastBall(u16 x, u16 y, u16 color);
extern s32 APP_LCD_DrawFastNoire(u16 x, u16 y, u16 color);
extern s32 APP_LCD_DrawFastCroche(u16 x, u16 y, u16 color);
extern s32 APP_LCD_DrawFastCorner(u16 x, u16 y, u16 color);
extern s32 APP_LCD_Digits_drawNorm(int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC);
extern s32 APP_LCD_Digits_draw(int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC);
extern s32 APP_LCD_CursorSet(u16 column, u16 line);
extern s32 APP_LCD_GCursorSet(u16 x, u16 y);

extern s32 APP_LCD_FontInit(u8 *font, app_lcd_color_depth_t colour_depth);    // new
extern s32 APP_LCD_SpecialCharInit(u8 num, u8 table[8]);
extern s32 APP_LCD_CharKernGet(s16 ascii_offset, char c);
extern s32 APP_LCD_StringKernGet(s16 ascii_offset, const char *str);
extern s32 APP_LCD_PrintChar(s16 x, s16 y, s16 w_stop, s16 ascii_offset, char c);   // new
extern s32 APP_LCD_PrintString(s16 x, s16 y, s16 w_stop, u8 alignment, s16 ascii_offset, const char *str);   // new
extern s32 APP_LCD_PrintFormattedString(s16 x, s16 y, s16 w_stop, u8 alignment, s16 ascii_offset, const char *format, ...);   //new
extern s32 APP_LCD_PrintProgress(mios32_lcd_bitmap_t bitmap, u32 progress_color, s16 x, s16 y, s16 w_stop, s16 height, s16 progress);   // new
extern s32 APP_LCD_BitmapPrintChar(mios32_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, s16 ascii_offset, char c);   // new
extern s32 APP_LCD_BitmapPrintString(mios32_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, u8 alignment, s16 ascii_offset, const char *str);   // new
extern s32 APP_LCD_BitmapPrintFormattedString(mios32_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, u8 alignment, s16 ascii_offset, const char *format, ...);   //new


extern u16 APP_LCD_ColourConvert(u32 rgb);
extern s32 APP_LCD_BColourSet(u16 color);   // used for: rectDraw fill.
extern s32 APP_LCD_FColourSet(u16 color);   // used for: rectDraw border, pixelSet, legacy 2 native PrintChar/Fusion...
extern s32 APP_LCD_BColourSetRGB(u32 rgb);   // used for: rectDraw fill.
extern s32 APP_LCD_FColourSetRGB(u32 rgb);   // used for: rectDraw border, pixelSet, legacy 2 native PrintChar/Fusion...

extern mios32_lcd_bitmap_t APP_LCD_BitmapInit(u8 *memory, u16 width, u16 height, u16 line_offset, app_lcd_color_depth_t colour_depth);   //new

extern s32 APP_LCD_PixelSet(u16 x, u16 y, u32 colour);
extern s32 APP_LCD_BitmapPixelSet(mios32_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour);
extern s32 APP_LCD_Rectangle(u16 x, u16 y, u16 w, u16 h, u8 border, u16 bd_color, u8 fill, u16 fill_color);
extern s32 APP_LCD_BitmapRectangle(mios32_lcd_bitmap_t bitmap, s16 x, s16 y, u16 width, u16 height, u8 border, u32 bd_color, u8 fill, u32 back_color);   //new
extern s32 APP_LCD_BitmapByteSet(mios32_lcd_bitmap_t bitmap, s16 x, s16 y, u8 value);
extern s32 APP_LCD_Bitmap16BitLuma(mios32_lcd_bitmap_t bitmap, s16 x, s16 y, u16 width, u16 height, float luma);   // new
extern u16 APP_LCD_HelpPixelLuma(u16 pix_mem, float luma);
extern u16 APP_LCD_PixelFusion(u16 fore_pix, float fore_luma, u16 back_pix, float back_luma, app_lcd_fusion_t fusion);
extern s32 APP_LCD_BitmapFusion(mios32_lcd_bitmap_t top_bmp, float top_luma, mios32_lcd_bitmap_t bmp, s16 top_pos_x, s16 top_pos_y, app_lcd_fusion_t fusion);   // new
extern s32 APP_LCD_SendBitmap(mios32_lcd_bitmap_t bitmap, u16 x_pos, u16 y_pos);
extern s32 APP_LCD_BitmapHBoundaryPrint(mios32_lcd_bitmap_t bitmap, u16 b_x, u16 b_width);
extern s32 APP_LCD_BitmapPrint(mios32_lcd_bitmap_t bitmap);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////
extern u8 app_lcd_back_grayscale;
extern u8 app_lcd_fore_grayscale;

#endif /* _APP_LCD_H */
