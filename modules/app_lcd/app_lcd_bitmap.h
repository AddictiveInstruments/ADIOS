/*
 * A rectangle of pixels, as passed between an application and a display
 * driver.
 *
 * Shared by every app_lcd driver, and by the applications that hand them
 * bitmaps to draw, so it lives beside the drivers rather than inside any one
 * of them. Each driver's own app_lcd.h includes this file; an application
 * gets it for free by including app_lcd.h.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _APP_LCD_BITMAP_H
#define _APP_LCD_BITMAP_H

typedef struct {
  u8  *memory;       // where the pixels are
  u16 width;         // in pixels
  u16 height;        // in pixels
  u16 line_offset;   // bytes from the start of one line to the next
  u8  colour_depth;  // bits per pixel
} adios_lcd_bitmap_t;

#endif /* _APP_LCD_BITMAP_H */
