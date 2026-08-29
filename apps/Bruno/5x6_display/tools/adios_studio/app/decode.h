// Turns a raw MIDI message into the two things the monitor shows: a short
// human label ("Note On  E4  vel 100") and the raw hex. Pure, no Qt, so it is
// trivially testable and reusable.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace adios {

struct Decoded {
    std::string hex;     // "90 40 64"
    std::string label;   // "Note On   ch 1   E4   vel 100"
    bool        isSysex = false;
    bool        isRealtime = false;   // clock/start/stop/... - the monitor can mute these
};

Decoded decode(const std::vector<uint8_t>& msg);

// "F0 00 22 15 ..." → a vector, tolerant of extra spaces and 0x prefixes.
// Returns false if a token is not a byte. Used by the SysEx send box.
bool parseHexLine(const std::string& text, std::vector<uint8_t>& out);

} // namespace adios
