/*
 * Header file for MIDI layer
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_MIDI_H
#define _ADIOS_MIDI_H

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// the default MIDI port for MIDI output
#ifndef ADIOS_MIDI_DEFAULT_PORT
#define ADIOS_MIDI_DEFAULT_PORT USB0
#endif

// SysEx device ID this core answers on at startup. A bootloader's persistent
// info block, where there is one, overrides it (see ADIOS_MIDI_Init) - so on
// an instrument with a bootloader this is only the value before anything was
// ever written into that block. It matters most for a project built WITHOUT
// a bootloader (ADIOS_USE_BOOTLOADER = 0): there is no info block to hold an
// ID at all, making this the only way to give the core an identity other
// than 0, short of the application calling ADIOS_MIDI_DeviceIDSet() itself.
#ifndef ADIOS_MIDI_DEFAULT_DEVICE_ID
#define ADIOS_MIDI_DEFAULT_DEVICE_ID 0x00
#endif


// the default MIDI port for debugging output via ADIOS_MIDI_SendDebugMessage
#ifndef ADIOS_MIDI_DEBUG_PORT
#define ADIOS_MIDI_DEBUG_PORT USB0
#endif


/////////////////////////////////////////////////////////////////////////////
// Uses by ADIOS SysEx parser
/////////////////////////////////////////////////////////////////////////////

// MIDI commands and acknowledge reply codes
#define ADIOS_MIDI_SYSEX_TERM_GREETING 0x0c   // host pulls the app's terminal greeting + command list
#define ADIOS_MIDI_SYSEX_DEBUG    0x0d
#define ADIOS_MIDI_SYSEX_DISACK   0x0e
#define ADIOS_MIDI_SYSEX_ACK      0x0f

// core type reported by query 0x0b - "APP" for a normal application (the
// default), overridden to "BSL" by the bootloader build and "UPDATER" by the
// BSL-update tool (see bootloader/). ADIOS Studio bases its upload-range
// protection on this answer.
// Line activity, for an indicator. OPT-IN: define ADIOS_USE_MIDI_ACT in a
// project's adios_config.h to compile it at all - without it there is no
// storage, no marking and no accessor, which is how a bootloader carries none
// of it.
//
// The marking happens inside the MIDI engine itself, so EVERY port reports:
// USB, DIN and SPI alike, in both directions, and whether the traffic arrives
// as packages or byte by byte.
#ifndef ADIOS_MIDI_ACT_MS
# define ADIOS_MIDI_ACT_MS 50
#endif

// Flags returned by ADIOS_MIDI_ActGet(), the same values for every port. RX
// is bit 0 and TX bit 1 ON PURPOSE: that is also how the pair is stored, so
// the accessor hands back the stored bits without translating anything.
#define ADIOS_MIDI_ACT_RX 0x01
#define ADIOS_MIDI_ACT_TX 0x02

#ifndef ADIOS_MIDI_CORE_TYPE_STR
#define ADIOS_MIDI_CORE_TYPE_STR "APP"
#endif

// How this program identifies itself when a host asks. Declare all three in
// your adios_config.h; they are reported by the identification queries and
// are the only way a host can tell one machine from another on a MIDI port.
//
//   #define ADIOS_APP_NAME1   "My Machine"
//   #define ADIOS_APP_NAME2   "(C) 2026 Me"
//   #define ADIOS_APP_VERSION "v1.000"
//
// The bootloader and the BSL-update tool carry their own three, so the host
// can read which bootloader is installed and not only which application.
#ifndef ADIOS_APP_NAME1
#define ADIOS_APP_NAME1 "Unnamed App."
#endif

#ifndef ADIOS_APP_NAME2
#define ADIOS_APP_NAME2 ""
#endif

#ifndef ADIOS_APP_VERSION
#define ADIOS_APP_VERSION "v0.000"
#endif

// disacknowledge arguments
#define ADIOS_MIDI_SYSEX_DISACK_LESS_BYTES_THAN_EXP  0x01
#define ADIOS_MIDI_SYSEX_DISACK_MORE_BYTES_THAN_EXP  0x02
#define ADIOS_MIDI_SYSEX_DISACK_WRONG_CHECKSUM       0x03
#define ADIOS_MIDI_SYSEX_DISACK_WRITE_FAILED         0x04
#define ADIOS_MIDI_SYSEX_DISACK_WRITE_ACCESS         0x05
#define ADIOS_MIDI_SYSEX_DISACK_MIDI_TIMEOUT         0x06
#define ADIOS_MIDI_SYSEX_DISACK_WRONG_DEBUG_CMD      0x07
#define ADIOS_MIDI_SYSEX_DISACK_WRONG_ADDR_RANGE     0x08
#define ADIOS_MIDI_SYSEX_DISACK_ADDR_NOT_ALIGNED     0x09
#define ADIOS_MIDI_SYSEX_DISACK_BS_NOT_AVAILABLE     0x0a
#define ADIOS_MIDI_SYSEX_DISACK_OVERRUN              0x0b
#define ADIOS_MIDI_SYSEX_DISACK_FRAME_ERROR          0x0c
#define ADIOS_MIDI_SYSEX_DISACK_UNKNOWN_QUERY        0x0d
#define ADIOS_MIDI_SYSEX_DISACK_INVALID_COMMAND      0x0e
#define ADIOS_MIDI_SYSEX_DISACK_PROG_ID_NOT_ALLOWED  0x0f
#define ADIOS_MIDI_SYSEX_DISACK_UNSUPPORTED_DEBUG    0x10


/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


typedef enum {
  DEFAULT    = 0x00,
  MIDI_DEBUG = 0x01,

  // One range per USB CONTROLLER, sixteen cables each - sixteen being the
  // ceiling of USB MIDI 1.0, whose cable number is a 4-bit field.
  //
  // The range says which controller, not which role: a controller may be a
  // device or a host, and the cables are numbered the same way either way.
  // So a keyboard plugged into the second socket arrives on USB16..USB31.
  USB0 = 0x10,
  USB1 = 0x11,
  USB2 = 0x12,
  USB3 = 0x13,
  USB4 = 0x14,
  USB5 = 0x15,
  USB6 = 0x16,
  USB7 = 0x17,
  USB8 = 0x18,
  USB9 = 0x19,
  USB10 = 0x1a,
  USB11 = 0x1b,
  USB12 = 0x1c,
  USB13 = 0x1d,
  USB14 = 0x1e,
  USB15 = 0x1f,

  USB16 = 0x20,
  USB17 = 0x21,
  USB18 = 0x22,
  USB19 = 0x23,
  USB20 = 0x24,
  USB21 = 0x25,
  USB22 = 0x26,
  USB23 = 0x27,
  USB24 = 0x28,
  USB25 = 0x29,
  USB26 = 0x2a,
  USB27 = 0x2b,
  USB28 = 0x2c,
  USB29 = 0x2d,
  USB30 = 0x2e,
  USB31 = 0x2f,

  // named after the classic 5-pin DIN MIDI connector - the physical/
  // hardware MIDI ports, as opposed to USBn/SPIMn. Not "UARTn":
  // that bare name collides with real CMSIS peripheral macros (UART4/5/7/
  // 8/9/10 on STM32F4/F7/H7) and with this driver's own ADIOS_UARTn
  // logical-port aliases in adios_uart.c.
  DIN0 = 0x30,
  DIN1 = 0x31,
  DIN2 = 0x32,
  DIN3 = 0x33,
  DIN4 = 0x34,
  DIN5 = 0x35,
  DIN6 = 0x36,
  DIN7 = 0x37,
  DIN8 = 0x38,
  DIN9 = 0x39,
  DIN10 = 0x3a,
  DIN11 = 0x3b,
  DIN12 = 0x3c,
  DIN13 = 0x3d,
  DIN14 = 0x3e,
  DIN15 = 0x3f,

  // The ranges were compacted on 2026-08-17, when the second USB controller
  // took 0x20: DIN moved 0x20 -> 0x30 and SPIM 0x50 -> 0x40. That was the
  // planned realignment, not an accident, and it is the reason the previous
  // note here - which said 0x30 and 0x40 were to stay empty - is gone.
  //
  // These numbers still travel: they appear in SysEx and in host-side tools.
  // So a new transport takes the next free range, 0x50, and does not reuse
  // one that has meant something else before.
  SPIM0 = 0x40,
  SPIM1 = 0x41,
  SPIM2 = 0x42,
  SPIM3 = 0x43,
  SPIM4 = 0x44,
  SPIM5 = 0x45,
  SPIM6 = 0x46,
  SPIM7 = 0x47,
  SPIM8 = 0x48,
  SPIM9 = 0x49,
  SPIM10 = 0x4a,
  SPIM11 = 0x4b,
  SPIM12 = 0x4c,
  SPIM13 = 0x4d,
  SPIM14 = 0x4e,
  SPIM15 = 0x4f
 
} adios_midi_port_t;


typedef enum {
  NoteOff       = 0x8,
  NoteOn        = 0x9,
  PolyPressure  = 0xa,
  CC            = 0xb,
  ProgramChange = 0xc,
  Aftertouch    = 0xd,
  PitchBend     = 0xe
} adios_midi_event_t;


typedef enum {
  Chn1,
  Chn2,
  Chn3,
  Chn4,
  Chn5,
  Chn6,
  Chn7,
  Chn8,
  Chn9,
  Chn10,
  Chn11,
  Chn12,
  Chn13,
  Chn14,
  Chn15,
  Chn16
} adios_midi_chn_t;


typedef union {
  struct {
    u32 ALL;
  };
  struct {
    u8 bytes[4];
  };
  struct {
    u8 cin_cable;
    u8 evnt0;
    u8 evnt1;
    u8 evnt2;
  };
  struct {
    u8 type:4;
    u8 cable:4;
    u8 chn:4; // adios_midi_chn_t
    u8 event:4; // adios_midi_event_t
    u8 value1;
    u8 value2;
  };

  // C++ doesn't allow to redefine names in anonymous unions
  // as a simple workaround, we rename these redundant names
  struct {
    u8 cin:4;
    u8 dummy1_cable:4;
    u8 dummy1_chn:4; // adios_midi_chn_t 
    u8 dummy1_event:4; // adios_midi_event_t 
    u8 note:8;
    u8 velocity:8;
  };
  struct {
    u8 dummy2_cin:4;
    u8 dummy2_cable:4;
    u8 dummy2_chn:4; // adios_midi_chn_t 
    u8 dummy2_event:4; // adios_midi_event_t 
    u8 cc_number:8;
    u8 value:8;
  };
  struct {
    u8 dummy3_cin:4;
    u8 dummy3_cable:4;
    u8 dummy3_chn:4; // adios_midi_chn_t 
    u8 dummy3_event:4; // adios_midi_event_t
    u8 program_change:8;
    u8 dummy3:8;
  };
} adios_midi_package_t;


// command states
typedef enum {
  ADIOS_MIDI_SYSEX_CMD_STATE_BEGIN,
  ADIOS_MIDI_SYSEX_CMD_STATE_CONT,
  ADIOS_MIDI_SYSEX_CMD_STATE_END
} adios_midi_sysex_cmd_state_t;


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_MIDI_Init(u32 mode);

extern s32 ADIOS_MIDI_CheckAvailable(adios_midi_port_t port);

extern s32 ADIOS_MIDI_RS_OptimisationSet(adios_midi_port_t port, u8 enable);
extern s32 ADIOS_MIDI_RS_OptimisationGet(adios_midi_port_t port);
extern s32 ADIOS_MIDI_RS_Reset(adios_midi_port_t port);

extern s32 ADIOS_MIDI_SendPackage_NonBlocking(adios_midi_port_t port, adios_midi_package_t package);
extern s32 ADIOS_MIDI_SendPackage(adios_midi_port_t port, adios_midi_package_t package);

extern s32 ADIOS_MIDI_SendEvent(adios_midi_port_t port, u8 evnt0, u8 evnt1, u8 evnt2);
extern s32 ADIOS_MIDI_SendNoteOff(adios_midi_port_t port, adios_midi_chn_t chn, u8 note, u8 vel);
extern s32 ADIOS_MIDI_SendNoteOn(adios_midi_port_t port, adios_midi_chn_t chn, u8 note, u8 vel);
extern s32 ADIOS_MIDI_SendPolyPressure(adios_midi_port_t port, adios_midi_chn_t chn, u8 note, u8 val);
extern s32 ADIOS_MIDI_SendCC(adios_midi_port_t port, adios_midi_chn_t chn, u8 cc_number, u8 val);
extern s32 ADIOS_MIDI_SendProgramChange(adios_midi_port_t port, adios_midi_chn_t chn, u8 prg);
extern s32 ADIOS_MIDI_SendAftertouch(adios_midi_port_t port, adios_midi_chn_t chn, u8 val);
extern s32 ADIOS_MIDI_SendPitchBend(adios_midi_port_t port, adios_midi_chn_t chn, u16 val);

extern s32 ADIOS_MIDI_SendSysEx(adios_midi_port_t port, u8 *stream, u32 count);
extern s32 ADIOS_MIDI_SendMTC(adios_midi_port_t port, u8 val);
extern s32 ADIOS_MIDI_SendSongPosition(adios_midi_port_t port, u16 val);
extern s32 ADIOS_MIDI_SendSongSelect(adios_midi_port_t port, u8 val);
extern s32 ADIOS_MIDI_SendTuneRequest(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendClock(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendTick(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendStart(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendStop(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendContinue(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendActiveSense(adios_midi_port_t port);
extern s32 ADIOS_MIDI_SendReset(adios_midi_port_t port);

extern s32 ADIOS_MIDI_SendDebugStringHeader(adios_midi_port_t port, char command, char first_byte);
extern s32 ADIOS_MIDI_SendDebugStringBody(adios_midi_port_t port, char *str_from_second_byte, u32 len);
extern s32 ADIOS_MIDI_SendDebugStringFooter(adios_midi_port_t port);

extern s32 ADIOS_MIDI_SendDebugMessage(const char *format, ...);
extern s32 ADIOS_MIDI_SendDebugString(const char *str);
extern s32 ADIOS_MIDI_SendDebugHexDump(const u8 *src, u32 len);

extern s32 ADIOS_MIDI_ReceivePackage(adios_midi_port_t port, adios_midi_package_t package, void *_callback_package);
extern s32 ADIOS_MIDI_Receive_Handler(void *callback_event);

extern s32 ADIOS_MIDI_Periodic_mS(void);

// Line activity of ONE port, READ AND CLEAR. Flags also expire on their own
// after ADIOS_MIDI_ACT_MS, so an indicator that stops being polled goes out
// instead of staying lit for ever.
//   if( ADIOS_MIDI_ActGet(DIN2) & ADIOS_MIDI_ACT_RX ) // something came in
extern s32 ADIOS_MIDI_ActGet(adios_midi_port_t port);

extern s32 ADIOS_MIDI_DirectTxCallback_Init(s32 (*callback_tx)(adios_midi_port_t port, adios_midi_package_t package));
extern s32 ADIOS_MIDI_DirectRxCallback_Init(s32 (*callback_rx)(adios_midi_port_t port, u8 midi_byte));

extern s32 ADIOS_MIDI_SendByteToRxCallback(adios_midi_port_t port, u8 midi_byte);
extern s32 ADIOS_MIDI_SendPackageToRxCallback(adios_midi_port_t port, adios_midi_package_t midi_package);

extern s32 ADIOS_MIDI_DefaultPortSet(adios_midi_port_t port);
extern adios_midi_port_t ADIOS_MIDI_DefaultPortGet(void);

extern s32 ADIOS_MIDI_DebugPortSet(adios_midi_port_t port);
extern adios_midi_port_t ADIOS_MIDI_DebugPortGet(void);

extern s32 ADIOS_MIDI_DeviceIDSet(u8 device_id);
extern u8  ADIOS_MIDI_DeviceIDGet(void);

extern s32 ADIOS_MIDI_SysExCallback_Init(s32 (*callback_sysex)(adios_midi_port_t port, u8 sysex_byte));

extern s32 ADIOS_MIDI_DebugCommandCallback_Init(s32 (*callback_debug_command)(adios_midi_port_t port, char c));
extern s32 ADIOS_MIDI_QueryCallback_Init(s32 (*callback_query)(adios_midi_port_t port, u8 query));
extern s32 ADIOS_MIDI_TermGreetingCallback_Init(s32 (*callback_greeting)(adios_midi_port_t port));
extern s32 ADIOS_MIDI_FilebrowserCommandCallback_Init(s32 (*callback_filebrowser_command)(adios_midi_port_t port, char c));

extern s32 ADIOS_MIDI_TimeOutCallback_Init(s32 (*callback_timeout)(adios_midi_port_t port));


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

extern const u8 adios_midi_pcktype_num_bytes[16];
extern const u8 adios_midi_expected_bytes_common[8];
extern const u8 adios_midi_expected_bytes_system[16];

extern const u8 adios_midi_sysex_header[5];

#endif /* _ADIOS_MIDI_H */
