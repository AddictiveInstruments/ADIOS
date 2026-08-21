/*
 * Application specific OLED driver for up to 1 * SSD1322 (more toDo)
 * Referenced from ADIOS_LCD routines
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <adios.h>
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
/**SPI2 GPIO Configuration
  PA1   ------> APP_LCD_CS			// local
  PA2   ------> APP_LCD_DC			// local
  PA5   ------> APP_LCD_LITE		// local

 */
#define APP_LCD_SPI  		1
#define APP_LCD_PORT 		GPIOB
#define APP_LCD_CS 	 		LL_GPIO_PIN_1
#define APP_LCD_DC 	 		LL_GPIO_PIN_2
#define APP_LCD_LITE 		LL_GPIO_PIN_0
#define APP_LCD_RST     	LL_GPIO_PIN_12

#define CS_ENA() 		ADIOS_SYS_STM_PINSET_0(APP_LCD_PORT, APP_LCD_CS)
#define CS_DIS() 		ADIOS_SYS_STM_PINSET_1(APP_LCD_PORT, APP_LCD_CS)
#define DC_COMMAND() 	ADIOS_SYS_STM_PINSET_0(APP_LCD_PORT, APP_LCD_DC)
#define DC_DATA() 		ADIOS_SYS_STM_PINSET_1(APP_LCD_PORT, APP_LCD_DC)
#define RST_IDLE() 			ADIOS_SYS_STM_PINSET_1(APP_LCD_PORT, APP_LCD_RST)
#define RST_ACT() 			ADIOS_SYS_STM_PINSET_0(APP_LCD_PORT, APP_LCD_RST)
#define LITE_ON() 			ADIOS_SYS_STM_PINSET_1(APP_LCD_PORT, APP_LCD_LITE)
#define LITE_OFF() 			ADIOS_SYS_STM_PINSET_0(APP_LCD_PORT, APP_LCD_LITE)

#define swap(a, b) { s16 t = a; a = b; b = t; }
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define APP_LCD_PIXEL_COUNT	APP_LCD_WIDTH * APP_LCD_HEIGHT

// Commands
#define APP_LCD_NOP     	0x00
#define APP_LCD_SWRESET 	0x01	// used
#define APP_LCD_RDDID   	0x04
#define APP_LCD_RDDST   	0x09

#define APP_LCD_RDMODE  	0x0A
#define APP_LCD_RDMADCTL  	0x0B
#define APP_LCD_RDPIXFMT  	0x0C
#define APP_LCD_RDIMGFMT  	0x0D
#define APP_LCD_RDSELFDIAG  0x0F

#define APP_LCD_SLPIN   	0x10
#define APP_LCD_SLPOUT 	 	0x11	// used
#define APP_LCD_PTLON   	0x12
#define APP_LCD_NORON   	0x13

#define APP_LCD_INVOFF  	0x20
#define APP_LCD_INVON   	0x21	// used
#define APP_LCD_ALLPIXON   	0x23	// used
#define APP_LCD_GAMMASET 	0x26
#define APP_LCD_DISPOFF	 	0x28
#define APP_LCD_DISPON  	0x29	// used

#define APP_LCD_CASET   	0x2A
#define APP_LCD_PASET   	0x2B
#define APP_LCD_RAMWR   	0x2C
#define APP_LCD_RAMRD   	0x2E

#define APP_LCD_PTLAR   	0x30
#define APP_LCD_MADCTL  	0x36	// used
#define APP_LCD_PIXFMT  	0x3A	// used

#define APP_LCD_IMCTR	 	0xB0	// used
#define APP_LCD_FRMCTR1 	0xB1	// used
#define APP_LCD_FRMCTR2 	0xB2
#define APP_LCD_FRMCTR3 	0xB3
#define APP_LCD_INVCTR  	0xB4	// used
#define APP_LCD_DFUNCTR 	0xB6

#define APP_LCD_PWCTR1  	0xC0	// used
#define APP_LCD_PWCTR2  	0xC1	// used
#define APP_LCD_PWCTR3  	0xC2
#define APP_LCD_PWCTR4  	0xC3
#define APP_LCD_PWCTR5  	0xC4
#define APP_LCD_VMCTR1  	0xC5	// used
#define APP_LCD_VMCTR2  	0xC7

#define APP_LCD_RDID1   	0xDA
#define APP_LCD_RDID2   	0xDB
#define APP_LCD_RDID3   	0xDC
#define APP_LCD_RDID4   	0xDD

#define APP_LCD_GMCTRP1 	0xE0	// used
#define APP_LCD_GMCTRN1 	0xE1	// used
#define APP_LCD_GMCTRD1 	0xE2
#define APP_LCD_GMCTRD2 	0xE3

#define APP_LCD_IMGFUNC 	0xE9	// used

#define APP_LCD_ADJCTR1 	0xD7
#define APP_LCD_ADJCTR2 	0xF2
#define APP_LCD_ADJCTR3 	0xF7	// used
#define APP_LCD_ADJCTR4 	0xF8
#define APP_LCD_ADJCTR5 	0xF9
#define APP_LCD_ADJCTR6 	0xFC
#define APP_LCD_ADJCTR7 	0xFF

#define APP_LCD_MADCTL_MY  	0x80	// used
#define APP_LCD_MADCTL_MX  	0x40	// used
#define APP_LCD_MADCTL_MV  	0x20	// used
#define APP_LCD_MADCTL_ML  	0x10	// used
#define APP_LCD_MADCTL_RGB 	0x00	// used
#define APP_LCD_MADCTL_BGR 	0x08	// used
#define APP_LCD_MADCTL_MH  	0x04	// used

/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////
void APP_LCD_DummyFunc(void);

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// Cursor position, in pixels. Owned by this driver: it is the one that
// knows where it last drew. (It used to live in a layer above, which meant
// two modules writing the same variable.)
static u16 app_lcd_x = 0;
static u16 app_lcd_y = 0;

static u32 display_available = 0;

// default color for legacy 1Bit bitmap
static u32 app_lcd_back_color = 0;
static u32 app_lcd_fore_color = 0;
// screen orientation and effective size
static u8  app_lcd_rotation=1;
static u16 app_lcd_width=APP_LCD_WIDTH;
static u16 app_lcd_height=APP_LCD_HEIGHT;
static const u16 ball[12] = {
							 0b0000000011110000,
							 0b0000001111111100,
							 0b0000011111111110,
							 0b0000011111111110,
							 0b0000111111111111,
							 0b0000111111111111,
							 0b0000111111111111,
							 0b0000111111111111,
							 0b0000011111111110,
							 0b0000011111111110,
							 0b0000001111111100,
							 0b0000000011110000};
static const u8 noire[5] = {
							 0b00011100,
							 0b00011110,
							 0b00011111,
							 0b00001111,
							 0b00000110};
static const u8 croche[4] = {
							 0b00001111,
							 0b00001110,
							 0b00011100,
							 0b11110000};
static const u8 corner[8] = {
							 0b00000001,
							 0b00000011,
							 0b00000111,
							 0b00001111,
							 0b00011111,
							 0b00111111,
							 0b01111111,
							 0b11111111};
//static const u8 beat[7] = {
//							 0b00001100,
//							 0b00011110,
//							 0b00111110,
//							 0b01111100,
//							 0b00111110,
//							 0b00011110,
//							 0b00001100};
static const u16 beat[9] = { 0b0000000011000110,
							 0b0000000111101111,
							 0b0000000111111111,
							 0b0000000111111111,
							 0b0000000011111110,
							 0b0000000011111110,
							 0b0000000001111100,
							 0b0000000000111000,
							 0b0000000000010000};
//static bool _cp437    = false;

// font bitmap
static adios_lcd_bitmap_t font_bmp;

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

	// (this driver used to publish its geometry into a shared structure, for
	// a layer above to read back. It knows its own geometry - APP_LCD_WIDTH,
	// APP_LCD_HEIGHT and the rest are in its own header - so there is nobody
	// to announce it to. One of those six lines assigned num_x twice, the
	// second time from APP_LCD_NUM_Y, and nothing ever noticed.)

	// configure GPIO
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* GPIO Ports Clock Enable */
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

	// initialize SPI interface
	// ensure that fast pin drivers are activated, do t before cause MISO pin is used as DC pin
	ADIOS_SPI_IO_Init(APP_LCD_SPI, ADIOS_SPI_PIN_DRIVER_STRONG);
	/**/
	LL_GPIO_ResetOutputPin(APP_LCD_PORT, APP_LCD_CS | APP_LCD_DC | APP_LCD_LITE);
	LL_GPIO_SetOutputPin(APP_LCD_PORT, APP_LCD_RST);
	/**/
	GPIO_InitStruct.Pin = APP_LCD_CS | APP_LCD_DC | APP_LCD_LITE | APP_LCD_RST;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
	LL_GPIO_Init(APP_LCD_PORT, &GPIO_InitStruct);

	LITE_OFF();
	RST_IDLE();		// clear RST
	CS_DIS();		// clear CS
	DC_DATA();		// Data
	// initialize SPI interface

	// init SPI port
	ADIOS_SPI_TransferModeInit(APP_LCD_SPI, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_8);
  
	APP_LCD_FColourSet(APP_LCD_WHITE);		// set default(startup) forecolor to full white
	APP_LCD_BColourSet(APP_LCD_BLACK);		// set default(startup) forecolor to full black

	// dummy command
	APP_LCD_Cmd(0x00);
	for(u16 d=0; d<150; d++)ADIOS_DELAY_Wait_uS(1000);		// wait for 150ms
	// wait for some, ili9488 ^startup is very long :/
	RST_ACT();		// clear RST
	ADIOS_DELAY_Wait_uS(20000);	// wait for 20ms
	RST_IDLE();		// clear RST
	for(u8 d=0; d<150; d++)ADIOS_DELAY_Wait_uS(1000);		// wait for 150ms
	APP_LCD_DelayedInit(0);
