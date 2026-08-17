/*
 * USB HID host class.
 *
 * Thin: TinyUSB already talks to the device, this file only decides what an
 * application gets to see. Two views are offered - every raw report as it
 * came in, and boot-protocol keyboards decoded into single key events. The
 * decoded view exists because a keyboard is what an instrument most often
 * hosts, and diffing the 6-key boot report is fiddly enough that every
 * application would otherwise rewrite it.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 *
 * ==========================================================================
 */

#include <mios32.h>

#if defined(MIOS32_USE_USB_HOST_HID)

#include <tusb.h>


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static void (*report_callback)(u8 dev, u8 instance, const u8 *report, u16 len);
static void (*keyboard_callback)(u8 keycode, u8 modifiers, u8 pressed);

// Previous boot report per keyboard interface, for the pressed/released diff.
// Index: TinyUSB interface index, which is unique across attached devices.
typedef struct {
  u8 in_use;
  u8 dev;
  u8 instance;
  u8 report[8]; // boot keyboard report: modifiers, reserved, 6 keycodes
} kb_state_t;

static kb_state_t kb_state[CFG_TUH_HID];

static u8 hid_present;


/////////////////////////////////////////////////////////////////////////////
//! Initializes the USB HID host class.
//! \param[in] mode currently only mode 0 is supported
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_Init(u32 mode)
{
  u8 i;

  if( mode != 0 )
    return -1;

  report_callback = NULL;
  keyboard_callback = NULL;
  hid_present = 0;

  for(i=0; i<CFG_TUH_HID; ++i)
    kb_state[i].in_use = 0;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the raw report callback.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_ReportCallback_Init(void (*callback)(u8 dev, u8 instance, const u8 *report, u16 len))
{
  report_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the decoded keyboard callback.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_KeyboardCallback_Init(void (*callback)(u8 keycode, u8 modifiers, u8 pressed))
{
  keyboard_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \return 1 while at least one HID device is attached
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_CheckAvailable(void)
{
  return hid_present ? 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
// Keyboard decode: diff the previous and the current boot report.
//
// A boot report carries up to six concurrently held keys, unordered. A key
// present now but not before went down; present before but not now, it came
// up. The modifier byte travels with every event so a handler never has to
// keep state of its own.
/////////////////////////////////////////////////////////////////////////////

static u8 report_contains(const u8 *report, u8 keycode)
{
  u8 i;
  for(i=2; i<8; ++i)
    if( report[i] == keycode )
      return 1;
  return 0;
}

static void keyboard_diff(kb_state_t *kb, const u8 *now)
{
  u8 i;

  for(i=2; i<8; ++i) {
    u8 key = now[i];
    if( key && !report_contains(kb->report, key) )
      keyboard_callback(key, now[0], 1); // pressed
  }

  for(i=2; i<8; ++i) {
    u8 key = kb->report[i];
    if( key && !report_contains(now, key) )
      keyboard_callback(key, now[0], 0); // released
  }
}


/////////////////////////////////////////////////////////////////////////////
// TinyUSB host class callbacks
/////////////////////////////////////////////////////////////////////////////

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report_desc, uint16_t desc_len)
{
  (void)report_desc;
  (void)desc_len;

  hid_present = 1;

  if( tuh_hid_interface_protocol(dev_addr, idx) == HID_ITF_PROTOCOL_KEYBOARD ) {
    u8 i;
    for(i=0; i<CFG_TUH_HID; ++i) {
      if( !kb_state[i].in_use ) {
        u8 k;
        kb_state[i].in_use = 1;
        kb_state[i].dev = dev_addr;
        kb_state[i].instance = idx;
        for(k=0; k<8; ++k)
          kb_state[i].report[k] = 0;
        break;
      }
    }
  }

  // Nothing arrives unless asked for: reception must be primed once here,
  // and re-primed after every report (see below).
  tuh_hid_receive_report(dev_addr, idx);
}


void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx)
{
  u8 i, any = 0;

  for(i=0; i<CFG_TUH_HID; ++i) {
    if( kb_state[i].in_use && kb_state[i].dev == dev_addr && kb_state[i].instance == idx )
      kb_state[i].in_use = 0;
    if( kb_state[i].in_use )
      any = 1;
  }

  // Only reports "no HID left" when the LAST one goes - another device may
  // still be attached through the hub.
  if( !any && tuh_hid_itf_get_total_count() == 0 )
    hid_present = 0;
}


void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len)
{
  if( report_callback != NULL )
    report_callback(dev_addr, idx, report, len);

  if( keyboard_callback != NULL && len >= 8 &&
      tuh_hid_interface_protocol(dev_addr, idx) == HID_ITF_PROTOCOL_KEYBOARD ) {
    u8 i;
    for(i=0; i<CFG_TUH_HID; ++i) {
      if( kb_state[i].in_use && kb_state[i].dev == dev_addr && kb_state[i].instance == idx ) {
        u8 k;
        keyboard_diff(&kb_state[i], report);
        for(k=0; k<8; ++k)
          kb_state[i].report[k] = report[k];
        break;
      }
    }
  }

  // Ask for the next one, or the device goes silent after a single report.
  tuh_hid_receive_report(dev_addr, idx);
}

#endif /* MIOS32_USE_USB_HOST_HID */
