// $Id: app_lcd.c 2179 2015-06-10 18:36:14Z hawkeye $
/*
 * Application specific OLED driver for up to 1 * SSD1322 (more toDo)
 * Referenced from MIOS32_LCD routines
 *
 * ==========================================================================
 *
 *  Copyright (C) 2011 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include <glcd_font.h>
//#include <glcd_font_4bit.h>
#include "app_lcd.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/////////////////////////////////////////////////////////////////////////////
// Local defines
/////////////////////////////////////////////////////////////////////////////

// 0: J15 pins are configured in Push Pull Mode (3.3V)
// 1: J15 pins are configured in Open Drain mode (perfect for 3.3V->5V levelshifting)
#ifndef APP_LCD_OUTPUT_MODE
#define APP_LCD_OUTPUT_MODE  0
#endif

// for LPC17 only: should J10 be used for CS lines
// This option is nice if no J15 shiftregister is connected to a LPCXPRESSO.
// This shiftregister is available on the MBHP_CORE_LPC17 module
#ifndef APP_LCD_USE_J10_FOR_CS
#define APP_LCD_USE_J10_FOR_CS 0
#endif

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static u32 display_available = 0;

// default color for legacy 1Bit bitmap
u16 app_lcd_back_color = 0;
u16 app_lcd_fore_color = 0;


// font bitmap
static mios32_lcd_bitmap_t font_bmp;

/////////////////////////////////////////////////////////////////////////////
// Initializes application specific LCD driver
// IN: <mode>: optional configuration
// OUT: returns < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode
  
  if( MIOS32_BOARD_J15_PortInit(APP_LCD_OUTPUT_MODE) < 0 )
    return -2; // failed to initialize J15
  
#if APP_LCD_USE_J10_FOR_CS
  int pin;
  for(pin=0; pin<8; ++pin)
    MIOS32_BOARD_J10_PinInit(pin, APP_LCD_OUTPUT_MODE ? MIOS32_BOARD_PIN_MODE_OUTPUT_OD : MIOS32_BOARD_PIN_MODE_OUTPUT_PP);
#endif
  
  // set LCD type
  mios32_lcd_parameters.lcd_type = MIOS32_LCD_TYPE_GLCD_CUSTOM;
  mios32_lcd_parameters.num_x = APP_LCD_NUM_X;
  mios32_lcd_parameters.width = APP_LCD_WIDTH;
  mios32_lcd_parameters.num_x = APP_LCD_NUM_Y;
  mios32_lcd_parameters.height = APP_LCD_HEIGHT;
  mios32_lcd_parameters.colour_depth = APP_LCD_COLOUR_DEPTH;
  
  // set default(startup) forecolor to full white
  APP_LCD_FColourSet((u32)0xf);
  
  // use CS1 as OLED RC and CS2 as Oled Selection Reg RC
  u16 ctr;
  MIOS32_BOARD_J15_DataSet(0x03);
  //  APP_LCD_Clear();
  MIOS32_BOARD_J15_DataSet(0x01);
  MIOS32_BOARD_J15_SerDataShift(0x00);
  MIOS32_BOARD_J15_SerDataShift(0x00);
  MIOS32_BOARD_J15_SerDataShift(0x00);
  // oled rc
  MIOS32_BOARD_J15_DataSet(0x03);
  // wait for some
  for (ctr=0; ctr<1000; ++ctr)
    MIOS32_DELAY_Wait_uS(1000);
  // end of reset
  
  // Initialize LCD
  APP_LCD_Cmd(0x11); //Exit Sleep
  for (ctr=0; ctr<50; ++ctr)
    MIOS32_DELAY_Wait_uS(1000);
  
  // initialize LCDs
       APP_LCD_Cmd(0xa8); // Set MUX Ratio
       APP_LCD_Cmd(0x3f);

       APP_LCD_Cmd(0xd3); // Set Display Offset
       APP_LCD_Cmd(0x00);

       APP_LCD_Cmd(0x40); // Set Display Start Line

       if( !rotated ) {
   APP_LCD_Cmd(0xa0); // Set Segment re-map
   APP_LCD_Cmd(0xc0); // Set COM Output Scan Direction
       } else {
   APP_LCD_Cmd(0xa1); // Set Segment re-map: rotated
   APP_LCD_Cmd(0xc8); // Set COM Output Scan Direction: rotated
       }

       APP_LCD_Cmd(0xda); // Set COM Pins hardware configuration
       APP_LCD_Cmd(0x12);

       APP_LCD_Cmd(0x81); // Set Contrast Control
       APP_LCD_Cmd(0x7f); // middle

       APP_LCD_Cmd(0xa4); // Disable Entiere Display On

       APP_LCD_Cmd(0xa6); // Set Normal Display

       APP_LCD_Cmd(0xd5); // Set OSC Frequency
       APP_LCD_Cmd(0x80);

       APP_LCD_Cmd(0x8d); // Enable charge pump regulator
       APP_LCD_Cmd(0x14);

       APP_LCD_Cmd(0xaf); // Display On

       APP_LCD_Cmd(0x20); // Enable Page mode
       APP_LCD_Cmd(0x02);
  display_available = 20;
  return (display_available & (1 << mios32_lcd_device)) ? 0 : -1; // return -1 if display not available
}


/////////////////////////////////////////////////////////////////////////////
// Sends data byte to LCD
// IN: data byte in <data>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Data(u8 data)
{
#if 0  // TODO
  // select LCD depending on current cursor position
  // THIS PART COULD BE CHANGED TO ARRANGE THE 8 DISPLAYS ON ANOTHER WAY
  u8 cs = mios32_lcd_y / APP_LCD_HEIGHT;
  
  if( cs >= 8 )
    return -1; // invalid CS line
#endif

  // chip select and DC
#if APP_LCD_USE_J10_FOR_CS
  MIOS32_BOARD_J10_Set(~(1 << cs));
#else
//  // oled selection
//  MIOS32_BOARD_J15_DataSet(0x01);
//  MIOS32_BOARD_J15_SerDataShift(0x00);
//  MIOS32_BOARD_J15_SerDataShift(0x00);
//  MIOS32_BOARD_J15_SerDataShift(0x00);
//  // oled rc
//    MIOS32_BOARD_J15_DataSet(0x03);
  #endif
    
    
    MIOS32_BOARD_J15_DataSet(0x00);
  MIOS32_BOARD_J15_RS_Set(1); // RS pin used to control DC
  // send data
  MIOS32_BOARD_J15_SerDataShift(data);
  
  MIOS32_BOARD_J15_DataSet(0x03);
  // increment graphical cursor
  //++mios32_lcd_x;
  
#if 0
  // if end of display segment reached: set X position of all segments to 0
  if( (mios32_lcd_x % APP_LCD_WIDTH) == 0 ) {
    APP_LCD_Cmd(0x75); // set X=0
    APP_LCD_Data(0x00);
    APP_LCD_Data(0x00);
  }
#endif
  
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Sends command byte to LCD
// IN: command byte in <cmd>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Cmd(u8 cmd)
{
  // select all LCDs
#if APP_LCD_USE_J10_FOR_CS
  MIOS32_BOARD_J10_Set(0x00);
#else
//  // oled selection
//  MIOS32_BOARD_J15_DataSet(0x01);
//  MIOS32_BOARD_J15_SerDataShift(0x00);
//  MIOS32_BOARD_J15_SerDataShift(0x00);
//  MIOS32_BOARD_J15_SerDataShift(0x00);
//  //
//  MIOS32_BOARD_J15_DataSet(0x03);
#endif
  
  
  MIOS32_BOARD_J15_DataSet(0x00);
  MIOS32_BOARD_J15_RS_Set(0); // RS pin used to control DC
  MIOS32_BOARD_J15_SerDataShift(cmd);
  
  MIOS32_BOARD_J15_DataSet(0x03);
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Clear Screen
// IN: -
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Clear(void)
{
  u8 x, y;
  APP_LCD_GCursorSet(0, 0);
  for(y=0; y<mios32_lcd_parameters.height/8; ++y) {
    APP_LCD_GCursorSet(0, y);
    MIOS32_BOARD_J15_RS_Set(1); // RS pin used to control DC
    MIOS32_BOARD_J15_DataSet(0x00);
    for(x=0; x<mios32_lcd_parameters.width; ++x)
    MIOS32_BOARD_J15_SerDataShift(0xff);
    MIOS32_BOARD_J15_DataSet(0x03);
  }
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_CursorSet(u16 column, u16 line)
{
  // mios32_lcd_x/y set by MIOS32_LCD_CursorSet() function
  return APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
}


/////////////////////////////////////////////////////////////////////////////
// Sets graphical cursor to given position
// IN: <x> and <y>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_GCursorSet(u16 x, u16 y)
{
  s32 error = 0;

  // set X position
  error |= APP_LCD_Cmd(0x00 | (x & 0xf));
  error |= APP_LCD_Cmd(0x10 | ((x>>4) & 0xf));

  // set Y position
  error |= APP_LCD_Cmd(0xb0 | ((y>>3) & 7));

  return error;
}

/////////////////////////////////////////////////////////////////////////////
//! Initializes the graphical font<BR>
//! Only relevant for SSD1322
//! \param[in] *font pointer to font, colour_depth Is1BIT or IsILI
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_FontInit(u8 *font, app_lcd_color_depth_t colour_depth)
{
  font_bmp.memory = (u8 *)&font[MIOS32_LCD_FONT_BITMAP_IX] + (size_t)font[MIOS32_LCD_FONT_X0_IX];
  font_bmp.width = font[MIOS32_LCD_FONT_WIDTH_IX];
  font_bmp.height = font[MIOS32_LCD_FONT_HEIGHT_IX];
  font_bmp.line_offset = font[MIOS32_LCD_FONT_OFFSET_IX];
  font_bmp.colour_depth = colour_depth;
  
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Initializes a single special character
// IN: character number (0-7) in <num>, pattern in <table[8]>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_SpecialCharInit(u8 num, u8 table[8])
{
  return -1; // not supported
}

/////////////////////////////////////////////////////////////////////////////
// return a character kerning length
// IN: the character
// OUT: kerning length
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_CharKernGet(char c)
{
  s32 len=0;
  if(font_bmp.line_offset == 0){
    // kerning(char offset)
    if(font_bmp.colour_depth == Is1BIT) {
      u8 height = (font_bmp.height/8) + ((font_bmp.height%8) ? 1 : 0);
      u8 *byte = font_bmp.memory + ((u8)(c/16)*height*font_bmp.width*16) + (font_bmp.width*(c%16));
      len++;
      int i, j;
      for(i=1; i< font_bmp.width; i++){
        u32 char_slice = 0;
        for(j=0; j< height; j++)char_slice |= ( (*(byte+i+(j*font_bmp.width*16))) << (j*8) );
        //DEBUG_MSG("%c slice:%d -> %x", c, i, char_slice);
        if(char_slice==0){
          len +=i;
          break;
        }
      }
        //DEBUG_MSG("%c %d", c, len);
    }
  }else{
    // no kerning(legacy)
    len=font_bmp.width;
  }

  return len; // not supported
}

/////////////////////////////////////////////////////////////////////////////
// return a character kerning length
// IN: the character
// OUT: kerning length
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_StringKernGet(const char *str)
{
  s32 len=0;
  while( *str != '\0' )len +=(s32)APP_LCD_CharKernGet(*str++);
  return len; // not supported
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a single character in bitmap(1 or 4Bit depends on bitmap)
//! \param[in] destination bitmap, x/y position, fusion mod, character to be print
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_PrintChar(mios32_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, char c)
{
  if( !font_bmp.width )
    return -2;  // font not initialized yet!
  
  // legacy 1bit to 1bit
  if((bitmap.colour_depth == font_bmp.colour_depth) && (font_bmp.colour_depth == Is1BIT)) {
    mios32_lcd_bitmap_t char_bmp = font_bmp;
    u8 height = (char_bmp.height/8) + ((char_bmp.height%8) ? 1 : 0);
    char_bmp.line_offset = char_bmp.width*16;   // font table in ASCII format(16 char by line)
    char_bmp.memory += ((u8)(c/16)*char_bmp.line_offset*height) + (font_bmp.width*(c%16));
    char_bmp.width=(u16)APP_LCD_CharKernGet(c);
    APP_LCD_BitmapFusion(char_bmp, luma, bitmap, x, y, fusion);
    
  }else return -1;   // not supported
  
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a \\0 (zero) terminated string
//! \param[in] destination bitmap, x/y position, fusion mod,
//! str pointer to string.
//! \param[in]
//! \return < 0 on errors, or string length in pixels
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_PrintString(mios32_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, u8 alignment, const char *str)
{
  s32 status = 0;
  u16 offset = 0;
  
  // calc start point depending on alignment
  const char *s = str;
  u16 len=0;
  // kerning(char offset)
  while( *s != '\0' ){
    len += (u16)APP_LCD_CharKernGet(*s);
    s++;
  }
  if(alignment==APP_LCD_STRING_ALIGN_CENTER)x -= len/2;
  if(alignment==APP_LCD_STRING_ALIGN_RIGHT)x -= len;
  // start spelling
  offset = 0;
  while( *str != '\0' ){
    status |= APP_LCD_PrintChar(bitmap, luma, x+offset, y, fusion, *str);
    offset +=(u16)APP_LCD_CharKernGet(*str);
    str++;
  }
  return (status<0)?status:(s32)offset;
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a \\0 (zero) terminated formatted string (like printf)
//! \param[in] destination bitmap, x/y position, fusion mod,
//! *format zero-terminated format string - 64 characters supported maximum!
//! \param ... additional arguments
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_PrintFormattedString(mios32_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, u8 alignment, const char *format, ...)
{
  char buffer[64]; // TODO: tmp!!! Provide a streamed COM method later!
  va_list args;
  
  va_start(args, format);
  vsprintf((char *)buffer, format, args);
  return APP_LCD_PrintString(bitmap, luma, x, y, fusion, alignment, buffer);
}


/////////////////////////////////////////////////////////////////////////////
//! Only supported for graphical SSD1322 OLEDs:
//! initializes a bitmap type.
//!
//! Example:
//! \code
//!   // global array (!)
//!   u8 bitmap_array[APP_OLED_BITMAP_SIZE];
//!
//!   // Initialisation:
//!   mios32_lcd_bitmap_t bitmap = BM_LCD_BitmapClear(bitmap_array,
//!   						    APP_LCD_NUM_X*APP_OLED_WIDTH,
//!   						    APP_LCD_NUM_Y*APP_OLED_HEIGHT,
//!   						    APP_LCD_NUM_X*APP_OLED_WIDTH.
//!                   APP_LCD_COLOUR_DEPTH);
//! \endcode
//!
//! \param[in] memory pointer to the bitmap array
//! \param[in] width width of the bitmap (usually APP_LCD_NUM_X*APP_OLED_WIDTH)
//! \param[in] height height of the bitmap (usually APP_LCD_NUM_Y*APP_LCD_HEIGHT)
//! \param[in] line_offset byte offset between each line (usually same value as width)
//! \param[in] colour_depth how many bits are allocated by each pixel (usually APP_LCD_COLOUR_DEPTH)
//! \return a configured bitmap as mios32_lcd_bitmap_t
/////////////////////////////////////////////////////////////////////////////
mios32_lcd_bitmap_t APP_LCD_BitmapInit(u8 *memory, u16 width, u16 height, u16 line_offset, app_lcd_color_depth_t colour_depth)
{
  mios32_lcd_bitmap_t bitmap;
  
  bitmap.memory = memory;
  bitmap.width = width;
  bitmap.height = height;
  bitmap.line_offset = line_offset;
  bitmap.colour_depth = colour_depth;
  
  return bitmap;
}


/////////////////////////////////////////////////////////////////////////////
// Sets a pixel in the bitmap
// IN: bitmap, x/y position and fusion mode for native 4Bit bitmap
// color is given by APP_LCD_FColourSet,
// app_lcd_fore_color>0 is a white pixel for legacy 1Bit bimap
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPixelSet(mios32_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour)
{
  if( x >= bitmap.width || y >= bitmap.height )
    return -1; // pixel is outside bitmap
  
  
  if(bitmap.colour_depth == 1) {  // 1bit format
    u8 *pixel = (u8 *)&bitmap.memory[bitmap.line_offset*(y / 8) + x];
    u8 mask = 1 << (y % 8);
    *pixel &= ~mask;
    if( colour ) *pixel |= mask;
    
  }else return -1;  // not supported
  
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Draw a rectangle in the bm_cs_lcd_screen_bmp from position and size
// IN: x1/y1 first point, x2/y2 second point, border(e.g. 0x55 is dot line) and fill 0=none 1=empty 2=fill
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapRectangle(mios32_lcd_bitmap_t bitmap, s16 x, s16 y, u16 width, u16 height, u8 border, u32 bd_color, u8 fill, u32 back_color)
{
  if( (x >= bitmap.width) || (y >= bitmap.height) || ((x + width) < 0) || ((y + height) < 0) )return -1; // pixel is outside bm_cs_lcd_screen_bmp
  s16 i, j;

//  /* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
//  if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
//    // toDo
//
//    /* legacy 1bit pixel print */
//  }else if(bitmap.colour_depth == 1) {  // 1bit format
    // fill rect first
    if(fill)for(i=0; i< (width); i++)for(j=0; j< (height); j++)APP_LCD_BitmapPixelSet(bitmap, (u16)(x+i), (u16)(y+j), back_color);

    // border
    if(border){
    u16 border_pix=0;
    for(i=0; i< (width); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+i), (u16)y, bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+i), (u16)y, back_color);
      border_pix++;
    }
    for(i=1; i< (height); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+width-1), (u16)(y+i), bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+width-1), (u16)(y+i), back_color);
      border_pix++;
    }
    for(i=1; i< (width); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+width-i-1), (u16)(y + height-1), bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+width-i-1), (u16)(y + height-1), back_color);
      border_pix++;
    }
    for(i=1; i< (height); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)x, (u16)(y+height-i-1), bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)x, (u16)(y+height-i-1), back_color);
      border_pix++;
    }
    }
  //}else return -1;  // not supported

  return 1; // ok
}


