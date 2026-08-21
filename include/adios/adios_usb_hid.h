/*
 * Header file for the USB HID host class.
 *
 * ==========================================================================
 *
 * HOW TO USE IT
 *
 * 1) Ask for the class in your adios_config.h:
 *
 *      #define ADIOS_USE_USB_HOST_HID
 *
 * 2) Install a callback for what you care about:
 *
 *      // a keyboard, decoded to key events (boot protocol)
 *      ADIOS_USB_HID_KeyboardCallback_Init(my_key_handler);
 *      // ...or every raw report, any HID device
 *      ADIOS_USB_HID_ReportCallback_Init(my_report_handler);
 *
 * The key handler receives (keycode, modifiers, pressed) per key change; the
 * raw handler receives every report as the device sent it. Both may be
 * installed at once - the raw one sees keyboard reports too.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_USB_HID_H
#define _ADIOS_USB_HID_H

#if defined(ADIOS_USE_USB_HOST_HID)

// What an attached interface turned out to be.
typedef enum {
  ADIOS_USB_HID_TYPE_NONE = 0,
  ADIOS_USB_HID_TYPE_KEYBOARD,
  ADIOS_USB_HID_TYPE_MOUSE,
  ADIOS_USB_HID_TYPE_GENERIC  // something else: raw reports only
} adios_usb_hid_type_t;


extern s32 ADIOS_USB_HID_Init(u32 mode);

// Called by the USB layer once the host stack has run - not by an application.
extern s32 ADIOS_USB_HID_Periodic_mS(void);

// Told when an interface arrives or leaves, with what it is. One device can
// bring several interfaces - a keyboard usually presents two - so this is
// called once per interface, not once per device.
//
// Reported from the periodic call rather than the moment it happens: the
// arrival lands while the stack is still bringing the device up, which is no
// place to run application code. The delay is one pass of the tick.
extern s32 ADIOS_USB_HID_ChangeCallback_Init(void (*callback)(u8 dev, u8 itf, adios_usb_hid_type_t type, u8 connected));

// Raw reports, any device. dev/instance identify who sent it.
extern s32 ADIOS_USB_HID_ReportCallback_Init(void (*callback)(u8 dev, u8 instance, const u8 *report, u16 len));

// Boot-protocol keyboards, decoded: one call per key going down or up.
// modifiers is the usual HID bitmap (ctrl/shift/alt/gui, left and right).
extern s32 ADIOS_USB_HID_KeyboardCallback_Init(void (*callback)(u8 keycode, u8 modifiers, u8 pressed));

// Boot-protocol mice, decoded. dx/dy/wheel are movements SINCE THE LAST
// REPORT, not positions - a mouse has no idea where it is. buttons is a
// bitmap: bit 0 left, bit 1 right, bit 2 middle.
extern s32 ADIOS_USB_HID_MouseCallback_Init(void (*callback)(s8 dx, s8 dy, s8 wheel, u8 buttons));

// What is on that interface right now, ADIOS_USB_HID_TYPE_NONE if nothing.
extern s32 ADIOS_USB_HID_TypeGet(u8 itf);

// 1 while at least one HID device is attached and delivering.
extern s32 ADIOS_USB_HID_CheckAvailable(void);

#endif /* ADIOS_USE_USB_HOST_HID */

#endif /* _ADIOS_USB_HID_H */
