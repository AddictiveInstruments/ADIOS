// SysEx framing for the ADIOS bootloader and the 5x6 application.
//
// PORTABLE: nothing here touches an OS. This file and sysex.cpp are the part
// that survives - the console prototype and the eventual Qt application share
// them unchanged, and only the MIDI transport underneath differs.
//
// Everything below was read out of the firmware, not guessed:
//   - the header and the strict device-id equality  adios_midi.c:101, :1777
//   - the address/length encoding                   bsl_sysex.c RecAddrAndLen
//   - the 7-bit payload packing                     bsl_sysex.c payload loop
//   - the checksum                                  bsl_sysex.c:618
// and the same code was verified byte-identical in the June 2025 tree
// (D:/MIOS32/trunk), so ONE encoder serves both the old and the new bootloader.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tr5x6 {

using Bytes = std::vector<uint8_t>;

// The fifth header byte says WHO is being addressed.
enum class Target : uint8_t {
    Os  = 0x32,   // bootloader, updater, and the OS inside an application
    App = 0x44,   // the 5x6 application's own protocol
};

// Bootloader commands (bsl_sysex.c). The old June 2025 bootloader knows only
// ReadMem and WriteMem; EntryOverride was added in August 2026.
enum : uint8_t {
    CMD_QUERY          = 0x00,
    CMD_READ_MEM       = 0x01,
    CMD_WRITE_MEM      = 0x02,
    CMD_ENTRY_OVERRIDE = 0x03,
    TERM_GREETING      = 0x0c,   // OS: pull the app's terminal greeting + commands
    CMD_PING           = 0x0f,
    CMD_RESET          = 0x7f,   // "release halt state" / restart request
};

// 5x6 application commands (tr5x6_sysex.h).
enum : uint8_t {
    APP_BANK_READ_INFO  = 0x01,
    APP_SLOT_READ_INFO  = 0x03,
    APP_READ_BLOCK      = 0x05,
    APP_WRITE_BLOCK     = 0x06,
    APP_PING            = 0x0f,
};

// Query sub-commands - AND THEY ARE NUMBERED DIFFERENTLY ON THE TWO CORES.
// The current firmware merged the old "Board" and "Core Family" entries into
// one "Processor", so everything from there on moved UP BY ONE, and 0x09
// (Version), 0x0a (Boundary) and 0x0b (Core type) are new. Measured on a real
// June 2025 board on 2026-08-27: asking for 0x05 expecting the flash size
// returns the SERIAL NUMBER instead.
//   old: 01 OS  02 Board  03 Family  04 ChipID 05 Serial 06 Flash 07 RAM
//        08 AppName1     09 AppName2
//   new: 01 OS  02 Proc   03 ChipID  04 Serial 05 Flash  06 RAM   07 AppName1
//        08 AppName2     09 Version  0a Boundary  0b CoreType
// Only 0x01 is common ground - which is exactly what makes it usable as the
// discriminator: it answers "MIOS32" on an old core and "ADIOS" on a new one.
enum : uint8_t { Q_OS_NAME = 0x01 };

// What a host actually wants to know, independent of the numbering.
enum class QueryItem { Processor, ChipId, Serial, Flash, Ram, AppName1,
                       AppName2, Version, Boundary, CoreType };

// The sub-command byte for that item on this core, or 0 when the core has no
// such entry (Version, Boundary and CoreType on a June 2025 board).
uint8_t querySub(QueryItem item, bool legacyCore);
const char* queryLabel(QueryItem item);

// Acknowledge codes carried in the response's command byte.
enum : uint8_t { ACK = 0x0f, DISACK = 0x0e };

// DISACK arguments (adios_midi.h) - what the board says went wrong.
enum : uint8_t {
    DIS_LESS_BYTES   = 0x01, DIS_MORE_BYTES  = 0x02, DIS_WRONG_CHECKSUM = 0x03,
    DIS_WRITE_FAILED = 0x04, DIS_WRONG_ADDR  = 0x08, DIS_UNALIGNED      = 0x09,
    DIS_UNKNOWN_QUERY= 0x0d, DIS_INVALID_CMD = 0x0e,
};

// ---------------------------------------------------------------------------

// F0 00 22 15 <target> <device_id> <cmd>  - the opening of every message.
Bytes header(Target t, uint8_t device_id, uint8_t cmd);

// Address and length travel as four 7-bit septets each, MSB first, DIVIDED BY
// 16 - the granularity is imposed by the encoding itself, so both must be
// multiples of 16. bsl_sysex.c reassembles them as
//     addr = A3<<25 | A2<<18 | A1<<11 | A0<<4
void appendAddrLen(Bytes& out, uint32_t addr, uint32_t len);

// The payload is the raw bytes seen as one bit stream, MSB first, re-emitted 7
// bits at a time with the first bit of each septet at 0x40. A trailing partial
// septet is LEFT-aligned; the board ignores the padding because it stops
// storing once it has `len` bytes.
Bytes pack7(const uint8_t* data, size_t len);

// (-sum) & 0x7f over the address/length septets AND every payload septet -
// never the header, the device id or the command byte.
uint8_t checksum(const Bytes& addrLenAndPayload);

// A complete write-memory message, ready to send.
//   F0 00 22 15 32 <id> 02 A3 A2 A1 A0 L3 L2 L1 L0 <packed> CS F7
// `len` must be a multiple of 16 and at most 1024 (BSL_SYSEX_MAX_BYTES).
Bytes writeMem(uint8_t device_id, uint32_t addr, const uint8_t* data, size_t len);

// F0 00 22 15 32 <id> 00 <sub> F7
Bytes query(uint8_t device_id, uint8_t sub);

// F0 00 22 15 32 <id> 0C F7 - asks a terminal-aware application to (re-)print
// its greeting and command list. DISACK back means "no terminal here".
Bytes termGreeting(uint8_t device_id);

// F0 00 22 15 <target> <id> 0F F7 - and NOTHING between the 0x0f and the F7:
// one stray byte and the board stays silent (adios_midi.c:2058, anti-loop).
Bytes ping(Target t, uint8_t device_id);

// F0 00 22 15 32 <id> 7F F7 - asks a running application to restart into its
// bootloader, or releases a halted bootloader.
Bytes reset(uint8_t device_id);

// F0 00 22 15 32 <id> 01 A3 A2 A1 A0 L3 L2 L1 L0 F7 - no checksum on the way out.
Bytes readMem(uint8_t device_id, uint32_t addr, uint32_t len);

// Unwinds a dump payload back into raw bytes.
Bytes unpack7(const uint8_t* septets, size_t n, size_t wanted);

// ---------------------------------------------------------------------------

struct Reply {
    bool     valid    = false;   // header matched
    Target   target   = Target::Os;
    uint8_t  deviceId = 0;
    uint8_t  cmd      = 0;       // 0x0f ACK, 0x0e DISACK, ...
    uint8_t  arg      = 0;       // first payload byte, when there is one
    Bytes    payload;            // everything between cmd and F7
    std::string text() const;    // payload as ASCII (query answers)
};

// Recognises one complete F0..F7 message. Returns valid=false for anything
// that is not ours.
Reply parse(const uint8_t* msg, size_t len);

} // namespace tr5x6
