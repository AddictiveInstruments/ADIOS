/*
 * Header file for the USB layer.
 *
 * This layer is a thin adapter over TinyUSB. It owns the device identity, the
 * port roles, and the periodic call that drives the stack; the classes
 * themselves (MIDI, and later HID and MSC) live in their own files.
 *
 * ==========================================================================
 *
 * HOW TO USE IT
 *
 * 1) Ask for a transport in your mios32_config.h. There is no master USB
 *    switch to set - asking for a class is what turns USB on:
 *
 *      #define MIOS32_USE_USB_MIDI
 *
 * 2) Give the product its own identity. THE DEFAULTS BELOW ARE NOT SHIPPABLE
 *    - see the note on the product ID:
 *
 *      #define MIOS32_USB_PRODUCT_ID   0x03e9
 *      #define MIOS32_USB_PRODUCT_STR  "Your Instrument"
 *
 * 3) Call the two entry points: MIOS32_USB_Init() once, then
 *    MIOS32_USB_Handler() regularly. The core already does both if you use the
 *    standard main loop, so an ordinary application writes nothing here.
 *
 * A minimal application therefore needs one line of configuration and no code.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_USB_H
#define _MIOS32_USB_H

#if defined(MIOS32_USE_USB_MIDI)


/////////////////////////////////////////////////////////////////////////////
// Device identity
/////////////////////////////////////////////////////////////////////////////

// The vendor ID is issued by the USB-IF and cannot be invented. 0x16c0 belongs
// to Van Ooijen Technische Informatica, who allocate product IDs under it.
#ifndef MIOS32_USB_VENDOR_ID
# define MIOS32_USB_VENDOR_ID       0x16c0
#endif

// !! NOT SHIPPABLE AS-IS !!
// 1000..1009 (0x03e8..0x03f1) is the range that vendor keeps free for lab use,
// which is what this default is. A product ID identifies a PRODUCT, so every
// instrument must declare its own: two devices answering the same ID is a
// fault, and it bites the day both are plugged into one host.
#ifndef MIOS32_USB_PRODUCT_ID
# define MIOS32_USB_PRODUCT_ID      0x03e8
#endif

#ifndef MIOS32_USB_VENDOR_STR
# define MIOS32_USB_VENDOR_STR      "Addictive Instruments"
#endif

// What the user reads in the MIDI device list of a DAW.
#ifndef MIOS32_USB_PRODUCT_STR
# define MIOS32_USB_PRODUCT_STR     "Unnamed Instrument"
#endif

// Binary-coded decimal, so 0x0100 reads as v1.00.
#ifndef MIOS32_USB_VERSION_ID
# define MIOS32_USB_VERSION_ID      0x0100
#endif


/////////////////////////////////////////////////////////////////////////////
// Port roles
/////////////////////////////////////////////////////////////////////////////

// A port has a role, and the role is a RUNTIME value even on boards that will
// never change it. A board that can switch - a Type-C port with CC detection,
// or an OTG ID pin - moves between these at will; a board that cannot sets one
// at startup and stays there. Same code either way, which is why the role is
// not a compile-time switch.
typedef enum {
  MIOS32_USB_ROLE_NONE   = 0,   // port idle, no stack running on it
  MIOS32_USB_ROLE_DEVICE = 1,
  MIOS32_USB_ROLE_HOST   = 2
} mios32_usb_role_t;

// How a port learns its role. Which of these a family can offer is a fact of
// the silicon and not a choice.
typedef enum {
  MIOS32_USB_ROLE_SRC_FIXED = 0, // the connector decides; nothing to detect
  MIOS32_USB_ROLE_SRC_ID    = 1, // OTG ID pin
  MIOS32_USB_ROLE_SRC_CC    = 2  // Type-C CC lines
} mios32_usb_role_source_t;

#ifndef MIOS32_USB_NUM_PORTS
# define MIOS32_USB_NUM_PORTS       1
#endif


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_USB_Init(u32 mode);

// Drives the stack. Must be called regularly; the core calls it from the 1 ms
// tick. Calling it more often is harmless and lowers latency.
extern s32 MIOS32_USB_Handler(void);

extern s32 MIOS32_USB_IsInitialized(void);

extern s32 MIOS32_USB_RoleSet(u8 port, mios32_usb_role_t role);
extern mios32_usb_role_t MIOS32_USB_RoleGet(u8 port);

// Called when a port changes role on its own - an ID pin grounded, a Type-C
// cable attached. Never called on a port whose role source is FIXED.
extern s32 MIOS32_USB_RoleChangeCallback_Init(void (*callback)(u8 port, mios32_usb_role_t role));


/////////////////////////////////////////////////////////////////////////////
// Implemented per family: clocks, pins, interrupt, role source
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_USB_LL_Init(u8 port, mios32_usb_role_t role);
extern mios32_usb_role_source_t MIOS32_USB_LL_RoleSourceGet(u8 port);

#endif /* MIOS32_USE_USB_MIDI */

#endif /* _MIOS32_USB_H */