return 0;
}

/* HOST Mode */
s32 APP_LCD_SPI_TransferModeInit(void){
// init SPI port
	ADIOS_SPI_TransferModeInit(APP_LCD_SPI, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_8);
	return 0;
}
/////////////////////////////////////////////////////////////////////////////
// Sends data byte to LCD
// IN: data byte in <data>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DelayedInit(u32 mode){
	//************* Start Initial Sequence **********//
//	RST_ACT();													// starts hardware reset
//	ADIOS_DELAY_Wait_uS(10000);							// wait for 10ms
//	RST_IDLE();													// ends hardware reset
//	ADIOS_DELAY_Wait_uS(10000);							// wait for 10ms
//	APP_LCD_Cmd(APP_LCD_SWRESET);						// software reset
//	for(u16 d=0; d<120; d++)ADIOS_DELAY_Wait_uS(1000);		// wait for 120ms

	APP_LCD_Cmd(APP_LCD_ADJCTR3);
	APP_LCD_Data(0xA9);
	APP_LCD_Data(0x51);
	APP_LCD_Data(0X2C);
	APP_LCD_Data(0X82);


	APP_LCD_Cmd(APP_LCD_PWCTR1);
	APP_LCD_Data(0x0f);
	APP_LCD_Data(0x0f);

	//VGH = 5*VCI   VGL = -3*VCI
	APP_LCD_Cmd(APP_LCD_PWCTR2);
	APP_LCD_Data(0x47);


	APP_LCD_Cmd(APP_LCD_VMCTR1);
	APP_LCD_Data(0x00);
	APP_LCD_Data(0x4d);
	APP_LCD_Data(0x80);

//	APP_LCD_Cmd(APP_LCD_IMCTR);
//	APP_LCD_Data(0x00);

	APP_LCD_Cmd(APP_LCD_FRMCTR1);
	APP_LCD_Data(0xB0);
	APP_LCD_Data(0X11);
	//APP_LCD_Cmd(0xB1);
	//APP_LCD_Data(0xA0);

	APP_LCD_Cmd(APP_LCD_INVCTR);
	APP_LCD_Data(0x02);

	APP_LCD_Cmd(APP_LCD_DFUNCTR);
	APP_LCD_Data(0x02);
	APP_LCD_Data(0x02);

	APP_LCD_Cmd(0xb7);
	APP_LCD_Data(0xc6);

	APP_LCD_Cmd(0xbe);
	APP_LCD_Data(0x00);
	APP_LCD_Data(0x04);

	APP_LCD_Cmd(APP_LCD_IMGFUNC);
	APP_LCD_Data(0x00);

	APP_LCD_Cmd(APP_LCD_MADCTL);
	APP_LCD_Data(0x02);

	APP_LCD_Cmd(APP_LCD_PIXFMT);
	APP_LCD_Data(0x66);  //18bits

	APP_LCD_Cmd(APP_LCD_GMCTRP1);
	APP_LCD_Data(0x00);
	APP_LCD_Data(0x07);
	APP_LCD_Data(0x0b);
	APP_LCD_Data(0x03);
	APP_LCD_Data(0x0f);
	APP_LCD_Data(0x05);
	APP_LCD_Data(0x30);
	APP_LCD_Data(0x56);
	APP_LCD_Data(0x47);
	APP_LCD_Data(0x04);
	APP_LCD_Data(0x0b);
	APP_LCD_Data(0x0a);
	APP_LCD_Data(0x2d);
	APP_LCD_Data(0x37);
	APP_LCD_Data(0x0F);

	APP_LCD_Cmd(APP_LCD_GMCTRN1);
	APP_LCD_Data(0x00);
	APP_LCD_Data(0x0e);
	APP_LCD_Data(0x13);
	APP_LCD_Data(0x04);
	APP_LCD_Data(0x11);
	APP_LCD_Data(0x07);
	APP_LCD_Data(0x39);
	APP_LCD_Data(0x45);
	APP_LCD_Data(0x50);
	APP_LCD_Data(0x07);
	APP_LCD_Data(0x10);
	APP_LCD_Data(0x0d);
	APP_LCD_Data(0x32);
	APP_LCD_Data(0x36);
	APP_LCD_Data(0x0F);

	APP_LCD_Cmd(APP_LCD_SLPOUT);
	for(u8 d=0; d<120; d++)ADIOS_DELAY_Wait_uS(1000);		// wait for 120ms

	APP_LCD_Cmd(0x21); //IPS
	APP_LCD_Cmd(APP_LCD_DISPON);
	for(u8 d=0; d<25; d++)ADIOS_DELAY_Wait_uS(1000);		// wait for 25ms
	//APP_LCD_Cmd(APP_LCD_ALLPIXON);      					// All pixels on(testing purpose)
	APP_LCD_SetRotation(1);									// Set orientation

	display_available = 1;
	//return (display_available & (1 << adios_lcd_device)) ? 0 : -1; // return -1 if display not available
	return display_available;
}

/////////////////////////////////////////////////////////////////////////////
// Sends data byte to LCD
// IN: data byte in <data>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_IsReady(void){
	return display_available;
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
  u8 cs = app_lcd_y / APP_LCD_HEIGHT;
  
  if( cs >= 8 )
    return -1; // invalid CS line
#endif

	DC_DATA();
	CS_ENA();
	ADIOS_SPI_TransferByte(APP_LCD_SPI, data);
	CS_DIS();
  
	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Sends command byte to LCD