/////////////////////////////////////////////////////////////////////////////
// Sets a byte in the bitmap, whathever its position in y,
// byte doesn't need to match the oled segment
// used for legacy 1bit bitmap
// IN: bm_cs_lcd_screen_bmp, x/y position and colour value (value range depends on APP_LCD_COLOUR_DEPTH)
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapByteSet(mios32_lcd_bitmap_t bitmap, s16 x, s16 y, u8 value)
{
  if( x >= bitmap.width || y >= bitmap.height || x < 0 || ((y + 8) < 0))
    return -1; // pixel is outside bm_cs_lcd_screen_bmp
  
  u8 mask;
  u8 val;
  if((y % 8) !=0){
    if(y > 0){
      u8 *byte1 = (u8 *)&bitmap.memory[bitmap.line_offset*(y / 8) + x];
      mask = 0xff << (y % 8);
      val = value << (y % 8);
      *byte1 &= ~mask;
      if( value ) *byte1 |= val;
    }
    if((y+8) >=0){
      u8 *byte2 = (u8 *)&bitmap.memory[bitmap.line_offset*((y+8) / 8) + x];
      mask = 0xff >> (8-((y+8) % 8));
      val = value >> (8-((y+8) % 8));
      *byte2 &= ~mask;
      if( value ) *byte2 |= val;
    }
  }else if(y >=0){
    u8 *byte = (u8 *)&bitmap.memory[bitmap.line_offset*(y / 8) + x];
    *byte = value;
  }
  
  return 1; // ok
}

