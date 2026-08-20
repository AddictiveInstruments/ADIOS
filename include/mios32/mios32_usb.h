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
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_USB_H
#define _MIOS32_USB_H

// Master switch, derived from the classes a project asks for - same idiom as
// MIOS32_USE_SPI. Nobody sets this by hand: asking for a class is the opt-in,
// and a project should never have to remember a second switch that only
// repeats what the first one already said.
#if !defined(MIOS32_USE_USB) && \
    (defined(MIOS32_USE_USB_MIDI) || defined(MIOS32_USE_USB_HOST_MIDI) || \
     defined(MIOS32_USE_USB_HOST_HID) || defined(MIOS32_USE_USB_HOST_MSC))
#define MIOS32_USE_USB
#endif

#if defined(MIOS32_USE_USB)


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

// How long the device lets go of the bus when it comes up, in milliseconds.
// A core that restarts into its bootloader, or an application that restarts,
// changes what it IS while the cable stays in - and the reset alone drops the
// pull-up too briefly for a host to call it an unplug. Letting go on purpose
// is what makes the change visible. See port_start() in mios32_usb.c.
#ifndef MIOS32_USB_DETACH_MS
# define MIOS32_USB_DETACH_MS       50
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
//
// Plain macros rather than an enumeration, and for a reason worth knowing:
// THE PREPROCESSOR CANNOT SEE ENUMERATORS. An unknown identifier in an #if is
// silently taken as 0, so a build asking "does any port detect its role?" -
// which is how the whole detection machinery is compiled in or left out -
// would answer "no" every time, and do it without a word of complaint.
#define MIOS32_USB_ROLE_SRC_FIXED   0  // the connector decides; nothing to detect
#define MIOS32_USB_ROLE_SRC_ID      1  // OTG ID pin
#define MIOS32_USB_ROLE_SRC_CC      2  // Type-C CC lines

typedef u8 mios32_usb_role_source_t;

#ifndef MIOS32_USB_NUM_PORTS
# define MIOS32_USB_NUM_PORTS       1
#endif

// What each port does at startup, when nothing can detect it. These are the
// usual arrangement rather than a guess about any particular board: a machine
// with two sockets almost always presents itself on one and drives the other.
// A board wired differently says so.
#ifndef MIOS32_USB_P0_ROLE
# define MIOS32_USB_P0_ROLE         MIOS32_USB_ROLE_DEVICE
#endif
#ifndef MIOS32_USB_P1_ROLE
# define MIOS32_USB_P1_ROLE         MIOS32_USB_ROLE_HOST
#endif

// How a port learns its role. The DEFAULT IS FIXED, because a board that
// gives each port its own dedicated connector has already decided in its
// mechanics: a type-A receptacle is a host socket, a type-B one is a device
// socket. A board that really detects says so, and says which way:
//   #define MIOS32_USB_P0_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_ID
// Which sources a family can offer is a fact of the silicon, not a choice -
// but WHETHER a board uses one is a fact of the board, so it is declared
// here, where every layer can see it, and not buried in a family.
#ifndef MIOS32_USB_P0_ROLE_SOURCE
# define MIOS32_USB_P0_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_FIXED
#endif
#ifndef MIOS32_USB_P1_ROLE_SOURCE
# define MIOS32_USB_P1_ROLE_SOURCE  MIOS32_USB_ROLE_SRC_FIXED
#endif

// Derived, and the reason the detection machinery costs nothing to a build
// that cannot use it: no port detecting means no reading, no debouncing and
// no pin claimed anywhere. A bootloader, whose socket is whatever the board
// wired and never changes while it runs, compiles none of it.
#if MIOS32_USB_P0_ROLE_SOURCE != MIOS32_USB_ROLE_SRC_FIXED || MIOS32_USB_P1_ROLE_SOURCE != MIOS32_USB_ROLE_SRC_FIXED
# define MIOS32_USB_ROLE_DETECTED 1
#else
# define MIOS32_USB_ROLE_DETECTED 0
#endif

// How many identical readings a DETECTED role must give before it is acted
// on, counted in turns of MIOS32_USB_Handler - about a millisecond each, so
// the default is roughly 20 ms. It exists because contacts scrape on the way
// in and a role change is expensive: one stack down, the other up. Raise it
// on a connector that chatters; there is no hurry, a plug is a human gesture.
#ifndef MIOS32_USB_ROLE_SETTLE
# define MIOS32_USB_ROLE_SETTLE     20
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

// Prepares the device port for a hand-over (bootloader <-> application): the
// session stays alive for the other side to adopt, only the interrupt is
// silenced. The host never sees a disconnect.
extern s32 MIOS32_USB_HandoffPrepare(void);


/////////////////////////////////////////////////////////////////////////////
// Implemented per family: clocks, pins, interrupt, role source
/////////////////////////////////////////////////////////////////////////////

extern s32 MIOS32_USB_LL_Init(u8 port, mios32_usb_role_t role);
extern mios32_usb_role_source_t MIOS32_USB_LL_RoleSourceGet(u8 port);

// What the port's role source is asking for AT THIS INSTANT - raw, with no
// debouncing and no memory of the last answer: deciding what a change means
// is the common layer's job, so that every family gets the same policy from
// one place. A port that detects nothing answers MIOS32_USB_ROLE_NONE, which
// is the fixed case saying "the project decides, not me".
extern mios32_usb_role_t MIOS32_USB_LL_RoleDetect(u8 port);

// Is the port's power switch reporting over-current? The switch protects the
// board by itself; this only makes the fact readable.
// Returns 1 asserted, 0 clear, -1 if the port has no flag wired.
extern s32 MIOS32_USB_LL_OverCurrent(u8 port);

// Does this port hold a live device session from a previous life? Read from
// the silicon, never from RAM - a core-only reset wipes the RAM and leaves
// the controller running, which is precisely the state being asked about.
extern s32 MIOS32_USB_LL_DeviceIsWarm(u8 port);

// Silences the port's interrupt (controller global enable + NVIC line) so a
// live session can be adopted with no software to serve it yet.
extern s32 MIOS32_USB_LL_IrqSilence(u8 port);

#endif /* MIOS32_USE_USB */

#endif /* _MIOS32_USB_H */