// IN: command byte in <cmd>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Cmd(u8 cmd)
{
	DC_COMMAND();
	CS_ENA();
	ADIOS_SPI_TransferByte(APP_LCD_SPI, cmd);
	CS_DIS();

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Set screen orientation
// IN: cr as rotation
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_SetRotation(u8 r)
{

	APP_LCD_Cmd(APP_LCD_MADCTL);
	app_lcd_rotation = r % 4; // can't be higher than 3
	switch (app_lcd_rotation) {
	case 0:
		APP_LCD_Data(APP_LCD_MADCTL_MX | APP_LCD_MADCTL_BGR);
		app_lcd_width = APP_LCD_HEIGHT;
		app_lcd_height = APP_LCD_WIDTH;
		break;
	case 1:
		APP_LCD_Data( APP_LCD_MADCTL_MV | APP_LCD_MADCTL_BGR);
		app_lcd_width = APP_LCD_WIDTH;
		app_lcd_height = APP_LCD_HEIGHT;
		break;
	case 2:
		APP_LCD_Data(APP_LCD_MADCTL_BGR);
		app_lcd_width = APP_LCD_HEIGHT;
		app_lcd_height = APP_LCD_WIDTH;
		break;
	case 3:
		APP_LCD_Data(APP_LCD_MADCTL_MY | APP_LCD_MADCTL_MV | APP_LCD_MADCTL_BGR);
		app_lcd_width = APP_LCD_WIDTH;
		app_lcd_height = APP_LCD_HEIGHT;
		break;
	}
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Clear Screen
// IN: -
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Lite(u8 enable)
{
	if(enable){
		LITE_ON();
	}else {
		LITE_OFF();
	}
	return 0;  //no error
}

/////////////////////////////////////////////////////////////////////////////
// Clear Screen
// IN: -
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Clear(void)
{
	//APP_LCD_FillRect(0, 0, APP_LCD_WIDTH, APP_LCD_HEIGHT, APP_LCD_BLACK);
	APP_LCD_Rectangle(0,0, app_lcd_width, app_lcd_height, 0, 0, 1, 0);
	return 0;  //no error
}

/////////////////////////////////////////////////////////////////////////////
// Send Multiple datas at a time
// IN: buffer and size
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Data_Multi(u8 *buff, size_t buff_size){
	DC_DATA();
	CS_ENA();
	while (buff_size > 0){
		u16 chunk_size = buff_size > 256 ? 256 : buff_size;
		//HAL_SPI_Transmit(&hspi2, buff, chunk_size, HAL_MAX_DELAY);
		//ADIOS_SPI_TransferByte(TFT_SPI, *buff);
		ADIOS_SPI_TransferBlock(APP_LCD_SPI, buff, NULL, chunk_size, NULL);
		buff += chunk_size;
		buff_size -= chunk_size;
	}
	CS_DIS();
	return 0; 	// no error
}

/////////////////////////////////////////////////////////////////////////////
// Send Multiple datas at a time
// IN: buffer and size
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_SendFastPixels(u32 n, u16 color)
{
	u8 r = (color >>8) & 0xF8;
	u8 g = (color >>3) & 0xFC;
	u8 b = color <<3;

	s32 cnt;
	u16 buf_size;

	if ((n*3) <= 255){
		cnt = n*3;
		buf_size = cnt;
	}
	else {
		cnt= n*3;
		buf_size = 255;
	}
	u8 frm_buf[buf_size];
	for (int i=0; i < buf_size/3; i++)
	{
		frm_buf[i*3] = r;
		frm_buf[i*3+1] = g;
		frm_buf[i*3+2] = b;
	}
	DC_DATA();
	CS_ENA();
	while(cnt>0)
	{
		ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, buf_size, NULL);
		cnt -= 255;
		if(cnt<255)buf_size=cnt;
	}
	CS_DIS();

	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastVLine(s16 x, s16 y, s16 h, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;

	APP_LCD_SetAddrWindow(x, y, x, y + h - 1);
	u32 n = h;
	APP_LCD_SendFastPixels(n, color);
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastHLine(s16 x, s16 y, s16 w, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y);
	u32 n = w;
	APP_LCD_SendFastPixels(n, color);
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastBeat(u16 x, u16 y, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	u16 w =9;
	u16 h =9;
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y + h -1);

	u8 r = (color >>8) & 0xF8;
	u8 g = (color >>3) & 0xFC;
	u8 b = color <<3;

	u8 frm_buf[w*h*3];
	for (int i=0; i < h; i++){
		for (int j=0; j < w; j++){
			if(beat[i]&(1<<j)){
				frm_buf[i*(w*3)+j*3] = r;
				frm_buf[i*(w*3)+j*3+1] = g;
				frm_buf[i*(w*3)+j*3+2] = b;
			}else{
				frm_buf[i*(w*3)+j*3] = 0;
				frm_buf[i*(w*3)+j*3+1] = 0;
				frm_buf[i*(w*3)+j*3+2] = 0;
			}
		}
	}
	DC_DATA();
	CS_ENA();
	ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, w*h*3, NULL);
	CS_DIS();
	return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastBall(u16 x, u16 y, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	u16 w =12;
	u16 h =12;
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y + h -1);

	u8 r = (color >>8) & 0xF8;
	u8 g = (color >>3) & 0xFC;
	u8 b = color <<3;

	u8 frm_buf[432];
	for (int i=0; i < 12; i++){
		for (int j=0; j < 12; j++){
			if(ball[i]&(1<<j)){
				frm_buf[i*36+j*3] = r;
				frm_buf[i*36+j*3+1] = g;
				frm_buf[i*36+j*3+2] = b;
			}else{
				frm_buf[i*36+j*3] = 0;
				frm_buf[i*36+j*3+1] = 0;
				frm_buf[i*36+j*3+2] = 0;
			}
		}
	}
	DC_DATA();
	CS_ENA();
	ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, 432, NULL);
	CS_DIS();
	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastNoire(u16 x, u16 y, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	u16 w =5;
	u16 h =5;
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y + h -1);

	u8 r = (color >>8) & 0xF8;
	u8 g = (color >>3) & 0xFC;
	u8 b = color <<3;

	u8 frm_buf[75];
	for (int i=0; i < 5; i++){
		for (int j=0; j < 5; j++){
			if(noire[i]&(1<<j)){
				frm_buf[i*15+j*3] = r;
				frm_buf[i*15+j*3+1] = g;
				frm_buf[i*15+j*3+2] = b;
			}else{
				frm_buf[i*15+j*3] = 0;
				frm_buf[i*15+j*3+1] = 0;
				frm_buf[i*15+j*3+2] = 0;
			}
		}
	}
	DC_DATA();
	CS_ENA();
	ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, 75, NULL);
	CS_DIS();
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastCroche(u16 x, u16 y, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	u16 w =4;
	u16 h =8;
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y + h -1);

	u8 r = (color >>8) & 0xF8;
	u8 g = (color >>3) & 0xFC;
	u8 b = color <<3;

	u8 frm_buf[96];
	for (int i=0; i < 8; i++){
		for (int j=0; j < 4; j++){
			if(croche[j]&(1<<i)){
				frm_buf[i*12+j*3] = r;
				frm_buf[i*12+j*3+1] = g;
				frm_buf[i*12+j*3+2] = b;
			}else{
				frm_buf[i*12+j*3] = 0;
				frm_buf[i*12+j*3+1] = 0;
				frm_buf[i*12+j*3+2] = 0;
			}
		}
	}
	DC_DATA();
	CS_ENA();
	ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, 96, NULL);
	CS_DIS();
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawFastCorner(u16 x, u16 y, u16 color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
	u16 w =8;
	u16 h =8;
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y + h -1);

	u8 r = (color >>8) & 0xF8;
	u8 g = (color >>3) & 0xFC;
	u8 b = color <<3;

	u8 frm_buf[192];
	for (int i=0; i < 8; i++){
		for (int j=0; j < 8; j++){
			if(corner[i]&(1<<j)){
				frm_buf[i*24+j*3] = r;
				frm_buf[i*24+j*3+1] = g;
				frm_buf[i*24+j*3+2] = b;
			}else{
				frm_buf[i*24+j*3] = 0;
				frm_buf[i*24+j*3+1] = 0;
				frm_buf[i*24+j*3+2] = 0;
			}
		}
	}
	DC_DATA();
	CS_ENA();
	ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, 192, NULL);
	CS_DIS();
	return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_SetAddrWindow(u16 x0, u16 y0, u16 x1, u16 y1)
{
	APP_LCD_Cmd(APP_LCD_CASET); // Column addr set
	{
		u8 data[] = {(x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF};
		APP_LCD_Data_Multi(data, sizeof(data));
	}
	APP_LCD_Cmd(APP_LCD_PASET);
	{
		u8 data[] = {(y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF};
		APP_LCD_Data_Multi(data, sizeof(data));
	}
	APP_LCD_Cmd(APP_LCD_RAMWR); // write to RAM*/
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_DrawPixel(s16 x, s16 y, u16 color)
{
	if ((x < 0) || (x >= app_lcd_width) || (y < 0) || (y >= app_lcd_height))
		return -1;

	APP_LCD_SetAddrWindow(x, y, x + 1, y + 1);
	APP_LCD_SendFastPixels(1, color);
	return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Digits_drawNorm(int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC) {

#if 0
	char nD=0;
	unsigned int num=abs(n),i,s,t,w,col,h,a,b,si=0,j=1,d=0,S1=cS,S2=5*cS,S3=2*cS,S4=7*cS,x1=(S3/2)+1,x2=(2*S1)+S2+1,y1=yLoc+x1,y3=yLoc+(2*S1)+S4+1;
	unsigned int seg[7][3]={{(S3/2)+1,yLoc,1},{x2,y1,0},{x2,y3+x1,0},{x1,(2*y3)-yLoc,1},{0,y3+x1,0},{0,y1,0},{x1,y3,1}};
	unsigned char nums[12]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x67,0x00,0x40},c=(c=abs(cS))>10?10:(c<1)?1:c,cnt=(cnt=abs(nD))>10?10:(cnt<1)?1:cnt;
	for (xLoc+=cnt*(d=(2*S1)+S2+(2*S3)+2);cnt>0;cnt--){
		for (i=(num>9)?num%10:((!cnt)&&(n<0))?11:((nD<0)&&(!num))?10:num,xLoc-=d,num/=10,j=0;j<7;++j){
			col=(nums[i]&(1<<j))?fC:bC;
			s=(2*S1)/S3;
			if (seg[j][2])for(w=S2,t=seg[j][1]+S3,h=seg[j][1]+(S3/2),a=xLoc+seg[j][0]+S1,b=seg[j][1];b<h;b++,a-=s,w+=(2*s))APP_LCD_DrawFastHLine(a,b,w,col);
			else for(w=S4,t=xLoc+seg[j][0]+S3,h=xLoc+seg[j][0]+S3/2,b=xLoc+seg[j][0],a=seg[j][1]+S1;b<h;b++,a-=s,w+=(2*s))APP_LCD_DrawFastVLine(b,a,w,col);
			for (;b<t;b++,a+=s,w-=(2*s))seg[j][2]?APP_LCD_DrawFastHLine(a,b,w,col):APP_LCD_DrawFastVLine(b,a,w,col);
		}
	}

#endif
	return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Digits_draw(int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC) {

#if 0

	unsigned int s, t, w, col , h, a, b, j=1, S1=1*cS, S2=18*cS, S3=5*cS, S4=18*cS, x1=(S3/2)+3, x2=(2*S1)+S2+5, y1=yLoc+x1+0, y3=yLoc+(2*S1)+S4+5;
	unsigned int seg[7][3]={ {x2,y1,0}, {x2,y3+x1,0}, {x1,(2*y3)-yLoc,1}, {(S3/2)+3,yLoc,1}, {0,y1,0}, {x1,y3,1}, {0,y3+x1,0} };
	for (j=0; j<7; ++j){
		col=(n&(1<<j))?fC:bC;
		s=(6*S1)/S3;
		if (seg[j][2])for(w=S2, t=seg[j][1]+S3, h=seg[j][1]+(S3/2), a=xLoc+seg[j][0]+S1, b=seg[j][1]; b<h; b++, a-=s,w+=(2*s)) APP_LCD_DrawFastHLine(a,b,w,col);
		else for(w=S4, t=xLoc+seg[j][0]+S3, h=xLoc+seg[j][0]+S3/2, b=xLoc+seg[j][0], a=seg[j][1]+S1; b<h; b++,a-=s, w+=(2*s)) APP_LCD_DrawFastVLine(b,a,w,col);
		for (; b<t; b++, a+=s, w-=(2*s)) seg[j][2]?APP_LCD_DrawFastHLine(a,b,w,col):APP_LCD_DrawFastVLine(b,a,w,col);
	}
#else
	unsigned int s, t, w, col , h, a, b, j=1, S1=1*cS, S2=13*cS, S3=4*cS, S4=13*cS, x1=(S3/2)+3, x2=(2*S1)+S2+5, y1=yLoc+x1+0, y3=yLoc+(2*S1)+S4+5;
	unsigned int seg[7][3]={ {x2,y1,0}, {x2,y3+x1,0}, {x1,(2*y3)-yLoc,1}, {(S3/2)+3,yLoc,1}, {0,y1,0}, {x1,y3,1}, {0,y3+x1,0} };
	for (j=0; j<7; ++j){
		col=(n&(1<<j))?fC:bC;
		s=(6*S1)/S3;
		if (seg[j][2])for(w=S2, t=seg[j][1]+S3, h=seg[j][1]+(S3/2), a=xLoc+seg[j][0]+S1, b=seg[j][1]; b<h; b++, a-=s,w+=(2*s)) APP_LCD_DrawFastHLine(a,b,w,col);
		else for(w=S4, t=xLoc+seg[j][0]+S3, h=xLoc+seg[j][0]+S3/2, b=xLoc+seg[j][0], a=seg[j][1]+S1; b<h; b++,a-=s, w+=(2*s)) APP_LCD_DrawFastVLine(b,a,w,col);
		for (; b<t; b++, a+=s, w-=(2*s)) seg[j][2]?APP_LCD_DrawFastHLine(a,b,w,col):APP_LCD_DrawFastVLine(b,a,w,col);
	}
#endif
	return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
// IN: <column> and <line>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_CursorSet(u16 column, u16 line)
{
  // app_lcd_x/y set by ADIOS_LCD_CursorSet() function
  return APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
}


/////////////////////////////////////////////////////////////////////////////
// Sets graphical cursor to given position
// IN: <x> and <y>
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_GCursorSet(u16 x, u16 y)
{
  s32 error = 0;
#if 0
  
  app_lcd_x = x;
  app_lcd_y = y;
  
  error |= APP_LCD_Cmd(0x2A);
  error |= APP_LCD_Data(0x00);
  error |= APP_LCD_Data(x);
  error |= APP_LCD_Data(0x00);
  error |= APP_LCD_Data(127);
  error |= APP_LCD_Cmd(0x2B);
  error |= APP_LCD_Data(0x00);
  error |= APP_LCD_Data(y);
  error |= APP_LCD_Data(0x00);
  error |= APP_LCD_Data(159);
  error |= APP_LCD_Cmd(0x2C);   //LCD_WriteCMD(GRAMWR);
#endif
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
  font_bmp.memory = (u8 *)&font[GLCD_FONT_BITMAP_IX] + (size_t)font[GLCD_FONT_X0_IX];
  font_bmp.width = font[GLCD_FONT_WIDTH_IX];
  font_bmp.height = font[GLCD_FONT_HEIGHT_IX];
  font_bmp.line_offset = font[GLCD_FONT_OFFSET_IX];
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
s32 APP_LCD_CharKernGet(s16 ascii_offset, char c)
{

  s32 len=0;
#if 1
  if(font_bmp.line_offset == 0){
    // kerning(char offset)
    if(font_bmp.colour_depth == Is1BIT) {
      u8 height = (font_bmp.height/8) + ((font_bmp.height%8) ? 1 : 0);
      u8 *byte = font_bmp.memory + ((u8)((c+ascii_offset)/16)*height*font_bmp.width*16) + (font_bmp.width*((c+ascii_offset)%16));
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
#endif
  return len; // not supported
}

/////////////////////////////////////////////////////////////////////////////
// return a character kerning length
// IN: the character
// OUT: kerning length
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_StringKernGet(s16 ascii_offset, const char *str)
{
  s32 len=0;
#if 1
  while( *str != '\0' )len +=(s32)APP_LCD_CharKernGet(ascii_offset, *str++);
#endif
  return len; // not supported
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a single character in bitmap(1 or 4Bit depends on bitmap)
//! \param[in] destination bitmap, x/y position, fusion mod, character to be print
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_PrintChar(s16 x, s16 y, s16 w_stop, s16 ascii_offset, char c)
{
  if( !font_bmp.width )
    return -2;  // font not initialized yet!

  adios_lcd_bitmap_t char_bmp = font_bmp;
    u8 lines = (char_bmp.height/8) + ((char_bmp.height%8) ? 1 : 0);
    char_bmp.line_offset = char_bmp.width*16;   // font table in ASCII format(16 char by line)
    char_bmp.memory += ((u8)((c+ascii_offset)/16)*char_bmp.line_offset*lines) + (font_bmp.width*((c+ascii_offset)%16));

    char_bmp.width=(u16)APP_LCD_CharKernGet(ascii_offset, c);
    //if((w_stop!=0)&&((x+char_bmp.width)>=w_stop))char_bmp.width= w_stop-x;
    APP_LCD_SendBitmap(char_bmp, x, y);

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a \\0 (zero) terminated string
//! \param[in] destination bitmap, x/y position, fusion mod,
//! str pointer to string.
//! \param[in]
//! \return < 0 on errors, or string length in pixels
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_PrintString(s16 x, s16 y, s16 w_stop, u8 alignment, s16 ascii_offset, const char *str)
{
  s32 status = 0;
  u16 offset = 0;
#if 1
  // calc start point depending on alignment
  const char *s = str;
  u16 len=0;
  // kerning(char offset)
  while( *s != '\0' ){
    len += (u16)APP_LCD_CharKernGet(ascii_offset, *s);
    s++;
  }
  if(alignment==APP_LCD_STRING_ALIGN_CENTER)x -= len/2;
  if(alignment==APP_LCD_STRING_ALIGN_RIGHT)x -= len;
  if(alignment==APP_LCD_STRING_ALIGN_CENTER)w_stop=0;
  // start spelling
  offset = 0;
  u8 while_stop = 1;
  while( (*str != '\0') && while_stop ){
	  if((w_stop!=0)&&((offset +(u16)APP_LCD_CharKernGet(ascii_offset, *str))>=w_stop)){
		  while_stop=0;
	  }else{
		  status |= APP_LCD_PrintChar(x+offset, y, x+w_stop, ascii_offset, *str);
	  	  offset +=(u16)APP_LCD_CharKernGet(ascii_offset, *str);
	  }
	  str++;
  }
  if((w_stop!=0)&&(w_stop>offset)){
	  adios_lcd_bitmap_t char_bmp = font_bmp;
	  if(alignment==APP_LCD_STRING_ALIGN_LEFT){
		  APP_LCD_Rectangle(x+offset, y, w_stop-offset, char_bmp.height, 0, 0, 2, app_lcd_back_color);
	  }else   if(alignment==APP_LCD_STRING_ALIGN_RIGHT){
		  APP_LCD_Rectangle(x- (w_stop-offset), y, w_stop-offset, char_bmp.height, 0, 0, 2, app_lcd_back_color);
	  }
  }
#endif
  return (status<0)?status:(s32)offset;
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a \\0 (zero) terminated formatted string (like printf)
//! \param[in] destination bitmap, x/y position, fusion mod,
//! *format zero-terminated format string - 64 characters supported maximum!
//! \param ... additional arguments
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_PrintFormattedString(s16 x, s16 y, s16 w_stop, u8 alignment, s16 ascii_offset, const char *format, ...)
{
#if 1
  char buffer[64]; // TODO: tmp!!! Provide a streamed COM method later!
  va_list args;
  
  va_start(args, format);
  vsprintf((char *)buffer, format, args);
  return APP_LCD_PrintString(x, y, w_stop, alignment, ascii_offset, buffer);
#else
  return 0;
#endif
}


s32 APP_LCD_PrintProgress(adios_lcd_bitmap_t bitmap, u32 progress_color, s16 x, s16 y, s16 w_stop, s16 height, s16 progress)
{

  s32 status = 0;
#if 0
  u16 offset = 0;
  //s16 width = ((w_stop/8)+((w_stop%8)?1:0))*8;
  s16 width = w_stop;
  int lines = (height/8)+((height%8)?1:0);
  // global array (!)
  int size = width*lines;
  u8 bitmap_array[size];


  // Initialisation:
  adios_lcd_bitmap_t bmp = APP_LCD_BitmapInit((u8*)bitmap_array, width, height, width, Is1BIT);
  u8* bmp_mem_ptr = bmp.memory;
  //clear mem
  for(int l =0; l<lines; l++)for (int i=0; i<width;i++)*bmp_mem_ptr++ =0x00;
  // prints messages in bitmap
  APP_LCD_BitmapPrintString(bmp, 1.0, width/2, 5, 0, APP_LCD_STRING_ALIGN_CENTER, ascii_offset, str1);
  APP_LCD_BitmapPrintString(bmp, 1.0, width/2, 25, 0, APP_LCD_STRING_ALIGN_CENTER, ascii_offset, str2);

#endif
  // prints each vertical line to display
  APP_LCD_BColourSetRGB(progress_color);
  for (int w=0; w<w_stop;w++){
	  if(w>progress)APP_LCD_BColourSet(APP_LCD_BLACK);
	  u8* bmp_mem_ptr = bitmap.memory +w;
	  adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, height, w_stop, Is1BIT);
	  APP_LCD_SendBitmap(bmp2print, x+w, y);
  }
  APP_LCD_BColourSet(APP_LCD_BLACK);

  return status; // all good
}

/////////////////////////////////////////////////////////////////////////////
// color format converter
// Only relevant for colour GLCDs
// IN: r/g/b value
// OUT: 16bits(565) format
/////////////////////////////////////////////////////////////////////////////
u16 APP_LCD_ColourConvert(u32 rgb)
{
  rgb &= 0x00ffffff;
  u8 r = (rgb >> 19) & 0x1f;
  u8 g = (rgb >> 10) & 0x3f;
  u8 b = (rgb >> 3) & 0x1f;
  return ((r<<11) | (g<<5) | b);
}
/////////////////////////////////////////////////////////////////////////////
// Sets the background colour
// Only relevant for colour GLCDs
// IN: 16bit(565) format value
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BColourSet(u16 color)
{
  app_lcd_back_color = (u32)color;
  return -1; // n.a.
}


/////////////////////////////////////////////////////////////////////////////
// Sets the foreground colour
// Only relevant for colour legacy 1Bit bitmap and font
// IN: 16bit(565) format value
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_FColourSet(u16 color)
{
  app_lcd_fore_color = (u32)color;
  return 0; // no error
}
/////////////////////////////////////////////////////////////////////////////
// Sets the background colour
// Only relevant for colour GLCDs
// IN: r/g/b value
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BColourSetRGB(u32 rgb)
{
  app_lcd_back_color = (rgb&0xff000000) | APP_LCD_ColourConvert(rgb);
  return -1; // n.a.
}


/////////////////////////////////////////////////////////////////////////////
// Sets the foreground colour
// Only relevant for colour legacy 1Bit bitmap and font
// IN: r/g/b value
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_FColourSetRGB(u32 rgb)
{
  app_lcd_fore_color =  (rgb&0xff000000) | APP_LCD_ColourConvert(rgb);
  return 0; // no error
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
//!   adios_lcd_bitmap_t bitmap = BM_LCD_BitmapClear(bitmap_array,
//!   						    APP_LCD_NUM_X*APP_OLED_WIDTH,
//!   						    APP_LCD_NUM_Y*APP_OLED_HEIGHT,
//!   						    APP_LCD_NUM_X*APP_OLED_WIDTH.
//!                   APP_LCD_COLOUR_DEPTH);
//! \endcode
//!
//! \param[in] memory pointer to the bitmap array
//! \param[in] app_lcd_width app_lcd_width of the bitmap (usually APP_LCD_NUM_X*APP_OLED_WIDTH)
//! \param[in] app_lcd_height app_lcd_height of the bitmap (usually APP_LCD_NUM_Y*APP_LCD_HEIGHT)
//! \param[in] line_offset byte offset between each line (usually same value as app_lcd_width)
//! \param[in] colour_depth how many bits are allocated by each pixel (usually APP_LCD_COLOUR_DEPTH)
//! \return a configured bitmap as adios_lcd_bitmap_t
/////////////////////////////////////////////////////////////////////////////
adios_lcd_bitmap_t APP_LCD_BitmapInit(u8 *memory, u16 app_lcd_width, u16 app_lcd_height, u16 line_offset, app_lcd_color_depth_t colour_depth)
{
  adios_lcd_bitmap_t bitmap;
  
  bitmap.memory = memory;
  bitmap.width = app_lcd_width;
  bitmap.height = app_lcd_height;
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
s32 APP_LCD_PixelSet(u16 x, u16 y, u32 colour)
{
#if 0
  if( x >= APP_LCD_WIDTH || y >= APP_LCD_HEIGHT )
    return -1; // pixel is outside bitmap
  
  colour &= 0x00ffffff;
  u8 r = (colour >> 19) & 0x1f;
  u8 g = (colour >> 10) & 0x3f;
  u8 b = (colour >> 3) & 0x1f;
  u16 color = (r<<11) | (g<<5) | b;
  APP_LCD_GCursorSet(x, y);
  APP_LCD_Data(color >> 8);
  APP_LCD_Data(color & 0xff);
#endif
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Sets a pixel in the bitmap
// IN: bitmap, x/y position and fusion mode for native 4Bit bitmap
// color is given by APP_LCD_FColourSet,
// app_lcd_fore_color>0 is a white pixel for legacy 1Bit bimap
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPixelSet(adios_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour)
{
  if( x >= bitmap.width || y >= bitmap.height )
    return -1; // pixel is outside bitmap
#if 1
  
  /* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
  if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
    // prepare colour for 5:6:5
    colour &= 0x00ffffff;
    u8 r = (colour >> 19) & 0x1f;
    u8 g = (colour >> 10) & 0x3f;
    u8 b = (colour >> 3) & 0x1f;
    u16 color = (r<<11) | (g<<5) | b;
    u8 *pixel = bitmap.memory + ((bitmap.line_offset*y + x)*2);
    *pixel++ = color>>8;
    *pixel = (color & 0xff);
    
    /* legacy 1bit pixel print */
  }else if(bitmap.colour_depth == 1) {  // 1bit format
    u8 *pixel = (u8 *)&bitmap.memory[bitmap.line_offset*(y / 8) + x];
    u8 mask = 1 << (y % 8);
    *pixel &= ~mask;
    if( colour ) *pixel |= mask;
    
  }else return -1;  // not supported
#endif
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Draw a rectangle in the bm_cs_lcd_screen_bmp from position and size
// IN: x1/y1 first point, x2/y2 second point, border(e.g. 0x55 is dot line) and fill 0=none 1=empty 2=fill
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Rectangle(u16 x, u16 y, u16 w, u16 h, u8 border, u16 bd_color, u8 fill, u16 fill_color)
{
	if( x >= app_lcd_width || y >= app_lcd_height )
		return -1; // pixel is outside bitmap
#if 0
	if ((x >= app_lcd_width) || (y >= app_lcd_height))
		return;
	if ((x + w - 1) >= app_lcd_width)
		w = app_lcd_width - x;
	if ((y + h - 1) >= app_lcd_height)
		h = app_lcd_height - y;
	APP_LCD_SetAddrWindow(x, y, x + w - 1, y + h - 1);
	u32 n = w*h;
	APP_LCD_SendFastPixels(n, fill_color);
#endif
#if 1
	// fill rect first

	if(fill==1)fill_color=APP_LCD_BLACK;
	if(fill>0){
		u16 xf=x+border;
		u16 yf=y+border;
		u16 wf=w-2*border;
		u16 hf=h-2*border;
		if ((xf + wf - 1) >= app_lcd_width)
			wf = app_lcd_width - xf;
		if ((yf + hf - 1) >= app_lcd_height)
			hf = app_lcd_height - yf;
		APP_LCD_SetAddrWindow(xf, yf, xf + wf - 1, yf + hf - 1);
		u32 n = wf*hf;
		APP_LCD_SendFastPixels(n, fill_color);
	}

	if(border){
			u16 by = border;
			if ((y + border - 1) >= app_lcd_height)
				by = app_lcd_height - y;
			APP_LCD_SetAddrWindow(x, y, x + w - 1, y+by-1);
			u32 n = by*w;
			APP_LCD_SendFastPixels(n, bd_color);

			by = border;
			if ((y + h - 1) >= app_lcd_height)
				by = app_lcd_height -y - h;
			APP_LCD_SetAddrWindow(x, y+h-by, x + w - 1, y+h+by-1);
			n = by*w;
			APP_LCD_SendFastPixels(n, bd_color);

			u16 bx = border;
			if ((x + border - 1) >= app_lcd_width)
				bx = app_lcd_width - x;
			APP_LCD_SetAddrWindow(x, y, x+bx- 1, y+h-1);
			n = bx*h;
			APP_LCD_SendFastPixels(n, bd_color);

			bx = border;
			if ((x + w - 1) >= app_lcd_width)
				bx = app_lcd_width - x - w ;
			APP_LCD_SetAddrWindow(x+w-bx, y, x+w-1 , y+h-1);
			n = bx*h;
			APP_LCD_SendFastPixels(n, bd_color);
	}

#endif
#if 0

		for(i=0; i< (app_lcd_width); i++)for(j=0; j< (app_lcd_height); j++)APP_LCD_PixelSet((u16)(x+i), (u16)(y+j), back_color);

	// border
	u16 border_pix=0;
	for(i=0; i< (app_lcd_width); i++){
		if((border >> (border_pix%8))&0x01)
			APP_LCD_PixelSet((u16)(x+i), (u16)y, bd_color);
		else
			APP_LCD_PixelSet((u16)(x+i), (u16)y, back_color);
		border_pix++;
	}
	for(i=1; i< (app_lcd_height); i++){
		if((border >> (border_pix%8))&0x01)
			APP_LCD_PixelSet((u16)(x+app_lcd_width-1), (u16)(y+i), bd_color);
		else
			APP_LCD_PixelSet((u16)(x+app_lcd_width-1), (u16)(y+i), back_color);
		border_pix++;
	}
	for(i=1; i< (app_lcd_width); i++){
		if((border >> (border_pix%8))&0x01)
			APP_LCD_PixelSet((u16)(x+app_lcd_width-i-1), (u16)(y + app_lcd_height-1), bd_color);
		else
			APP_LCD_PixelSet((u16)(x+app_lcd_width-i-1), (u16)(y + app_lcd_height-1), back_color);
		border_pix++;
	}
	for(i=1; i< (app_lcd_height); i++){
		if((border >> (border_pix%8))&0x01)
			APP_LCD_PixelSet((u16)x, (u16)(y+app_lcd_height-i-1), bd_color);
		else
			APP_LCD_PixelSet((u16)x, (u16)(y+app_lcd_height-i-1), back_color);
		border_pix++;
	}
#endif
return 1; // ok
}


/////////////////////////////////////////////////////////////////////////////
//! Prints a single character in bitmap(1 or 4Bit depends on bitmap)
//! \param[in] destination bitmap, x/y position, fusion mod, character to be print
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPrintChar(adios_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, s16 ascii_offset, char c)
{
  if( !font_bmp.width )
    return -2;  // font not initialized yet!

#if 0
    adios_lcd_bitmap_t char_bmp = font_bmp;
    u8 lines = (char_bmp.height/8) + ((char_bmp.height%8) ? 1 : 0);
    char_bmp.line_offset = char_bmp.width*16;   // font table in ASCII format(16 char by line)
    char_bmp.memory += ((u8)((c+ascii_offset)/16)*char_bmp.line_offset*lines) + (font_bmp.width*((c+ascii_offset)%16));

    char_bmp.width=(u16)APP_LCD_CharKernGet(ascii_offset, c);
    //if((w_stop!=0)&&((x+char_bmp.width)>=w_stop))char_bmp.width= w_stop-x;
    APP_LCD_SendBitmap(char_bmp, x, y);
#else


  // legacy 1bit to 1bit
  //if((bitmap.colour_depth == font_bmp.colour_depth) && (font_bmp.colour_depth == Is1BIT)) {
    adios_lcd_bitmap_t char_bmp = font_bmp;
    u8 height = (char_bmp.height/8) + ((char_bmp.height%8) ? 1 : 0);
    char_bmp.line_offset = char_bmp.width*16;   // font table in ASCII format(16 char by line)
    char_bmp.memory += ((u8)((c+ascii_offset)/16)*char_bmp.line_offset*height) + (font_bmp.width*((c+ascii_offset)%16));
    char_bmp.width=(u16)APP_LCD_CharKernGet(ascii_offset, c);
    APP_LCD_BitmapFusion(char_bmp, luma, bitmap, x, y, fusion);

    // toDo ili special depth 5:6:5
  //}
#endif

#if 0
  else if((bitmap.colour_depth == font_bmp.colour_depth) && (font_bmp.colour_depth == Is16BIT)) {


    adios_lcd_bitmap_t char_bmp = font_bmp;
    char_bmp.line_offset = char_bmp.width*16;   // font table in ASCII format(16 char by line)
    char_bmp.memory += (char_bmp.width*char_bmp.height*((size_t)c & 0xf0)*2 + ((((size_t)c %16)*char_bmp.width)*2));
    APP_LCD_BitmapFusion(char_bmp, luma, bitmap, x, y, fusion);

    // legacy 1bit to '16bit' depth
  }else if((bitmap.colour_depth == Is16BIT) && (font_bmp.colour_depth == Is1BIT)) {

    adios_lcd_bitmap_t char_bmp = font_bmp;
    u8 lines = (char_bmp.height/8) + ((char_bmp.height%8) ? 1 : 0);
    char_bmp.line_offset = char_bmp.width*16;   // font table in ASCII format(16 char by line)
    char_bmp.memory += ((u8)(c/16)*char_bmp.line_offset*lines) + (font_bmp.width*(c%16));

    char_bmp.width=(u16)APP_LCD_CharKernGet(c);

    //APP_LCD_SendBitmap(char_bmp, x, y);

	u16 color = APP_LCD_RED;
	// prepare colors
	u8 br = (color >>8) & 0xF8;
	u8 bg = (color >>3) & 0xFC;
	u8 bb = color <<3;
	color = APP_LCD_WHITE;
	u8 fr = (color >>8) & 0xF8;
	u8 fg = (color >>3) & 0xFC;
	u8 fb = (color <<3);
	// prepare buffer
	u8 height=8;
	u8 remain_line = (bitmap.height %8);
	u8 y_lines = (bitmap.height >> 3)+(remain_line? 1:0) ;
	u16 buf_size = bitmap.width*height*3;
	u8 frm_buf[buf_size];

	//u16 initial_y = app_lcd_y;
	for(int line=0; line<y_lines; line++) {
	//int line=0;
		// address command
		if((line+1==y_lines) && remain_line){
			height=remain_line;
			buf_size = bitmap.width*height*3;
		}

		u8 *memory_ptr = bitmap.memory + (line * bitmap.width);

	    APP_LCD_Digits_drawNorm((memory_ptr-font_bmp.memory)/100, 20, 40+(line*40), 1, APP_LCD_WHITE, APP_LCD_BLACK);	// temp
	    APP_LCD_Digits_drawNorm((memory_ptr-font_bmp.memory)/10, 35, 40+(line*40), 1, APP_LCD_WHITE, APP_LCD_BLACK);	// temp
	    APP_LCD_Digits_drawNorm((memory_ptr-font_bmp.memory)%10, 50, 40+(line*40), 1, APP_LCD_WHITE, APP_LCD_BLACK);	// temp
		APP_LCD_SetAddrWindow(x_pos, y_pos+(line*8), x_pos + bitmap.width - 1, y_pos + (line* 8) + height -1);
		// calculate pointer to bitmap line
	    // transfer bitmap
		for(int y=0; y<height; y++){
			for(int x=0; x<bitmap.width; x++){
				u16 offset = 3*(bitmap.width*y+x);
				if(*memory_ptr & (1<<y)){
					frm_buf[offset] = fr;
					frm_buf[offset+1] = fg;
					frm_buf[offset+2] = fb;
				}else{
					frm_buf[offset] = br;
					frm_buf[offset+1] = bg;
					frm_buf[offset+2] = bb;
				}
				//DEBUG_MSG("%d %d %d", x, y, memory_ptr);
				memory_ptr++;
			}
			memory_ptr = bitmap.memory + (line * bitmap.width);

		}
		//if(line==1)ADIOS_BOARD_LED_Set(1, 1);
		DC_DATA();
		CS_ENA();
		ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, buf_size, NULL);
		CS_DIS();
#endif
    //APP_LCD_BitmapFusion(char_bmp, luma, bitmap, x, y, fusion);
#if 0
    // '16bit' to legacy 1bit depth
  }else if((bitmap.colour_depth == Is1BIT) && (font_bmp.colour_depth == Is16BIT)) {
    // write it if you need it ;)
    return -1;    // not supported
  }else return -1;   // not supported
#endif
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a \\0 (zero) terminated string
//! \param[in] destination bitmap, x/y position, fusion mod,
//! str pointer to string.
//! \param[in]
//! \return < 0 on errors, or string length in pixels
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPrintString(adios_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, u8 alignment, s16 ascii_offset, const char *str)
{
  s32 status = 0;
  u16 offset = 0;
#if 1
  // calc start point depending on alignment
  const char *s = str;
  u16 len=0;
  // kerning(char offset)
  while( *s != '\0' ){
    len += (u16)APP_LCD_CharKernGet(ascii_offset, *s);
    s++;
  }
  if(alignment==APP_LCD_STRING_ALIGN_CENTER)x -= len/2;
  if(alignment==APP_LCD_STRING_ALIGN_RIGHT)x -= len;
  // start spelling
  offset = 0;
  while( *str != '\0' ){
	  status |= APP_LCD_BitmapPrintChar(bitmap, luma, x+offset, y, fusion, ascii_offset, *str);
	  offset +=(u16)APP_LCD_CharKernGet(ascii_offset, *str);
	  str++;
  }
#endif
  return (status<0)?status:(s32)offset;
}

/////////////////////////////////////////////////////////////////////////////
//! Prints a \\0 (zero) terminated formatted string (like printf)
//! \param[in] destination bitmap, x/y position, fusion mod,
//! *format zero-terminated format string - 64 characters supported maximum!
//! \param ... additional arguments
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPrintFormattedString(adios_lcd_bitmap_t bitmap, float luma, s16 x, s16 y, app_lcd_fusion_t fusion, u8 alignment, s16 ascii_offset, const char *format, ...)
{
#if 1
  char buffer[64]; // TODO: tmp!!! Provide a streamed COM method later!
  va_list args;

  va_start(args, format);
  vsprintf((char *)buffer, format, args);
  return APP_LCD_BitmapPrintString(bitmap, luma, x, y, fusion, alignment, ascii_offset, buffer);
#else
  return 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////
// Draw a rectangle in the bm_cs_lcd_screen_bmp from position and size
// IN: x1/y1 first point, x2/y2 second point, border(e.g. 0x55 is dot line) and fill 0=none 1=empty 2=fill
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapRectangle(adios_lcd_bitmap_t bitmap, s16 x, s16 y, u16 app_lcd_width, u16 app_lcd_height, u8 border, u32 bd_color, u8 fill, u32 back_color)
{
  if( (x >= bitmap.width) || (y >= bitmap.height) || ((x + app_lcd_width) < 0) || ((y + app_lcd_height) < 0) )return -1; // pixel is outside bm_cs_lcd_screen_bmp
#if 1
  s16 i, j;

//  /* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
//  if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
//    // toDo
//
//    /* legacy 1bit pixel print */
//  }else if(bitmap.colour_depth == 1) {  // 1bit format
    // fill rect first
    if(fill)for(i=0; i< (app_lcd_width); i++)for(j=0; j< (app_lcd_height); j++)APP_LCD_BitmapPixelSet(bitmap, (u16)(x+i), (u16)(y+j), back_color);

    // border
    if(border){
    u16 border_pix=0;
    for(i=0; i< (app_lcd_width); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+i), (u16)y, bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+i), (u16)y, back_color);
      border_pix++;
    }
    for(i=1; i< (app_lcd_height); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+app_lcd_width-1), (u16)(y+i), bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+app_lcd_width-1), (u16)(y+i), back_color);
      border_pix++;
    }
    for(i=1; i< (app_lcd_width); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+app_lcd_width-i-1), (u16)(y + app_lcd_height-1), bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)(x+app_lcd_width-i-1), (u16)(y + app_lcd_height-1), back_color);
      border_pix++;
    }
    for(i=1; i< (app_lcd_height); i++){
      if((border >> (border_pix%8))&0x01)
        APP_LCD_BitmapPixelSet(bitmap, (u16)x, (u16)(y+app_lcd_height-i-1), bd_color);
      else
        APP_LCD_BitmapPixelSet(bitmap, (u16)x, (u16)(y+app_lcd_height-i-1), back_color);
      border_pix++;
    }
    }
  //}else return -1;  // not supported
#endif
  return 1; // ok
}


/////////////////////////////////////////////////////////////////////////////
// Sets a byte in the bitmap, whathever its position in y,
// byte doesn't need to match the oled segment
// used for legacy 1bit bitmap
// IN: bm_cs_lcd_screen_bmp, x/y position and colour value (value range depends on APP_LCD_COLOUR_DEPTH)
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapByteSet(adios_lcd_bitmap_t bitmap, s16 x, s16 y, u8 value)
{
  if( x >= bitmap.width || y >= bitmap.height || x < 0 || ((y + 8) < 0))
    return -1; // pixel is outside bm_cs_lcd_screen_bmp
#if 1
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
#endif
  return 1; // ok
}

/////////////////////////////////////////////////////////////////////////////
// local, used by APP_LCD_Bitmap4BitLuma and APP_LCD_BitmapFusion
/////////////////////////////////////////////////////////////////////////////
u16 APP_LCD_HelpPixelLuma(u16 pix_mem, float luma)
{
#if 0
  if(luma == 1.0)return pix_mem;
  u8 r = (u8)(((pix_mem >> 11) & 0x1f)*(luma));
  u8 g = (u8)(((pix_mem >> 5) & 0x3f)*(luma));
  u8 b = (u8)((pix_mem & 0x1f)*(luma));
  return ((r<<11) | (g<<5) | b);
#else
  return 0;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//! Change the Luminance of a native 4Bit bitmap within given boundaries
//! IN: bitmap, x/y position, app_lcd_width and heigth, luma
//! Notes:
//! luma is a float between -1.0 and +16.0(from black to all pixels saturated)
//! luma neutral is 0.0
//!
//! OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Bitmap16BitLuma(adios_lcd_bitmap_t bitmap, s16 x, s16 y, u16 app_lcd_width, u16 app_lcd_height, float luma)
{
  if( (x >= bitmap.width) || (y >= bitmap.height) || ((x+app_lcd_width) < 0) || ((y+app_lcd_height) < 0))
    return -2;  // bitmap is outside screen
#if 0
  /* native 4bit depth only */
  if(bitmap.colour_depth == Is16BIT) {
    u16 xi, yi;
    // loop y (with crop)
    for(yi=((y<0)? 0 : y); yi<(((app_lcd_height+y)>bitmap.height)? bitmap.height : (app_lcd_height+y)); yi++){
      // loop x (with crop)
      for(xi=((x<0)? 0 : x); xi<(((app_lcd_width+x)>bitmap.width)? bitmap.width : (app_lcd_width+x)); xi++){
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
#endif
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
#if 0
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
#else
  return 0;
#endif
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
s32 APP_LCD_BitmapFusion(adios_lcd_bitmap_t top_bmp, float top_luma, adios_lcd_bitmap_t bmp, s16 top_pos_x, s16 top_pos_y, app_lcd_fusion_t fusion)
{
  //if( (top_pos_x >= bmp.width) || (top_pos_y >= bmp.height) || ((top_pos_x+top_bmp.width) < 0) || ((top_pos_y+top_bmp.height) < 0))
    //return -2;  // bitmap is outside screen

    int i, j;
    u8 lines = top_bmp.height/8 + ((top_bmp.height%8) ? 1 : 0);
    u8 *byte_ptr = top_bmp.memory;
    for(i=0; i< top_bmp.width; i++){
      // forward to legacy 1bit process
      for(j=0; j< lines; j++){
        APP_LCD_BitmapByteSet(bmp, top_pos_x+i, top_pos_y+(j*8), *(byte_ptr+i+(j*top_bmp.line_offset)));
      }
    }

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Transfers Bitmap to the TFT
// Notes: using back/fore colors respectively from pixel off/on for 1bit,
// trasferred to APP_LCD_NativeBitmapPrint for native 16bit
// IN: bitmap
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_SendBitmap(adios_lcd_bitmap_t bitmap, u16 x_pos, u16 y_pos)
{

	if( x_pos >= app_lcd_width || y_pos >= app_lcd_height )
		return -1; // pixel is outside bitmap
	if ((x_pos + bitmap.width - 1) >= app_lcd_width)
		bitmap.width = app_lcd_width - x_pos;
	if ((y_pos + bitmap.height - 1) >= app_lcd_height)
		bitmap.height = app_lcd_height - y_pos;

#if 0
	//if( !ADIOS_LCD_TypeIsGLCD() )
	//return -1; // no GLCD

	// abort if max. app_lcd_width reached
	//if( app_lcd_x >= adios_lcd_parameters.app_lcd_width )
	//return -2;

	/* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
	if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
		//    u16 *memory_ptr = bitmap.memory + ((bitmap.line_offset*top_pos_y + top_pos_x)*2);
		//    // transfer bitmap
		//    int top_pos_x, y;
		//    for(y=0; y<8; ++y){
		//      for(x=0; x<bitmap.width; ++x){
		//        APP_LCD_Data(*memory_ptr >> 8);
		//        APP_LCD_Data(*memory_ptr++ & 0xff);
		//      }
		//    }
		/* legacy 1bit pixel print */
	}else if(bitmap.colour_depth == 1) {  // 1bit format
#endif

		//u16 color = APP_LCD_RED;
		// prepare colors
		u8 transparency = (app_lcd_back_color && 0xff000000)>> 24;
		u16 color = app_lcd_back_color & 0xffff;
		u8 br = (u8)((color >>8) & 0xF8);
		u8 bg = (u8)((color >>3) & 0xFC);
		u8 bb = (u8)(color <<3);
		color = app_lcd_fore_color & 0xffff;
		u8 fr = (u8)((color >>8) & 0xF8);
		u8 fg = (u8)((color >>3) & 0xFC);
		u8 fb = (u8)(color <<3);
		// prepare buffer
		u8 height=8;
		u8 remain_line = (bitmap.height %8);
		u8 y_lines = (bitmap.height >> 3)+(remain_line? 1:0) ;
		u16 buf_size = bitmap.width*height*3;
		u8 frm_buf[buf_size];

		//u16 initial_y = app_lcd_y;
		for(int line=0; line<y_lines; line++) {
		//int line=0;
			// address command
			if((line+1==y_lines) && remain_line){
				height=remain_line;
				buf_size = bitmap.width*height*3;
			}

			u8 *memory_ptr = bitmap.memory + (line * bitmap.line_offset);

		    //APP_LCD_Digits_drawNorm((memory_ptr-font_bmp.memory)/100, 20, 40+(line*40), 1, APP_LCD_WHITE, APP_LCD_BLACK);	// temp
		    //APP_LCD_Digits_drawNorm((memory_ptr-font_bmp.memory)/10, 35, 40+(line*40), 1, APP_LCD_WHITE, APP_LCD_BLACK);	// temp
		    //APP_LCD_Digits_drawNorm((memory_ptr-font_bmp.memory)%10, 50, 40+(line*40), 1, APP_LCD_WHITE, APP_LCD_BLACK);	// temp
			APP_LCD_SetAddrWindow(x_pos, y_pos+(line*8), x_pos + bitmap.width - 1, y_pos + (line* 8) + height -1);
			// calculate pointer to bitmap line
		    // transfer bitmap
			for(int y=0; y<height; y++){
				for(int x=0; x<bitmap.width; x++){
					u16 offset = 3*(bitmap.width*y+x);
					if(*memory_ptr & (1<<y)){
						frm_buf[offset] = fr;
						frm_buf[offset+1] = fg;
						frm_buf[offset+2] = fb;
					}else{
						frm_buf[offset] = br;
						frm_buf[offset+1] = bg;
						frm_buf[offset+2] = bb;
					}
					//DEBUG_MSG("%d %d %d", x, y, memory_ptr);
					memory_ptr++;
				}
				memory_ptr = bitmap.memory + (line * bitmap.line_offset);

			}

			DC_DATA();
			CS_ENA();
			ADIOS_SPI_TransferBlock(APP_LCD_SPI, frm_buf, NULL, buf_size, NULL);
			CS_DIS();


		}
#if 0
	}else return -1;  // not supported



#endif
	return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Transfers Bitmap to the TFT
// Notes: using back/fore colors respectively from pixel off/on for 1bit,
// trasferred to APP_LCD_NativeBitmapPrint for native 16bit
// IN: bitmap
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapHBoundaryPrint(adios_lcd_bitmap_t bitmap, u16 b_x, u16 b_width)
{
#if 0
  //if( !ADIOS_LCD_TypeIsGLCD() )
  //return -1; // no GLCD
  
  // abort if max. app_lcd_width reached
  //if( app_lcd_x >= adios_lcd_parameters.app_lcd_width )
  //return -2;
  
  /* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
  if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
    //    u16 *memory_ptr = bitmap.memory + ((bitmap.line_offset*top_pos_y + top_pos_x)*2);
    //    // transfer bitmap
    //    int top_pos_x, y;
    //    for(y=0; y<8; ++y){
    //      for(x=0; x<bitmap.width; ++x){
    //        APP_LCD_Data(*memory_ptr >> 8);
    //        APP_LCD_Data(*memory_ptr++ & 0xff);
    //      }
    //    }
    /* legacy 1bit pixel print */
  }else if(bitmap.colour_depth == 1) {  // 1bit format
    //fill fromr regular 1bit using back and fore colors
    // all GLCDs support the same bitmap scrambling
    int line;
    int y_lines = (bitmap.height >> 3);

    u16 initial_y = app_lcd_y;
    for(line=0; line<y_lines; ++line) {
      
      // calculate pointer to bitmap line
      u8 *memory_ptr = bitmap.memory + line * bitmap.line_offset + b_x;
      
      // set graphical cursor after second line has reached
      //    if( line > 0 ) {
      //      app_lcd_x = initial_x;
      //      app_lcd_y += 1;
      //      APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
      //    }
      
      // transfer bitmap
      int x, y;
      for(y=0; y<8; ++y){
        for(x=b_x; ((b_width+b_x)>bitmap.width)? (x<bitmap.width) : (x< (b_width+b_x)); ++x){
          //for(x=b_x; x< (b_width+b_x); ++x){
          if(*memory_ptr & (1<<y)){
            APP_LCD_Data(app_lcd_fore_color >> 8);
            APP_LCD_Data(app_lcd_fore_color & 0xff);
          }else{
            APP_LCD_Data(app_lcd_back_color >> 8);
            APP_LCD_Data(app_lcd_back_color & 0xff);
          }
          //DEBUG_MSG("%d %d %d", x, y, memory_ptr);
          memory_ptr++;
        }
        memory_ptr = bitmap.memory + line * bitmap.line_offset + b_x;
        app_lcd_y += 1;
        APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
      }
    }
    // fix graphical cursor if more than one line has been print
    app_lcd_x += bitmap.width;
    if( y_lines >= 1 ) {
      app_lcd_y = initial_y;
      APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
    }
  }else return -1;  // not supported
  
  
  
#endif
  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
// Transfers Bitmap to the TFT
// Notes: using back/fore colors respectively from pixel off/on for 1bit,
// trasferred to APP_LCD_NativeBitmapPrint for native 16bit
// IN: bitmap
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPrint(adios_lcd_bitmap_t bitmap)
{
#if 0
  //if( !ADIOS_LCD_TypeIsGLCD() )
  //return -1; // no GLCD
  
  // abort if max. app_lcd_width reached
  //if( app_lcd_x >= adios_lcd_parameters.app_lcd_width )
  //return -2;
  
  /* native 16bit depth. r(15:11), g(10:5), b(4:0)   */
  if(bitmap.colour_depth == APP_LCD_COLOUR_DEPTH){
    
    u8 *memory_ptr = bitmap.memory;
    u16 initial_x = app_lcd_x;
    u16 initial_y = app_lcd_y;
    // transfer bitmap
    int x, y;
    APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
    
    for(y=0; y<(((initial_y + bitmap.height)<=APP_LCD_HEIGHT)? bitmap.height : (APP_LCD_HEIGHT-initial_y)); ++y){
      for(x=0; x<(((initial_x + bitmap.width)<=APP_LCD_WIDTH)? bitmap.width : (APP_LCD_WIDTH-initial_x)); ++x){
        APP_LCD_Data(*memory_ptr++);
        APP_LCD_Data(*memory_ptr++);
        //DEBUG_MSG("%d %d %d", x, y, memory_ptr);
      }
      if((app_lcd_x + bitmap.width)>APP_LCD_WIDTH)memory_ptr +=(app_lcd_x + bitmap.width -APP_LCD_WIDTH)*2;
      app_lcd_y += 1;
      APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
    }
    
    /* legacy 1bit pixel print */
  }else if(bitmap.colour_depth == 1) {  // 1bit format
    //fill fromr regular 1bit using back and fore colors
    // all GLCDs support the same bitmap scrambling
    int line;
    int y_lines = (bitmap.height >> 3);

    u16 initial_y = app_lcd_y;
    for(line=0; line<y_lines; ++line) {
      
      // calculate pointer to bitmap line
      u8 *memory_ptr = bitmap.memory + line * bitmap.line_offset;
      
      // transfer bitmap
      int x, y;
      for(y=0; y<8; ++y){
        for(x=0; x<bitmap.width; ++x){
          if(*memory_ptr & (1<<y)){
            APP_LCD_Data(app_lcd_fore_color >> 8);
            APP_LCD_Data(app_lcd_fore_color & 0xff);
          }else{
            APP_LCD_Data(app_lcd_back_color >> 8);
            APP_LCD_Data(app_lcd_back_color & 0xff);
          }
          //DEBUG_MSG("%d %d %d", x, y, memory_ptr);
          memory_ptr++;
        }
        memory_ptr = bitmap.memory + line * bitmap.line_offset;
        app_lcd_y += 1;
        APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
      }
    }
    // fix graphical cursor if more than one line has been print
    app_lcd_x += bitmap.width;
    if( y_lines >= 1 ) {
      app_lcd_y = initial_y;
      APP_LCD_GCursorSet(app_lcd_x, app_lcd_y);
    }
  }else return -1;  // not supported
#endif
  return 0; // no error
}


void APP_LCD_DummyFunc(void){
	CS_DIS();
}
