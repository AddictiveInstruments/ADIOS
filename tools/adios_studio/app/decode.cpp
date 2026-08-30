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

} // namespace

Decoded decode(const std::vector<uint8_t>& m)
{
    Decoded d;
    d.hex = hexDump(m);
    if (m.empty()) { d.label = "(vide)"; return d; }

    const uint8_t s = m[0];
    const uint8_t hi = s & 0xf0;
    auto d1 = [&](int i) { return i < (int)m.size() ? m[i] : 0; };
    // Fixed-width columns so every line aligns: TYPE(9) CHAN(5) DATA1(7) DATA2.
    auto fmt = [](const char* type, const std::string& chStr,
                  const std::string& a, const std::string& b) {
        char out[96];
        std::snprintf(out, sizeof(out), "%-9s %-5s %-7s %s", type, chStr.c_str(), a.c_str(), b.c_str());
        return std::string(out);
    };
    auto chanS = [&] { char c[8]; std::snprintf(c, sizeof(c), "ch %2d", int(s & 0x0f) + 1); return std::string(c); };

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
    case 0x80: d.label = fmt("Note Off", chanS(), noteName(d1(1)), "vel " + std::to_string(d1(2))); break;
    case 0x90: d.label = fmt(d1(2) == 0 ? "Note Off" : "Note On", chanS(), noteName(d1(1)), "vel " + std::to_string(d1(2))); break;
    case 0xa0: d.label = fmt("Poly AT", chanS(), noteName(d1(1)), "prs " + std::to_string(d1(2))); break;
    case 0xb0: d.label = fmt("CC", chanS(), "#" + std::to_string(d1(1)), "val " + std::to_string(d1(2))); break;
    case 0xc0: d.label = fmt("Prog Chg", chanS(), std::to_string(d1(1)), ""); break;
    case 0xd0: d.label = fmt("Chan AT", chanS(), std::to_string(d1(1)), ""); break;
    case 0xe0: d.label = fmt("Pitch", chanS(), std::to_string((int(d1(2)) << 7 | d1(1)) - 8192), ""); break;
    default:
        switch (s) {
        case 0xf1: d.label = fmt("MTC Qtr", "", std::to_string(d1(1)), ""); break;
        case 0xf2: d.label = fmt("Song Pos", "", std::to_string(int(d1(2)) << 7 | d1(1)), ""); break;
        case 0xf3: d.label = fmt("Song Sel", "", std::to_string(d1(1)), ""); break;
        case 0xf6: d.label = fmt("Tune Req", "", "", ""); break;
        case 0xf8: d.label = fmt("Clock", "", "", ""); d.isRealtime = true; break;
        case 0xfa: d.label = fmt("Start", "", "", ""); d.isRealtime = true; break;
        case 0xfb: d.label = fmt("Continue", "", "", ""); d.isRealtime = true; break;
        case 0xfc: d.label = fmt("Stop", "", "", ""); d.isRealtime = true; break;
        case 0xfe: d.label = fmt("Act Sens", "", "", ""); d.isRealtime = true; break;
        case 0xff: d.label = fmt("Reset", "", "", ""); d.isRealtime = true; break;
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
