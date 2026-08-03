// $Id: mios32_usb_midi.c 2025 2014-07-06 18:22:18Z tk $
//! \defgroup MIOS32_USB_MIDI
//!
//! USB MIDI layer for MIOS32 - STM32G0xx
//!
//! NOT YET IMPLEMENTED - see mios32_usb.c in this same directory for why
//! (STM32G0xx needs a USB_DRD driver, a different peripheral IP from
//! STM32F4xx's OTG_FS; most G0 chips have no USB peripheral at all).
//!
//! Applications shouldn't call these functions directly, instead please use \ref MIOS32_MIDI layer functions
//!
//! \{
/* ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>

// this module can be optionally disabled in a local mios32_config.h file (included from mios32.h)
#if !defined(MIOS32_DONT_USE_USB_MIDI)

/////////////////////////////////////////////////////////////////////////////
//! Initializes USB MIDI layer
//! \param[in] mode currently only mode 0 supported
//! \return < 0 always - not implemented on STM32G0xx yet, see mios32_usb.c
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_Init(u32 mode)
{
  return -1; // not implemented on STM32G0xx yet
}

/////////////////////////////////////////////////////////////////////////////
//! \return < 0 - no-op on STM32G0xx, see mios32_usb.c
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_ChangeConnectionState(u8 dev, u8 connected)
{
  return -1;
}

void MIOS32_USB_MIDI_EP1_IN_Callback(u8 bEP, u8 bEPStatus)
{
}

void MIOS32_USB_MIDI_EP2_OUT_Callback(u8 bEP, u8 bEPStatus)
{
}

/////////////////////////////////////////////////////////////////////////////
//! \return 0 - never available on STM32G0xx yet, see mios32_usb.c
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_MIDI_CheckAvailable(u8 cable)
{
  return 0;
}

s32 MIOS32_USB_MIDI_PackageSend_NonBlocking(mios32_midi_package_t package)
{
  return -1;
}

s32 MIOS32_USB_MIDI_PackageSend(mios32_midi_package_t package)
{
  return -1;
}

s32 MIOS32_USB_MIDI_PackageReceive(mios32_midi_package_t *package)
{
  return -1; // no package available
}

s32 MIOS32_USB_MIDI_Periodic_mS(void)
{
  return 0;
}

//! \}

#endif /* MIOS32_DONT_USE_USB_MIDI */
