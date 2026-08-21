/*
 * Header file for the USB MIDI transport.
 *
 * Addressed by a single index 0..31, which is (port - USB0). The MIDI port
 * ranges were laid out so 0x10..0x2f is contiguous, which is what lets that
 * index split into controller and cable without a lookup table.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_USB_MIDI_H
#define _ADIOS_USB_MIDI_H

#if defined(ADIOS_USE_USB)

// Cables this machine presents on the first controller. The USB MIDI 1.0
// class allows no more than sixteen: its cable number is a 4-bit field.
#ifndef ADIOS_USB_MIDI_NUM_PORTS
#define ADIOS_USB_MIDI_NUM_PORTS 1
#endif


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_USB_MIDI_Init(u32 mode);
extern s32 ADIOS_USB_MIDI_CheckAvailable(u8 idx);
extern s32 ADIOS_USB_MIDI_PackageSend_NonBlocking(u8 idx, adios_midi_package_t package);
extern s32 ADIOS_USB_MIDI_PackageSend(u8 idx, adios_midi_package_t package);
extern s32 ADIOS_USB_MIDI_PackageReceive(adios_midi_package_t *package, u8 *idx);
extern s32 ADIOS_USB_MIDI_Periodic_mS(void);


/////////////////////////////////////////////////////////////////////////////
// Attached MIDI devices
//
// Each one is given a block of consecutive ports out of the host range, in
// the order it arrives, and keeps that block until it leaves. A second device
// therefore never renumbers the first, and a route an application stored stays
// pointing at the same instrument.
//
// What an application needs is where a device's block starts and how long it
// is; from there the ports are the usual ones.
/////////////////////////////////////////////////////////////////////////////

typedef struct {
  u8 connected;   // 1 while the interface is attached
  u8 first_port;  // its first OS port (USB16 and up)
  u8 num_ports;   // how many consecutive ports it owns
  u8 num_in;      // cables it can send to us on
  u8 num_out;     // cables we can send to it on
} adios_usb_midi_host_info_t;

// Told when a device arrives or leaves. Reported from the periodic call, not
// from the moment it happens: an arrival lands while the stack is still
// bringing the device up, which is no place to run application code.
extern s32 ADIOS_USB_MIDI_HostChangeCallback_Init(void (*callback)(u8 itf, u8 connected));

// What is on that interface. < 0 if the index is out of range; an interface
// with nothing on it simply reports connected = 0.
extern s32 ADIOS_USB_MIDI_HostInfoGet(u8 itf, adios_usb_midi_host_info_t *info);

// How many interfaces may be attached at once - the range of valid itf.
extern s32 ADIOS_USB_MIDI_HostNumGet(void);

#endif /* ADIOS_USE_USB */

#endif /* _ADIOS_USB_MIDI_H */
