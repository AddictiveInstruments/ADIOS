#include "sysex.h"
#include <string>

namespace tr5x6 {

static const uint8_t MANUFACTURER[3] = { 0x00, 0x22, 0x15 };

Bytes header(Target t, uint8_t device_id, uint8_t cmd)
{
    Bytes b;
    b.push_back(0xf0);
    b.insert(b.end(), MANUFACTURER, MANUFACTURER + 3);
    b.push_back(static_cast<uint8_t>(t));
    b.push_back(device_id & 0x7f);
    b.push_back(cmd & 0x7f);
    return b;
}

void appendAddrLen(Bytes& out, uint32_t addr, uint32_t len)
{
    const uint32_t a = addr >> 4;   // the encoding carries addr/16
    const uint32_t l = len  >> 4;
    out.push_back((a >> 21) & 0x7f);
    out.push_back((a >> 14) & 0x7f);
    out.push_back((a >>  7) & 0x7f);
    out.push_back( a        & 0x7f);
    out.push_back((l >> 21) & 0x7f);
    out.push_back((l >> 14) & 0x7f);
    out.push_back((l >>  7) & 0x7f);
    out.push_back( l        & 0x7f);
}

Bytes pack7(const uint8_t* data, size_t len)
{
    Bytes out;
    out.reserve((len * 8 + 6) / 7);
    uint32_t acc = 0;
    int nbits = 0;
    for (size_t i = 0; i < len; ++i) {
        acc = (acc << 8) | data[i];
        nbits += 8;
        while (nbits >= 7) {
            nbits -= 7;
            out.push_back(static_cast<uint8_t>((acc >> nbits) & 0x7f));
        }
    }
    if (nbits > 0)                       // left-aligned remainder, zero-padded
        out.push_back(static_cast<uint8_t>((acc << (7 - nbits)) & 0x7f));
    return out;
}

uint8_t checksum(const Bytes& body)
{
    uint8_t sum = 0;
    for (uint8_t b : body) sum = static_cast<uint8_t>(sum + b);
    return static_cast<uint8_t>((-static_cast<int>(sum)) & 0x7f);
}

Bytes writeMem(uint8_t device_id, uint32_t addr, const uint8_t* data, size_t len)
{
    Bytes body;                                   // what the checksum covers
    appendAddrLen(body, addr, static_cast<uint32_t>(len));
    const Bytes packed = pack7(data, len);
    body.insert(body.end(), packed.begin(), packed.end());

    Bytes msg = header(Target::Os, device_id, CMD_WRITE_MEM);
    msg.insert(msg.end(), body.begin(), body.end());
    msg.push_back(checksum(body));
    msg.push_back(0xf7);
    return msg;
}

Bytes query(uint8_t device_id, uint8_t sub)
{
    Bytes msg = header(Target::Os, device_id, CMD_QUERY);
    msg.push_back(sub & 0x7f);
    msg.push_back(0xf7);
    return msg;
}

Bytes ping(Target t, uint8_t device_id)
{
    Bytes msg = header(t, device_id, CMD_PING);
    msg.push_back(0xf7);                          // nothing in between
    return msg;
}

Bytes reset(uint8_t device_id)
{
    // 0x7f is a QUERY SUB-COMMAND, not a command of its own: the dispatcher
    // knows only 0x00, 0x0d, 0x0e and 0x0f, and anything else earns an
    // INVALID_COMMAND disacknowledge (adios_midi.c:1834-1851, and the same
    // shape in the June 2025 tree). Sent as a bare command it does nothing
    // but make the board complain - which is exactly what it did.
    Bytes msg = header(Target::Os, device_id, CMD_QUERY);
    msg.push_back(CMD_RESET);
    msg.push_back(0xf7);
    return msg;
}

std::string Reply::text() const
{
    std::string s;
    for (uint8_t c : payload) {
        if (c == 0) break;
        if (c >= 0x20 && c < 0x7f) s.push_back(static_cast<char>(c));
    }
    return s;
}

Reply parse(const uint8_t* msg, size_t len)
{
    Reply r;
    if (len < 8 || msg[0] != 0xf0 || msg[len - 1] != 0xf7) return r;
    if (msg[1] != MANUFACTURER[0] || msg[2] != MANUFACTURER[1] ||
        msg[3] != MANUFACTURER[2]) return r;
    if (msg[4] != static_cast<uint8_t>(Target::Os) &&
        msg[4] != static_cast<uint8_t>(Target::App)) return r;

    r.valid    = true;
    r.target   = static_cast<Target>(msg[4]);
    r.deviceId = msg[5];
    r.cmd      = msg[6];
    r.payload.assign(msg + 7, msg + len - 1);
    r.arg      = r.payload.empty() ? 0 : r.payload[0];
    return r;
}


// Numbering differs between cores (see sysex.h). Only 0x01 is common.
uint8_t querySub(QueryItem item, bool legacyCore)
{
    switch (item) {
    case QueryItem::Processor: return legacyCore ? 0x02 : 0x02;
    case QueryItem::ChipId:    return legacyCore ? 0x04 : 0x03;
    case QueryItem::Serial:    return legacyCore ? 0x05 : 0x04;
    case QueryItem::Flash:     return legacyCore ? 0x06 : 0x05;
    case QueryItem::Ram:       return legacyCore ? 0x07 : 0x06;
    case QueryItem::AppName1:  return legacyCore ? 0x08 : 0x07;
    case QueryItem::AppName2:  return legacyCore ? 0x09 : 0x08;
    case QueryItem::Version:   return legacyCore ? 0x00 : 0x09;  // 0 = absent
    case QueryItem::Boundary:  return legacyCore ? 0x00 : 0x0a;
    case QueryItem::CoreType:  return legacyCore ? 0x00 : 0x0b;
    }
    return 0;
}

const char* queryLabel(QueryItem item)
{
    switch (item) {
    case QueryItem::Processor: return "processor";
    case QueryItem::ChipId:    return "chip id";
    case QueryItem::Serial:    return "serial";
    case QueryItem::Flash:     return "flash";
    case QueryItem::Ram:       return "RAM";
    case QueryItem::AppName1:  return "app line 1";
    case QueryItem::AppName2:  return "app line 2";
    case QueryItem::Version:   return "version";
    case QueryItem::Boundary:  return "boundary";
    case QueryItem::CoreType:  return "core type";
    }
    return "?";
}


// The board answers a read with a WriteMem-shaped dump: the same 7-bit
// packing, so the payload has to be unwound the same way the firmware winds it.
Bytes unpack7(const uint8_t* septets, size_t n, size_t wanted)
{
    Bytes out;
    out.reserve(wanted);
    uint32_t acc = 0;
    int nbits = 0;
    for (size_t i = 0; i < n && out.size() < wanted; ++i) {
        acc = (acc << 7) | (septets[i] & 0x7f);
        nbits += 7;
        while (nbits >= 8 && out.size() < wanted) {
            nbits -= 8;
            out.push_back(static_cast<uint8_t>((acc >> nbits) & 0xff));
        }
    }
    return out;
}

Bytes readMem(uint8_t device_id, uint32_t addr, uint32_t len)
{
    Bytes msg = header(Target::Os, device_id, CMD_READ_MEM);
    appendAddrLen(msg, addr, len);
    msg.push_back(0xf7);          // no checksum on the request
    return msg;
}

} // namespace tr5x6