/////////////////////////////////////////////////////////////////////////////
// local, used by APP_LCD_Bitmap4BitLuma and APP_LCD_BitmapFusion
/////////////////////////////////////////////////////////////////////////////
u16 APP_LCD_HelpPixelLuma(u16 pix_mem, float luma)
{
  if(luma == 1.0)return pix_mem;
  u8 r = (u8)(((pix_mem >> 11) & 0x1f)*(luma));
  u8 g = (u8)(((pix_mem >> 5) & 0x3f)*(luma));
  u8 b = (u8)((pix_mem & 0x1f)*(luma));
  return ((r<<11) | (g<<5) | b);
}


/////////////////////////////////////////////////////////////////////////////
//! Change the Luminance of a native 4Bit bitmap within given boundaries
//! IN: bitmap, x/y position, width and heigth, luma
//! Notes:
//! luma is a float between -1.0 and +16.0(from black to all pixels saturated)
//! luma neutral is 0.0
//!
//! OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Bitmap16BitLuma(mios32_lcd_bitmap_t bitmap, s16 x, s16 y, u16 width, u16 height, float luma)
{
  if( (x >= bitmap.width) || (y >= bitmap.height) || ((x+width) < 0) || ((y+height) < 0))
    return -2;  // bitmap is outside screen
  
  /* native 4bit depth only */
  if(bitmap.colour_depth == Is16BIT) {
    u16 xi, yi;
    // loop y (with crop)
    for(yi=((y<0)? 0 : y); yi<(((height+y)>bitmap.height)? bitmap.height : (height+y)); yi++){
      // loop x (with crop)
      for(xi=((x<0)? 0 : x); xi<(((width+x)>bitmap.width)? bitmap.width : (width+x)); xi++){
        // set pointer
        u8* bmp_mem_ptr = bitmap.memory + (yi*bitmap.line_offset + xi)*2;
        // get the pixels
        u16 bmp_pix = *bmp_mem_ptr <<8;
        bmp_pix |= *(bmp_mem_ptr+1);
        // set luma
        bmp_pix = APP_LCD_HelpPixelLuma(bmp_pix, luma);
        *bmp_mem_ptr++ = bmp_pix >>8;
        *bmp_mem_ptr++ = bmp_pix &0xff;
      }
    }
  }else return -1;  // not supported
  return 1; // ok
}

