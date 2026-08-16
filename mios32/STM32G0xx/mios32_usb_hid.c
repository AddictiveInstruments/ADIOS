// $Id$
//! \defgroup MIOS32_USB_HID
//!
//! USB HID (mouse/keyboard) layer for MIOS32 - STM32G0xx
//!
//! NOT YET IMPLEMENTED - see mios32_usb.c in this same directory for why
//! (STM32G0xx needs a USB_DRD driver, a different peripheral IP from
//! STM32F4xx's OTG_FS; most G0 chips have no USB peripheral at all).
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
#if !defined(MIOS32_DONT_USE_USB_HID)

/////////////////////////////////////////////////////////////////////////////
//! Initializes USB HID layer
//! \param[in] mode currently only mode 0 supported
//! \return < 0 always - not implemented on STM32G0xx yet, see mios32_usb.c
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_Init(u32 mode)
{
  return -1; // not implemented on STM32G0xx yet
}

/////////////////////////////////////////////////////////////////////////////
//! \return 0 - never available on STM32G0xx yet, see mios32_usb.c
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_CheckAvailable(u8 dev)
{
  return 0;
}

s32 MIOS32_USB_HID_MouseCallback_Init(void (*mouse_callback)(mios32_mouse_data_t mouse_data))
{
  return -1;
}

s32 MIOS32_USB_HID_KeyboardCallback_Init(void (*keyboard_callback)(mios32_kbd_state_t kbd_state, mios32_kbd_key_t kbd_key))
{
  return -1;
}

//! \}

#endif /* MIOS32_DONT_USE_USB_HID */
