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


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // initialize all LEDs
  MIOS32_BOARD_LED_Init(0xffffffff);


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
  // PWM modulate the status LED (this is a sign of life)
  u32 timestamp = MIOS32_TIMESTAMP_Get();
  MIOS32_BOARD_LED_Set(1, (timestamp % 20) <= ((timestamp / 100) % 10));

    
    
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
 s32 status;
 if(midi_package.event>=0x8 && midi_package.event<=0xe){
  if((port & 0xf0) == MCAN0){
   status = MIOS32_MIDI_SendPackage(USB0 + (port & 0x0f), midi_package);
#if APP_MSG_VERBOSE >=1
   DEBUG_MSG("[APP_MIDI_NotifyPackage] FW midi package [0x%02x,0x%02x,0x%02x] MCAN%d->USB%d, status: %s\n", midi_package.evnt0, midi_package.evnt1, midi_package.evnt2, port & 0x0f,port & 0x0f, (status>=0)? "Ok" : "Error");
#endif
   
  }else if((port & 0xf0) == USB0){
   status = MIOS32_MIDI_SendPackage(MCAN0 + (port & 0x0f), midi_package); // toDo
   
#if APP_MSG_VERBOSE >=1
   DEBUG_MSG("[APP_MIDI_NotifyPackage] FW midi package [0x%02x,0x%02x,0x%02x] USB%d->MCAN%d, status: %s\n", midi_package.evnt0, midi_package.evnt1, midi_package.evnt2, port & 0x0f,port & 0x0f, (status>=0)? "Ok" : "Error");
#endif
  }
 }
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
