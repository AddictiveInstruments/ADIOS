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
#include <app_lcd.h>
#include "app.h"
u32 count = 0;



/////////////////////////////////////////////////////////////////////////////
// What the OS calls when something happens on the USB host socket. The bodies
// are at the end of this file.
/////////////////////////////////////////////////////////////////////////////

#if defined(ADIOS_USE_USB_HOST_HID)
static void APP_HID_Keyboard(u8 keycode, u8 modifiers, u8 pressed);
static void APP_HID_Mouse(s8 dx, s8 dy, s8 wheel, u8 buttons);
static void APP_HID_Change(u8 dev, u8 itf, adios_usb_hid_type_t type, u8 connected);
#endif

#if defined(ADIOS_USE_USB_HOST_MIDI)
static void APP_MIDI_HostChange(u8 itf, u8 connected);
#endif

#if defined(ADIOS_USE_USB_HOST_MSC)
static void APP_MSD_Change(u8 dev, u8 connected);
static void APP_MSD_Ready(u8 ready);
#endif


/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  // The display is yours to start, and yours to place: put this call where
  // it belongs in your own init sequence. Uncomment to activate it - the
  // driver itself is chosen by LCD= in this project's Makefile.
  //APP_LCD_Init(0);

#if defined(ADIOS_USE_USB_HOST_HID)
  ADIOS_USB_HID_KeyboardCallback_Init(APP_HID_Keyboard);
  ADIOS_USB_HID_MouseCallback_Init(APP_HID_Mouse);
  ADIOS_USB_HID_ChangeCallback_Init(APP_HID_Change);
#endif

#if defined(ADIOS_USE_USB_HOST_MIDI)
  ADIOS_USB_MIDI_HostChangeCallback_Init(APP_MIDI_HostChange);
#endif

#if defined(ADIOS_USE_USB_HOST_MSC)
  ADIOS_USB_MSC_ChangeCallback_Init(APP_MSD_Change);
  ADIOS_USB_MSC_ReadyCallback_Init(APP_MSD_Ready);
#endif

  // initialize all LEDs
  ADIOS_SOL_Init();

  // initialize the sign-of-life LED
  ADIOS_SOL_Init();
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
  // PWM modulate the SOF LED (this is a sign of life)
  u32 timestamp = ADIOS_TIMESTAMP_Get();
  if( ((timestamp % 20) <= ((timestamp / 100) % 10)) & 1 )
    ADIOS_SOL_Set();
  else
    ADIOS_SOL_Clr();

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
  // Host bring-up check: everything received from devices attached to the
  // host socket (USB16..31) is mirrored to the FIRST device cable - the same
  // one ADIOS Studio talks on. That mixing is not a compromise, it is what any
  // MIDI port does: sends all go through one task, so every message is
  // written whole, and a note lands BETWEEN two SysEx messages, never inside
  // one. Notes and a firmware upload do not even share a direction.
  if( port >= USB16 && port <= USB31 )
    ADIOS_MIDI_SendPackage_NonBlocking(USB0, midi_package);

//	if(midi_package.event==NoteOn){
//		 ADIOS_MIDI_SendDebugMessage("Note On\n");
//	  // forward USB0->DIN0 and DIN0->USB0
//	  switch( port ) {
//	    case USB0:
//	    	ADIOS_MIDI_SendPackage(USB0, midi_package);
//	    	ADIOS_MIDI_SendPackage(DIN0,  midi_package);
//	    	ADIOS_MIDI_SendPackage(DIN1,  midi_package);
//	    	break;
//	    case DIN0:
//	    	ADIOS_MIDI_SendPackage(USB0, midi_package);
//	    	ADIOS_MIDI_SendPackage(DIN0,  midi_package);
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


/////////////////////////////////////////////////////////////////////////////
// USB host socket - what is plugged into it, and what it sends
/////////////////////////////////////////////////////////////////////////////

#if defined(ADIOS_USE_USB_HOST_HID)

// Every key of an attached keyboard becomes a MIDI note on the first device
// cable, so it shows up in a monitor on the PC exactly like the pad does. HID
// usage codes start at 4 (letter A); +56 lands A on middle C.
static void APP_HID_Keyboard(u8 keycode, u8 modifiers, u8 pressed)
{
  adios_midi_package_t p;

  (void)modifiers;

  p.ALL = 0;
  p.type  = pressed ? NoteOn : NoteOff;
  p.event = pressed ? NoteOn : NoteOff;
  p.chn   = Chn1;
  p.note  = (keycode + 56) & 0x7f;
  p.velocity = pressed ? 100 : 0;


  ADIOS_MIDI_SendPackage_NonBlocking(USB0, p);
}


