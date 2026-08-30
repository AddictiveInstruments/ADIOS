// 5x6 upgrader - console prototype.
//
//   5x6_upgrader                                    list the MIDI ports
//   5x6_upgrader <in> <out> [id]                    identify the board
//   5x6_upgrader <in> <out> <id> send <file.hex>    raw upload (manual BSL entry)
//   5x6_upgrader <in> <out> <id> upgrade <mig.hex> <app.hex>
//                                                   the whole sequence
//
// The identify step decides everything else: the application's ping gives the
// MACHINE (0x50 TR-505 / 0x62 TR-626), which selects the migration image, and
// the OS name says whether this is a June 2025 core that needs upgrading.

#include "sysex.h"
#include "hexfile.h"
#include "../platform/midi_win.h"
#include "upgrade.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

using namespace tr5x6;

namespace {

std::mutex         g_mutex;
std::vector<Reply> g_replies;
MidiOut*           g_out = nullptr;

void onSysex(const std::vector<uint8_t>& msg)
{
    Reply r = parse(msg.data(), msg.size());
    if (!r.valid) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_replies.push_back(std::move(r));
}

void clearReplies()
{
    std::lock_guard<std::mutex> lk(g_mutex);
    g_replies.clear();
}

Reply waitReply(int ms)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (!g_replies.empty()) return g_replies.front();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return Reply{};
}

Reply exchange(MidiOut& out, const Bytes& msg, int ms)
{
    clearReplies();
    std::string err;
    if (!out.sendSysex(msg, err)) {
        std::printf("  send failed: %s\n", err.c_str());
        return Reply{};
    }
    return waitReply(ms);
}

// ---- the two hooks upgrade() needs, wired to this program's transport -----

void linkSend(Link& lk, const Bytes& b)
{
    std::string err;
    clearReplies();
    lk.out->sendSysex(b, err);
}

Reply linkExchange(Link& lk, const Bytes& b, int ms)
{
    return exchange(*lk.out, b, ms);
}

