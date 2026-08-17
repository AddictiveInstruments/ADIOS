
#ifndef _APP_LCD_BMP_H
#define _APP_LCD_BMP_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////
#define APP_LCD_BMP_MAGIC 19778
#define APP_LCD_BMP_GET_PADDING(a) ((a) % 4)

/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////
typedef enum{
	BMP_FILE_NOT_OPENED = -4,
	BMP_HEADER_NOT_INITIALIZED,
	BMP_INVALID_FILE,
	BMP_ERROR,
	BMP_OK = 0
}app_lcd_bmp_error_t;

typedef struct{
	unsigned int   bfSize;
	unsigned int   bfReserved;
	unsigned int   bfOffBits;
	
	unsigned int   biSize;
	int            biWidth;
	int            biHeight;
	unsigned short biPlanes;
	unsigned short biBitCount;
	unsigned int   biCompression;
	unsigned int   biSizeImage;
	int            biXPelsPerMeter;
	int            biYPelsPerMeter;
	unsigned int   biClrUsed;
	unsigned int   biClrImportant;
} app_lcd_bmp_header_t;

typedef struct{
	unsigned char blue;
	unsigned char green;
	unsigned char red;
} app_lcd_bmp_pixel_t;

// This is faster than a function call
#define APP_LCD_BMP_PIXEL(r,g,b) ((app_lcd_bmp_pixel_t){(b),(g),(r)})

typedef struct
{
	app_lcd_bmp_header_t   img_header;
	app_lcd_bmp_pixel_t  **img_pixels;
} app_lcd_bmp_img_t;

/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

// BMP_HEADER
extern void app_lcd_bmp_header_init(app_lcd_bmp_header_t *header, const int width, const int height);
extern app_lcd_bmp_error_t app_lcd_bmp_header_write(const app_lcd_bmp_header_t *header, FILE *img_file);
// purpose is to export the screen to a bitmap file, not to read one, but feel free to implement it if you need it
//extern app_lcd_bmp_error_t  app_lcd_bmp_header_read(app_lcd_bmp_header_t*, FILE*);

// APP_LCD_BMP_PIXEL
extern void app_lcd_bmp_pixel_init(app_lcd_bmp_pixel_t *pix, u8 *screen_pix);

// BMP_IMG
extern void app_lcd_bmp_img_alloc(app_lcd_bmp_img_t *img);
extern void app_lcd_bmp_img_init (app_lcd_bmp_img_t *img, const int width, const int height);
extern void app_lcd_bmp_img_free (app_lcd_bmp_img_t *img);
extern app_lcd_bmp_error_t app_lcd_bmp_img_write(const app_lcd_bmp_img_t *img, const char *filename);
// purpose is to export the screen to a bitmap file, not to read one, but feel free to implement it if you need it
//extern app_lcd_bmp_error_t  app_lcd_bmp_img_read(app_lcd_bmp_img_t*, const char*);

#endif /* _APP_LCD_BMP_H */
