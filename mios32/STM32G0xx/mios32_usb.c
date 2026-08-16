// $Id$
//! \defgroup MIOS32_USB
//!
//! USB driver for MIOS32 - STM32G0xx
//!
//! NOT YET IMPLEMENTED. STM32G0xx uses a USB_DRD (Dual Role Device)
//! peripheral, which is a completely different IP block from STM32F4xx's
//! USB_OTG_FS (different register layout, different programming model -
//! PMA-based buffer descriptors instead of OTG's FIFO/DMA scheme). The
//! previous version of this file was a byte-for-byte copy of the F4xx
//! OTG_FS implementation and could never have compiled for any G0 chip.
//!
//! Also note: most STM32G0xx chips have NO USB peripheral at all in
//! silicon (verified against the CMSIS headers) - only STM32G0B0/G0B1/G0C1
//! have a real USB_DRD peripheral. G030K6/G031K8/G041/G050K8/G051/G061/
//! G070CB/G071/G081 cannot support USB under any circumstances.
//!
//! TODO (future project, not yet scheduled): write a real USB_DRD-based
//! driver for G0B0/G0B1/G0C1. See memory note
//! mios32-midi-architecture-overview.md for context.
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
#if !defined(MIOS32_DONT_USE_USB)

#if !defined(MIOS32_PROCESSOR_STM32G0B0) && !defined(MIOS32_PROCESSOR_STM32G0B1) && !defined(MIOS32_PROCESSOR_STM32G0C1)
#warning "MIOS32_USB: this STM32G0xx chip has no USB peripheral in silicon - USB will stay unavailable at runtime (MIOS32_USB_Init() always returns -1)."
#else
#warning "MIOS32_USB: not implemented yet for STM32G0xx (needs a USB_DRD driver, different peripheral IP from STM32F4xx's OTG_FS) - USB will stay unavailable at runtime even though this chip has the hardware for it."
#endif

/////////////////////////////////////////////////////////////////////////////
// Global variables (referenced by other modules, must stay defined)
/////////////////////////////////////////////////////////////////////////////

void (*pEpInt_IN[7])(void);
void (*pEpInt_OUT[7])(void);

uint8_t USBD_DeviceQualifierDesc[0x0A];

/////////////////////////////////////////////////////////////////////////////
//! Initializes USB interface
//! \param[in] mode currently only mode 0 supported
//! \return < 0 always - not implemented on STM32G0xx yet, see file header
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_Init(u32 mode)
{
  return -1; // not implemented on STM32G0xx yet
}

/////////////////////////////////////////////////////////////////////////////
//! \return 0 - USB is never initialized on STM32G0xx yet, see file header
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_IsInitialized(void)
{
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! \return 0 - no-op on STM32G0xx, see file header
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_ForceSingleUSB(void)
{
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! \return 0 - no-op on STM32G0xx, see file header
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_ForceDeviceMode(void)
{
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! \return -1 - USB host mode not implemented on STM32G0xx, see file header
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HOST_Process(void)
{
  return -1;
}

//! \}

#endif /* MIOS32_DONT_USE_USB */