/////////////////////////////////////////////////////////////////////////////
//! Only supported for graphical SSD1322 OLEDs:
//! fusion, with different modes, of two bitmaps at specific position.
//! Luminance of the source can be modified at the same time
//! Note: if you need to mod luma the dest please use APP_LCD_Bitmap4BitLuma
//! see notes in APP_LCD_Bitmap4BitLuma
//!
//! Example for a legacy 1Bit(bitmap) to a native 4Bit bitmap(screen_bmp):
//! \code
//!   // bitmap is source
//!   APP_LCD_FColourSet(55);
//!   APP_LCD_BitmapFusion(bitmap, 0.0, screen_bmp, 0, 0, XOR);
//!   APP_LCD_BitmapPrint(screen_bmp);
//! \endcode
//!
//! \param[in] source, destination bitmaps, x/y position, fusion mode
//! fusion modes(new):
//!   REPLACE, replace bit or nibble (pixels)
//!   NOBLACK, pixel or nibble replace except if 0(black)
//!   OR, or bit or nibble (pixels)
//!   AND, and bit or nibble (pixels)
//!   XOR, xor bit or nibble (pixels)
//! \return < 0 on errors, resulting bimap is in destination bitmap
/////////////////////////////////////////////////////////////////////////////
u16 APP_LCD_PixelFusion(u16 fore_pix, float fore_luma, u16 back_pix, float back_luma, app_lcd_fusion_t fusion)
{
  u16 pix;
        //Process luma
        fore_pix = APP_LCD_HelpPixelLuma(fore_pix, fore_luma);
        back_pix = APP_LCD_HelpPixelLuma(back_pix, back_luma);
        pix = back_pix;
        switch (fusion) {
          case NOBLACK:
            if(!fore_pix){
              break;
            }
          case REPLACE:
            pix = fore_pix;
            break;
          case OR:
            pix |= fore_pix;
            break;
          case AND:
            pix &= fore_pix;
            break;
          case XOR:
            pix ^= fore_pix;
            break;
          default:
            break;
        }

  return pix; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Only supported for graphical SSD1322 OLEDs:
//! fusion, with different modes, of two bitmaps at specific position.
//! Luminance of the source can be modified at the same time
//! Note: if you need to mod luma the dest please use APP_LCD_Bitmap4BitLuma
//! see notes in APP_LCD_Bitmap4BitLuma
//!
//! Example for a legacy 1Bit(bitmap) to a native 4Bit bitmap(screen_bmp):
//! \code
//!   // bitmap is source
//!   APP_LCD_FColourSet(55);
//!   APP_LCD_BitmapFusion(bitmap, 0.0, screen_bmp, 0, 0, XOR);
//!   APP_LCD_BitmapPrint(screen_bmp);
//! \endcode
//!
//! \param[in] source, destination bitmaps, x/y position, fusion mode
//! fusion modes(new):
//!   REPLACE, replace bit or nibble (pixels)
//!   NOBLACK, pixel or nibble replace except if 0(black)
//!   OR, or bit or nibble (pixels)
//!   AND, and bit or nibble (pixels)
//!   XOR, xor bit or nibble (pixels)
//! \return < 0 on errors, resulting bimap is in destination bitmap
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapFusion(mios32_lcd_bitmap_t top_bmp, float top_luma, mios32_lcd_bitmap_t bmp, s16 top_pos_x, s16 top_pos_y, app_lcd_fusion_t fusion)
{
  if( (top_pos_x >= bmp.width) || (top_pos_y >= bmp.height) || ((top_pos_x+top_bmp.width) < 0) || ((top_pos_y+top_bmp.height) < 0))
    return -2;  // bitmap is outside screen
  
  if((top_bmp.colour_depth == bmp.colour_depth) && (bmp.colour_depth == Is1BIT) ) {
    int i, j;
    u8 height = top_bmp.height/8 + ((top_bmp.height%8) ? 1 : 0);
    u8 *byte = top_bmp.memory;
    for(i=0; i< top_bmp.width; i++){
      // forward to legacy 1bit process
      for(j=0; j< height; j++){
        if(!byte)APP_LCD_BitmapByteSet(bmp, top_pos_x+i, top_pos_y+(j*8), *(byte+i+(j*top_bmp.line_offset)));
      }
    }
  }else return -1;  // not supported
  
  return 0; // no error
}

///////////////////////////////////////////////////////////////////////////////
//// Transfers Bitmap to the TFT
//// Notes: using back/fore colors respectively from pixel off/on for 1bit,
//// trasferred to APP_LCD_NativeBitmapPrint for native 16bit
//// IN: bitmap
//// OUT: returns < 0 on errors
///////////////////////////////////////////////////////////////////////////////
//s32 APP_LCD_BitmapHBoundaryPrint(mios32_lcd_bitmap_t bitmap, u16 b_x, u16 b_width)
//{
//
//  //if( !MIOS32_LCD_TypeIsGLCD() )
//  //return -1; // no GLCD
//
//  // abort if max. width reached
//  //if( mios32_lcd_x >= mios32_lcd_parameters.width )
//  //return -2;
//
//  /* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
//  if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
//    //    u16 *memory_ptr = bitmap.memory + ((bitmap.line_offset*top_pos_y + top_pos_x)*2);
//    //    // transfer bitmap
//    //    int top_pos_x, y;
//    //    for(y=0; y<8; ++y){
//    //      for(x=0; x<bitmap.width; ++x){
//    //        APP_LCD_Data(*memory_ptr >> 8);
//    //        APP_LCD_Data(*memory_ptr++ & 0xff);
//    //      }
//    //    }
//    /* legacy 1bit pixel print */
//  }else if(bitmap.colour_depth == 1) {  // 1bit format
//    //fill fromr regular 1bit using back and fore colors
//    // all GLCDs support the same bitmap scrambling
//    int line;
//    int y_lines = (bitmap.height >> 3);
//
//    u16 initial_y = mios32_lcd_y;
//    for(line=0; line<y_lines; ++line) {
//
//      // calculate pointer to bitmap line
//      u8 *memory_ptr = bitmap.memory + line * bitmap.line_offset + b_x;
//
//      // set graphical cursor after second line has reached
//      //    if( line > 0 ) {
//      //      mios32_lcd_x = initial_x;
//      //      mios32_lcd_y += 1;
//      //      APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
//      //    }
//
//      // transfer bitmap
//      int x, y;
//      for(y=0; y<8; ++y){
//        for(x=b_x; ((b_width+b_x)>bitmap.width)? (x<bitmap.width) : (x< (b_width+b_x)); ++x){
//          //for(x=b_x; x< (b_width+b_x); ++x){
//          if(*memory_ptr & (1<<y)){
//            APP_LCD_Data(app_lcd_fore_color >> 8);
//            APP_LCD_Data(app_lcd_fore_color & 0xff);
//          }else{
//            APP_LCD_Data(app_lcd_back_color >> 8);
//            APP_LCD_Data(app_lcd_back_color & 0xff);
//          }
//          //DEBUG_MSG("%d %d %d", x, y, memory_ptr);
//          memory_ptr++;
//        }
//        memory_ptr = bitmap.memory + line * bitmap.line_offset + b_x;
//        mios32_lcd_y += 1;
//        APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
//      }
//    }
//    // fix graphical cursor if more than one line has been print
//    mios32_lcd_x += bitmap.width;
//    if( y_lines >= 1 ) {
//      mios32_lcd_y = initial_y;
//      APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
//    }
//  }else return -1;  // not supported
//
//
//
//
//  return 0; // no error
//}

/////////////////////////////////////////////////////////////////////////////
// Transfers Bitmap to the TFT
// Notes: using back/fore colors respectively from pixel off/on for 1bit,
// trasferred to APP_LCD_NativeBitmapPrint for native 16bit
// IN: bitmap
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPrint(mios32_lcd_bitmap_t bitmap)
{
  
  if( lcd_testmode )
    return -1; // direct access disabled in testmode

  if( !MIOS32_LCD_TypeIsGLCD() )
    return -1; // no GLCD

  // abort if max. width reached
  if( mios32_lcd_x >= mios32_lcd_parameters.width )
    return -2;

  // all GLCDs support the same bitmap scrambling
  int line;
  int y_lines = (bitmap.height >> 3);

  u16 initial_x = mios32_lcd_x;
  u16 initial_y = mios32_lcd_y;
  for(line=0; line<y_lines; ++line) {

    // calculate pointer to bitmap line
    u8 *memory_ptr = bitmap.memory + line * bitmap.line_offset;

    // set graphical cursor after second line has reached
    if( line > 0 ) {
      mios32_lcd_x = initial_x;
      mios32_lcd_y += 8;
      APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
    }

    // transfer character
    int x;
    for(x=0; x<bitmap.width; ++x)
      APP_LCD_Data(*memory_ptr++);
  }

  // fix graphical cursor if more than one line has been print
  if( y_lines >= 1 ) {
    mios32_lcd_y = initial_y;
    APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
  }

  return 0; // no error
}
