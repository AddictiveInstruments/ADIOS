/*
 * Header file for the USB HID host class.
 *
 * ==========================================================================
 *
 * HOW TO USE IT
 *
 * 1) Ask for the class in your mios32_config.h:
 *
 *      #define MIOS32_USE_USB_HOST_HID
 *
 * 2) Install a callback for what you care about:
 *
 *      // a keyboard, decoded to key events (boot protocol)
 *      MIOS32_USB_HID_KeyboardCallback_Init(my_key_handler);
 *      // ...or every raw report, any HID device
 *      MIOS32_USB_HID_ReportCallback_Init(my_report_handler);
 *
 * The key handler receives (keycode, modifiers, pressed) per key change; the
 * raw handler receives every report as the device sent it. Both may be
 * installed at once - the raw one sees keyboard reports too.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#ifndef _MIOS32_USB_HID_H
#define _MIOS32_USB_HID_H

#if defined(MIOS32_USE_USB_HOST_HID)

extern s32 MIOS32_USB_HID_Init(u32 mode);

// Called by the USB layer once the host stack has run - not by an application.
extern s32 MIOS32_USB_HID_Periodic_mS(void);

// Raw reports, any device. dev/instance identify who sent it.
extern s32 MIOS32_USB_HID_ReportCallback_Init(void (*callback)(u8 dev, u8 instance, const u8 *report, u16 len));

// Boot-protocol keyboards, decoded: one call per key going down or up.
// modifiers is the usual HID bitmap (ctrl/shift/alt/gui, left and right).
extern s32 MIOS32_USB_HID_KeyboardCallback_Init(void (*callback)(u8 keycode, u8 modifiers, u8 pressed));

// 1 while at least one HID device is attached and delivering.
extern s32 MIOS32_USB_HID_CheckAvailable(void);

#endif /* MIOS32_USE_USB_HOST_HID */

#endif /* _MIOS32_USB_HID_H */
