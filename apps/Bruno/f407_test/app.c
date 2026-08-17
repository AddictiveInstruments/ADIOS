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
#include <app_lcd.h>
#include "app.h"
u32 count = 0;

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
// Host bring-up check for HID: every key of an attached USB keyboard becomes
// a MIDI note on the first device cable, so pressing keys shows up in a MIDI
// monitor on the PC exactly like the pad does. HID usage codes start at 4
// (letter A); +56 lands A on middle C.
static void APP_HID_KeyNote(u8 keycode, u8 modifiers, u8 pressed)
{
  (void)modifiers;

  mios32_midi_package_t p;
  p.ALL = 0;
  p.type  = pressed ? NoteOn : NoteOff;
  p.event = pressed ? NoteOn : NoteOff;
  p.chn   = Chn1;
  p.note  = (keycode + 56) & 0x7f;
  p.velocity = pressed ? 100 : 0;

  MIOS32_MIDI_SendPackage_NonBlocking(USB0, p);
}

void APP_Init(void)
{
  // The display is yours to start, and yours to place: put this call where
  // it belongs in your own init sequence. Uncomment to activate it - the
  // driver itself is chosen by LCD= in this project's Makefile.
  //APP_LCD_Init(0);

#if defined(MIOS32_USE_USB_HOST_HID)
  MIOS32_USB_HID_KeyboardCallback_Init(APP_HID_KeyNote);
#endif

  // initialize all LEDs
  MIOS32_SOL_Init();

  // initialize the sign-of-life LED
  MIOS32_SOL_Init();
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
// jobs as explained in $MIOS32_PATH/apps/tutorials/006_rtos_tasks
/////////////////////////////////////////////////////////////////////////////
void APP_Tick(void)
{
  // PWM modulate the SOF LED (this is a sign of life)
  u32 timestamp = MIOS32_TIMESTAMP_Get();
  if( ((timestamp % 20) <= ((timestamp / 100) % 10)) & 1 )
    MIOS32_SOL_Set();
  else
    MIOS32_SOL_Clr();

#if defined(MIOS32_USE_USB_HOST_MSC)
  // Host bring-up check for MSC: announce a medium once when it appears, with
  // its capacity and the first bytes of its sector 0 - a read through the
  // whole chain, visible in Studio's console.
  {
    static u8 was_available = 0;
    u8 now = MIOS32_USB_MSC_CheckAvailable() ? 1 : 0;

    if( now && !was_available ) {
      u32 num_sectors; u16 sector_size;
      static u8 sector0[512];

      MIOS32_USB_MSC_SizeGet(&num_sectors, &sector_size);
      MIOS32_MIDI_SendDebugMessage("USB-MSD: %u sectors of %u bytes (%u MB)\n",
                                   num_sectors, sector_size, num_sectors / 2048);

      if( MIOS32_USB_MSC_SectorRead(0, sector0) == 0 )
        MIOS32_MIDI_SendDebugMessage("USB-MSD: sector 0 reads %02x %02x .. %02x %02x (%s)\n",
                                     sector0[0], sector0[1], sector0[510], sector0[511],
                                     (sector0[510] == 0x55 && sector0[511] == 0xaa) ? "boot signature OK" : "no boot signature");
      else
        MIOS32_MIDI_SendDebugMessage("USB-MSD: sector 0 read FAILED\n");
    }
    was_available = now;
  }
#endif
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
void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
  // Host bring-up check: everything received from devices attached to the
  // host socket (USB16..31) is mirrored to the FIRST device cable - the same
  // one MIOS Studio talks on. That mixing is not a compromise, it is what any
  // MIDI port does: sends all go through one task, so every message is
  // written whole, and a note lands BETWEEN two SysEx messages, never inside
  // one. Notes and a firmware upload do not even share a direction.
  if( port >= USB16 && port <= USB31 )
    MIOS32_MIDI_SendPackage_NonBlocking(USB0, midi_package);

//	if(midi_package.event==NoteOn){
//		 MIOS32_MIDI_SendDebugMessage("Note On\n");
//	  // forward USB0->DIN0 and DIN0->USB0
//	  switch( port ) {
//	    case USB0:
//	    	MIOS32_MIDI_SendPackage(USB0, midi_package);
//	    	MIOS32_MIDI_SendPackage(DIN0,  midi_package);
//	    	MIOS32_MIDI_SendPackage(DIN1,  midi_package);
//	    	break;
//	    case DIN0:
//	    	MIOS32_MIDI_SendPackage(USB0, midi_package);
//	    	MIOS32_MIDI_SendPackage(DIN0,  midi_package);
//	    	break;
//	  }
//	}
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
