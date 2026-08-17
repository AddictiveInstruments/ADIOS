/*
 * USB MIDI transport.
 *
 * Serves the seven entry points mios32_midi.c expects from a transport, on
 * top of TinyUSB's MIDI class.
 *
 * There is no format conversion here, and that is not an accident: a
 * mios32_midi_package_t IS a USB MIDI 1.0 event packet. Its first byte holds
 * the cable in the high nibble and the code index number in the low nibble,
 * which is exactly what the class puts on the wire, and the three that follow
 * are the MIDI bytes. So the two talk to each other by plain copy.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#include <mios32.h>

#if defined(MIOS32_USE_USB_MIDI)

#include <tusb.h>


/////////////////////////////////////////////////////////////////////////////
// How long a blocking send waits before giving up, in milliseconds. A host
// that has stopped collecting must not be able to stall the application
// forever - losing a package is recoverable, a frozen instrument is not.
/////////////////////////////////////////////////////////////////////////////

#ifndef MIOS32_USB_MIDI_SEND_TIMEOUT_MS
# define MIOS32_USB_MIDI_SEND_TIMEOUT_MS  100
#endif


/////////////////////////////////////////////////////////////////////////////
//! Initializes the USB MIDI transport.
//! The stack itself is brought up by MIOS32_USB_Init(), which the core calls
//! first; there is nothing left to do here.
//! \param[in] mode currently only mode 0 is supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_Init(u32 mode)
{
  if( mode != 0 )
    return -1;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \param[in] cable the USB cable, 0..MIOS32_USB_MIDI_NUM_PORTS-1
//! \return 1 if the cable can carry MIDI right now
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_CheckAvailable(u8 cable)
{
  if( cable >= MIOS32_USB_MIDI_NUM_PORTS )
    return 0;

  // Mounted means the host has configured the device. Before that the cables
  // exist in the descriptor but lead nowhere.
  return tud_midi_mounted() ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a package, or reports that it could not be sent right now.
//! \param[in] package the package to send
//! \return 0 on success, -1 if the cable is not available, -2 if the buffer
//!         is full - which is a "try again", not a failure
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_PackageSend_NonBlocking(mios32_midi_package_t package)
{
  if( !MIOS32_USB_MIDI_CheckAvailable(package.cable) )
    return -1;

  if( !tud_midi_packet_write(package.bytes) )
    return -2; // no room

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Sends a package, waiting for room if need be.
//! \param[in] package the package to send
//! \return 0 on success, -1 if the cable is not available, -3 on timeout
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_PackageSend(mios32_midi_package_t package)
{
  u32 waited_ms = 0;

  for(;;) {
    s32 status = MIOS32_USB_MIDI_PackageSend_NonBlocking(package);

    if( status != -2 )
      return status; // sent, or the cable is gone

    if( waited_ms >= MIOS32_USB_MIDI_SEND_TIMEOUT_MS )
      return -3;

    // The buffer only drains when the stack runs, and the caller is holding
    // the only thread there is. Waiting without driving it would deadlock.
    MIOS32_USB_Handler();
    MIOS32_DELAY_Wait_uS(1000);
    ++waited_ms;
  }
}


/////////////////////////////////////////////////////////////////////////////
//! Takes the next received package, if there is one.
//! \param[out] package the package
//! \return 0 if a package was returned, -1 if nothing is waiting
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_PackageReceive(mios32_midi_package_t *package)
{
  if( !tud_midi_mounted() )
    return -1;

  if( !tud_midi_packet_read(package->bytes) )
    return -1;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Periodic service.
//! The stack is driven by MIOS32_USB_Handler() from the same tick, so this
//! exists only to keep the transport interface uniform.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_Periodic_mS(void)
{
  return 0;
}

#endif /* MIOS32_USE_USB_MIDI */
