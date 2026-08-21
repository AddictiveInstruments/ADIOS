/*
 * ili9488.h
 *
 *  Created on: Dec 14, 2021
 *      Author: timagr615
 */

#ifndef _5X6_TFT_H_
#define _5X6_TFT_H_

#include <adios.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define ILI9488_NOP     0x00
#define ILI9488_SWRESET 0x01
#define ILI9488_RDDID   0x04
#define ILI9488_RDDST   0x09

#define ILI9488_SLPIN   0x10
#define ILI9488_SLPOUT  0x11
#define ILI9488_PTLON   0x12
#define ILI9488_NORON   0x13

#define ILI9488_RDMODE  0x0A
#define ILI9488_RDMADCTL  0x0B
#define ILI9488_RDPIXFMT  0x0C
#define ILI9488_RDIMGFMT  0x0D
#define ILI9488_RDSELFDIAG  0x0F

#define ILI9488_INVOFF  0x20
#define ILI9488_INVON   0x21
#define ILI9488_GAMMASET 0x26
#define ILI9488_DISPOFF 0x28
#define ILI9488_DISPON  0x29

#define ILI9488_CASET   0x2A
#define ILI9488_PASET   0x2B
#define ILI9488_RAMWR   0x2C
#define ILI9488_RAMRD   0x2E

#define ILI9488_PTLAR   0x30
#define ILI9488_MADCTL  0x36
#define ILI9488_PIXFMT  0x3A

#define ILI9488_FRMCTR1 0xB1
#define ILI9488_FRMCTR2 0xB2
#define ILI9488_FRMCTR3 0xB3
#define ILI9488_INVCTR  0xB4
#define ILI9488_DFUNCTR 0xB6

#define ILI9488_PWCTR1  0xC0
#define ILI9488_PWCTR2  0xC1
#define ILI9488_PWCTR3  0xC2
#define ILI9488_PWCTR4  0xC3
#define ILI9488_PWCTR5  0xC4
#define ILI9488_VMCTR1  0xC5
#define ILI9488_VMCTR2  0xC7

#define ILI9488_RDID1   0xDA
#define ILI9488_RDID2   0xDB
#define ILI9488_RDID3   0xDC
#define ILI9488_RDID4   0xDD

#define ILI9488_GMCTRP1 0xE0
#define ILI9488_GMCTRN1 0xE1
/*
#define ILI9488_PWCTR6  0xFC
*/

// Color definitions
#define ILI9488_BLACK      			0x0000      /*   0,   0,   0 */
#define ILI9488_NAVY				0x000F      /*   0,   0, 128 */
#define ILI9488_DARKGREEN   		0x03E0      /*   0, 128,   0 */
#define ILI9488_DARKCYAN    		0x03EF      /*   0, 128, 128 */
#define ILI9488_MAROON      		0x7800      /* 128,   0,   0 */
#define ILI9488_PURPLE      		0x780F      /* 128,   0, 128 */
#define ILI9488_OLIVE       		0x7BE0      /* 128, 128,   0 */
#define ILI9488_LIGHTGREY   		0xC618      /* 192, 192, 192 */
#define ILI9488_DARKGREY    		0x7BEF      /* 128, 128, 128 */
#define ILI9488_BLUE        		0x001F      /*   0,   0, 255 */
#define ILI9488_GREEN       		0x07E0      /*   0, 255,   0 */
#define ILI9488_CYAN        		0x07FF      /*   0, 255, 255 */
#define ILI9488_RED         		0xF800      /* 255,   0,   0 */
#define ILI9488_MAGENTA     		0xF81F      /* 255,   0, 255 */
#define ILI9488_YELLOW      		0xFFE0      /* 255, 255,   0 */
#define ILI9488_WHITE       		0xFFFF      /* 255, 255, 255 */
#define ILI9488_ORANGE      		0xFD20      /* 255, 165,   0 */
#define ILI9488_GREENYELLOW 		0xAFE5      /* 173, 255,  47 */
#define ILI9488_PINK        		0xF81F

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

//***** Functions prototypes *****//
void TFT_Init();
void TFT_SendCommand(u8 com);
void TFT_SendData(u8 data);
//2.2 Write multiple/DMA
void TFT_SendData_Multi(u8 *buff, size_t buff_size);
void TFT_SendByte(u8 data);
//void TFT_WaitLastData();

void TFT_SetAddrWindow(u16 x0, u16 y0, u16 x1, u16 y1);
//void setScrollArea(u16 topFixedArea, u16 bottomFixedArea);
//void scroll(u16 pixels);
//void pushColor(u16 color);
//void pushColors(u16 *data, u8 len, u8 first);
//void drawImage(const u8* img, u16 x, u16 y, u16 w, u16 h);
void TFT_FillScreen(u16 color);

void TFT_DrawPixel(s16 x, s16 y, u16 color);
void TFT_DrawFastVLine(s16 x, s16 y, s16 h, u16 color);
void TFT_DrawFastHLine(s16 x, s16 y, s16 w, u16 color);
void TFT_SendFastPixels(s32 n, u16 color);
void TFT_DrawLine(s16 x0, s16 y0, s16 x1, s16 y1,u16 color);
void TFT_WriteLine(s16 x0, s16 y0, s16 x1, s16 y1,u16 color);
void TFT_FillRect(s16 x, s16 y, s16 w, s16 h, u16 color);


void TFT_SetRotation(u8 r);
void TFT_InvertDisplay(u8  i);
u16  TFT_Color565(u8 r, u8 g, u8 b);
void TFT_DrawChar(s16 x, s16 y, unsigned char c, u16 color, u16 bg, u8 size);
void TFT_PrintText(char text[], s16 x, s16 y, u16 color, u16 bg, u8 size);

void TFT_Write16BitColor(u16 color);
void TFT_Digits_draw(int n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC);
//void testLines(u8 color);



#endif /* _5X6_TFT_H_ */
