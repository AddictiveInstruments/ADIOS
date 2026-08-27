#include "hexfile.h"
#include <cstdio>
#include <cstdlib>

namespace tr5x6 {

namespace {

int hexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool hexByte(const std::string& s, size_t pos, uint8_t& out)
{
    if (pos + 1 >= s.size()) return false;
    const int hi = hexNibble(s[pos]), lo = hexNibble(s[pos + 1]);
    if (hi < 0 || lo < 0) return false;
    out = static_cast<uint8_t>((hi << 4) | lo);
    return true;
}

} // namespace

bool loadHex(const std::string& path, HexImage& out, std::string& err)
{
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open " + path; return false; }

    out.segments.clear();
    uint32_t upper = 0;            // extended linear address, record type 04
    unsigned lineNo = 0;
    char buf[600];

    while (std::fgets(buf, sizeof(buf), f)) {
        ++lineNo;
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty()) continue;
        if (line[0] != ':') { std::fclose(f); err = "line " + std::to_string(lineNo) + ": no ':'"; return false; }

        uint8_t len = 0, offHi = 0, offLo = 0, type = 0;
        if (!hexByte(line, 1, len) || !hexByte(line, 3, offHi) ||
            !hexByte(line, 5, offLo) || !hexByte(line, 7, type)) {
            std::fclose(f); err = "line " + std::to_string(lineNo) + ": bad record header"; return false;
        }

        // checksum over the whole record, including the trailing byte: must be 0
        uint8_t sum = 0;
        const size_t nbytes = static_cast<size_t>(len) + 5;   // len+addr+type+data+cs
        for (size_t i = 0; i < nbytes; ++i) {
            uint8_t b = 0;
            if (!hexByte(line, 1 + i * 2, b)) {
                std::fclose(f); err = "line " + std::to_string(lineNo) + ": truncated"; return false;
            }
            sum = static_cast<uint8_t>(sum + b);
        }
        if (sum != 0) { std::fclose(f); err = "line " + std::to_string(lineNo) + ": bad checksum"; return false; }

        if (type == 0x01) break;                       // EOF

        if (type == 0x04) {                            // extended linear address
            uint8_t hi = 0, lo = 0;
            if (len != 2 || !hexByte(line, 9, hi) || !hexByte(line, 11, lo)) {
                std::fclose(f); err = "line " + std::to_string(lineNo) + ": bad type 04"; return false;
            }
            upper = (static_cast<uint32_t>(hi) << 24) | (static_cast<uint32_t>(lo) << 16);
            continue;
        }

        if (type != 0x00) continue;                    // 02/03/05 ignored

        const uint32_t addr = upper |
                              (static_cast<uint32_t>(offHi) << 8) | offLo;

        // extend the current segment when this record follows it exactly,
        // otherwise open a new one - that is what keeps the two stages apart
        if (out.segments.empty() ||
            out.segments.back().addr + out.segments.back().data.size() != addr) {
            out.segments.push_back(HexSegment{addr, {}});
        }
        for (uint8_t i = 0; i < len; ++i) {
            uint8_t b = 0;
            hexByte(line, 9 + static_cast<size_t>(i) * 2, b);
            out.segments.back().data.push_back(b);
        }
    }

    std::fclose(f);
    if (out.segments.empty()) { err = "no data records in " + path; return false; }
    return true;
}

} // namespace tr5x6
