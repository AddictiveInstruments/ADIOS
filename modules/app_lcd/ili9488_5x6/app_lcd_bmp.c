/* Copyright 2016 - 2017 Marc Volker Dickmann
 * Project: LibBMP
 */
 



/////////////////////////////////////////////////////////////////////////////
// Include files & defines
/////////////////////////////////////////////////////////////////////////////

#include <adios.h>

// this module has to be enabled in a local adios_config.h file (included from adios.h)
#if defined(APP_LCD_USE_BMP_EXPORT)

#include <stdio.h>
#include <stdlib.h>
#include "app_lcd_bmp.h"

#ifndef DEBUG_MSG
#define DEBUG_MSG ADIOS_MIDI_SendDebugMessage
#endif

#define DEBUG_BMP_VERBOSE_LEVEL 0

// BMP_HEADER
/////////////////////////////////////////////////////////////////////////////
// header initialization
/////////////////////////////////////////////////////////////////////////////
void app_lcd_bmp_header_init(app_lcd_bmp_header_t *header, const int width, const int height)
{
  header->bfSize = (sizeof (app_lcd_bmp_pixel_t) * width + APP_LCD_BMP_GET_PADDING (width)) * abs(height);
  header->bfReserved = 0;
  header->bfOffBits = 54;
  header->biSize = 40;
  header->biWidth = width;
  header->biHeight = height;
  header->biPlanes = 1;
  header->biBitCount = 24;
  header->biCompression = 0;
  header->biSizeImage = 0;
  header->biXPelsPerMeter = 0;
  header->biYPelsPerMeter = 0;
  header->biClrUsed = 0;
  header->biClrImportant = 0;
}

/////////////////////////////////////////////////////////////////////////////
// header write
/////////////////////////////////////////////////////////////////////////////
app_lcd_bmp_error_t app_lcd_bmp_header_write(const app_lcd_bmp_header_t *header, FILE *img_file)
{
  if (header == NULL)
  {
#if DEBUG_BMP_VERBOSE_LEVEL >= 1
    DEBUG_MSG("[app_lcd_bmp_header_write]ERROR: Not a valid Header!");
#endif
    return BMP_HEADER_NOT_INITIALIZED;
  }
  else if (img_file == NULL)
  {
#if DEBUG_BMP_VERBOSE_LEVEL >= 2
    DEBUG_MSG("[app_lcd_bmp_header_write]ERROR: Can't open file!");
#endif
    return BMP_FILE_NOT_OPENED;
  }
  
  // Since an adress must be passed to fwrite, create a variable!
  const unsigned short magic = APP_LCD_BMP_MAGIC;
  fwrite (&magic, sizeof (magic), 1, img_file);
  
  // Use the type instead of the variable because its a pointer!
  fwrite (header, sizeof (app_lcd_bmp_header_t), 1, img_file);
  return BMP_OK;
}

/////////////////////////////////////////////////////////////////////////////
// header read
/////////////////////////////////////////////////////////////////////////////
// purpose is to export the screen to a bitmap file, not to read one, but feel free to implement it if you need it
//app_lcd_bmp_error_t app_lcd_bmp_header_read (app_lcd_bmp_header_t *header, FILE *img_file)
//{
//	if (img_file == NULL)
//	{
//		return BMP_FILE_NOT_OPENED;
//	}
//
//	// Since an adress must be passed to fread, create a variable!
//	unsigned short magic;
//
//	// Check if its an bmp file by comparing the magic nbr:
//	if (fread (&magic, sizeof (magic), 1, img_file) != 1 ||
//	    magic != APP_LCD_BMP_MAGIC)
//	{
//		return BMP_INVALID_FILE;
//	}
//
//	if (fread (header, sizeof (app_lcd_bmp_header_t), 1, img_file) != 1)
//	{
//		return BMP_ERROR;
//	}
//
//	return BMP_OK;
//}

// APP_LCD_BMP_PIXEL
/////////////////////////////////////////////////////////////////////////////
// pixel initialization
/////////////////////////////////////////////////////////////////////////////
void app_lcd_bmp_pixel_init(app_lcd_bmp_pixel_t *pix, u8 *screen_pix)
{
  u16 colour = (u16)((*screen_pix++)<<8);
  colour |= (u16)(*screen_pix);
  pix->red = ((colour >> 11)& 0x1f)<<3;
  pix->green = ((colour >> 5)& 0x3f)<<2;
  pix->blue = (colour & 0x1f)<<3;
}

// BMP_IMG
/////////////////////////////////////////////////////////////////////////////
// image alloc
/////////////////////////////////////////////////////////////////////////////
void app_lcd_bmp_img_alloc(app_lcd_bmp_img_t *img)
{
  const size_t h = abs (img->img_header.biHeight);
  
  // Allocate the required memory for the pixels:
  img->img_pixels = malloc (sizeof (app_lcd_bmp_pixel_t*) * h);
  
//  for (size_t y = 0; y < h; y++)
//  {
//    img->img_pixels[y] = malloc (sizeof (app_lcd_bmp_pixel_t) * img->img_header.biWidth);
//  }
}

