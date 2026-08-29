// macOS MIDI transport - CoreMIDI, behind the SAME three classes as the
// Windows one. Nothing above platform/ knows which of the two it is talking to.
//
// Written against CoreMIDI directly rather than pulling RtMidi in: this is the
// API RtMidi wraps on macOS, it costs no dependency, and the whole file is the
// only thing a third platform would have to reproduce.
//
// Two differences from Windows worth knowing, because they change behaviour
// rather than just syntax:
//   - CoreMIDI SHARES ports. Several applications can hold the same one at
//     once, so the "is another application using this port?" failure that
//     Windows produces simply does not happen here.
//   - MIDISendSysex is ASYNCHRONOUS. The request object must outlive the call
//     and the caller has to wait for it, which sendSysex does below - the
//     Windows side waits on MHDR_DONE for the same reason.

#include "midi_win.h"          // the interface is shared; only this file differs

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

#include <atomic>
#include <thread>
#include <chrono>

namespace tr5x6 {

namespace {

MIDIClientRef g_client = 0;

// One client for the whole process. CoreMIDI wants a name; it is what other
// MIDI applications will see us as.
MIDIClientRef client()
{
    if (!g_client) {
        CFStringRef name = CFSTR("5x6 firmware update");
        MIDIClientCreate(name, nullptr, nullptr, &g_client);
    }
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

struct InCtx {
    std::function<void(const std::vector<uint8_t>&)> cb;
    std::vector<uint8_t> assembly;
    MIDIPortRef port = 0;
};

// Runs on CoreMIDI's own high-priority thread, never on the caller's - the
// same contract as the Windows callback, and the reason everything above goes
// through a queued signal before touching a widget.
void readProc(const MIDIPacketList* list, void* refCon, void*)
{
    InCtx* ctx = static_cast<InCtx*>(refCon);
    if (!ctx) return;

    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        for (UInt16 b = 0; b < p->length; ++b) {
            const uint8_t byte = p->data[b];
            // A long SysEx arrives across several packets, so completion is
            // the F7 and nothing else.
            if (byte == 0xf0) ctx->assembly.clear();
            ctx->assembly.push_back(byte);
            if (byte == 0xf7) {
                if (ctx->cb) ctx->cb(ctx->assembly);
                ctx->assembly.clear();
            }
        }
        p = MIDIPacketNext(p);
    }
}

struct SysexDone {
    std::atomic<bool> done{false};
};

void sysexCompletion(MIDISysexSendRequest* req)
{
    if (req && req->completionRefCon)
        static_cast<SysexDone*>(req->completionRefCon)->done = true;
}

} // namespace

std::vector<std::string> midiOutPorts()
{
    std::vector<std::string> v;
    const ItemCount n = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < n; ++i)
        v.push_back(endpointName(MIDIGetDestination(i)));
    return v;
}

std::vector<std::string> midiInPorts()
{
    std::vector<std::string> v;
    const ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; ++i)
        v.push_back(endpointName(MIDIGetSource(i)));
    return v;
}

// ---------------------------------------------------------------- MidiOut --

MidiOut::~MidiOut() { close(); }

bool MidiOut::open(unsigned index, std::string& err)
{
    close();
    if (index >= MIDIGetNumberOfDestinations()) { err = "no such MIDI output"; return false; }

    MIDIPortRef port = 0;
    if (MIDIOutputPortCreate(client(), CFSTR("out"), &port) != noErr) {
        err = "cannot create the MIDI output port";
        return false;
    }
    // Both halves are needed at send time, so they travel together.
    h_    = reinterpret_cast<void*>(static_cast<uintptr_t>(port));
    dest_ = reinterpret_cast<void*>(static_cast<uintptr_t>(MIDIGetDestination(index)));
    return true;
}

void MidiOut::close()
{
    if (h_) MIDIPortDispose(static_cast<MIDIPortRef>(reinterpret_cast<uintptr_t>(h_)));
    h_ = nullptr;
    dest_ = nullptr;
}

bool MidiOut::sendSysex(const std::vector<uint8_t>& msg, std::string& err)
{
    if (!h_ || !dest_) { err = "MIDI out not open"; return false; }

    SysexDone flag;
    MIDISysexSendRequest req = {};
    req.destination     = static_cast<MIDIEndpointRef>(reinterpret_cast<uintptr_t>(dest_));
    req.data            = msg.data();
    req.bytesToSend     = static_cast<UInt32>(msg.size());
    req.complete        = false;
    req.completionProc  = sysexCompletion;
    req.completionRefCon= &flag;

    if (MIDISendSysex(&req) != noErr) { err = "MIDISendSysex refused the message"; return false; }

    // The request and the buffer must stay alive until CoreMIDI is done with
    // them - returning early would hand it a dangling pointer.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!flag.done && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (!flag.done) { err = "the MIDI output stalled"; return false; }
    return true;
}

// ----------------------------------------------------------------- MidiIn --

MidiIn::~MidiIn() { close(); }

bool MidiIn::open(unsigned index,
                  std::function<void(const std::vector<uint8_t>&)> cb,
                  std::string& err)
{
    close();
    if (index >= MIDIGetNumberOfSources()) { err = "no such MIDI input"; return false; }

    InCtx* ctx = new InCtx();
    ctx->cb = std::move(cb);

    MIDIPortRef port = 0;
    if (MIDIInputPortCreate(client(), CFSTR("in"), readProc, ctx, &port) != noErr) {
        delete ctx;
        err = "cannot create the MIDI input port";
        return false;
    }
    if (MIDIPortConnectSource(port, MIDIGetSource(index), ctx) != noErr) {
        MIDIPortDispose(port);
        delete ctx;
        err = "cannot connect that MIDI source";
        return false;
    }
    ctx->port = port;

    h_   = reinterpret_cast<void*>(static_cast<uintptr_t>(port));
    buf_ = ctx;
    return true;
}

void MidiIn::close()
{
    if (!h_) return;
    InCtx* ctx = static_cast<InCtx*>(buf_);
    MIDIPortDispose(static_cast<MIDIPortRef>(reinterpret_cast<uintptr_t>(h_)));
    delete ctx;
    h_ = nullptr;
    buf_ = nullptr;
}

} // namespace tr5x6