void logInfo(const char* s) { std::printf("  %s\n", s); std::fflush(stdout); }
void logErr (const char* s) { std::printf("  ** %s\n", s); std::fflush(stdout); }
void logProgress(size_t done, size_t total)
{
    std::printf("\r     %zu/%zu (%zu%%)   ", done, total, total ? done * 100 / total : 0);
    if (done >= total) std::printf("\n");
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------

void listPorts()
{
    auto in  = midiInPorts();
    auto out = midiOutPorts();
    std::printf("MIDI in:\n");
    for (size_t i = 0; i < in.size();  ++i) std::printf("  %2zu  %s\n", i, in[i].c_str());
    if (in.empty())  std::printf("  (none)\n");
    std::printf("MIDI out:\n");
    for (size_t i = 0; i < out.size(); ++i) std::printf("  %2zu  %s\n", i, out[i].c_str());
    if (out.empty()) std::printf("  (none)\n");
    std::printf("\nusage:\n"
                "  5x6_upgrader <in> <out> [id]\n"
                "  5x6_upgrader <in> <out> <id> send <file.hex>\n"
                "  5x6_upgrader <in> <out> <id> upgrade <migration.hex> <app.hex>\n");
}

bool identify(MidiOut& out, uint8_t devId)
{
    std::printf("app ping (0x44)\n");
    Reply r = exchange(out, ping(Target::App, devId), 400);
    if (r.valid && r.cmd == ACK) {
        const char* machine = (r.arg == 0x62) ? "TR-626"
                            : (r.arg == 0x50) ? "TR-505" : "unknown";
        std::printf("  -> %s (0x%02X), %s banks\n", machine, r.arg,
                    (r.arg == 0x62) ? "8" : "16");
    } else {
        std::printf("  -> no answer (no 5x6 application on this id?)\n");
    }

    std::printf("\nOS query (0x32)\n");
    r = exchange(out, query(devId, Q_OS_NAME), 300);
    const std::string os = (r.valid && r.cmd == ACK) ? r.text() : std::string();
    const bool legacy = (os != "ADIOS");
    std::printf("  %-11s %s\n", "OS", os.empty() ? "-" : os.c_str());

    const QueryItem items[] = {
        QueryItem::Processor, QueryItem::Flash, QueryItem::Ram,
        QueryItem::AppName1,  QueryItem::AppName2, QueryItem::Version,
        QueryItem::Boundary,  QueryItem::CoreType };

    for (QueryItem it : items) {
        const uint8_t sub = querySub(it, legacy);
        if (sub == 0) { std::printf("  %-11s (not on this core)\n", queryLabel(it)); continue; }
        r = exchange(out, query(devId, sub), 300);
        if (r.valid && r.cmd == ACK)
            std::printf("  %-11s %s\n", queryLabel(it), r.text().c_str());
        else if (r.valid && r.cmd == DISACK)
            std::printf("  %-11s refused (0x%02X)\n", queryLabel(it), r.arg);
        else
            std::printf("  %-11s -\n", queryLabel(it));
    }

    std::printf("\n%s\n", legacy
        ? "LEGACY core (MIOS32): this board needs the upgrade."
        : "current core (ADIOS).");
    return legacy;
}

bool sendHex(MidiOut& out, uint8_t devId, const char* path)
{
    HexImage img;
    std::string err;
    if (!loadHex(path, img, err)) { std::printf("hex: %s\n", err.c_str()); return false; }

    size_t total = 0;
    for (const auto& s : img.segments) total += s.data.size();
    std::printf("%s: %zu segment(s), %zu bytes\n", path, img.segments.size(), total);

    const size_t BLOCK = 256;
    size_t done = 0;
    for (const auto& s : img.segments) {
        for (size_t off = 0; off < s.data.size(); off += BLOCK) {
            const size_t n = (s.data.size() - off < BLOCK) ? (s.data.size() - off) : BLOCK;
            std::vector<uint8_t> chunk(s.data.begin() + off, s.data.begin() + off + n);
            while (chunk.size() % 16) chunk.push_back(0xff);

            const uint32_t addr = s.addr + static_cast<uint32_t>(off);
            Reply r = exchange(out, writeMem(devId, addr, chunk.data(), chunk.size()), 3000);
            if (!r.valid)        { std::printf("\n  0x%08X: no answer\n", addr); return false; }
            if (r.cmd == DISACK) { std::printf("\n  0x%08X: refused (0x%02X)\n", addr, r.arg); return false; }
            done += n;
            logProgress(done, total);
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) { listPorts(); return 0; }

    const unsigned inIdx  = static_cast<unsigned>(std::atoi(argv[1]));
    const unsigned outIdx = static_cast<unsigned>(std::atoi(argv[2]));
    const uint8_t  devId  = static_cast<uint8_t>(argc > 3 ? std::atoi(argv[3]) : 0);

    std::string err;
    MidiIn  in;
    MidiOut out;
    if (!in.open(inIdx, onSysex, err))  { std::printf("MIDI in: %s\n",  err.c_str()); return 1; }
    if (!out.open(outIdx, err))         { std::printf("MIDI out: %s\n", err.c_str()); return 1; }
    g_out = &out;

    std::printf("device id %u\n\n", devId);

    // Diagnostic: read the bootloader info block and name what is in it. The
    // board must be held in BSL - ReadMem is a bootloader command.
    if (argc > 4 && std::strcmp(argv[4], "info") == 0) {
        const uint32_t boundary = (argc > 5)
            ? static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 0)) : 0x2800u;
        const uint32_t base = 0x08000000u + boundary - 0x100u;
        std::printf("reading the info block at 0x%08X (boundary 0x%04X)\n", base, boundary);

        Reply r = exchange(out, readMem(devId, base, 256), 3000);
        if (!r.valid || r.cmd == DISACK) {
            std::printf("  no dump (hold the board in BSL - ReadMem is a bootloader command)\n");
            return 1;
        }
        // the dump comes back WriteMem-shaped: cmd, addr/len, payload, checksum
        if (r.payload.size() < 10) { std::printf("  short dump\n"); return 1; }
        Bytes raw = unpack7(r.payload.data() + 8, r.payload.size() - 9, 256);
        if (raw.size() < 0xd4) { std::printf("  only %zu bytes back\n", raw.size()); return 1; }

        auto flag = [&](const char* name, size_t confirm, size_t value) {
            if (raw[confirm] == 0x42)
                std::printf("  %-12s %u\n", name, raw[value]);
            else
                std::printf("  %-12s not set (marker 0x%02X)\n", name, raw[confirm]);
        };
        flag("device id", 0xd0, 0xd1);
        flag("FASTBOOT", 0xd2, 0xd3);
        if (raw[0xd2] == 0x42 && raw[0xd3])
            std::printf("\n  ** FASTBOOT IS ON: the bootloader skips its whole wait loop,\n"
                        "     so there is NO window to upload into.\n");
        return 0;
    }

    // Diagnostic: ONE write-memory command, with the frame printed. Isolates
    // the encoding from everything else - use it with the board held in BSL by
    // its hold pin, where no window can expire under us.
    if (argc > 5 && std::strcmp(argv[4], "poke") == 0) {
        const uint32_t addr = static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 0));
        std::vector<uint8_t> data(256, 0xff);
        for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);

        Bytes msg = writeMem(devId, addr, data.data(), data.size());
        std::printf("write 256 bytes at 0x%08X - frame is %zu bytes\n", addr, msg.size());
        std::printf("  head:");
        for (size_t i = 0; i < 16 && i < msg.size(); ++i) std::printf(" %02X", msg[i]);
        std::printf("\n  tail:");
        for (size_t i = (msg.size() > 4 ? msg.size() - 4 : 0); i < msg.size(); ++i)
            std::printf(" %02X", msg[i]);
        std::printf("\n");

        Reply r = exchange(out, msg, 3000);
        if (!r.valid)             std::printf("  -> no answer\n");
        else if (r.cmd == ACK)    std::printf("  -> ACK 0x%02X (written)\n", r.arg);
        else if (r.cmd == DISACK) std::printf("  -> DISACK 0x%02X %s\n", r.arg,
            r.arg == 0x01 ? "LESS_BYTES" : r.arg == 0x02 ? "MORE_BYTES" :
            r.arg == 0x03 ? "WRONG_CHECKSUM" : r.arg == 0x04 ? "WRITE_FAILED" :
            r.arg == 0x08 ? "WRONG_ADDR_RANGE" : r.arg == 0x0e ? "INVALID_COMMAND" : "");
        else                      std::printf("  -> cmd 0x%02X arg 0x%02X\n", r.cmd, r.arg);
        return 0;
    }

    // Diagnostic: send the restart request and WATCH what happens. A board
    // that resets stops answering for a moment, then answers again.
    if (argc > 4 && std::strcmp(argv[4], "reset") == 0) {
        std::printf("sending query sub-command 0x7f (restart request)\n");
        Reply r = exchange(out, reset(devId), 500);
        if (r.valid && r.cmd == ACK)
            std::printf("  ACK 0x%02X\n", r.arg);
        else if (r.valid && r.cmd == DISACK)
            std::printf("  DISACK 0x%02X %s\n", r.arg,
                r.arg == 0x0d ? "(unknown query - sub-command not recognised)" : "");
        else
            std::printf("  no reply (expected: this command resets instead of answering)\n");

        // now poll: does it stop answering, and does it come back?
        std::printf("watching the board for 6 s\n");
        bool wentAway = false;
        for (int i = 0; i < 60; ++i) {
            Reply p = exchange(out, ping(Target::App, devId), 90);
            const bool alive = p.valid && p.cmd == ACK;
            std::printf("  %4d ms  %s\n", i * 100, alive ? "answers" : "silent");
            if (!alive) wentAway = true;
            else if (wentAway) { std::printf("  -> it restarted and came back\n"); return 0; }
        }
        std::printf("%s", wentAway ? "  -> it went silent and did NOT come back\n"
                                   : "  -> it never stopped answering: NO RESET\n");
        return 0;
    }

    if (argc > 5 && std::strcmp(argv[4], "send") == 0)
        return sendHex(out, devId, argv[5]) ? 0 : 1;

    if (argc > 6 && std::strcmp(argv[4], "upgrade") == 0) {
        Link lk;
        lk.out = &out;
        lk.deviceId = devId;
        lk.sendFn = linkSend;
        lk.exchangeFn = linkExchange;

        Log log;
        log.infoFn = logInfo;
        log.errFn = logErr;
        log.progressFn = logProgress;

        UpgradeImages img;
        img.migrationHex   = argv[5];
        img.applicationHex = argv[6];
        img.legacyBoundary = 0x2800;   // THIS fleet - carried, never guessed

        return upgrade(lk, img, log) ? 0 : 1;
    }

    identify(out, devId);
    return 0;
}
