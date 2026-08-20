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
static void (*mouse_callback)(s8 dx, s8 dy, s8 wheel, u8 buttons);
static void (*change_callback)(u8 dev, u8 itf, mios32_usb_hid_type_t type, u8 connected);

// What each attached interface turned out to be, so an application can ask
// without keeping its own tally.
static mios32_usb_hid_type_t itf_type[CFG_TUH_HID];

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

// A HID device only speaks when asked, so a request must be standing at all
// times - and asking can fail, transiently, when the bus is busy elsewhere.
// A dropped request is not recoverable by itself: the device simply never
// reports again, which looks like a keyboard that dies after a few keystrokes
// rather than like an error. So a failed request is remembered here and
// retried from the periodic call below.
static struct {
  u8 pending; // a request is owed for this interface
  u8 dev;
} arm[CFG_TUH_HID];


/////////////////////////////////////////////////////////////////////////////
// Asks for the next report, and remembers the request if it could not be
// placed. Every request goes through here - there is no other caller of
// tuh_hid_receive_report() in this file, by design.
/////////////////////////////////////////////////////////////////////////////

// TEMPORARY diagnostic: how often a request had to be deferred, and how often
// the retry then placed it. Read over SWD - it is what distinguishes "the fix
// works" from "the failure did not happen this time". Remove with the rest of
// the bench scaffolding.

// Arrivals and departures, held until the periodic call can report them.
//
// They are NOT passed on the moment they happen: that moment is inside the
// stack's own bringing-up of the device, and application code has no business
// running there - it disturbs the enumeration still in progress, which shows
// up as other devices on the same bus never appearing at all. Noted here,
// delivered a tick later, where anything is allowed.
#define HID_EVENT_MAX 8

static struct {
  u8 connected;
  u8 dev;
  u8 itf;
  mios32_usb_hid_type_t type;
} event_q[HID_EVENT_MAX];

static u8 event_n;

static void event_add(u8 connected, u8 dev, u8 itf, mios32_usb_hid_type_t type)
{
  if( event_n < HID_EVENT_MAX ) {
    event_q[event_n].connected = connected;
    event_q[event_n].dev  = dev;
    event_q[event_n].itf  = itf;
    event_q[event_n].type = type;
    ++event_n;
  }
}

static void request_report(u8 dev_addr, u8 idx)
{

  if( idx >= CFG_TUH_HID )
    return;


  arm[idx].dev = dev_addr;
  arm[idx].pending = tuh_hid_receive_report(dev_addr, idx) ? 0 : 1;

}


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
  mouse_callback = NULL;
  change_callback = NULL;
  hid_present = 0;
  event_n = 0;

  for(i=0; i<CFG_TUH_HID; ++i) {
    kb_state[i].in_use = 0;
    arm[i].pending = 0;
    itf_type[i] = MIOS32_USB_HID_TYPE_NONE;
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the hook called when an interface arrives or leaves.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_ChangeCallback_Init(void (*callback)(u8 dev, u8 itf, mios32_usb_hid_type_t type, u8 connected))
{
  change_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs the decoded mouse callback.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_MouseCallback_Init(void (*callback)(s8 dx, s8 dy, s8 wheel, u8 buttons))
{
  mouse_callback = callback;
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! \param[in] itf the interface index
//! \return what is on it, MIOS32_USB_HID_TYPE_NONE if nothing
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_TypeGet(u8 itf)
{
  if( itf >= CFG_TUH_HID )
    return MIOS32_USB_HID_TYPE_NONE;

  return itf_type[itf];
}


/////////////////////////////////////////////////////////////////////////////
//! Retries whatever could not be asked for earlier. Called from the USB
//! layer right after the host stack has run, so a request that failed while
//! the bus was busy gets placed as soon as it frees up.
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_USB_HID_Periodic_mS(void)
{
  u8 idx;

  // Arrivals and departures, now that we are out of the stack's own work.
  while( event_n ) {
    u8 i;

    if( change_callback != NULL )
      change_callback(event_q[0].dev, event_q[0].itf, event_q[0].type, event_q[0].connected);

    for(i=1; i<event_n; ++i)
      event_q[i-1] = event_q[i];
    --event_n;
  }

  for(idx=0; idx<CFG_TUH_HID; ++idx)
    if( arm[idx].pending )
      request_report(arm[idx].dev, idx);

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

// A boot report can carry something that is not a list of keys at all. When
// more keys are held than the protocol can describe, the keyboard fills every
// one of the six slots with the same reserved code - 0x01 ErrorRollOver, or
// 0x02/0x03 for its own faults. It is the keyboard saying "I cannot report
// this combination", and the keys actually held are unchanged and unknown.
//
// Such a report must be dropped whole. Read as keys it produces six identical
// phantom presses, and the real keys go missing - then the next valid report
// looks like six releases at once.
static u8 report_is_error(const u8 *report)
{
  u8 i;
  for(i=2; i<8; ++i)
    if( report[i] >= 1 && report[i] <= 3 )
      return 1;
  return 0;
}

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

  {
    mios32_usb_hid_type_t type;

    switch( tuh_hid_interface_protocol(dev_addr, idx) ) {
    case HID_ITF_PROTOCOL_KEYBOARD: type = MIOS32_USB_HID_TYPE_KEYBOARD; break;
    case HID_ITF_PROTOCOL_MOUSE:    type = MIOS32_USB_HID_TYPE_MOUSE;    break;
    default:                        type = MIOS32_USB_HID_TYPE_GENERIC;  break;
    }

    if( idx < CFG_TUH_HID )
      itf_type[idx] = type;

    event_add(1, dev_addr, idx, type);

    if( type == MIOS32_USB_HID_TYPE_KEYBOARD ) {
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
  }

  // Nothing arrives unless asked for - but the asking is deliberately NOT done
  // here. Starting a transfer from inside a mount callback disturbs the stack
  // while it is still bringing devices up: with several devices arriving
  // together behind a hub, the ones still to be enumerated can be missed, and
  // transfers already placed can stop completing. So the request is only
  // marked as owed, and the periodic call places it on the next turn, once
  // enumeration has had its moment.
  if( idx < CFG_TUH_HID ) {
    arm[idx].dev = dev_addr;
    arm[idx].pending = 1;
  }
}


void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx)
{
  u8 i, any = 0;

  event_add(0, dev_addr, idx, (idx < CFG_TUH_HID) ? itf_type[idx] : MIOS32_USB_HID_TYPE_NONE);

  // Drop any request still owed to it, or the retry would keep asking a
  // device that has left - and would take the interface index with it when
  // the next device is given the same one.
  if( idx < CFG_TUH_HID ) {
    arm[idx].pending = 0;
    itf_type[idx] = MIOS32_USB_HID_TYPE_NONE;
  }

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

  // A boot mouse report is buttons, then two movements, then the wheel if it
  // has one. The movements are signed and RELATIVE - how far it moved since
  // the last report, which is all a mouse ever knows.
  if( mouse_callback != NULL && len >= 3 &&
      tuh_hid_interface_protocol(dev_addr, idx) == HID_ITF_PROTOCOL_MOUSE ) {
    mouse_callback((s8)report[1], (s8)report[2],
                   (len >= 4) ? (s8)report[3] : 0,
                   report[0]);
  }

  if( keyboard_callback != NULL && len >= 8 && !report_is_error(report) &&
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
  request_report(dev_addr, idx);
}

#endif /* MIOS32_USE_USB_HOST_HID */