// Movements are relative - how far it moved since the last report - so they
// are reported as they come rather than accumulated into a position.
static void APP_HID_Mouse(s8 dx, s8 dy, s8 wheel, u8 buttons)
{
  // A button going down or up is reported with no movement at all, so the
  // buttons have to be watched in their own right - filtering on movement
  // alone throws every click away.
  static u8 last_buttons = 0;
  u8 changed = buttons ^ last_buttons;

  last_buttons = buttons;

  if( !dx && !dy && !wheel && !changed )
    return;

  ADIOS_MIDI_SendDebugMessage("Mouse: dx=%d dy=%d wheel=%d buttons=%c%c%c%s\n",
                               dx, dy, wheel,
                               (buttons & 1) ? 'L' : '-',
                               (buttons & 2) ? 'R' : '-',
                               (buttons & 4) ? 'M' : '-',
                               changed ? " *" : "");
}


static void APP_HID_Change(u8 dev, u8 itf, adios_usb_hid_type_t type, u8 connected)
{
  const char *what = "device";

  switch( type ) {
  case ADIOS_USB_HID_TYPE_KEYBOARD: what = "keyboard"; break;
  case ADIOS_USB_HID_TYPE_MOUSE:    what = "mouse";    break;
  default: break;
  }

  ADIOS_MIDI_SendDebugMessage("HID: %s on device %d interface %d %s\n",
                               what, dev, itf, connected ? "connected" : "disconnected");
}

#endif /* ADIOS_USE_USB_HOST_HID */


#if defined(ADIOS_USE_USB_HOST_MIDI)

// Where this device landed in the port range is what an application needs:
// its ports are first_port up to first_port + num_ports - 1.
static void APP_MIDI_HostChange(u8 itf, u8 connected)
{
  adios_usb_midi_host_info_t info;

  if( !connected || ADIOS_USB_MIDI_HostInfoGet(itf, &info) < 0 || !info.connected ) {
    ADIOS_MIDI_SendDebugMessage("MIDI: interface %d disconnected\n", itf);
    return;
  }

  ADIOS_MIDI_SendDebugMessage("MIDI: interface %d connected, ports USB%d..%d (%d in / %d out)\n",
                               itf,
                               info.first_port - USB0, info.first_port - USB0 + info.num_ports - 1,
                               info.num_in, info.num_out);
}

#endif /* ADIOS_USE_USB_HOST_MIDI */


#if defined(ADIOS_USE_USB_HOST_MSC)

// Connected: everything plugged in, whether or not it holds a medium.
static void APP_MSD_Change(u8 dev, u8 connected)
{
  ADIOS_MIDI_SendDebugMessage("MSD: device %d %s, %d attached\n",
                               dev, connected ? "connected" : "disconnected",
                               ADIOS_USB_MSC_ConnectedNumGet());
}


// Ready: the one medium the sector calls are reading. Its capacity says which
// it is, so the first sector is read again on every change.
static void APP_MSD_Ready(u8 ready)
{
  static u8 sector0[512];
  u32 num_sectors; u16 sector_size;
  s32 status;

  if( !ready ) {
    ADIOS_MIDI_SendDebugMessage("MSD: no medium ready\n");
    return;
  }

  if( ADIOS_USB_MSC_SizeGet(&num_sectors, &sector_size) < 0 )
    return;

  ADIOS_MIDI_SendDebugMessage("MSD: medium ready, %u sectors of %u bytes (%u MB)\n",
                               num_sectors, sector_size, num_sectors / 2048);

  status = ADIOS_USB_MSC_SectorRead(0, sector0);
  if( status == 0 )
    ADIOS_MIDI_SendDebugMessage("MSD: sector 0 reads %02x %02x .. %02x %02x (%s)\n",
                                 sector0[0], sector0[1], sector0[510], sector0[511],
                                 (sector0[510] == 0x55 && sector0[511] == 0xaa) ? "boot signature OK" : "no boot signature");
  else
    ADIOS_MIDI_SendDebugMessage("MSD: sector 0 read FAILED (%d)\n", status);
}

#endif /* ADIOS_USE_USB_HOST_MSC */
