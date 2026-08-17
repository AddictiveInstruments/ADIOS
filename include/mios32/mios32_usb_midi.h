/*
 * Header file for the USB MIDI transport.
 *
 * Addressed by a single index 0..31, which is (port - USB0). The MIDI port
 * ranges were laid out so 0x10..0x2f is contiguous, which is what lets that
 * index split into controller and cable without a lookup table.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_USB_MIDI_H
#define _MIOS32_USB_MIDI_H

#if defined(MIOS32_USE_USB)

// Cables this machine presents on the first controller. The USB MIDI 1.0
// class allows no more than sixteen: its cable number is a 4-bit field.
#ifndef MIOS32_USB_MIDI_NUM_PORTS
#define MIOS32_USB_MIDI_NUM_PORTS 1
#endif


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_USB_MIDI_Init(u32 mode);
extern s32 MIOS32_USB_MIDI_CheckAvailable(u8 idx);
extern s32 MIOS32_USB_MIDI_PackageSend_NonBlocking(u8 idx, mios32_midi_package_t package);
extern s32 MIOS32_USB_MIDI_PackageSend(u8 idx, mios32_midi_package_t package);
extern s32 MIOS32_USB_MIDI_PackageReceive(mios32_midi_package_t *package, u8 *idx);
extern s32 MIOS32_USB_MIDI_Periodic_mS(void);

#endif /* MIOS32_USE_USB */

#endif /* _MIOS32_USB_MIDI_H */
