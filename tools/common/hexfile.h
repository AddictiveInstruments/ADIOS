// Intel HEX loader. PORTABLE - no OS, shared with the eventual Qt build.
//
// The images this tool sends carry TWO stages in disjoint address ranges (the
// update tool above the app origin, the new bootloader at 0x08000000), so the
// loader keeps them as separate segments rather than flattening everything
// into one span with a hole in the middle.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tr5x6 {

struct HexSegment {
    uint32_t             addr = 0;
    std::vector<uint8_t> data;
};

struct HexImage {
    std::vector<HexSegment> segments;   // in file order, contiguity guaranteed
};

// Records handled: 00 data, 01 EOF, 04 extended linear address.
// A gap of any size starts a new segment.
bool loadHex(const std::string& path, HexImage& out, std::string& err);

} // namespace tr5x6
