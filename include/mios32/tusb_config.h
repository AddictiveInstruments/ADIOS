/*
 * TinyUSB configuration for this OS.
 *
 * TinyUSB includes this file by its bare name, so it must sit on the include
 * path - which is why it lives here beside the other public headers rather
 * than inside drivers/tinyusb/.
 *
 * ==========================================================================
 *
 * HOW TO USE IT
 *
 * 1) Ask for the transport in your mios32_config.h:
 *
 *      #define MIOS32_USE_USB_MIDI
 *
 *    That is the whole opt-in. It switches the USB device stack on and gives
 *    you one MIDI cable.
 *
 * 2) If you want more than one cable, say so:
 *
 *      #define MIOS32_USB_MIDI_NUM_PORTS 4
 *
 * 3) Give the product its own identity - see mios32_usb.h. The defaults here
 *    are deliberately neutral and must not be shipped as-is.
 *
 * Nothing else in this file is meant to be edited by an application. Every
 * value below is either derived from the two settings above or is a fixed
 * consequence of the USB MIDI 1.0 class.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _TUSB_CONFIG_H
#define _TUSB_CONFIG_H

#include <mios32_config.h>


/////////////////////////////////////////////////////////////////////////////
// Which USB peripheral, and therefore which TinyUSB port
/////////////////////////////////////////////////////////////////////////////

// CFG_TUSB_MCU selects the controller driver (dwc2, fsdev, ...). It is a fact
// of the silicon, so the family makefile states it - mios32/<FAMILY>/
// mios32_family.mk - and never a header in the common tree.
#ifndef CFG_TUSB_MCU
# error "CFG_TUSB_MCU is not defined. It is set by mios32/<FAMILY>/mios32_family.mk; a family that reaches this point has no USB support declared yet."
#endif


/////////////////////////////////////////////////////////////////////////////
// Scheduling
/////////////////////////////////////////////////////////////////////////////

// OPT_OS_NONE, always, and on purpose: the OS drives TinyUSB itself by calling
// its task from the 1 ms tick. Handing TinyUSB an RTOS instead would give it a
// second scheduler to answer to and tie this file to whether the project uses
// FreeRTOS - which is a project decision, not a USB one.
#define CFG_TUSB_OS                 OPT_OS_NONE

#ifndef CFG_TUSB_DEBUG
# define CFG_TUSB_DEBUG             0
#endif


/////////////////////////////////////////////////////////////////////////////
// Memory
/////////////////////////////////////////////////////////////////////////////

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))


/////////////////////////////////////////////////////////////////////////////
// Device stack
/////////////////////////////////////////////////////////////////////////////

#if defined(MIOS32_USE_USB_MIDI)
# define CFG_TUD_ENABLED            1
#else
# define CFG_TUD_ENABLED            0
#endif

// Full speed. The high-speed cores of the larger parts run in FS mode unless an
// external PHY is fitted, which no board here has.
#define CFG_TUD_MAX_SPEED           OPT_MODE_FULL_SPEED

#define CFG_TUD_ENDPOINT0_SIZE      64

// One MIDI cable unless the project asks for more. The USB MIDI 1.0 class caps
// this at 16 per interface.
#ifndef MIOS32_USB_MIDI_NUM_PORTS
# define MIOS32_USB_MIDI_NUM_PORTS  1
#endif
#if MIOS32_USB_MIDI_NUM_PORTS < 1 || MIOS32_USB_MIDI_NUM_PORTS > 16
# error "MIOS32_USB_MIDI_NUM_PORTS must be 1..16 - the USB MIDI 1.0 class allows no more than 16 cables on one interface."
#endif

#define CFG_TUD_MIDI                (CFG_TUD_ENABLED ? 1 : 0)
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_VENDOR              0

// One bulk packet each way. Going wider buys nothing at full speed: the wire
// delivers 64 bytes per frame whatever the buffer holds.
#define CFG_TUD_MIDI_RX_BUFSIZE     64
#define CFG_TUD_MIDI_TX_BUFSIZE     64


/////////////////////////////////////////////////////////////////////////////
// Controller settings
//
// These live in TinyUSB's own namespace and are read only by the controller
// driver the family selected. Setting one that the other family's driver
// ignores costs nothing.
/////////////////////////////////////////////////////////////////////////////

// Synopsys core (STM32F4 and friends): move data through the FIFOs rather
// than by DMA. At full speed there is nothing to gain from DMA, and the
// driver refuses to build unless one of the two is chosen.
#define CFG_TUD_DWC2_SLAVE_ENABLE   1
#define CFG_TUD_DWC2_DMA_ENABLE     0

// Does the board actually wire VBUS to the controller's sensing pin?
//
// Default NO, and deliberately so: with sensing enabled on a board that does
// not wire it, the controller never sees a host and THE DEVICE SIMPLY NEVER
// ENUMERATES - silently, with no error anywhere. That failure costs hours to
// find. The price of defaulting off is only that tud_umount_cb() cannot fire
// on cable removal.
//
// A board that wires it says so:
//   #define MIOS32_USB_VBUS_SENSING 1
#ifndef MIOS32_USB_VBUS_SENSING
# define MIOS32_USB_VBUS_SENSING    0
#endif
#define CFG_TUD_VBUS_DETECT_HW      MIOS32_USB_VBUS_SENSING


/////////////////////////////////////////////////////////////////////////////
// Host stack
/////////////////////////////////////////////////////////////////////////////

// Opt-in like the device side: asking for a host class turns the host stack
// on. A project that only wants to BE a MIDI device pays nothing for this.
#if defined(MIOS32_USE_USB_HOST_MIDI) || defined(MIOS32_USE_USB_HOST_HID) || defined(MIOS32_USE_USB_HOST_MSC)
# define CFG_TUH_ENABLED            1
#else
# define CFG_TUH_ENABLED            0
#endif

#if CFG_TUH_ENABLED

# define CFG_TUH_MAX_SPEED          OPT_MODE_FULL_SPEED

// Same reasoning as the device side - see CFG_TUD_DWC2_SLAVE_ENABLE.
# define CFG_TUH_DWC2_SLAVE_ENABLE  1
# define CFG_TUH_DWC2_DMA_ENABLE    0

// A hub is what makes "MIDI and HID and MSC at the same time" possible on a
// single socket. Without it the port serves exactly one device.
# ifndef MIOS32_USB_HOST_HUB
#  define MIOS32_USB_HOST_HUB       1
# endif
# define CFG_TUH_HUB                MIOS32_USB_HOST_HUB

// How many devices may be attached at once, hubs included. Each costs RAM,
// so this is a project decision.
# ifndef MIOS32_USB_HOST_MAX_DEVICES
#  define MIOS32_USB_HOST_MAX_DEVICES 4
# endif
# define CFG_TUH_DEVICE_MAX         (MIOS32_USB_HOST_MAX_DEVICES + CFG_TUH_HUB)

# define CFG_TUH_ENUMERATION_BUFSIZE 256

# if defined(MIOS32_USE_USB_HOST_MIDI)
#  define CFG_TUH_MIDI              1
#  define CFG_TUH_MIDI_RX_BUFSIZE   64
#  define CFG_TUH_MIDI_TX_BUFSIZE   64
# else
#  define CFG_TUH_MIDI              0
# endif

# if defined(MIOS32_USE_USB_HOST_HID)
// Two per device: a keyboard reporting both a boot protocol and a consumer
// page is one device with two interfaces, and missing the second one looks
// like a keyboard whose media keys are dead.
#  define CFG_TUH_HID               (2 * CFG_TUH_DEVICE_MAX)
# else
#  define CFG_TUH_HID               0
# endif

# if defined(MIOS32_USE_USB_HOST_MSC)
#  define CFG_TUH_MSC               1
# else
#  define CFG_TUH_MSC               0
# endif

# define CFG_TUH_CDC                0
# define CFG_TUH_VENDOR             0

#endif /* CFG_TUH_ENABLED */

#endif /* _TUSB_CONFIG_H */
