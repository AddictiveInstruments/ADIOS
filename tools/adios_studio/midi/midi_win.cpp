#include "midi.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

namespace adios {

namespace {
std::string mmErr(MMRESULT r)
{
    char b[MAXERRORLENGTH] = {0};
    if (midiInGetErrorTextA(r, b, MAXERRORLENGTH) == MMSYSERR_NOERROR && b[0])
        return b;
    return "MMSYSERR " + std::to_string(r);
}

uint64_t now_us()
{
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return uint64_t(c.QuadPart) * 1000000ull / uint64_t(freq.QuadPart);
}
} // namespace

std::vector<std::string> inPorts()
{
    std::vector<std::string> v;
    UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        MIDIINCAPSA c{};
        v.push_back(midiInGetDevCapsA(i, &c, sizeof(c)) == MMSYSERR_NOERROR ? c.szPname : "<?>");
    }
    return v;
}

std::vector<std::string> outPorts()
{
    std::vector<std::string> v;
    UINT n = midiOutGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        MIDIOUTCAPSA c{};
        v.push_back(midiOutGetDevCapsA(i, &c, sizeof(c)) == MMSYSERR_NOERROR ? c.szPname : "<?>");
    }
    return v;
}

// ---------------------------------------------------------------- Out --------

Out::~Out() { close(); }

bool Out::open(unsigned index, std::string& err)
{
    close();
    HMIDIOUT h = nullptr;
    MMRESULT r = midiOutOpen(&h, index, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) {
        err = "cannot open MIDI out (" + mmErr(r) + ") - port already in use?";
        return false;
    }
    h_ = h;
    return true;
}

void Out::close()
{
    if (!h_) return;
    midiOutReset(static_cast<HMIDIOUT>(h_));
    midiOutClose(static_cast<HMIDIOUT>(h_));
    h_ = nullptr;
}

