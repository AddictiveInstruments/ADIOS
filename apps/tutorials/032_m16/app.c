// $Id: app.c 1920 2014-01-08 19:29:35Z tk $
/*
 * MIOS32 Application Template
 *
 * ==========================================================================
 *
 *  Copyright (C) <year> <your name> (<your email address>)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include "app.h"
#include <glcd_font.h>
#include <string.h>
#include <stdio.h>
#include "terminal.h"

#define APP_MSG_VERBOSE 0

u8 *test;

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////
u8 sysex_stream[1024];
u16 sysex_ctr = 0;
u32 can_rx_ctr, can_tx_ctr;

static s32 APP_MIDI_NotifySysex(mios32_midi_port_t port, u8 midi_in);
static s32 APP_MIDI_NotifyRx(mios32_midi_port_t port, u8 midi_in);
static s32 APP_CAN_MIDI_NotifySysexStream(u8 can, mcan_header_t header, u8* stream, u16 size);
static s32 APP_CAN_MIDI_NotifyPackage(u8 can, mcan_header_t header, mios32_midi_package_t package);

static s32 APP_SPIM_M16_Status_Notify(mios32_spim_m16_cmd_t stat_cmd, u16 stat_val);

u32 count_in, uart_count_out, spim_count_out;

mios32_midi_package_t package_in, package_out;
//midi_router_node_entry_t ** sarray;
//size_t sarray_len = 0;
//u8 offset = 2;

char* port_name[7] = {"DEFAULT", "USB", "UART", "IIC", "SPI", "OSC", "MCAN"};

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // initialize all LEDs
  MIOS32_BOARD_LED_Init(0xffffffff);
  
  int i;
  MIOS32_MIDI_RS_OptimisationSet(UART0, 1);
//  // enable running status for all SPI Ports
//  mios32_midi_package_t midi_package;
//  midi_package.ALL = 0;
//  midi_package.cin = 1;
//  midi_package.cable = 0;
//  midi_package.evnt0 = 8;
//  midi_package.evnt1 = 0XFF;
//  midi_package.evnt2 = 0XFF;
//
//     MIOS32_MIDI_SendPackage(SPIM0, midi_package);

  
  // install SysEx Callback
  //MIOS32_MIDI_SysExCallback_Init(APP_MIDI_NotifySysex);
  // install Rx Callback
  //MIOS32_MIDI_DirectRxCallback_Init(APP_MIDI_NotifyRx);
  
  // install MCAN SysEx Callback
  //MIOS32_CAN_MIDI_SysExStreamCallback_Init(APP_CAN_MIDI_NotifySysexStream);
  // install MCAN Package Callback
  //MIOS32_CAN_MIDI_PackageCallback_Init(APP_CAN_MIDI_NotifyPackage);

  MIOS32_SPIM_M16_StatCallback_Init(APP_SPIM_M16_Status_Notify);
  MIOS32_SPIM_M16_SofEnable(1);
  // initialize BM_RTR
  //BM_RTR_Init(0);
  // start terminal

	//MIOS32_DOUT_SRSet(0, (u8)(m16_rx_act&0xff));
	//MIOS32_DOUT_SRSet(1, (u8)(m16_rx_act>>8));
  TERMINAL_Init(0);

}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{
  
//  MIOS32_LCD_Clear();
//  //    // OLED Routing Menu example for possible 'Matrix Mode'
//  MIOS32_LCD_GCursorSet(0, 32);
//  MIOS32_LCD_FontInit((u8 *)GLCD_FONT_TINY); // 4x7 font
//  //*****************************12345678901234567890123456789012
//  MIOS32_LCD_PrintFormattedString("     Ch: A  0   1   2   3   4   5   6   7   8   9");
//  MIOS32_LCD_GCursorSet(0, 40);
//  MIOS32_LCD_PrintFormattedString("NOTE ON: x ");
//  MIOS32_LCD_GCursorSet(46, 40);
//  MIOS32_LCD_PrintFormattedString("xxxx xxxx xxxx xxxx");
//  MIOS32_LCD_GCursorSet(0, 48);
//  MIOS32_LCD_PrintFormattedString("MIDI CC: x ");
//  MIOS32_LCD_GCursorSet(46, 48);
//  MIOS32_LCD_PrintFormattedString("xxxx xxxx xxxx xxxx");
  while(1) {
    
  }
  
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the main task which also handles DIN, ENC
// and AIN events. You could add more jobs here, but they shouldn't consume
// more than 300 uS to ensure the responsiveness of buttons, encoders, pots.
// Alternatively you could create a dedicated task for application specific
// jobs as explained in $MIOS32_PATH/apps/tutorials/006_rtos_tasks
/////////////////////////////////////////////////////////////////////////////
void APP_Tick(void)
{
  // PWM modulate the status LED (this is a sign of life)
  //u32 timestamp = MIOS32_TIMESTAMP_Get();
  //MIOS32_BOARD_LED_Set(1, (timestamp % 20) <= ((timestamp / 100) % 10));

    
    
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
// This hook is called on local incoming midi package
/////////////////////////////////////////////////////////////////////////////
s32 APP_CAN_MIDI_NotifyPackage(u8 can, mcan_header_t header, mios32_midi_package_t package)
{
  // forward to router
  //BM_RTR_CAN_MIDI_NotifyPackage(route, package);
  return 0;
}
/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI package has been received
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
  // forward to router
  //mcan_header_t header;
 //header.src_node = MIOS32_CAN_MIDI_NodeIDGet();
  //header.src_port = port;
  DEBUG_MSG("0x%02x : 0x%08x", port, midi_package.ALL);
  if(midi_package.event>=0x8 && midi_package.event<=0xe){
    if(port == USB0){
    u8	target= SPIM0 + midi_package.chn;
      MIOS32_MIDI_SendPackage(target, midi_package);
      //MIOS32_MIDI_SendPackage(UART0, midi_package);
      package_in.ALL = midi_package.ALL;
      count_in++;
      //DEBUG_MSG("0x%02x 0x%08x", target, midi_package.ALL);

    }
//    if(port==UART0){
//
//      DEBUG_MSG("UART0");
//      
//    }
    if((port & 0xf0) == SPIM0){
     MIOS32_MIDI_SendPackage(USB0, midi_package);
      package_out.ALL = midi_package.ALL;
      spim_count_out++;
      //DEBUG_MSG("0x%02x : 0x%08x", port, package_out.ALL);
      //DEBUG_MSG("[] SPIM0: in: %d, out: %d", count_in, spim_count_out);
    }
//    if(port == UART0){
//      MIOS32_MIDI_SendPackage(USB0, midi_package);
//      package_out.ALL = midi_package.ALL;
//      uart_count_out++;
//      //DEBUG_MSG("%d - pckg_out: 0x%08x", count_out, package_out.ALL);
//      //DEBUG_MSG("[] UART0: in: %d, out: %d", count_in, uart_count_out);
//    }
  }
  
  
  //BM_RTR_MIDI_NotifyPackage(route, midi_package);
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called on local incoming midi package
/////////////////////////////////////////////////////////////////////////////
s32 APP_CAN_MIDI_NotifySysexStream(u8 can, mcan_header_t header, u8* stream, u16 size)
{
  //if(memcmp(stream, bm_rtr_midi_sysex_header, 5) == 0){
    // forward to router
    //BM_RTR_CAN_MIDI_NotifySysexStream(route, stream, size);
    //return 1;
  //}
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI package has been received
/////////////////////////////////////////////////////////////////////////////
s32 APP_MIDI_NotifySysex(mios32_midi_port_t port, u8 midi_in)
{
  //temp
  DEBUG_MSG("[APP_MIDI_NotifySysex] midi_in: %02x", midi_in);
  return 0; // stop
}

/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI byte has been received
/////////////////////////////////////////////////////////////////////////////
s32 APP_MIDI_NotifyRx(mios32_midi_port_t port, u8 midi_byte)
{
  //temp
  DEBUG_MSG("[APP_MIDI_NotifyRx] midi_byte: %02x", midi_byte);
  return 0;  // 0:will continue to MIDI_PackageReceive, 1: finish
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
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value)
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
void APP_AIN_NotifyChange(u32 pin, u32 pin_value)
{
}


s32 APP_SPIM_M16_Status_Notify(mios32_spim_m16_cmd_t stat_cmd, u16 stat_val)
{
  //DEBUG_MSG("%d - val: 0x%04x", stat_cmd, stat_val);
  u32 leds = MIOS32_BOARD_LED_Get();
  switch(stat_cmd){
    case M16_CMD_RX_STAT:
      
      if(stat_val)leds |= 1;
      else leds &=~1;
      MIOS32_BOARD_LED_Set(1, leds);
      //		MIOS32_DOUT_SRSet(0, (u8)(stat_val&0xff));
      //		MIOS32_DOUT_SRSet(1, (u8)(stat_val>>8));
      break;
    case M16_CMD_TX_STAT:
      if(stat_val)leds |= 2;
      else leds &=~2;
      MIOS32_BOARD_LED_Set(2, leds);
      //		MIOS32_DOUT_SRSet(2, (u8)(stat_val&0xff));
      //		MIOS32_DOUT_SRSet(3, (u8)(stat_val>>8));
      break;
    case M16_CMD_OVL_STAT:
      
      break;
    case M16_CMD_GPIO_BASE:
      
      break;
    case (M16_CMD_GPIO_BASE+0x10):
      
      break;
    case (M16_CMD_GPIO_BASE+0x20):
      
      break;
    default:
      break;
	}
	return 0; // no error
}
