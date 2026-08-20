/*
 * Header File for GLCD Fonts
 *
 * ==========================================================================
 */

#ifndef _GLCD_FONT_H
#define _GLCD_FONT_H

/////////////////////////////////////////////////////////////////////////////
// How a font array is laid out
//
// Every font below starts with four header bytes, then the bitmap:
//
//   [0] width of one character, in pixels
//   [1] height
//   [2] X0 - horizontal offset applied when drawing
//   [3] offset between two consecutive characters, in bytes
//   [4] first byte of the bitmap
//
// A display driver needs these to walk the array, which is why they live
// here with the fonts they describe rather than in whatever draws them.
/////////////////////////////////////////////////////////////////////////////

#define GLCD_FONT_WIDTH_IX    0
#define GLCD_FONT_HEIGHT_IX   1
#define GLCD_FONT_X0_IX       2
#define GLCD_FONT_OFFSET_IX   3
#define GLCD_FONT_BITMAP_IX   4


/////////////////////////////////////////////////////////////////////////////
// Defines array pointers to all available fonts
/////////////////////////////////////////////////////////////////////////////

extern const u8 GLCD_FONT_NORMAL[];
extern const u8 GLCD_FONT_NORMAL_INV[];
extern const u8 GLCD_FONT_BIG[];
extern const u8 GLCD_FONT_SMALL[];
extern const u8 GLCD_FONT_TINY[];
extern const u8 GLCD_FONT_TINY_WIDE[];
extern const u8 GLCD_FONT_TINY_INV[];
extern const u8 GLCD_FONT_BITLOW[];
extern const u8 GLCD_FONT_1BIT_PIX[];
extern const u8 GLCD_FONT_PIXEL12X10[];
extern const u8 GLCD_FONT_9BITRPR[];

//extern const u8 GLCD_FONT_KNOB_ICONS[];
//extern const u8 GLCD_FONT_METER_ICONS_H[];
//extern const u8 GLCD_FONT_METER_ICONS_V[];
//extern const u8 GLCD_FONT_MINIKNOB_ICONS[];
//extern const u8 GLCD_FONT_MINITOGSEL_ICONS[];

#endif /* _GLCD_FONT_H */