bool Out::send(const Bytes& msg, std::string& err)
{
    if (!h_) { err = "MIDI out not open"; return false; }
    if (msg.empty()) return true;

    // SysEx (or any F0-led long message) goes through the long-message path;
    // everything else is a packed short message. A running-status-less short
    // message is at most three bytes.
    if (msg[0] == 0xf0 || msg.size() > 3) {
        MIDIHDR hdr{};
        hdr.lpData          = reinterpret_cast<LPSTR>(const_cast<uint8_t*>(msg.data()));
        hdr.dwBufferLength  = static_cast<DWORD>(msg.size());
        hdr.dwBytesRecorded = static_cast<DWORD>(msg.size());
        MMRESULT r = midiOutPrepareHeader(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
        if (r != MMSYSERR_NOERROR) { err = "prepare: " + mmErr(r); return false; }
        r = midiOutLongMsg(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
        if (r != MMSYSERR_NOERROR) {
            midiOutUnprepareHeader(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
            err = "send: " + mmErr(r);
            return false;
        }
        while (!(hdr.dwFlags & MHDR_DONE)) Sleep(1);
        midiOutUnprepareHeader(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
        return true;
    }

    DWORD packed = 0;
    for (size_t i = 0; i < msg.size(); ++i) packed |= DWORD(msg[i]) << (8 * i);
    MMRESULT r = midiOutShortMsg(static_cast<HMIDIOUT>(h_), packed);
    if (r != MMSYSERR_NOERROR) { err = "send: " + mmErr(r); return false; }
    return true;
}

// ----------------------------------------------------------------- In --------

namespace {
constexpr int IN_BUFFERS = 8;
constexpr DWORD SYSEX_BUF = 65536;   // a firmware dump answer can be large

struct InCtx {
    In::Callback cb;
    Bytes        assembly;            // reassembles a split SysEx
    MIDIHDR      hdr[IN_BUFFERS]{};
    std::vector<char> raw[IN_BUFFERS];
    HMIDIIN      h = nullptr;
    bool         closing = false;
};

void CALLBACK inProc(HMIDIIN, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR)
{
    InCtx* ctx = reinterpret_cast<InCtx*>(inst);
    if (!ctx) return;

    if (msg == MIM_DATA) {
        // Short message packed into p1: status, data1, data2 low-to-high. Its
        // length follows from the status byte.
        const uint8_t status = uint8_t(p1 & 0xff);
        int len = 3;
        const uint8_t hi = status & 0xf0;
        if (hi == 0xc0 || hi == 0xd0) len = 2;          // program change, aftertouch
        else if (status >= 0xf0) {
            switch (status) {
            case 0xf1: case 0xf3: len = 2; break;       // MTC, song select
            case 0xf2: len = 3; break;                  // song position
            default:   len = 1; break;                  // clock, start, ... realtime
            }
        }
        Bytes m;
        for (int i = 0; i < len; ++i) m.push_back(uint8_t((p1 >> (8 * i)) & 0xff));
        if (ctx->cb) ctx->cb(m, now_us());
    } else if (msg == MIM_LONGDATA) {
        MIDIHDR* h = reinterpret_cast<MIDIHDR*>(p1);
        if (h->dwBytesRecorded > 0) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(h->lpData);
            if (!ctx->assembly.empty() && p[0] == 0xf0) {
                // a fresh F0 while a fragment waits: the previous one never got
                // its F7 - hand it over as-is rather than glue two together
                if (ctx->cb) ctx->cb(ctx->assembly, now_us());
                ctx->assembly.clear();
            }
            ctx->assembly.insert(ctx->assembly.end(), p, p + h->dwBytesRecorded);
            if (ctx->assembly.back() == 0xf7) {
                if (ctx->cb) ctx->cb(ctx->assembly, now_us());
                ctx->assembly.clear();
            }
        }
        if (!ctx->closing) midiInAddBuffer(ctx->h, h, sizeof(MIDIHDR));
    }
}
} // namespace

In::~In() { close(); }

bool In::open(unsigned index, Callback cb, std::string& err)
{
    close();
    InCtx* ctx = new InCtx();
    ctx->cb = std::move(cb);

    HMIDIIN h = nullptr;
    MMRESULT r = midiInOpen(&h, index, reinterpret_cast<DWORD_PTR>(inProc),
                            reinterpret_cast<DWORD_PTR>(ctx), CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        delete ctx;
        err = "cannot open MIDI in (" + mmErr(r) + ") - port already in use?";
        return false;
    }
    ctx->h = h;

    for (int i = 0; i < IN_BUFFERS; ++i) {
        ctx->raw[i].resize(SYSEX_BUF);
        ctx->hdr[i].lpData         = ctx->raw[i].data();
        ctx->hdr[i].dwBufferLength = SYSEX_BUF;
        if ((r = midiInPrepareHeader(h, &ctx->hdr[i], sizeof(MIDIHDR))) != MMSYSERR_NOERROR ||
            (r = midiInAddBuffer   (h, &ctx->hdr[i], sizeof(MIDIHDR))) != MMSYSERR_NOERROR) {
            midiInClose(h); delete ctx;
            err = "buffer: " + mmErr(r);
            return false;
        }
    }
    midiInStart(h);
    h_ = h; ctx_ = ctx;
    return true;
}

void In::close()
{
    if (!h_) return;
    InCtx* ctx = static_cast<InCtx*>(ctx_);
    ctx->closing = true;
    midiInStop(static_cast<HMIDIIN>(h_));
    midiInReset(static_cast<HMIDIIN>(h_));
    for (int i = 0; i < IN_BUFFERS; ++i)
        midiInUnprepareHeader(static_cast<HMIDIIN>(h_), &ctx->hdr[i], sizeof(MIDIHDR));
    midiInClose(static_cast<HMIDIIN>(h_));
    delete ctx;
    h_ = nullptr; ctx_ = nullptr;
}

} // namespace adios
