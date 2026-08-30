// macOS MIDI transport for ADIOS Studio - CoreMIDI, behind the SAME two
// classes as the Windows one. Richer than the upgrader's SysEx-only transport:
// a monitor needs every message, so the read side is a full MIDI stream parser
// (running status, realtime insertion, SysEx reassembly) and the send side
// takes any complete message, not just F0..F7.
//
// Untested on the bench (built and used on Windows so far); written to
// CoreMIDI's documented contract and modelled on the upgrader's proven mac
// file, which is why the shapes match.

#include "midi.h"

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace adios {

namespace {

MIDIClientRef g_client = 0;
MIDIClientRef client()
{
    if (!g_client) MIDIClientCreate(CFSTR("ADIOS Studio"), nullptr, nullptr, &g_client);
    return g_client;
}

std::string endpointName(MIDIEndpointRef ep)
{
    CFStringRef cf = nullptr;
    if (MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cf) != noErr || !cf)
        return "<?>";
    char buf[256] = {0};
    CFStringGetCString(cf, buf, sizeof(buf), kCFStringEncodingUTF8);
    CFRelease(cf);
    return buf;
}

uint64_t now_us()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Expected total length of a channel/system message given its status byte.
int msgLen(uint8_t status)
{
    switch (status & 0xf0) {
    case 0x80: case 0x90: case 0xa0: case 0xb0: case 0xe0: return 3;
    case 0xc0: case 0xd0: return 2;
    default: break;
    }
    switch (status) {
    case 0xf1: case 0xf3: return 2;
    case 0xf2: return 3;
    default: return 1;   // realtime, tune request
    }
}

struct InCtx {
    In::Callback cb;
    MIDIPortRef  port = 0;
    // running parser state
    Bytes    sysex;
    bool     inSysex = false;
    uint8_t  status = 0;
    Bytes    running;
    int      need = 0;

    void emit(const Bytes& m) { if (cb && !m.empty()) cb(m, now_us()); }

    void feed(uint8_t b)
    {
        if (b >= 0xf8) { emit(Bytes{b}); return; }   // realtime: inserts anywhere
        if (inSysex) {
            sysex.push_back(b);
            if (b == 0xf7) { emit(sysex); sysex.clear(); inSysex = false; }
            return;
        }
        if (b == 0xf0) { inSysex = true; sysex.clear(); sysex.push_back(b); status = 0; return; }
        if (b & 0x80) {                              // new status
            status = b; running.assign(1, b); need = msgLen(b);
            if (need == 1) { emit(running); running.clear(); status = 0; }
            return;
        }
        // data byte
        if (!status) return;                         // orphan data, ignore
        if (running.empty()) running.push_back(status);   // running status reuse
        running.push_back(b);
        if ((int)running.size() >= need) { emit(running); running.assign(1, status); running.clear(); }
    }
};

void readProc(const MIDIPacketList* list, void* refCon, void*)
{
    InCtx* ctx = static_cast<InCtx*>(refCon);
    if (!ctx) return;
    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        for (UInt16 b = 0; b < p->length; ++b) ctx->feed(p->data[b]);
        p = MIDIPacketNext(p);
    }
}

struct SysexDone { std::atomic<bool> done{false}; };
void sysexCompletion(MIDISysexSendRequest* req)
{
    if (req && req->completionRefCon) static_cast<SysexDone*>(req->completionRefCon)->done = true;
}

} // namespace

std::vector<std::string> outPorts()
{
    std::vector<std::string> v;
    const ItemCount n = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < n; ++i) v.push_back(endpointName(MIDIGetDestination(i)));
    return v;
}

std::vector<std::string> inPorts()
{
    std::vector<std::string> v;
    const ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; ++i) v.push_back(endpointName(MIDIGetSource(i)));
    return v;
}

// ---------------------------------------------------------------- Out --------

Out::~Out() { close(); }

bool Out::open(unsigned index, std::string& err)
{
    close();
    if (index >= MIDIGetNumberOfDestinations()) { err = "no such MIDI output"; return false; }
    MIDIPortRef port = 0;
    if (MIDIOutputPortCreate(client(), CFSTR("out"), &port) != noErr) {
        err = "cannot create the MIDI output port"; return false;
    }
    h_    = reinterpret_cast<void*>(static_cast<uintptr_t>(port));
    dest_ = reinterpret_cast<void*>(static_cast<uintptr_t>(MIDIGetDestination(index)));
    return true;
}

void Out::close()
{
    if (h_) MIDIPortDispose(static_cast<MIDIPortRef>(reinterpret_cast<uintptr_t>(h_)));
    h_ = nullptr; dest_ = nullptr;
}

bool Out::send(const Bytes& msg, std::string& err)
{
    if (!h_ || !dest_) { err = "MIDI out not open"; return false; }
    if (msg.empty()) return true;

    MIDIEndpointRef dest = static_cast<MIDIEndpointRef>(reinterpret_cast<uintptr_t>(dest_));

    if (msg[0] == 0xf0) {   // SysEx: async, must outlive the call
        SysexDone flag;
        MIDISysexSendRequest req = {};
        req.destination     = dest;
        req.data            = msg.data();
        req.bytesToSend     = static_cast<UInt32>(msg.size());
        req.complete        = false;
        req.completionProc  = sysexCompletion;
        req.completionRefCon= &flag;
        if (MIDISendSysex(&req) != noErr) { err = "MIDISendSysex refused"; return false; }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!flag.done && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!flag.done) { err = "the MIDI output stalled"; return false; }
        return true;
    }

    // Short message via a one-packet list.
    Byte buf[64];
    MIDIPacketList* pl = reinterpret_cast<MIDIPacketList*>(buf);
    MIDIPacket* pkt = MIDIPacketListInit(pl);
    pkt = MIDIPacketListAdd(pl, sizeof(buf), pkt, 0, msg.size(), msg.data());
    if (!pkt) { err = "message too long for a packet"; return false; }
    MIDIPortRef port = static_cast<MIDIPortRef>(reinterpret_cast<uintptr_t>(h_));
    if (MIDISend(port, dest, pl) != noErr) { err = "MIDISend refused"; return false; }
    return true;
}

// ----------------------------------------------------------------- In --------

In::~In() { close(); }

bool In::open(unsigned index, Callback cb, std::string& err)
{
    close();
    if (index >= MIDIGetNumberOfSources()) { err = "no such MIDI input"; return false; }
    InCtx* ctx = new InCtx();
    ctx->cb = std::move(cb);
    MIDIPortRef port = 0;
    if (MIDIInputPortCreate(client(), CFSTR("in"), readProc, ctx, &port) != noErr) {
        delete ctx; err = "cannot create the MIDI input port"; return false;
    }
    if (MIDIPortConnectSource(port, MIDIGetSource(index), ctx) != noErr) {
        MIDIPortDispose(port); delete ctx; err = "cannot connect that MIDI source"; return false;
    }
    ctx->port = port;
    h_ = reinterpret_cast<void*>(static_cast<uintptr_t>(port));
    ctx_ = ctx;
    return true;
}

void In::close()
{
    if (!h_) return;
    InCtx* ctx = static_cast<InCtx*>(ctx_);
    MIDIPortDispose(static_cast<MIDIPortRef>(reinterpret_cast<uintptr_t>(h_)));
    delete ctx;
    h_ = nullptr; ctx_ = nullptr;
}

} // namespace adios
