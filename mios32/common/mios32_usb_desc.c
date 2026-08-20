/*
 * USB descriptors.
 *
 * Everything a host reads to decide what this machine is: the device
 * identity, the MIDI interface with its cables, and the strings the user
 * finally sees in a DAW's port list.
 *
 * The identity itself is NOT here - it is in mios32_usb.h, where a project
 * overrides it. This file only arranges it.
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#include <mios32.h>

#if defined(MIOS32_USE_USB_MIDI)

#include <tusb.h>


/////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////

#define MIOS32_USB_EP_MIDI_OUT   0x01
#define MIOS32_USB_EP_MIDI_IN    0x81
#define MIOS32_USB_EP_MIDI_SIZE  64


/////////////////////////////////////////////////////////////////////////////
// String indices
/////////////////////////////////////////////////////////////////////////////

enum {
  STR_LANGUAGE = 0,
  STR_MANUFACTURER,
  STR_PRODUCT,
  STR_SERIAL,
  STR_MIDI_ITF,
  STR_CABLE_1        // one per cable, consecutive from here
};


/////////////////////////////////////////////////////////////////////////////
// Device descriptor
/////////////////////////////////////////////////////////////////////////////

static const tusb_desc_device_t desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,

  // Declared at interface level, not here: a MIDI device is an Audio class
  // device, and the class lives on the interfaces.
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = MIOS32_USB_VENDOR_ID,
  .idProduct          = MIOS32_USB_PRODUCT_ID,
  .bcdDevice          = MIOS32_USB_VERSION_ID,

  .iManufacturer      = STR_MANUFACTURER,
  .iProduct           = STR_PRODUCT,
  .iSerialNumber      = STR_SERIAL,

  .bNumConfigurations = 1
};

uint8_t const *tud_descriptor_device_cb(void)
{
  return (uint8_t const *)&desc_device;
}


/////////////////////////////////////////////////////////////////////////////
// Configuration descriptor
//
// USB MIDI 1.0 wants four jack descriptors per cable and one cable-ID byte
// per cable inside each endpoint. The preprocessor cannot loop, so the lists
// below are spelled out once, up to the 16 cables the class allows, and the
// right one is picked by MIOS32_USB_MIDI_NUM_PORTS.
/////////////////////////////////////////////////////////////////////////////

#define JACKS_1   TUD_MIDI_DESC_JACK_DESC(1,  STR_CABLE_1 +  0)
#define JACKS_2   JACKS_1,  TUD_MIDI_DESC_JACK_DESC(2,  STR_CABLE_1 +  1)
#define JACKS_3   JACKS_2,  TUD_MIDI_DESC_JACK_DESC(3,  STR_CABLE_1 +  2)
#define JACKS_4   JACKS_3,  TUD_MIDI_DESC_JACK_DESC(4,  STR_CABLE_1 +  3)
#define JACKS_5   JACKS_4,  TUD_MIDI_DESC_JACK_DESC(5,  STR_CABLE_1 +  4)
#define JACKS_6   JACKS_5,  TUD_MIDI_DESC_JACK_DESC(6,  STR_CABLE_1 +  5)
#define JACKS_7   JACKS_6,  TUD_MIDI_DESC_JACK_DESC(7,  STR_CABLE_1 +  6)
#define JACKS_8   JACKS_7,  TUD_MIDI_DESC_JACK_DESC(8,  STR_CABLE_1 +  7)
#define JACKS_9   JACKS_8,  TUD_MIDI_DESC_JACK_DESC(9,  STR_CABLE_1 +  8)
#define JACKS_10  JACKS_9,  TUD_MIDI_DESC_JACK_DESC(10, STR_CABLE_1 +  9)
#define JACKS_11  JACKS_10, TUD_MIDI_DESC_JACK_DESC(11, STR_CABLE_1 + 10)
#define JACKS_12  JACKS_11, TUD_MIDI_DESC_JACK_DESC(12, STR_CABLE_1 + 11)
#define JACKS_13  JACKS_12, TUD_MIDI_DESC_JACK_DESC(13, STR_CABLE_1 + 12)
#define JACKS_14  JACKS_13, TUD_MIDI_DESC_JACK_DESC(14, STR_CABLE_1 + 13)
#define JACKS_15  JACKS_14, TUD_MIDI_DESC_JACK_DESC(15, STR_CABLE_1 + 14)
#define JACKS_16  JACKS_15, TUD_MIDI_DESC_JACK_DESC(16, STR_CABLE_1 + 15)

#define IN_IDS_1   TUD_MIDI_JACKID_IN_EMB(1)
#define IN_IDS_2   IN_IDS_1,  TUD_MIDI_JACKID_IN_EMB(2)
#define IN_IDS_3   IN_IDS_2,  TUD_MIDI_JACKID_IN_EMB(3)
#define IN_IDS_4   IN_IDS_3,  TUD_MIDI_JACKID_IN_EMB(4)
#define IN_IDS_5   IN_IDS_4,  TUD_MIDI_JACKID_IN_EMB(5)
#define IN_IDS_6   IN_IDS_5,  TUD_MIDI_JACKID_IN_EMB(6)
#define IN_IDS_7   IN_IDS_6,  TUD_MIDI_JACKID_IN_EMB(7)
#define IN_IDS_8   IN_IDS_7,  TUD_MIDI_JACKID_IN_EMB(8)
#define IN_IDS_9   IN_IDS_8,  TUD_MIDI_JACKID_IN_EMB(9)
#define IN_IDS_10  IN_IDS_9,  TUD_MIDI_JACKID_IN_EMB(10)
#define IN_IDS_11  IN_IDS_10, TUD_MIDI_JACKID_IN_EMB(11)
#define IN_IDS_12  IN_IDS_11, TUD_MIDI_JACKID_IN_EMB(12)
#define IN_IDS_13  IN_IDS_12, TUD_MIDI_JACKID_IN_EMB(13)
#define IN_IDS_14  IN_IDS_13, TUD_MIDI_JACKID_IN_EMB(14)
#define IN_IDS_15  IN_IDS_14, TUD_MIDI_JACKID_IN_EMB(15)
#define IN_IDS_16  IN_IDS_15, TUD_MIDI_JACKID_IN_EMB(16)

#define OUT_IDS_1   TUD_MIDI_JACKID_OUT_EMB(1)
#define OUT_IDS_2   OUT_IDS_1,  TUD_MIDI_JACKID_OUT_EMB(2)
#define OUT_IDS_3   OUT_IDS_2,  TUD_MIDI_JACKID_OUT_EMB(3)
#define OUT_IDS_4   OUT_IDS_3,  TUD_MIDI_JACKID_OUT_EMB(4)
#define OUT_IDS_5   OUT_IDS_4,  TUD_MIDI_JACKID_OUT_EMB(5)
#define OUT_IDS_6   OUT_IDS_5,  TUD_MIDI_JACKID_OUT_EMB(6)
#define OUT_IDS_7   OUT_IDS_6,  TUD_MIDI_JACKID_OUT_EMB(7)
#define OUT_IDS_8   OUT_IDS_7,  TUD_MIDI_JACKID_OUT_EMB(8)
#define OUT_IDS_9   OUT_IDS_8,  TUD_MIDI_JACKID_OUT_EMB(9)
#define OUT_IDS_10  OUT_IDS_9,  TUD_MIDI_JACKID_OUT_EMB(10)
#define OUT_IDS_11  OUT_IDS_10, TUD_MIDI_JACKID_OUT_EMB(11)
#define OUT_IDS_12  OUT_IDS_11, TUD_MIDI_JACKID_OUT_EMB(12)
#define OUT_IDS_13  OUT_IDS_12, TUD_MIDI_JACKID_OUT_EMB(13)
#define OUT_IDS_14  OUT_IDS_13, TUD_MIDI_JACKID_OUT_EMB(14)
#define OUT_IDS_15  OUT_IDS_14, TUD_MIDI_JACKID_OUT_EMB(15)
#define OUT_IDS_16  OUT_IDS_15, TUD_MIDI_JACKID_OUT_EMB(16)

#define _CAT(a, b)  a##b
#define CAT(a, b)   _CAT(a, b)

#define MIDI_JACKS   CAT(JACKS_,   MIOS32_USB_MIDI_NUM_PORTS)
#define MIDI_IN_IDS  CAT(IN_IDS_,  MIOS32_USB_MIDI_NUM_PORTS)
#define MIDI_OUT_IDS CAT(OUT_IDS_, MIOS32_USB_MIDI_NUM_PORTS)

#define MIOS32_USB_MIDI_DESC_LEN                                  \
  (TUD_MIDI_DESC_HEAD_LEN                                         \
   + MIOS32_USB_MIDI_NUM_PORTS * TUD_MIDI_DESC_JACK_LEN           \
   + 2 * TUD_MIDI_DESC_EP_LEN(MIOS32_USB_MIDI_NUM_PORTS))

#define MIOS32_USB_CONFIG_LEN  (TUD_CONFIG_DESC_LEN + MIOS32_USB_MIDI_DESC_LEN)

// Two interfaces per MIDI function: Audio Control, then MIDI Streaming.
#define MIOS32_USB_ITF_NUM  2

static const uint8_t desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, MIOS32_USB_ITF_NUM, 0, MIOS32_USB_CONFIG_LEN,
                        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  TUD_MIDI_DESC_HEAD(0, STR_MIDI_ITF, MIOS32_USB_MIDI_NUM_PORTS),
  MIDI_JACKS,
  TUD_MIDI_DESC_EP(MIOS32_USB_EP_MIDI_OUT, MIOS32_USB_EP_MIDI_SIZE, MIOS32_USB_MIDI_NUM_PORTS),
  MIDI_IN_IDS,
  TUD_MIDI_DESC_EP(MIOS32_USB_EP_MIDI_IN, MIOS32_USB_EP_MIDI_SIZE, MIOS32_USB_MIDI_NUM_PORTS),
  MIDI_OUT_IDS
};

// A descriptor whose declared length disagrees with the bytes actually
// emitted enumerates erratically rather than failing outright, which is the
// worst way to be wrong. Catch it at build time instead.
_Static_assert(sizeof(desc_configuration) == MIOS32_USB_CONFIG_LEN,
               "USB configuration descriptor: declared length does not match the emitted bytes.");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index;
  return desc_configuration;
}


/////////////////////////////////////////////////////////////////////////////
// Strings
/////////////////////////////////////////////////////////////////////////////

static uint16_t desc_str_buffer[33];

// Serial number, taken from the chip's electronic signature. A fixed serial
// would make every unit look like the same device to a host that remembers
// ports. MIOS32_SYS_SerialNumberGet() already knows where that signature
// lives on each family - its address is not the same everywhere, so reading
// it directly here would put a family fact in the common tree.
static char serial_str[25];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void)langid;

  u8 len = 0;

  if( index == STR_LANGUAGE ) {
    desc_str_buffer[1] = 0x0409; // English (US)
    len = 1;
  } else {
    const char *str = NULL;
    char cable_str[40];

    switch( index ) {
    case STR_MANUFACTURER: str = MIOS32_USB_VENDOR_STR;  break;
    case STR_PRODUCT:      str = MIOS32_USB_PRODUCT_STR; break;
    case STR_MIDI_ITF:     str = MIOS32_USB_PRODUCT_STR; break;

    case STR_SERIAL:
      if( !serial_str[0] )
        MIOS32_SYS_SerialNumberGet(serial_str);
      str = serial_str;
      break;

    default:
      // Per-cable names. This is what a DAW lists, so they are numbered from
      // 1 like the ports on the panel, not from 0 like the array.
      if( index >= STR_CABLE_1 && index < (STR_CABLE_1 + MIOS32_USB_MIDI_NUM_PORTS) ) {
        // Built by hand rather than with sprintf. That one call is enough to
        // pull the whole formatting machinery into the image - around 1.3 kB
        // of printf and the 64-bit division it leans on - which is a great
        // deal to pay for appending a number below 100, and decides on its
        // own whether a bootloader still fits in its flash sector.
        u8 cable = index - STR_CABLE_1 + 1;
        u8 pos = 0;
        const char *p = MIOS32_USB_PRODUCT_STR;
        while( *p && pos < (sizeof(cable_str) - 4) )
          cable_str[pos++] = *p++;
        cable_str[pos++] = 0x20;
        if( cable >= 10 )
          cable_str[pos++] = 0x30 + (cable / 10);
        cable_str[pos++] = 0x30 + (cable % 10);
        cable_str[pos] = 0;
        str = cable_str;
      } else {
        return NULL;
      }
    }

    while( str[len] && len < 31 ) {
      desc_str_buffer[len + 1] = str[len];
      ++len;
    }
  }

  desc_str_buffer[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);

  return desc_str_buffer;
}

#endif /* MIOS32_USE_USB_MIDI */
