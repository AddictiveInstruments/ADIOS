#include "decode.h"

#include <cctype>
#include <cstdio>
#include <sstream>

namespace adios {

namespace {
const char* NOTE_NAMES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

std::string noteName(uint8_t n)
{
    char b[8];
    std::snprintf(b, sizeof(b), "%s%d", NOTE_NAMES[n % 12], int(n) / 12 - 1);
    return b;
}

std::string hexDump(const std::vector<uint8_t>& m, size_t cap = 32)
{
    std::string s;
    char b[4];
    const size_t n = m.size() < cap ? m.size() : cap;
    for (size_t i = 0; i < n; ++i) {
        std::snprintf(b, sizeof(b), "%02X", m[i]);
        if (i) s += ' ';
        s += b;
    }
    if (m.size() > cap) s += " ...";
    return s;
}

std::string ch(uint8_t status) { return "ch " + std::to_string((status & 0x0f) + 1); }
} // namespace

Decoded decode(const std::vector<uint8_t>& m)
{
    Decoded d;
    d.hex = hexDump(m);
    if (m.empty()) { d.label = "(vide)"; return d; }

    const uint8_t s = m[0];
    const uint8_t hi = s & 0xf0;
    auto d1 = [&](int i) { return i < (int)m.size() ? m[i] : 0; };

    if (s == 0xf0) {
        d.isSysex = true;
        // Recognise our own family for a friendlier line.
        if (m.size() >= 5 && m[1] == 0x00 && m[2] == 0x22 && m[3] == 0x15) {
            const char* who = m[4] == 0x32 ? "OS/BSL" : m[4] == 0x44 ? "App" : "?";
            const int cmd = m.size() >= 7 ? m[6] : -1;
            char b[64];
            std::snprintf(b, sizeof(b), "SysEx ADIOS  %s  cmd 0x%02X  (%d o)",
                          who, cmd, int(m.size()));
            d.label = b;
        } else {
            d.label = "SysEx  (" + std::to_string(m.size()) + " o)";
        }
        return d;
    }

    switch (hi) {
    case 0x80: d.label = "Note Off  " + ch(s) + "   " + noteName(d1(1)) + "   vel " + std::to_string(d1(2)); break;
    case 0x90: d.label = (d1(2) == 0 ? "Note Off  " : "Note On   ") + ch(s) + "   " + noteName(d1(1)) + "   vel " + std::to_string(d1(2)); break;
    case 0xa0: d.label = "Poly AT   " + ch(s) + "   " + noteName(d1(1)) + "   " + std::to_string(d1(2)); break;
    case 0xb0: d.label = "CC        " + ch(s) + "   #" + std::to_string(d1(1)) + "   " + std::to_string(d1(2)); break;
    case 0xc0: d.label = "Prog Chg  " + ch(s) + "   " + std::to_string(d1(1)); break;
    case 0xd0: d.label = "Chan AT   " + ch(s) + "   " + std::to_string(d1(1)); break;
    case 0xe0: {
        int bend = (int(d1(2)) << 7 | d1(1)) - 8192;
        d.label = "Pitch     " + ch(s) + "   " + std::to_string(bend);
        break;
    }
    default:
        switch (s) {
        case 0xf1: d.label = "MTC Qtr   " + std::to_string(d1(1)); break;
        case 0xf2: d.label = "Song Pos  " + std::to_string(int(d1(2)) << 7 | d1(1)); break;
        case 0xf3: d.label = "Song Sel  " + std::to_string(d1(1)); break;
        case 0xf6: d.label = "Tune Req"; break;
        case 0xf8: d.label = "Clock";   d.isRealtime = true; break;
        case 0xfa: d.label = "Start";   d.isRealtime = true; break;
        case 0xfb: d.label = "Continue"; d.isRealtime = true; break;
        case 0xfc: d.label = "Stop";    d.isRealtime = true; break;
        case 0xfe: d.label = "Active Sensing"; d.isRealtime = true; break;
        case 0xff: d.label = "Reset";   d.isRealtime = true; break;
        default:   d.label = "?"; break;
        }
        break;
    }
    return d;
}

bool parseHexLine(const std::string& text, std::vector<uint8_t>& out)
{
    out.clear();
    std::istringstream is(text);
    std::string tok;
    while (is >> tok) {
        if (tok.size() > 2 && (tok[0] == '0') && (tok[1] == 'x' || tok[1] == 'X'))
            tok = tok.substr(2);
        if (tok.empty() || tok.size() > 2) return false;
        for (char c : tok) if (!std::isxdigit((unsigned char)c)) return false;
        out.push_back(uint8_t(std::stoul(tok, nullptr, 16)));
    }
    return !out.empty();
}

} // namespace adios
