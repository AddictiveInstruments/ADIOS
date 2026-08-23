/*
 * ADIOS Application Template
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
#include "app.h"
#include "app_lcd.h"
#include "glcd_font.h"
#include "tr5x6_decod.h"
#include "tr5x6_rom.h"
#include "midio_sysex.h"
#include "tr5x6_pict.h"

#include <FreeRTOS.h>
#include <portmacro.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// define priority level for SPI Handler task:
// use same priority as ADIOS specific tasks (3)
#define PRIORITY_TASK_TFT_HANDLER	( tskIDLE_PRIORITY + 3 )
#define PRIORITY_TASK_ROM_HANDLER	( tskIDLE_PRIORITY + 2 )
#define x_offset	8
#define y_offset	15
#define w_max		464
#define h_max		280

#define xfer_bmp_bank_height	(s16)(26)
#define xfer_bmp_slot_height	(s16)(41)
#define xfer_bmp_height			(s16)(xfer_bmp_bank_height+xfer_bmp_slot_height)
#define xfer_bmp_width			(s16)(5*23-1)
#define xfer_bmp_size 			(int)(((xfer_bmp_height/8)+((xfer_bmp_height%8)?1:0))*xfer_bmp_width)
#define xfer_bmp_slot_size 		(int)(((xfer_bmp_slot_height/8)+((xfer_bmp_slot_height%8)?1:0))*xfer_bmp_width)
#define xfer_bmp_bank_size 		(int)(((xfer_bmp_bank_height/8)+((xfer_bmp_bank_height%8)?1:0))*xfer_bmp_width)
// local prototype of the task function
static void Legend_Draw(int x, int y, const char *func, const char *butt);
static void SettingsMenu_Draw(void);
static void SettingsMenu_Legend(void);
static void Formatting_Page(void);
static void About_Page(void);
static void TASK_SettingsMenu(void *pvParameters);
static void TASK_TFT_Periodic(void *pvParameters);
static u8   TFT_BankInhibit(void);
static void TFT_Digits(void);
static void TFT_Beat(void);
static void TFT_BankSelect(void);
#if TR5X6_UNIT_SELECT==626
static void TFT_InstGridDraw(void);
#endif
static void TFT_XferInstFlagsLatch(void);
static void TFT_XferRefresh(void);
static u8   TFT_XferIsIdle(void);
static void TFT_Slotinfo(void);
static void TFT_InstSelect(void);
static void TFT_Steps(void);
static void TFT_Scale(void);
static void TFT_Labels(void);
static void TFT_Mode(void);
static void TFT_Group(void);
static void TFT_MidiActivity(void);
static void TASK_ROM_Periodic(void *pvParameters);
static void APP_TFT_Background(void);
static s32 NOTIFY_MIDI_TimeOut(adios_midi_port_t port);
static s32 NOTIFY_MIDI_Rx(adios_midi_port_t port, u8 byte);

TaskHandle_t xSettings = NULL;
TaskHandle_t xTFTRefresh = NULL;
TaskHandle_t xROMCheck = NULL;
static u8 normal_start = 0;
u32 old_count;
u8 old_segments[40];
static u8 bank_change_inhibit = 0;
static u8 first_start = 1;
static u8 bank_changed = 0;

#if TR5X6_UNIT_SELECT==626
static u8 inst_change_exit=0;
static u8 inst_grid_name=0;
static u8 inst_grid_shown=0;
static u8 inst_grid_sel=0;
static u8 inst_grid_blink=0;
#endif

static s8 xfer_delay=0;
static u16 xfer_flag=0;
static u8 beat_flag=0;
static u8 beat_enabled=1;
static u32 midi_clock_ctr;
static u16 beat_color = APP_LCD_DARKGREY;
adios_lcd_bitmap_t bmp;		//dummy bitmap
//static u8 uart1_act=0;
static u8 menu_pos = 0;
static u8 menu_edit = 0;
static u8 last_id = 0;
static u8 curr_id = 0;
static u8 formatting=0;

//// define a Mutex for LCD access
//xSemaphoreHandle xSPISemaphore;
//#define MUTEX_SPI_TAKE { while( xSemaphoreTakeRecursive(xSPISemaphore, (portTickType)1) != pdTRUE ); }
//#define MUTEX_SPI_GIVE { xSemaphoreGiveRecursive(xSPISemaphore); }

//temp
//static int address=0;
//static int count=0;
//static s32 upload_progress=0;
//static s32 upload_progress_old=0;



/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
	// ROM R/W init
	TR5X6_ROM_Init();
	TR5X6_ROM_HOST();
	// initialize all LEDs
	ADIOS_SOL_Init();

	// initialize LCD Bus Decoder.
	TR5X6_DECOD_Init();

	// TFT init - the driver is started HERE, after the bus decoder it talks
	// through and before anything is drawn. Nothing starts a display behind
	// your back any more: this call is the application's, and its place in
	// this sequence is a decision, not a default.
	APP_LCD_Init(0);
	APP_LCD_BColourSet(APP_LCD_BLACK);
	APP_LCD_FColourSet(APP_LCD_WHITE);
	APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);
	APP_LCD_Clear();	// clear the TFT
	// check magic number and boot page request
	u8 rom_empty = 0;
	// NOT the very last byte of flash any more: the last two belong to the
	// persistent device-ID record (see tr5x6_rom.h). A machine formatted by an
	// older firmware still carries its magic at the old address, looks empty
	// here, and would be formatted - hence the one-shot migration app, to be
	// run once after the bootloader update and before this firmware.
	if((*((volatile u8*)(TR5X6_FLASH_MAGIC_ADDR)))!=TR5X6_MAGIC_NUMBER)rom_empty = 1;
	if( (tr5x6_decod_buttons.ALL==0x0a) || rom_empty ){
		tr5x6_decod_buttons_flags.ALL=0;
		curr_id = ADIOS_MIDI_DeviceIDGet();
		last_id = curr_id;
		if(rom_empty){
			APP_LCD_Lite(1);	// swwitch on
			// periodic screen task
			menu_pos = 1;
			menu_edit = 1;
			formatting = 1;
			APP_LCD_PrintString(240, y_offset+10, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Format   BANKs");
			Formatting_Page();
			for(u16 d=0; d<2000; d++)ADIOS_DELAY_Wait_uS(1000);		// wait 2s
			ADIOS_SYS_Reset();
			//TR5X6_MEM_Format();
		}else{
			SettingsMenu_Draw();
			SettingsMenu_Legend();
			APP_LCD_Lite(1);
			// periodic screen task
			xTaskCreate(TASK_SettingsMenu, "Settings_Menu", (TFT_TASK_STACK_SIZE)/4, NULL, PRIORITY_TASK_TFT_HANDLER, &xSettings);
		}

	}else {
		adios_lcd_bitmap_t logo_bmp = APP_LCD_BitmapInit((u8*)tr5x6_logo, 400, 56, 400, Is1BIT);
		for (int w=0; w<400;w++){
			u8* bmp_mem_ptr = logo_bmp.memory +w;
			adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, 56, 400, Is1BIT);
			APP_LCD_SendBitmap(bmp2print, 40+w, y_offset+110);
		}
		APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
		APP_LCD_PrintString(40, y_offset+167, 0, APP_LCD_STRING_ALIGN_LEFT, -32, (char*)TR5X6_VERSION);
		APP_LCD_Lite(1);
		// initialize SysEx mgnt.
		MIDIO_SYSEX_Init(0);
		// install Direct MIDI RX callback
		ADIOS_MIDI_DirectRxCallback_Init(&NOTIFY_MIDI_Rx);

		// install SysEx callback
		ADIOS_MIDI_SysExCallback_Init(APP_SYSEX_Parser);

		// install timeout callback function
		ADIOS_MIDI_TimeOutCallback_Init(NOTIFY_MIDI_TimeOut);

		// TFT

		normal_start = 1;		// marks TFT ready
		// periodic screen task
		xTaskCreate(TASK_TFT_Periodic, "TFT_Handler", (TFT_TASK_STACK_SIZE)/4, NULL, PRIORITY_TASK_TFT_HANDLER, &xTFTRefresh);
		// periodic ROM task
		xTaskCreate(TASK_ROM_Periodic, "ROM_Handler", (ROM_TASK_STACK_SIZE)/4, NULL, PRIORITY_TASK_ROM_HANDLER, &xROMCheck);
		first_start=1;
	}

	// Hold the splash screen before the periodic tasks above start painting
	// over it. This wait belongs to the application: it decides what it shows
	// at startup, so it decides how long that stays up. Set APP_SPLASH_MS to
	// 0 to go straight to the running screen.
	{
		int ms;
		for(ms=0; ms<APP_SPLASH_MS; ++ms)
			ADIOS_DELAY_Wait_uS(1000);
	}
}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{

}


/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the main task which also handles DIN, ENC
// and AIN events. You could add more jobs here, but they shouldn't consume
// more than 300 uS to ensure the responsiveness of buttons, encoders, pots.
// Alternatively you could create a dedicated task for application specific
// jobs as explained in $ADIOS_PATH/apps/tutorials/006_rtos_tasks
/////////////////////////////////////////////////////////////////////////////
void APP_Tick(void)
{
	// Button interception
	TR5X6_DECOD_BUTT_Handler();
	// Sysex Time out tick	}
	if(normal_start)MIDIO_SYSEX_TimeOut_Period();
	// check for decoding

}


/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the MIDI task which checks for incoming
// MIDI events. You could add more MIDI related jobs here, but they shouldn't
// consume more than 300 uS to ensure the responsiveness of incoming MIDI.
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_Tick(void)
{

}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI package has been received
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_NotifyPackage(adios_midi_port_t port, adios_midi_package_t midi_package)
{
	if(port==DIN0){
		if(normal_start)ADIOS_MIDI_SendPackage(DIN2,  midi_package);
	}
}



/////////////////////////////////////////////////////////////////////////////
// Installed via ADIOS_MIDI_DirectRxCallback_Init
/////////////////////////////////////////////////////////////////////////////
static s32 NOTIFY_MIDI_Rx(adios_midi_port_t port, u8 midi_byte)
{
//#if 0
	// check for MIDI clock fom TR
	if(normal_start){
		if(port==DIN0){
			if( midi_byte == 0xf8 ) {


				if( (midi_clock_ctr % 24) == 0 ){
					beat_enabled=1;		// beat
					beat_flag=1;
				}else if( (midi_clock_ctr % 24) == 6 ){
					beat_enabled=0;		// beat
					beat_flag=1;
				}
						// beat

				++midi_clock_ctr;

				return 0; // no error, no filtering
			}

			// check for MIDI start or continue
			if( midi_byte == 0xfa || midi_byte == 0xfb ) {
				beat_color = APP_LCD_RED;
				beat_flag=1;
				midi_clock_ctr = 0;
				//    total_delay = 0;

				return 0; // no error, no filtering
			}

			// check for MIDI stop
			if( midi_byte == 0xfc ) {
				beat_color = APP_LCD_DARKGREY;
				beat_flag=1;

				return 0; // no error, no filtering
			}
		}
	}


	return 0; // no error, no filtering
//#else



	//return 0; // no error, no filtering
//#endif
}


/////////////////////////////////////////////////////////////////////////////
// This function parses an incoming sysex stream for ADIOS commands
/////////////////////////////////////////////////////////////////////////////
s32 APP_SYSEX_Parser(adios_midi_port_t port, u8 midi_in)
{
	// host passthrough: whatever the TR-505 sends (DIN0 = UART0 = USART1) is
	// merged straight onto the instrument's physical MIDI OUT (UART2 =
	// USART3) - raw UART index here, not a MIDI port number
	if(port==DIN0){
		ADIOS_UART_TxBufferPut(2, midi_in);
	}else{
		// App sysex parser
		MIDIO_SYSEX_Parser(port, midi_in);
	}

	return 1; // no error
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_SRIN_NotifyToggle(u32 pin, u32 pin_value)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a pot has been moved
/////////////////////////////////////////////////////////////////////////////
void APP_ADC_NotifyChange(u32 port, u32 chn, u32 value)
{
}

/////////////////////////////////////////////////////////////////////////////
// prints screen background
/////////////////////////////////////////////////////////////////////////////
void APP_TFT_Background(void){
#if 1
	APP_LCD_Clear();	// clear the TFT
	// background
	//adios_lcd_bitmap_t bmp;		// dummy bitmap: todo remove
	APP_LCD_BColourSet(APP_LCD_BLACK);
	APP_LCD_FColourSet(APP_LCD_WHITE);
	APP_LCD_Rectangle(x_offset, y_offset, w_max, 91, 2, APP_LCD_DARKGREY, 0, 0);
#ifndef REDUCED_APP_LCD
	APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);
	APP_LCD_PrintString(x_offset+47, y_offset+8, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "SCALE");
#endif
	for(int i=1; i<=16; i++)APP_LCD_DrawFastVLine(x_offset+w_max-2-i*23, (i==16)?y_offset+2:y_offset+40,  (i==16)?94:50, APP_LCD_DARKGREY);
	APP_LCD_DrawFastHLine(x_offset+2, y_offset+39, w_max-4, APP_LCD_DARKGREY);
	APP_LCD_DrawFastHLine(x_offset+2, y_offset+64, w_max-4, APP_LCD_DARKGREY);
#ifndef REDUCED_APP_LCD
	APP_LCD_PrintString(x_offset+47, y_offset+46, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "PATTERN");
	APP_LCD_PrintString(x_offset+47, y_offset+71, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "STEP");
#endif


	APP_LCD_Rectangle(x_offset, y_offset+93, w_max, 71, 2, APP_LCD_DARKGREY, 0, 0);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
#if TR5X6_UNIT_SELECT==505
	APP_LCD_PrintString(x_offset+48, y_offset+103, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "INSTRUMENT");
	APP_LCD_PrintString(x_offset+48, y_offset+118, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "/ ACCENT");
#else // TR5X6_UNIT_SELECT==626
	APP_LCD_PrintString(x_offset+48, y_offset+110, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "INSTRUMENT");
#endif
	APP_LCD_PrintString(x_offset+48, y_offset+144, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "SOUND  BANK");
	APP_LCD_DrawFastHLine(x_offset+2, y_offset+136, w_max-4, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+94, y_offset+95,  69, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+w_max-(6*23)-2, y_offset+137,  25, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+w_max-(5*23)-2, y_offset+95,  69, APP_LCD_DARKGREY);

	APP_LCD_Rectangle(x_offset, y_offset+166, w_max, 114, 2, APP_LCD_DARKGREY, 0, 0);
	APP_LCD_DrawFastHLine(x_offset+2, y_offset+188, w_max-4, APP_LCD_DARKGREY);
#if TR5X6_UNIT_SELECT==505
	APP_LCD_PrintString(x_offset+30, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "TRACK");
	APP_LCD_PrintString(x_offset+393, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "MODE");
	APP_LCD_DrawFastVLine(x_offset+58, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+186, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+324, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_PrintString(x_offset+335, y_offset+249, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "MIDI ACT.");
	APP_LCD_DrawFastHLine(x_offset+325, y_offset+243, 139, APP_LCD_DARKGREY);
	//APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_PrintString(x_offset+350, y_offset+262, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "RX");
	APP_LCD_PrintString(x_offset+438, y_offset+249, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "505 TX");
	APP_LCD_PrintString(x_offset+438, y_offset+262, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "5x6 TX");
#else // TR5X6_UNIT_SELECT==626
	APP_LCD_PrintString(x_offset+27, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "BANK");
	APP_LCD_PrintString(x_offset+79, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "TRACK");
	APP_LCD_PrintString(x_offset+236, y_offset+173, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "GROUP");
	APP_LCD_PrintString(x_offset+340, y_offset+173, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "PATTERN");
	APP_LCD_PrintString(x_offset+404, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "MODE");
	APP_LCD_DrawFastVLine(x_offset+53, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+106, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+230, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_DrawFastVLine(x_offset+344, y_offset+168, 110, APP_LCD_DARKGREY);
	APP_LCD_PrintString(x_offset+350, y_offset+249, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "MIDI ACT.");
	APP_LCD_DrawFastHLine(x_offset+345, y_offset+232, 119, APP_LCD_DARKGREY);
	//APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_PrintString(x_offset+365, y_offset+262, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "RX");
	APP_LCD_PrintString(x_offset+443, y_offset+249, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "626 TX");
	APP_LCD_PrintString(x_offset+443, y_offset+262, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "5x6 TX");
#endif


	// last step
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	for(int i=0; i<16; i++){
		APP_LCD_FColourSet(APP_LCD_DARKGREY);
		APP_LCD_PrintFormattedString(x_offset+w_max-359+i*23, y_offset+71, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%d", (i+1));
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////
// periodically called for screen print
/////////////////////////////////////////////////////////////////////////////
static void TASK_ROM_Periodic(void *pvParameters)
{
	portTickType xLastExecutionTime;
	s32 spi_err_reported = 0;

	// Initialise the xLastExecutionTime variable on task entry
	xLastExecutionTime = xTaskGetTickCount();

	while( 1 ) {
		// 10 ms: this task is what picks up a pending block write, so its
		// period is dead time added to every single block of an upload
		vTaskDelayUntil(&xLastExecutionTime, 10 / portTICK_RATE_MS);
		// check for requested write
		MIDIO_SYSEX_Cmd_WriteInfoRequest();
		MIDIO_SYSEX_Cmd_WriteBlockRequest();
		// dbg scaffolding: reported OUT of the critical path, so the ROM
		// sequences keep their timing. Non-zero means a transfer was refused
		// and the latch kept a stale value - see TR5X6_ROM_Addr_Set.
		if( tr5x6_rom_spi_err != spi_err_reported ) {
			ADIOS_MIDI_SendDebugMessage("SPI refused: %d", tr5x6_rom_spi_err);
			spi_err_reported = tr5x6_rom_spi_err;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////
// prepares and prints the transfer window
/////////////////////////////////////////////////////////////////////////////
static void TFT_XferSlotInfoReceived(void)
{
	u8 xfer_bmp_array[xfer_bmp_size];
	for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;

	s8 bank_progress = MIDIO_SYSEX_Bank_Progression();
	s16 height = xfer_bmp_height;
	if(bank_progress>=0){
		height = xfer_bmp_slot_height;
	}
	adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, height, xfer_bmp_width, Is1BIT);

	//memset(sysex_bmp.memory, 0x00, size);
	u8 cmd = MIDIO_SYSEX_Cmd_Current();
	u8 bank_slot = ((cmd==CMD_SLOT_WRITE_INFO) || (cmd==CMD_SLOT_READ_INFO))?1:0;
	u8 read_write = ((cmd==CMD_SLOT_WRITE_INFO) || (cmd==CMD_BANK_WRITE_INFO))?1:0;
	// prints messages in bitmap
	if(bank_slot){
		if(bank_progress>=0)
			APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 8, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Slot# %u", MIDIO_SYSEX_Slot_Current()+1);
		else APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 16, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Bank# %u/Slot# %u", MIDIO_SYSEX_Bank_Current()+1, MIDIO_SYSEX_Slot_Current()+1);

	}else
		APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 16, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Bank# %u", MIDIO_SYSEX_Bank_Current()+1);

	if(read_write)
		APP_LCD_BitmapPrintString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), (bank_progress>=0)?23:36, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Info write ok.");
	else
		APP_LCD_BitmapPrintString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), (bank_progress>=0)?23:36, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Info read ok.");

	APP_LCD_FColourSet(APP_LCD_WHITE);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_BColourSetRGB(0x7f3030);
	for (int w=0; w<xfer_bmp_width;w++){
		u8* bmp_mem_ptr = xfer_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, height, xfer_bmp_width, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, x_offset+w_max-2-xfer_bmp_width+w, y_offset+95);
	}
	APP_LCD_BColourSet(APP_LCD_BLACK);

	if(bank_progress>=0){
		for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;	// adds top line separation
		adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, xfer_bmp_bank_height-1, xfer_bmp_width, Is1BIT);
		APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 8-1, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Bank# %u - %u%s", MIDIO_SYSEX_Bank_Current()+1, bank_progress, "%");
		u8 r=0x7f-(40*bank_progress/100);u8 g=40*bank_progress/100;u8 b=40*bank_progress/100;
		APP_LCD_PrintProgress(xfer_bmp, (u32)((r<<16)|(g<<8)|b), x_offset+w_max-2-xfer_bmp_width, y_offset+95+xfer_bmp_slot_height+1, xfer_bmp_width, xfer_bmp_bank_height-1, xfer_bmp_width*bank_progress/100);
		APP_LCD_DrawFastHLine(x_offset+w_max-2-xfer_bmp_width, y_offset+95+xfer_bmp_slot_height, xfer_bmp_width, APP_LCD_DARKGREY);
	}


	// the ACCENT wipe may have taken the separator bar with it - whoever
	// redraws the window puts the bar back
	APP_LCD_DrawFastVLine(x_offset+w_max-3-xfer_bmp_width, y_offset+95, 40, APP_LCD_DARKGREY);
}


/////////////////////////////////////////////////////////////////////////////
// prepares and prints the transfer window
/////////////////////////////////////////////////////////////////////////////
static void TFT_XferSlotProgress(u8 _progress)
{
	u8 xfer_bmp_array[xfer_bmp_size];
	for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;

	s8 bank_progress = MIDIO_SYSEX_Bank_Progression();
	s16 height = xfer_bmp_height;
	if(bank_progress>=0){
		height = xfer_bmp_slot_height;
	}
	adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, height, xfer_bmp_width, Is1BIT);

	// prints messages in bitmap
	if(bank_progress>=0){
		APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 8, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Slot# %u", MIDIO_SYSEX_Slot_Current()+1);
	}else{
		APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 16, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Bank# %u/Slot# %u", MIDIO_SYSEX_Bank_Current()+1, MIDIO_SYSEX_Slot_Current()+1);
	}

	if(MIDIO_SYSEX_Cmd_Current()==CMD_READ_BLOCK){
		if(_progress==100)
			APP_LCD_BitmapPrintString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), (bank_progress>=0)?23:36, 0, APP_LCD_STRING_ALIGN_CENTER, -32,  "ROM read.");
		else APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), (bank_progress>=0)?23:36, 0, APP_LCD_STRING_ALIGN_CENTER, -32,  "%u%s", _progress, "% ...");

	}else{
		if(_progress==100)
			APP_LCD_BitmapPrintString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), (bank_progress>=0)?23:36, 0, APP_LCD_STRING_ALIGN_CENTER, -32,  "ROM written.");
		else APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), (bank_progress>=0)?23:36, 0, APP_LCD_STRING_ALIGN_CENTER, -32,  "%u%s", _progress, "% ...");

	}
	APP_LCD_FColourSet(APP_LCD_WHITE);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);

	if(_progress!=101){
		//APP_LCD_FColourSet(APP_LCD_WHITE);
		u8 r=0x7f-(40*_progress/100);u8 g=40*_progress/100;u8 b=40*_progress/100;
		APP_LCD_PrintProgress(xfer_bmp, (u32)((r<<16)|(g<<8)|b), x_offset+w_max-2-xfer_bmp_width, y_offset+95, xfer_bmp_width, height, xfer_bmp_width*_progress/100);   // new
	}else{
		//APP_LCD_FColourSet(APP_LCD_RED);
		  for (int w=0; w<xfer_bmp_width;w++){
			  u8* bmp_mem_ptr = xfer_bmp.memory +w;
			  adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, xfer_bmp_slot_height, height, Is1BIT);
			  APP_LCD_SendBitmap(bmp2print, x_offset+w_max-2-xfer_bmp_width+w, y_offset+95);
		  }
	}

	if(bank_progress>=0){
		for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;	// adds top line separation
		adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, xfer_bmp_bank_height-1, xfer_bmp_width, Is1BIT);
		APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 8-1, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Bank# %u - %u%s", MIDIO_SYSEX_Bank_Current()+1, bank_progress, "%");
		u8 r=0x7f-(40*bank_progress/100);u8 g=40*bank_progress/100;u8 b=40*bank_progress/100;
		APP_LCD_PrintProgress(xfer_bmp, (u32)((r<<16)|(g<<8)|b), x_offset+w_max-2-xfer_bmp_width, y_offset+95+xfer_bmp_slot_height+1, xfer_bmp_width, xfer_bmp_bank_height-1, xfer_bmp_width*bank_progress/100);
		APP_LCD_DrawFastHLine(x_offset+w_max-2-xfer_bmp_width, y_offset+95+xfer_bmp_slot_height, xfer_bmp_width, APP_LCD_DARKGREY);
	}

	// the ACCENT wipe may have taken the separator bar with it - whoever
	// redraws the window puts the bar back
	APP_LCD_DrawFastVLine(x_offset+w_max-3-xfer_bmp_width, y_offset+95, 40, APP_LCD_DARKGREY);
}

/////////////////////////////////////////////////////////////////////////////
// prepares and prints the transfer window
//
// Owns the slot info rectangle, so it decides on its own whether there is
// anything to write there. The 505 has a case the 626 has not: ACCENT takes
// the window over and wipes it instead. The flags read here do not even exist
// on the other machine - inst_acc is 505 only, inst_blk is 626 only - so the
// two gates cannot be merged.
/////////////////////////////////////////////////////////////////////////////
static void TFT_Slotinfo(void)
{
	u8 draw;
#if TR5X6_UNIT_SELECT==505
	if( ((tr5x6_decod_inst_acc_flags&0x1) & (tr5x6_decod_inst_acc&0x1))
	 || ((tr5x6_decod_inst_acc&0x1) && xfer_flag) ){
		xfer_flag = 0;
		if(!bank_changed){
			// ACCENT owns the zone: wipe the whole slot info window in two passes.
			// The top pass goes WITH its separator bar and the horizontal line;
			// the bottom pass, one row higher than the bank window, goes WITHOUT
			// the bar column - the lower bar piece right of the beat is constant,
			// never overwritten, and keeps its head pixel at the line level.
			APP_LCD_Rectangle(x_offset+w_max-3-xfer_bmp_width, y_offset+95, xfer_bmp_width+1, xfer_bmp_slot_height, 0, 0, 1, 0);
			APP_LCD_Rectangle(x_offset+w_max-2-xfer_bmp_width, y_offset+95+xfer_bmp_slot_height, xfer_bmp_width, xfer_bmp_bank_height, 0, 0, 1, 0);
		}
		return;
	}
	draw = ((((tr5x6_decod_inst_acc_flags&0x2) & (tr5x6_decod_inst_acc&0x2)) ||
	         (tr5x6_decod_inst_sel_flags & tr5x6_decod_inst_sel)) && (!bank_changed))
	    || ((tr5x6_decod_inst_acc&0x2) && xfer_flag);
#else
	draw = (((tr5x6_decod_inst_sel_flags & tr5x6_decod_inst_sel) ||
	         (tr5x6_decod_inst_blk_flags & tr5x6_decod_inst_blk)) && (!bank_changed))
	    || xfer_flag;
#endif
	if(!draw) return;
	xfer_flag = 0;

#if 0
	u8 xfer_bmp_array[xfer_bmp_size];
	for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;
	adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, xfer_bmp_height, xfer_bmp_width, Is1BIT);
	//memset(sysex_bmp.memory, 0x00, size);
	s8 selected = -1;
	for(int i=0; i<16; i++)if(tr5x6_decod_inst_sel &(1<<i)){selected = i; break;}
	// prints messages in bitmap
	APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 8, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%d-%s", selected+1, tr5x6_slots[selected].name);
	APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 23, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Slot: %s/%s", tr5x6_slot_name[tr5x6_slots[selected].size], tr5x6_slot_duration[tr5x6_slots[selected].size]);
	APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
	APP_LCD_BColourSet(APP_LCD_RED);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	//APP_LCD_FColourSet(APP_LCD_RED);
	for (int w=0; w<xfer_bmp_width;w++){
		u8* bmp_mem_ptr = xfer_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, xfer_bmp_height, xfer_bmp_width, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, x_offset+w_max-2-xfer_bmp_width+w, y_offset+95);
	}
	APP_LCD_BColourSet(APP_LCD_BLACK);
#else

	u8 xfer_bmp_array[xfer_bmp_size];
	for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;
	adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, xfer_bmp_height, xfer_bmp_width, Is1BIT);
	//memset(sysex_bmp.memory, 0x00, size);
	s8 selected = -1;
	for(int i=0; i<16; i++){
		if(tr5x6_decod_inst_sel &(1<<i)){
			selected = i;
#if TR5X6_UNIT_SELECT==626
			if(tr5x6_decod_inst_blk &(1<<i))selected += 16;
#endif
			break;
		}
	}

	// prints messages in bitmap
	char name[15];
	memcpy(name, tr5x6_slots[selected].name, 14);
	for(int i=1; i<14; i++)name[i]=toupper(name[i]);

	APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 10, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Slot# %d", selected+1);
	APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 27, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%s", name);
	APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 44, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Size: %s/%s", tr5x6_slot_name[tr5x6_slots[selected].size], tr5x6_slot_duration[tr5x6_slots[selected].size]);
	APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
	//APP_LCD_BColourSet(APP_LCD_RED);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	//APP_LCD_FColourSet(APP_LCD_RED);
	for (int w=0; w<xfer_bmp_width;w++){
		u8* bmp_mem_ptr = xfer_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, xfer_bmp_height, xfer_bmp_width, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, x_offset+w_max-2-xfer_bmp_width+w, y_offset+95);
	}
	//APP_LCD_BColourSet(APP_LCD_BLACK);

	// the ACCENT wipe may have taken the separator bar with it - whoever
	// redraws the window puts the bar back
	APP_LCD_DrawFastVLine(x_offset+w_max-3-xfer_bmp_width, y_offset+95, 40, APP_LCD_DARKGREY);
#endif
}

/////////////////////////////////////////////////////////////////////////////
// prepares and prints the transfer window
/////////////////////////////////////////////////////////////////////////////
static void TFT_XferError(u8 _error)
{
	u8 xfer_bmp_array[xfer_bmp_size];
	for(int i =0; i<xfer_bmp_size; i++)xfer_bmp_array[i] = 0x00;
	adios_lcd_bitmap_t xfer_bmp = APP_LCD_BitmapInit((u8*)xfer_bmp_array, xfer_bmp_width, xfer_bmp_slot_height, xfer_bmp_width, Is1BIT);
	//memset(sysex_bmp.memory, 0x00, size);

	// prints messages in bitmap
	APP_LCD_BitmapPrintFormattedString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 8, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Bank%u/Slot%u", MIDIO_SYSEX_Bank_Current()+1, MIDIO_SYSEX_Slot_Current()+1);
	APP_LCD_BitmapPrintString(xfer_bmp, 1.0, (s16)(xfer_bmp_width/2), 23, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Transfer Error!");
	APP_LCD_FColourSet(APP_LCD_RED);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	//APP_LCD_FColourSet(APP_LCD_RED);
	for (int w=0; w<xfer_bmp_width;w++){
		u8* bmp_mem_ptr = xfer_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, xfer_bmp_slot_height, xfer_bmp_width, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, x_offset+w_max-2-xfer_bmp_width+w, y_offset+95);
	}


	// the ACCENT wipe may have taken the separator bar with it - whoever
	// redraws the window puts the bar back
	APP_LCD_DrawFastVLine(x_offset+w_max-3-xfer_bmp_width, y_offset+95, 40, APP_LCD_DARKGREY);
}

#if TR5X6_UNIT_SELECT==626
/////////////////////////////////////////////////////////////////////////////
// prepares and prints an Instrument Grid Cell
/////////////////////////////////////////////////////////////////////////////
static void TFT_InstGridCellDraw(u8 inst, u8 blink)
{
	u8 x=inst%8; u8 y=inst/8;
	// Read the blocked bit BEFORE inst moves. A blocked instrument lives 16
	// slots higher, but the bitfield is only 16 bits wide - testing it again
	// after the += would shift right out of the word, and the corner would
	// never be drawn.
	u8 blocked = (tr5x6_decod_inst_blk & (1<<inst)) ? 1 : 0;

	u8 bmp_array[45*3];
	for(int i =0; i<(45*3); i++)bmp_array[i] = 0x00;
	adios_lcd_bitmap_t bmp = APP_LCD_BitmapInit((u8*)bmp_array, 45, 20, 45, Is1BIT);

	if(blocked) inst += 16;

	tr5x6_flash_info_t slot;
	slot.bank=TR5X6_ROM_BankGet();
	slot.slot=inst;
	TR5X6_FLASH_SlotRead(&slot);
	APP_LCD_FColourSetRGB(slot.color);
	// blink: 0 = full draw, 1 = blink ON phase, 2 = blink OFF phase (no label)
	if(blink != 2){
	if(inst_grid_name)
		APP_LCD_BitmapPrintFormattedString(bmp, 0, 22, 4, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%s", tr5x6_slots[inst].shortname);
	else APP_LCD_BitmapPrintFormattedString(bmp, 0, 22, 4, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%d", (inst+1));
	}

	// blink repaints start at column 8: the corner's 8x8 block at the cell's
	// left edge is never repainted, so it does not pump at 3 Hz
	for (int w=(blink?8:0); w<45;w++){
		u8* bmp_mem_ptr = bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, 20, 45, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, x_offset+w_max-369+x*46+w, y_offset+95+(21*y));
	}

	// draw the cell corner - full draws only, blink phases never touch it
	if(!blink && blocked){
		APP_LCD_DrawFastCorner(x_offset+w_max-369+x*46, y_offset+107+(21*y), APP_LCD_WHITE);
	}
}

/////////////////////////////////////////////////////////////////////////////
// prepares and prints the Instrument Grid
/////////////////////////////////////////////////////////////////////////////
static void TFT_InstGridDraw(void)
{
	if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst )inst_grid_name ^=0xff;
	if( tr5x6_decod_inst_sel_flags || tr5x6_decod_inst_blk_flags || (tr5x6_decod_buttons_flags.inst && inst_change_exit)){
		if(!inst_change_exit){
			APP_LCD_Rectangle(x_offset+95, y_offset+95, 367, 41, 0, 0, 1, 0);
			for(int i=1; i<8; i++)APP_LCD_DrawFastVLine(x_offset+w_max-2-i*46, y_offset+95, 41, APP_LCD_DARKGREY);
			APP_LCD_DrawFastHLine(x_offset+95, y_offset+115, 367, APP_LCD_DARKGREY);
			APP_LCD_DrawFastHLine(x_offset+w_max-2-xfer_bmp_width, y_offset+95+xfer_bmp_slot_height, xfer_bmp_width, APP_LCD_DARKGREY);
			for(int i=0; i<16; i++){
				TFT_InstGridCellDraw(i, 0);
			}
			inst_grid_blink = 0;
		}else{
			if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst ){
				for(int i=0; i<16; i++){
					TFT_InstGridCellDraw(i, 0);
				}
			}else{
				for(int i=0; i<16; i++){
					if( tr5x6_decod_inst_blk_flags &(1<<i) ){
						// the selection moves: give the OLD cell its label back
						// before it is abandoned mid-blink, label hidden
						if(inst_grid_sel != i)TFT_InstGridCellDraw(inst_grid_sel, 1);
						inst_grid_sel = i;
						TFT_InstGridCellDraw(i, 0);
						break;
					}
				}
			}
		}
	}
	inst_change_exit = 1;
	tr5x6_decod_buttons_flags.inst=0;

	// The selected instrument NUMBER flashes at ~3 Hz (160 ms half-period,
	// 4 passes of the 40 ms task): an action on an instrument flips the
	// 1-16 <> 17-30 bank AND selects that instrument - the flash shows WHICH
	// one the host is now on. Numbers only: the shortname view does not blink.
	++inst_grid_blink;
	if( !inst_grid_name && (inst_grid_blink & 3) == 0 )
		TFT_InstGridCellDraw(inst_grid_sel, (inst_grid_blink & 4) ? 2 : 1);
}
#endif

#if 0
static void TFT_BankChange(u8 bank_num)
{
	u8 bmp_array[229*2];
	for(int i =0; i<(229*2); i++)bmp_array[i] = 0x00;
	adios_lcd_bitmap_t bmp = APP_LCD_BitmapInit((u8*)bmp_array, 229, 16, 229, Is1BIT);
	tr5x6_flash_info_t slot;
	slot.bank=bank_num;
	slot.slot=0;
	TR5X6_FLASH_BankRead(&slot);
	APP_LCD_FColourSetRGB(slot.color);
	APP_LCD_BitmapPrintFormattedString(bmp, 0, 9, 3, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "%d-%s", bank_num+1, slot.name);

	//APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	//APP_LCD_FColourSet(APP_LCD_RED);
	for (int w=0; w<229;w++){
		u8* bmp_mem_ptr = bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, 16, 229, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, x_offset+95+w, y_offset+141);
	}

}
#endif

/////////////////////////////////////////////////////////////////////////////
// GRAPHIC ELEMENTS COMMON TO BOTH HOST MACHINES
//
// Each of these draws one element of the running screen, identically on the
// 505 and the 626. They take no argument: x_offset / y_offset / w_max are
// #define, and each element declares its own locals.
// Where an element is common except for a line or two, that line lives in a
// small helper of its own (see TFT_XferInstFlagsLatch) rather than splitting
// the element in half. A fix written here reaches both machines.
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Sound bank change is only allowed while the machine is showing its bank
// page. The rule is the same on both hosts - only the labels that mean "we
// are on that page" differ, because the two front panels do not carry the
// same legends.
// Returns the flag instead of writing it: these elements are meant to move
// to a file of their own, and nothing that leaves app.c may touch app.c's
// statics.
/////////////////////////////////////////////////////////////////////////////
static u8 TFT_BankInhibit(void)
{
	u8 on_bank_page;
#if TR5X6_UNIT_SELECT==505
	on_bank_page = tr5x6_decod_labels.grp_pat;
#else
	on_bank_page = tr5x6_decod_labels.measure || tr5x6_decod_labels.tempo;
#endif

	return on_bank_page ? 0 : 1;
}

/////////////////////////////////////////////////////////////////////////////
// The unit's own 7-segment digits, mirrored on the TFT. One digit per bit of
// tr5x6_decod_digits_flags, cleared as it is drawn.
/////////////////////////////////////////////////////////////////////////////
static void TFT_Digits(void)
{
	for(int i=0; i<tr5x6_digits_num; i++){
		if( tr5x6_decod_digits_flags & (1<<i) ){
			tr5x6_decod_digits_flags &= ~(1<<i);
			APP_LCD_Digits_draw(tr5x6_decod_digits[i], x_offset + tr5x6_decod_digits_pos[i], y_offset+197, 1, APP_LCD_WHITE, APP_LCD_BLACK);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// The beat indicator, driven by the host's MIDI clock: lit on the beat, dark
// off it. beat_color carries the transport state - red once a start or a
// continue has been seen, grey after a stop.
/////////////////////////////////////////////////////////////////////////////
static void TFT_Beat(void)
{
	if(beat_flag){
		if(beat_enabled)
			APP_LCD_DrawFastBeat(x_offset+w_max+5-(6*23), y_offset+145, beat_color);
		else
			APP_LCD_DrawFastBeat(x_offset+w_max+5-(6*23), y_offset+145, APP_LCD_BLACK);
		beat_flag=0;
	}
}

// Sound bank up/down, then read the bank name back from flash and show it.
static void TFT_BankSelect(void)
{
	// The two hosts do not trigger this the same way: the 505 acts on inc/dec
	// alone, the 626 wants INST held down at the same time - its front panel
	// gives those two buttons another job otherwise. The inhibit is honoured
	// on the 505 only; on the 626 it was commented out and stays that way,
	// its trigger already requiring INST.
	u8 trigger;
	u8 allowed;
#if TR5X6_UNIT_SELECT==505
	trigger = (tr5x6_decod_buttons_flags.inc & tr5x6_decod_buttons.inc)
	       || (tr5x6_decod_buttons_flags.dec & tr5x6_decod_buttons.dec);
	allowed = !bank_change_inhibit;
#else
	trigger = tr5x6_decod_buttons.inst
	       && ( (tr5x6_decod_buttons_flags.inc & tr5x6_decod_buttons.inc)
	         || (tr5x6_decod_buttons_flags.dec & tr5x6_decod_buttons.dec) );
	allowed = 1;
#endif

	if( !(trigger || first_start || tr5x6_sysex_bank_info_refresh) )
		return;

	if(allowed){
			u8 bank_num = TR5X6_ROM_BankGet();
			if((tr5x6_decod_buttons_flags.inc==1) && (tr5x6_decod_buttons.inc==1) ){
				bank_num +=1;
				bank_num &=(TR5X6_BANK_NUM-1);
				TR5X6_ROM_BankSet(bank_num, (tr5x6_xfer_state.STAT<=XFER_END)?1:0);
			}else if((tr5x6_decod_buttons_flags.dec==1) && (tr5x6_decod_buttons.dec==1) ){
				bank_num -=1;
				bank_num &=(TR5X6_BANK_NUM-1);
				TR5X6_ROM_BankSet(bank_num, (tr5x6_xfer_state.STAT<=XFER_END)?1:0);
			}

			bank_changed=1;
			tr5x6_flash_info_t slot;
			slot.bank=bank_num;
			slot.slot=0;
			TR5X6_FLASH_BankRead(&slot);
			APP_LCD_FColourSetRGB(slot.color);
			APP_LCD_PrintFormattedString(x_offset+104, y_offset+144, (10*23)-10, APP_LCD_STRING_ALIGN_LEFT, -32, "%d-%s", bank_num+1, slot.name);
	}

	// The trailing state, kept inside the trigger on BOTH hosts. On the 626 it
	// used to sit outside, so it ran on every pass of the display task even
	// when no button had been pressed.
#if TR5X6_UNIT_SELECT==505
	tr5x6_decod_buttons_flags.ALL = 0;
#else
	tr5x6_decod_buttons_flags.inc = 0;
	tr5x6_decod_buttons_flags.dec = 0;
#endif
	tr5x6_sysex_bank_info_refresh = 0;
	if(first_start)bank_changed=0;
	first_start=0;
}

// The ONE line the transfer state machine does not share: which selection
// flags get latched when a transfer ends. The 505 latches the accent bits,
// the 626 the instrument-select bits.
static void TFT_XferInstFlagsLatch(void)
{
#if TR5X6_UNIT_SELECT==505
	tr5x6_decod_inst_acc_flags = tr5x6_decod_inst_acc;
#else
	tr5x6_decod_inst_sel_flags = tr5x6_decod_inst_sel;
#endif
}


// Transfer state machine: consume the SysEx transfer flags, drive the
// progress window, and run the post-transfer display hold (xfer_delay).
static void TFT_XferRefresh(void)
{
	if(tr5x6_xfer_state.FLAG){

		if(tr5x6_xfer_state.FLAG_INFO){
			tr5x6_xfer_state.FLAG_INFO=0;
			xfer_flag=0;
			TFT_XferSlotInfoReceived();
			xfer_delay=0;
			//ADIOS_MIDI_SendDebugMessage("Info\n");
			//xfer_time_out=1000;
		}else if(tr5x6_xfer_state.FLAG_BEGIN){
			tr5x6_xfer_state.FLAG_BEGIN=0;
			xfer_flag=0;
			TFT_XferSlotProgress(0);
			xfer_delay=0;
			//ADIOS_MIDI_SendDebugMessage("Begin\n");
			//xfer_time_out=1000;

		}else if(tr5x6_xfer_state.FLAG_CONT){
			tr5x6_xfer_state.FLAG_CONT=0;
			xfer_flag=0;
			TFT_XferSlotProgress(MIDIO_SYSEX_Slot_Progression());
			xfer_delay=0;
			//ADIOS_MIDI_SendDebugMessage("get progress %u \n",MIDIO_SYSEX_BlockProgression() );
			//xfer_time_out=1000;
		}else if(tr5x6_xfer_state.FLAG_END){
			tr5x6_xfer_state.FLAG_END=0;
			///xfer_time_out=-1;
			//ADIOS_MIDI_SendDebugMessage("End detected\n");
			xfer_flag=0;
			if(tr5x6_xfer_state.STAT==XFER_INFO){
				xfer_delay=16;
				// slot info received messsage
			}
			else if(tr5x6_xfer_state.STAT==XFER_CONT){
				xfer_delay=16;
				TFT_XferSlotProgress(MIDIO_SYSEX_Slot_Progression());
			}
			else xfer_delay=0;
			TFT_XferInstFlagsLatch();
			tr5x6_xfer_state.STAT=XFER_END;
		}else if(tr5x6_xfer_state.FLAG_ERROR){
			tr5x6_xfer_state.FLAG_ERROR=0;
			//xfer_time_out=-1;
			//ADIOS_MIDI_SendDebugMessage("xfer error\n");
			xfer_flag=0;
			TFT_XferError(0);	// default error, toDo: Error handling
			TFT_XferInstFlagsLatch();
			xfer_delay=40;
		}

	}

	if( ((tr5x6_xfer_state.STAT==XFER_END) || (tr5x6_xfer_state.STAT==XFER_ERROR)) && (xfer_delay>0) ){
		xfer_delay--;
		//ADIOS_MIDI_SendDebugMessage("delay end %u\n", xfer_delay);
		if(xfer_delay==0){
			tr5x6_xfer_state.STAT=XFER_IDLE;
			xfer_flag=1;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// true when nothing of a transfer is left on screen
//
// The transfer window and the slot info are the same rectangle, so the slot
// info waits until the transfer has finished showing itself.
/////////////////////////////////////////////////////////////////////////////
static u8 TFT_XferIsIdle(void)
{
	return (xfer_delay==0) && (tr5x6_xfer_state.STAT==XFER_IDLE);
}

/////////////////////////////////////////////////////////////////////////////
// prints the name of the selected instrument
//
// The gate diverges: the 505 watches the ACCENT bits, the 626 the blocked
// bits. The 505 also has a case the 626 has not - ACCENT held down puts its
// own caption there instead of an instrument name.
/////////////////////////////////////////////////////////////////////////////
static void TFT_InstSelect(void)
{
	u8 draw;
#if TR5X6_UNIT_SELECT==505
	if( (tr5x6_decod_inst_acc_flags&0x1) & (tr5x6_decod_inst_acc&0x1) ){
		APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);
		APP_LCD_PrintString(x_offset+104, y_offset+111, 11*23-10, APP_LCD_STRING_ALIGN_LEFT, -32, "ACCENT");
		return;
	}
	draw = ((tr5x6_decod_inst_acc_flags&0x2) & (tr5x6_decod_inst_acc&0x2))
	    || (tr5x6_decod_inst_sel_flags & tr5x6_decod_inst_sel)
	    || bank_changed;
#else
	draw = (tr5x6_decod_inst_sel_flags & tr5x6_decod_inst_sel)
	    || (tr5x6_decod_inst_blk_flags & tr5x6_decod_inst_blk)
	    || bank_changed;
#endif
	if(!draw) return;

	for(int i=0; i<16; i++){
		if(tr5x6_decod_inst_sel &(1<<i)){
			u8 inst_num = i;
#if TR5X6_UNIT_SELECT==626
			// a blocked instrument lives 16 slots higher
			if(tr5x6_decod_inst_blk &(1<<i))inst_num +=16;
			inst_grid_sel = i;	// remembered as the grid view s starting blink cell
#endif
			APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);
			tr5x6_flash_info_t slot;
			slot.bank=TR5X6_ROM_BankGet();
			slot.slot=inst_num;
			TR5X6_FLASH_SlotRead(&slot);
			APP_LCD_FColourSetRGB(slot.color);
			APP_LCD_PrintFormattedString(x_offset+104, y_offset+111, 11*23-10, APP_LCD_STRING_ALIGN_LEFT, -32, "%d-%s", inst_num+1, slot.name);
			break;
		}
	}
}

// Last-step numbers and step dots across the 16 steps.
static void TFT_Steps(void)
{
	// last step/ dot steps
	for(int i=0; i<16; i++){
		if(tr5x6_decod_last_step_flags &(1<<i)){
			if(tr5x6_decod_last_step &(1<<i))APP_LCD_FColourSet(APP_LCD_WHITE);
			else APP_LCD_FColourSet(APP_LCD_DARKGREY);
			APP_LCD_PrintFormattedString(x_offset+w_max-358+i*23, y_offset+71, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "%d", (i+1));
		}
		if(tr5x6_decod_step_dots_flags &(1<<i) || tr5x6_decod_last_step_flags &(1<<i)){
			APP_LCD_DrawFastBall(x_offset+w_max-364+i*23, y_offset+46, (tr5x6_decod_step_dots &(1<<i))?((tr5x6_decod_last_step &(1<<i))?APP_LCD_WHITE:APP_LCD_DARKGREY):APP_LCD_BLACK);
		}
	}
	tr5x6_decod_last_step_flags = 0x0000;
	tr5x6_decod_step_dots_flags = 0x0000;
}

// The musical staff: 1/32, 1/16 and their triplets.
static void TFT_Scale(void)
{
	// Scale
	if(tr5x6_decod_scale_flag){
		tr5x6_decod_scale_flag=0;
		u16 w = 16*23-1;
		u16 scale_y_offset=y_offset +8;

		switch(tr5x6_decod_scale){
		case 0x00:
		case 0x02:
		case 0x07:
		case 0x0e:
			APP_LCD_Rectangle(x_offset+w_max-w, scale_y_offset, w-2, 23, 0, 0, 1, 0);
			break;
		default:
		}
		switch(tr5x6_decod_scale){
		case 0x00:
			APP_LCD_PrintString(x_offset+47, y_offset+23, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "  1/32  ");
			break;
		case 0x02:
			APP_LCD_PrintString(x_offset+47, y_offset+23, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "  1/16  ");
			break;
		case 0x07:
			APP_LCD_PrintString(x_offset+47, y_offset+23, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "1/32T");
			break;
		case 0x0e:
			APP_LCD_PrintString(x_offset+47, y_offset+23, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "1/16T");
			break;
		default:
		}
		switch(tr5x6_decod_scale){
		case 0x02:
			APP_LCD_DrawFastVLine(x_offset+w_max-4*23+11, scale_y_offset+5, 13, APP_LCD_WHITE);
			APP_LCD_DrawFastNoire(x_offset+w_max-4*23+7, scale_y_offset+18, APP_LCD_WHITE);
			APP_LCD_DrawFastVLine(x_offset+w_max-12*23+11, scale_y_offset+5, 13, APP_LCD_WHITE);
			APP_LCD_DrawFastNoire(x_offset+w_max-12*23+7, scale_y_offset+18, APP_LCD_WHITE);
		case 0x00:
			APP_LCD_DrawFastVLine(x_offset+w_max-8*23+11, scale_y_offset+5, 13, APP_LCD_WHITE);
			APP_LCD_DrawFastNoire(x_offset+w_max-8*23+7, scale_y_offset+18, APP_LCD_WHITE);
			APP_LCD_DrawFastVLine(x_offset+w_max-16*23+11, scale_y_offset+5, 13, APP_LCD_WHITE);
			APP_LCD_DrawFastNoire(x_offset+w_max-16*23+7, scale_y_offset+18, APP_LCD_WHITE);
			break;
		case 0x07:
			for(int i=0; i<2; i++){
				APP_LCD_DrawPixel(x_offset+w_max-(10+i*6)*23+6, scale_y_offset+1, APP_LCD_WHITE);
				APP_LCD_DrawFastHLine(x_offset+w_max-(10+i*6)*23+7, scale_y_offset, 6*23-13, APP_LCD_WHITE);
				APP_LCD_DrawPixel(x_offset+w_max-(4+i*6)*23-6, scale_y_offset+1, APP_LCD_WHITE);
			}
			APP_LCD_DrawPixel(x_offset+w_max-4*23+6, scale_y_offset+1, APP_LCD_WHITE);
			APP_LCD_DrawFastHLine(x_offset+w_max-4*23+7, scale_y_offset, 4*23-13, APP_LCD_WHITE);
			for(int i=1; i<=8; i++){
				APP_LCD_DrawFastCroche(x_offset+w_max-i*46+11, scale_y_offset+5, APP_LCD_WHITE);
				APP_LCD_DrawFastVLine(x_offset+w_max-i*46+10, scale_y_offset+5, 13, APP_LCD_WHITE);
				APP_LCD_DrawFastNoire(x_offset+w_max-i*46+6, scale_y_offset+18, APP_LCD_WHITE);
			}

			break;
		case 0x0e:
			for(int i=0; i<5; i++){
				APP_LCD_DrawPixel(x_offset+w_max-(4+i*3)*23+6, scale_y_offset+1, APP_LCD_WHITE);
				APP_LCD_DrawFastHLine(x_offset+w_max-(4+i*3)*23+7, scale_y_offset, 3*23-13, APP_LCD_WHITE);
				APP_LCD_DrawPixel(x_offset+w_max-(1+i*3)*23-6, scale_y_offset+1, APP_LCD_WHITE);
			}
			APP_LCD_DrawPixel(x_offset+w_max-1*23+6, scale_y_offset+1, APP_LCD_WHITE);
			APP_LCD_DrawFastHLine(x_offset+w_max-1*23+7, scale_y_offset, 1*23-13, APP_LCD_WHITE);
			for(int i=1; i<=16; i++){
				APP_LCD_DrawFastCroche(x_offset+w_max-i*23+11, scale_y_offset+5, APP_LCD_WHITE);
				APP_LCD_DrawFastVLine(x_offset+w_max-i*23+10, scale_y_offset+5, 13, APP_LCD_WHITE);
				APP_LCD_DrawFastNoire(x_offset+w_max-i*23+6, scale_y_offset+18, APP_LCD_WHITE);
			}

			break;
		default:
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// prints the front panel labels
//
// The top strip is where the two hosts truly part: not the same label sets,
// not the same layout logic. The 505 lights two independent zones (TEMPO/
// MEASURE and GROUP/PATTERN/LEVEL); the 626 lights ONE of seven labels,
// exclusively. SYNC differs too - the 505 erases it when off, the 626 greys
// it. CHAIN and BLOCK are the same logic on both hosts and only their
// position moves: the erase rectangle always sits at label_x-20, width 40,
// so the whole placement reduces to three numbers set before the #if.
/////////////////////////////////////////////////////////////////////////////
static void TFT_Labels(void)
{
	u16 chain_x, block_x, onoff_y;
#if TR5X6_UNIT_SELECT==505
	chain_x=232; block_x=278; onoff_y=262;

	if(tr5x6_decod_labels_flags.tempo || tr5x6_decod_labels_flags.measure){
		APP_LCD_Rectangle(x_offset+93, y_offset+173, 58, 10, 0, 0, 1, 0);
		if(tr5x6_decod_labels.tempo)
			APP_LCD_PrintString(x_offset+122, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "TEMPO");
		if(tr5x6_decod_labels.measure)
			APP_LCD_PrintString(x_offset+122, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "MEASURE");
	}
	if(tr5x6_decod_labels_flags.grp_pat || tr5x6_decod_labels_flags.level){
		APP_LCD_Rectangle(x_offset+202, y_offset+173, 107, 10, 0, 0, 1, 0);
		if(tr5x6_decod_labels.grp_pat){
			APP_LCD_PrintString(x_offset+202, y_offset+173, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "GROUP");
			APP_LCD_PrintString(x_offset+309, y_offset+173, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "PATTERN");}
		if(tr5x6_decod_labels.level)
			APP_LCD_PrintString(x_offset+309, y_offset+173, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, "LEVEL");
	}
	if(tr5x6_decod_labels_flags.sync){
		if(tr5x6_decod_labels.sync)
			APP_LCD_PrintString(x_offset+122, y_offset+262, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "MIDI  SYNC");
		else APP_LCD_Rectangle(x_offset+82, y_offset+262, 80, 10, 0, 0, 1, 0);
	}
#else
	chain_x=262; block_x=308; onoff_y=250;

	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	if(tr5x6_decod_labels_flags.ALL &0x7f){
		APP_LCD_Rectangle(x_offset+140, y_offset+173, 60, 10, 0, 0, 1, 0);
		if(tr5x6_decod_labels.tempo)
			APP_LCD_PrintString(x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "TEMPO");
		else if(tr5x6_decod_labels.measure)
			APP_LCD_PrintString(x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "MEASURE");
		else if(tr5x6_decod_labels.pitch)
			APP_LCD_PrintString(x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "PITCH");
		else if(tr5x6_decod_labels.level)
			APP_LCD_PrintString(x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "LEVEL");
		else if(tr5x6_decod_labels.shuffle)
			APP_LCD_PrintString(x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "SHUFFLE");
		else if(tr5x6_decod_labels.flam)
			APP_LCD_PrintString(x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "FLAM");
		else if(tr5x6_decod_labels.accent)
			APP_LCD_PrintString( x_offset+170, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "ACCENT");
	}
	if(tr5x6_decod_labels_flags.sync){
		if(tr5x6_decod_labels.sync){
			APP_LCD_FColourSet(APP_LCD_WHITE);
			APP_LCD_PrintString( x_offset+404, y_offset+236, 0, APP_LCD_STRING_ALIGN_CENTER, -32, " EXT SYNC ON ");
		}else {
			APP_LCD_FColourSet(APP_LCD_DARKGREY);
			APP_LCD_PrintString(x_offset+404, y_offset+236, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "EXT SYNC OFF");
			APP_LCD_FColourSet(APP_LCD_WHITE);
		}
	}
#endif

	// CHAIN and BLOCK, common to both hosts - only the three numbers above move
	if(tr5x6_decod_labels_flags.chain){
		if(tr5x6_decod_labels.chain)
			APP_LCD_PrintString(x_offset+chain_x, y_offset+onoff_y, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "CHAIN");
		else APP_LCD_Rectangle(x_offset+chain_x-20, y_offset+onoff_y, 40, 10, 0, 0, 1, 0);
	}
	if(tr5x6_decod_labels_flags.block){
		if(tr5x6_decod_labels.block)
			APP_LCD_PrintString(x_offset+block_x, y_offset+onoff_y, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "BLOCK");
		else APP_LCD_Rectangle(x_offset+block_x-20, y_offset+onoff_y, 40, 10, 0, 0, 1, 0);
	}
	tr5x6_decod_labels_flags.ALL = 0;
}


/////////////////////////////////////////////////////////////////////////////
// prints the sequencer mode - two stacked words, TRACK PLAY to TAP WRITE
//
// Same ten cases on both hosts, only the anchor moves (+11,-5 on the 626).
// Every coordinate derives from it: erase rectangles at (0,0) and (+13,+17),
// text lines at (+38,+1) and (+38,+18).
/////////////////////////////////////////////////////////////////////////////
static void TFT_Mode(void)
{
	u16 mx, my;
#if TR5X6_UNIT_SELECT==505
	mx=355; my=200;
#else
	mx=366; my=195;
#endif

	if(tr5x6_decod_mode_flag){
		tr5x6_decod_mode_flag=0;
		switch(tr5x6_decod_mode){
		case 0x01:
		case 0x02:
		case 0x04:
		case 0x08:
		case 0x10:
			APP_LCD_Rectangle(x_offset+mx, y_offset+my, 76, 16, 0, 0, 1, 0);
			APP_LCD_Rectangle(x_offset+mx+13, y_offset+my+17, 51, 16, 0, 0, 1, 0);
			break;
		default:
			break;
		}
		switch(tr5x6_decod_mode){
		case 0x01:
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+1,  0, APP_LCD_STRING_ALIGN_CENTER, -32, "TRACK");
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+18, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "PLAY");
			break;
		case 0x02:
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+1,  0, APP_LCD_STRING_ALIGN_CENTER, -32, "PATTERN");
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+18, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "PLAY");
			break;
		case 0x04:
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+1,  0, APP_LCD_STRING_ALIGN_CENTER, -32, "TRACK");
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+18, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "WRITE");
			break;
		case 0x08:
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+1,  0, APP_LCD_STRING_ALIGN_CENTER, -32, "STEP");
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+18, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "WRITE");
			break;
		case 0x10:
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+1,  0, APP_LCD_STRING_ALIGN_CENTER, -32, "TAP");
			APP_LCD_PrintString(x_offset+mx+38, y_offset+my+18, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "WRITE");
			break;
		default:
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// prints the pattern group letter A-F
//
// Same six cases on both hosts, anchor moved (+40,+4) on the 626. The erase
// rectangle sits at (anchor-9, anchor-1) on both.
/////////////////////////////////////////////////////////////////////////////
static void TFT_Group(void)
{
	u16 gx, gy;
#if TR5X6_UNIT_SELECT==505
	gx=221; gy=201;
#else
	gx=261; gy=205;
#endif

	if(tr5x6_decod_group_flag){
		tr5x6_decod_group_flag=0;
		switch(tr5x6_decod_group){
		case 0x01:
			APP_LCD_PrintString(x_offset+gx, y_offset+gy, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "A");
			break;
		case 0x02:
			APP_LCD_PrintString(x_offset+gx, y_offset+gy, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "B");
			break;
		case 0x04:
			APP_LCD_PrintString(x_offset+gx, y_offset+gy, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "C");
			break;
		case 0x08:
			APP_LCD_PrintString(x_offset+gx, y_offset+gy, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "D");
			break;
		case 0x10:
			APP_LCD_PrintString(x_offset+gx, y_offset+gy, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "E");
			break;
		case 0x20:
			APP_LCD_PrintString(x_offset+gx, y_offset+gy, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "F");
			break;
		default:
			APP_LCD_Rectangle(x_offset+gx-9, y_offset+gy-1, 20, 13, 0, 0, 1, 0);
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// the three MIDI activity indicators
//
// Each indicator names the port it watches, so this survives any future
// UART renumbering:
//   DIN2 (USART3) = the instrument's MIDI IN
//   DIN0 (USART1) = the host link
//   MIDIO_SYSEX_Act() = application-level SysEx transfers
// Same ports, same logic on both hosts - the y positions are even identical,
// only two x anchors move.
/////////////////////////////////////////////////////////////////////////////
static void TFT_MidiActivity(void)
{
	u16 in_x, hs_x;
#if TR5X6_UNIT_SELECT==505
	in_x=335; hs_x=441;
#else
	in_x=350; hs_x=446;
#endif

	u8 act_midi_in = ADIOS_MIDI_ActGet(DIN2) & ADIOS_MIDI_ACT_RX;
	u8 act_host    = ADIOS_MIDI_ActGet(DIN0) & ADIOS_MIDI_ACT_RX;
	u8 act_sysex   = MIDIO_SYSEX_Act() & 0x2;
	if(act_midi_in)APP_LCD_Rectangle(x_offset+in_x, y_offset+262, 11, 11, 1, APP_LCD_DARKGREY, 2, APP_LCD_RED);
	else APP_LCD_Rectangle(x_offset+in_x, y_offset+262, 11, 11, 1, APP_LCD_DARKGREY, 1, 0);
	if(act_host)APP_LCD_Rectangle(x_offset+hs_x, y_offset+249, 11, 11, 1, APP_LCD_DARKGREY, 2, APP_LCD_RED);
	else APP_LCD_Rectangle(x_offset+hs_x, y_offset+249, 11, 11, 1, APP_LCD_DARKGREY, 1, 0);
	if(act_sysex)APP_LCD_Rectangle(x_offset+hs_x, y_offset+262, 11, 11, 1, APP_LCD_DARKGREY, 2, APP_LCD_RED);
	else APP_LCD_Rectangle(x_offset+hs_x, y_offset+262, 11, 11, 1, APP_LCD_DARKGREY, 1, 0);
}
/////////////////////////////////////////////////////////////////////////////
// periodically called for screen print
/////////////////////////////////////////////////////////////////////////////
static void TASK_TFT_Periodic(void *pvParameters)
{
	portTickType xLastExecutionTime;

	// Initialise the xLastExecutionTime variable on task entry
	xLastExecutionTime = xTaskGetTickCount();

	while( 1 ) {
		// wait for 40 mS
		vTaskDelayUntil(&xLastExecutionTime, 40 / portTICK_RATE_MS);
		if(!APP_LCD_IsReady())return;
		if(first_start)APP_TFT_Background();	// prints the background
		if(normal_start){
			// check Bank Inhibit
			bank_change_inhibit = TFT_BankInhibit();
			// Digits
			TFT_Digits();
			// Heart Beat
			TFT_Beat();
			// Bank select
			APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
			APP_LCD_FColourSet(APP_LCD_WHITE);
			TFT_BankSelect();
			// Check for ROM Transfer state
			TFT_XferRefresh();
			// Instruments select - the ONE island the two hosts do not
			// share: the 626 runs it through its 16-instrument grid, the
			// 505 has no grid and runs the sequence directly.
#if TR5X6_UNIT_SELECT==505
			// Slot Info
			if(TFT_XferIsIdle())TFT_Slotinfo();
			// Instrument Select
			TFT_InstSelect();

			bank_changed = 0;
			tr5x6_decod_inst_acc_flags=0;
			tr5x6_decod_inst_sel_flags = 0x0000;
#else
			// How much instruments shown
			u8 num_sel=0;
			for(int i=0; i<16; i++)if(tr5x6_decod_inst_sel & (1<<i))num_sel++;

			if((num_sel==16) && TFT_XferIsIdle()){
				// prints the Instrument Grid
				TFT_InstGridDraw();

			} else if(num_sel==1){
				// prints the Instrument selection and slot info
				if(inst_change_exit){
					inst_grid_shown=0;
					inst_change_exit=0;
					tr5x6_decod_inst_sel_flags=tr5x6_decod_inst_sel;
					APP_LCD_Rectangle(x_offset+95, y_offset+95, 367, 41, 0, 0, 1, 0);
					APP_LCD_DrawFastVLine(x_offset+w_max-(5*23)-2, y_offset+95,  69, APP_LCD_DARKGREY);
				}

				if(TFT_XferIsIdle())TFT_Slotinfo();
				TFT_InstSelect();
			}
			// flags reset
			bank_changed = 0;
			tr5x6_decod_inst_sel_flags=0;
			tr5x6_decod_inst_blk_flags=0;
#endif
			// Steps
			APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
			APP_LCD_FColourSet(APP_LCD_WHITE);
			TFT_Steps();
			// Scale
			APP_LCD_FColourSet(APP_LCD_WHITE);
			TFT_Scale();
			// Labels
			TFT_Labels();
			// Mode
			APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);
			TFT_Mode();
			// Group
			TFT_Group();
			// MIDI activity
			TFT_MidiActivity();

			APP_LCD_FColourSet(APP_LCD_WHITE);
		}
	}
}

// EXTI calback must be placed here because they are common
void EXTI4_15_IRQHandler(void){
	// callback for DECOD function
	TR5X6_DECOD_EXTI_LCD_Callback();
}
void EXTI2_3_IRQHandler(void){
	// callback for DECOD function
	TR5X6_DECOD_EXTI_BUTT_Callback();
	//if(EXTI->FPR1 & EXTI_FPR1_FPIF3)
	//{
		//ADIOS_SOL_Set();

	//	tr5x6_decod_buttons.ALL=(u8)(GPIOD->IDR & 0xf);
		//if(tr5x6_decod_buttons.ALL)
//		tr5x6_decod_buttons.last = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_LAST)?0:1;
//		//tr5x6_decod_buttons.inst = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_INST)?0:1;
//		tr5x6_decod_buttons.inc = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_INC)?0:1;
//		tr5x6_decod_buttons.dec = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_DEC)?0:1;
//		//s32 led =  ADIOS_BOARD_LED_Get();

		//LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_3);
	//	EXTI->FPR1 |=EXTI_FPR1_FPIF3;
		//ADIOS_SOL_Clr();
	//}
}

s32 NOTIFY_MIDI_TimeOut(adios_midi_port_t port){
	ADIOS_MIDI_SendDebugMessage("midi time out!\n");
	return 0;
}


void SettingsMenu_Draw(void){
	APP_LCD_BColourSet(APP_LCD_BLACK);
	APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);

	if(menu_edit){
		if(menu_pos==0){
			APP_LCD_FColourSet(APP_LCD_WHITE);
			APP_LCD_PrintString(240, y_offset+10, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Settings   Menu");
			APP_LCD_PrintFormattedString(250, y_offset+40, 40,  APP_LCD_STRING_ALIGN_LEFT, -32, "%d", curr_id);
			APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
			APP_LCD_PrintString(120, y_offset+40, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Device   ID:");
			APP_LCD_PrintString(120, y_offset+60, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Format   BANKs");
			APP_LCD_FColourSet(APP_LCD_WHITE);
		}else if(menu_pos==1){
			APP_LCD_Clear();	// clear the TFT
			APP_LCD_PrintString(240, y_offset+10, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Format   BANKs");
			APP_LCD_FColourSet(APP_LCD_RED);
			APP_LCD_PrintString(240, y_offset+40, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Format  will  erase  all  the  Banks!");
			APP_LCD_PrintString(240, y_offset+60, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Please  backup  your  Banks  first...");
			APP_LCD_FColourSet(APP_LCD_WHITE);
			APP_LCD_PrintString(240, y_offset+100, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "It  will  take  around  6  minutes.");
			APP_LCD_PrintString(240, y_offset+120, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Are you sure?");
		}else if(menu_pos==2){
			APP_LCD_Clear();	// clear the TFT
			//APP_LCD_PrintString(bmp, 0, 240, y_offset+10, 0, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "About");
			About_Page();
		}

	}else{
		if(menu_pos==0){
			APP_LCD_FColourSet(APP_LCD_WHITE);
			APP_LCD_Rectangle(109, y_offset+40, 7, 51, 0, 0, 1, 0);
			APP_LCD_PrintString(116, y_offset+40, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, ">");
			APP_LCD_PrintString(120, y_offset+40, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Device   ID:");
			APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
			APP_LCD_PrintFormattedString(250, y_offset+40, 40,  APP_LCD_STRING_ALIGN_LEFT, -32, "%d", curr_id);
			APP_LCD_PrintString(120, y_offset+60, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Format   BANKs");
			APP_LCD_PrintString(120, y_offset+80, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "About");
			APP_LCD_FColourSet(APP_LCD_WHITE);
		}else if(menu_pos==1){
			APP_LCD_FColourSet(APP_LCD_WHITE);
			APP_LCD_Rectangle(109, y_offset+40, 7, 51, 0, 0, 1, 0);
			APP_LCD_PrintString(116, y_offset+60, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, ">");
			APP_LCD_PrintString(120, y_offset+60, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Format   BANKs");
			APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
			APP_LCD_PrintString(120, y_offset+40, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Device   ID:");
			APP_LCD_PrintFormattedString(250, y_offset+40, 40,  APP_LCD_STRING_ALIGN_LEFT, -32, "%d", curr_id);
			APP_LCD_PrintString(120, y_offset+80, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "About");
			APP_LCD_FColourSet(APP_LCD_WHITE);
		}else if(menu_pos==2){
			APP_LCD_FColourSet(APP_LCD_WHITE);
			APP_LCD_Rectangle(109, y_offset+40, 7, 51, 0, 0, 1, 0);
			APP_LCD_PrintString(116, y_offset+80, 0, APP_LCD_STRING_ALIGN_RIGHT, -32, ">");
			APP_LCD_PrintString(120, y_offset+80, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "About");
			APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
			APP_LCD_PrintString(120, y_offset+60, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Format   BANKs");
			APP_LCD_PrintString(120, y_offset+40, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Device   ID:");
			APP_LCD_PrintFormattedString(250, y_offset+40, 40,  APP_LCD_STRING_ALIGN_LEFT, -32, "%d", curr_id);
		}
		APP_LCD_PrintString(240, y_offset+10, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Settings   Menu");
	}
}




void Legend_Draw(int x, int y, const char *func, const char *butt){
	APP_LCD_BColourSet(APP_LCD_BLACK);
	APP_LCD_FColourSet(APP_LCD_WHITE);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_PrintFormattedString(x-2, y+2, 56, APP_LCD_STRING_ALIGN_RIGHT, -32, func);
	APP_LCD_PrintString(x+3, y+2, 0, APP_LCD_STRING_ALIGN_LEFT, -32, butt);
	u8 func_len = (u8)APP_LCD_StringKernGet(-32, butt) + 5;
	APP_LCD_Rectangle(x, y, func_len, 15, 1, APP_LCD_LIGHTGREY, 0, 0);
}

void SettingsMenu_Legend(void){

	if(formatting){
		Legend_Draw(x_offset+420, y_offset+32, "EXEC", "INST");
		Legend_Draw(x_offset+420, y_offset+12, "EXIT", "LAST");
	}else{
		if(menu_edit){
			if(menu_pos==0){
				Legend_Draw(x_offset+420, y_offset+52, "INC", "UP");
				Legend_Draw(x_offset+420, y_offset+72, "DEC", "DOWN");
				Legend_Draw(x_offset+420, y_offset+32, "ENTER", "INST");
				Legend_Draw(x_offset+420, y_offset+12, "CANCEL", "LAST");
			}else if(menu_pos==1){
				Legend_Draw(x_offset+160, y_offset+150, "NO", "LAST");
				Legend_Draw(x_offset+290, y_offset+150, "YES", "INST");
			}else if(menu_pos==2){
				Legend_Draw(x_offset+420, y_offset+12, "ESC", "LAST");
			}

		}else{
			Legend_Draw(x_offset+420, y_offset+52, "  UP", "UP");
			Legend_Draw(x_offset+420, y_offset+72, "DOWN", "DOWN");
			if(menu_pos==0){
				Legend_Draw(x_offset+420, y_offset+32, " EDIT", "INST");

			}else if(menu_pos==1){
				Legend_Draw(x_offset+420, y_offset+32, "EXEC", "INST");
			}else if(menu_pos==2){
				Legend_Draw(x_offset+420, y_offset+32, "EXEC", "INST");
			}
			Legend_Draw(x_offset+420, y_offset+12, "EXIT", "LAST");
		}
	}

}

void Formatting_Page(void){
	APP_LCD_BColourSet(APP_LCD_BLACK);
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_Rectangle(0, 40, 480, 200, 0, 0, 2, APP_LCD_BLACK);
	APP_LCD_PrintString(92, y_offset+50, 0, APP_LCD_STRING_ALIGN_LEFT, -32, "Formatting   ROM...");

	TR5X6_MEM_Format();

}

void About_Page(void)
{
	APP_LCD_Clear();	// clear the TFT
	APP_LCD_BColourSet(APP_LCD_BLACK);
	APP_LCD_FColourSet(APP_LCD_WHITE);
	//APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
	adios_lcd_bitmap_t logo_bmp = APP_LCD_BitmapInit((u8*)tr5x6_logo, 400, 56, 400, Is1BIT);
	for (int w=0; w<400;w++){
		u8* bmp_mem_ptr = logo_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, 56, 400, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, 40+w, y_offset+25);
	}
	APP_LCD_FontInit((u8*)GLCD_FONT_9BITRPR, Is1BIT);
	APP_LCD_PrintString(40, y_offset+82, 0, APP_LCD_STRING_ALIGN_LEFT, -32, (char*)TR5X6_VERSION);

	logo_bmp = APP_LCD_BitmapInit((u8*)ai_logo, 49, 32, 49, Is1BIT);
	for (int w=0; w<49;w++){
		u8* bmp_mem_ptr = logo_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, 32, 49, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, 90+w, y_offset+110);
	}
	logo_bmp = APP_LCD_BitmapInit((u8*)ai_name, 246, 24, 246, Is1BIT);
	for (int w=0; w<246;w++){
		u8* bmp_mem_ptr = logo_bmp.memory +w;
		adios_lcd_bitmap_t bmp2print = APP_LCD_BitmapInit(bmp_mem_ptr, 1, 24, 246, Is1BIT);
		APP_LCD_SendBitmap(bmp2print, 144+w, y_offset+114);
	}
	APP_LCD_PrintString(240, y_offset+140, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "(C)2024 B.Dupeyron");

	APP_LCD_FontInit((u8*)GLCD_FONT_PIXEL12X10, Is1BIT);
	APP_LCD_PrintString(240, y_offset+173, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Special   thanks   to:");
	APP_LCD_DrawFastHLine(156, y_offset+187, 11, APP_LCD_WHITE);
	APP_LCD_DrawFastHLine(171, y_offset+187, 152, APP_LCD_WHITE);
	//APP_LCD_FColourSet(APP_LCD_LIGHTGREY);
	APP_LCD_PrintString(240, y_offset+195, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "David,  Sunshine");
	APP_LCD_PrintString(240, y_offset+215, 0, APP_LCD_STRING_ALIGN_CENTER, -32, "Harry,  Cedric");
	APP_LCD_PrintString(240, y_offset+235, 0,  APP_LCD_STRING_ALIGN_CENTER, -32, "and   Michael.");

}
void TASK_SettingsMenu(void *pvParameters){
	portTickType xLastExecutionTime;

	// Initialise the xLastExecutionTime variable on task entry
	xLastExecutionTime = xTaskGetTickCount();

	while( 1 ) {
		// wait for 40 mS
		vTaskDelayUntil(&xLastExecutionTime, 40 / portTICK_RATE_MS);
		if(formatting){
			if(tr5x6_decod_buttons.inc && tr5x6_decod_buttons_flags.inc){
				tr5x6_decod_buttons_flags.inc = 0;
			}else if(tr5x6_decod_buttons.dec && tr5x6_decod_buttons_flags.dec){
				tr5x6_decod_buttons_flags.dec = 0;
			}else if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst){
				formatting=0;
				menu_edit=0;
				APP_LCD_Clear();	// clear the TFT
				SettingsMenu_Draw();
				SettingsMenu_Legend();
				tr5x6_decod_buttons_flags.inst=0;
			}else if(tr5x6_decod_buttons.last && tr5x6_decod_buttons_flags.last){
				ADIOS_SYS_Reset();
			}
		}else if(menu_edit){
			if(menu_pos==0){
				if(tr5x6_decod_buttons.inc && tr5x6_decod_buttons_flags.inc){
					curr_id = (curr_id+1) & 0x7f;
					SettingsMenu_Draw();
					SettingsMenu_Legend();
					tr5x6_decod_buttons_flags.inc = 0;
				}else if(tr5x6_decod_buttons.dec && tr5x6_decod_buttons_flags.dec){
					curr_id = (curr_id-1) & 0x7f;
					SettingsMenu_Draw();
					SettingsMenu_Legend();
					tr5x6_decod_buttons_flags.dec = 0;
				}else if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst){
					last_id = curr_id;
					ADIOS_MIDI_DeviceIDSet(curr_id);
					// ...and make it stick. DeviceIDSet only touches a RAM
					// variable, so until now a new ID was forgotten at the
					// next power-up and the menu was purely cosmetic.
					TR5X6_ROM_DeviceIDStore(curr_id);
					menu_edit=0;
					SettingsMenu_Draw();
					SettingsMenu_Legend();
					tr5x6_decod_buttons_flags.inst=0;
				}else if(tr5x6_decod_buttons.last && tr5x6_decod_buttons_flags.last){
					curr_id= last_id;
					ADIOS_MIDI_DeviceIDSet(curr_id);
					menu_edit=0;
					SettingsMenu_Draw();
					SettingsMenu_Legend();
					tr5x6_decod_buttons_flags.last=0;
				}
			}else if(menu_pos==1){
				if(tr5x6_decod_buttons.inc && tr5x6_decod_buttons_flags.inc){
					tr5x6_decod_buttons_flags.inc = 0;
				}else if(tr5x6_decod_buttons.dec && tr5x6_decod_buttons_flags.dec){
					tr5x6_decod_buttons_flags.dec = 0;
				}else if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst){
					formatting = 1;
					//vTaskSuspend(xSettings);
					Formatting_Page();
					SettingsMenu_Legend();
					//vTaskResume(xSettings);
					tr5x6_decod_buttons_flags.inst=0;
				}else if(tr5x6_decod_buttons.last && tr5x6_decod_buttons_flags.last){
					APP_LCD_Clear();	// clear the TFT
					menu_edit=0;
					SettingsMenu_Draw();
					SettingsMenu_Legend();
					tr5x6_decod_buttons_flags.last=0;
				}
			}else if(menu_pos==2){
				if(tr5x6_decod_buttons.inc && tr5x6_decod_buttons_flags.inc){
					tr5x6_decod_buttons_flags.inc = 0;
				}else if(tr5x6_decod_buttons.dec && tr5x6_decod_buttons_flags.dec){
					tr5x6_decod_buttons_flags.dec = 0;
				}else if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst){
					tr5x6_decod_buttons_flags.inst=0;
				}else if(tr5x6_decod_buttons.last && tr5x6_decod_buttons_flags.last){
					APP_LCD_Clear();	// clear the TFT
					menu_edit=0;
					SettingsMenu_Draw();
					SettingsMenu_Legend();
					tr5x6_decod_buttons_flags.last=0;
				}
			}

		}else{
			if(tr5x6_decod_buttons.inc && tr5x6_decod_buttons_flags.inc){
				menu_pos += 1;
				if(menu_pos>2) menu_pos= 0;
				SettingsMenu_Draw();
				SettingsMenu_Legend();
				tr5x6_decod_buttons_flags.inc = 0;
			}else if(tr5x6_decod_buttons.dec && tr5x6_decod_buttons_flags.dec){
				menu_pos -= 1;
				if(menu_pos>2) menu_pos= 2;
				SettingsMenu_Draw();
				SettingsMenu_Legend();
				tr5x6_decod_buttons_flags.dec = 0;
			}else if(tr5x6_decod_buttons.inst && tr5x6_decod_buttons_flags.inst){
				menu_edit=1;
				last_id = curr_id;
				SettingsMenu_Draw();
				SettingsMenu_Legend();
				tr5x6_decod_buttons_flags.inst=0;
			}else if(tr5x6_decod_buttons.last && tr5x6_decod_buttons_flags.last){
				ADIOS_SYS_Reset();
			}
		}
		//MUTEX_SPI_GIVE;
	}
}
