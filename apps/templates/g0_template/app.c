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
u32 count = 0;

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // NO display driver is compiled into this project - see the note in its
  // Makefile, the flash budget on this chip does not allow one. To add a
  // screen: set LCD= in the Makefile, include <app_lcd.h> above, and call
  // APP_LCD_Init(0) here.

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
  // DIAGNOSTIC (temporary): bypass MIOS32_TIMESTAMP entirely, just prove
  // APP_Tick() itself is being called repeatedly over time by counting
  // calls directly - toggles ~1x/second if this is really called every 1mS
  // as expected from the bare-metal loop.
  static u32 tick_count = 0;
  if( ++tick_count >= 500 ) {
    tick_count = 0;
    MIOS32_SOL_Tog();
  }
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