/////////////////////////////////////////////////////////////////////////////
// image initialization
/////////////////////////////////////////////////////////////////////////////
void app_lcd_bmp_img_init (app_lcd_bmp_img_t *img, const int width, const int height)
{
  // INIT the header with default values:
  app_lcd_bmp_header_init (&img->img_header, width, height);
  app_lcd_bmp_img_alloc (img);
}

/////////////////////////////////////////////////////////////////////////////
// image de-alloc
/////////////////////////////////////////////////////////////////////////////
void app_lcd_bmp_img_free (app_lcd_bmp_img_t *img)
{
//  const size_t h = abs (img->img_header.biHeight);
  
//  for (size_t y = 0; y < h; y++)
//  {
//    free (img->img_pixels[y]);
//  }
//  free (img->img_pixels);
}

/////////////////////////////////////////////////////////////////////////////
// image write
/////////////////////////////////////////////////////////////////////////////
app_lcd_bmp_error_t app_lcd_bmp_img_write(const app_lcd_bmp_img_t *img, const char *filename)
{
  FILE *img_file = fopen (filename, "wb");
  
  if (img_file == NULL)
  {
#if DEBUG_BMP_VERBOSE_LEVEL >= 2
    DEBUG_MSG("[app_lcd_bmp_img_write]ERROR: Can't open file!");
#endif
    return BMP_FILE_NOT_OPENED;
  }
  
  // NOTE: This way the correct error code could be returned.
  const app_lcd_bmp_error_t err = app_lcd_bmp_header_write (&img->img_header, img_file);
  
  if (err != BMP_OK)
  {
#if DEBUG_BMP_VERBOSE_LEVEL >= 2
    DEBUG_MSG("[app_lcd_bmp_img_write]ERROR: Could'nt write the header!");
#endif
    fclose (img_file);
    return err;
  }
  
  // Select the mode (bottom-up or top-down):
//  const size_t h = abs (img->img_header.biHeight);
//  const size_t offset = (img->img_header.biHeight > 0 ? h - 1 : 0);
  
  // Create the padding:
//  const unsigned char padding[3] = {'\0', '\0', '\0'};
  
//  // Write the content:
//  for (size_t y = 0; y < h; y++)
//  {
//    // Write a whole row of pixels to the file:
//    fwrite (img->img_pixels[abs (offset - y)], sizeof (app_lcd_bmp_pixel_t), img->img_header.biWidth, img_file);
//
//    // Write the padding for the row!
//    fwrite (padding, sizeof (unsigned char), APP_LCD_BMP_GET_PADDING (img->img_header.biWidth), img_file);
//  }
  
  // NOTE: All good!
  fclose (img_file);
  return BMP_OK;
}

/////////////////////////////////////////////////////////////////////////////
// image read
/////////////////////////////////////////////////////////////////////////////
// purpose is to export the screen to a bitmap file, not to read one, but feel free to implement it if you need it
//app_lcd_bmp_error_t app_lcd_bmp_img_read(app_lcd_bmp_img_t *img, const char *filename)
//{
//	FILE *img_file = fopen (filename, "rb");
//
//	if (img_file == NULL)
//	{
//		return BMP_FILE_NOT_OPENED;
//	}
//
//	// NOTE: This way the correct error code can be returned.
//	const enum app_lcd_bmp_error_t err = app_lcd_bmp_header_read (&img->img_header, img_file);
//
//	if (err != BMP_OK)
//	{
//		// ERROR: Could'nt read the image header!
//		fclose (img_file);
//		return err;
//	}
//
//	app_lcd_bmp_img_alloc (img);
//
//	// Select the mode (bottom-up or top-down):
//	const size_t h = abs (img->img_header.biHeight);
//	const size_t offset = (img->img_header.biHeight > 0 ? h - 1 : 0);
//	const size_t padding = APP_LCD_BMP_GET_PADDING (img->img_header.biWidth);
//
//	// Needed to compare the return value of fread
//	const size_t items = img->img_header.biWidth;
//
//	// Read the content:
//	for (size_t y = 0; y < h; y++)
//	{
//		// Read a whole row of pixels from the file:
//		if (fread (img->img_pixels[abs (offset - y)], sizeof (app_lcd_bmp_pixel_t), items, img_file) != items)
//		{
//			fclose (img_file);
//			return BMP_ERROR;
//		}
//
//		// Skip the padding:
//		fseek (img_file, padding, SEEK_CUR);
//	}
//
//	// NOTE: All good!
//	fclose (img_file);
//	return BMP_OK;
//}

#endif /* defined(APP_LCD_USE_BMP_EXPORT) */
