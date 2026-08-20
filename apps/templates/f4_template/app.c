/*
 * MIOS32 Application Template
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

#include <mios32.h>
#include <app_lcd.h>
#include "app.h"
u32 count = 0;



/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // The display is yours to start, and yours to place: put this call where
  // it belongs in your own init sequence. Uncomment to activate it - the
  // driver itself is chosen by LCD= in this project's Makefile.
  //APP_LCD_Init(0);

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
