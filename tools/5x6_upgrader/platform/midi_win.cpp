#include "midi_win.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

namespace tr5x6 {

// Long SysEx messages arrive in several buffers; this is the reassembly area
// handed to the driver and re-queued after every completion.
static const DWORD SYSEX_BUF = 4096;

struct InCtx {
    std::function<void(const std::vector<uint8_t>&)> cb;
    std::vector<uint8_t> assembly;
    MIDIHDR hdr{};
    std::vector<char> raw;
    HMIDIIN h = nullptr;
    bool closing = false;
};

static std::string mmError(MMRESULT r)
{
    char buf[MAXERRORLENGTH] = {0};
    if (midiInGetErrorTextA(r, buf, MAXERRORLENGTH) == MMSYSERR_NOERROR && buf[0])
        return buf;
    return "MMSYSERR " + std::to_string(r);
}

std::vector<std::string> midiOutPorts()
{
    std::vector<std::string> v;
    UINT n = midiOutGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        MIDIOUTCAPSA c{};
        if (midiOutGetDevCapsA(i, &c, sizeof(c)) == MMSYSERR_NOERROR)
            v.push_back(c.szPname);          // winmm truncates to 31 chars
        else
            v.push_back("<?>");
    }
    return v;
}

std::vector<std::string> midiInPorts()
{
    std::vector<std::string> v;
    UINT n = midiInGetNumDevs();
    for (UINT i = 0; i < n; ++i) {
        MIDIINCAPSA c{};
        if (midiInGetDevCapsA(i, &c, sizeof(c)) == MMSYSERR_NOERROR)
            v.push_back(c.szPname);
        else
            v.push_back("<?>");
    }
    return v;
}

// ---------------------------------------------------------------- MidiOut --

MidiOut::~MidiOut() { close(); }

bool MidiOut::open(unsigned index, std::string& err)
{
    close();
    HMIDIOUT h = nullptr;
    MMRESULT r = midiOutOpen(&h, index, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) {
        // The usual cause on Windows: another application already holds the
        // port. Windows gives one owner at a time, unlike CoreMIDI.
        err = "cannot open MIDI out (" + mmError(r) +
              ") - is another application using this port?";
        return false;
    }
    h_ = h;
    return true;
}

void MidiOut::close()
{
    if (!h_) return;
    midiOutReset(static_cast<HMIDIOUT>(h_));
    midiOutClose(static_cast<HMIDIOUT>(h_));
    h_ = nullptr;
}

bool MidiOut::sendSysex(const std::vector<uint8_t>& msg, std::string& err)
{
    if (!h_) { err = "MIDI out not open"; return false; }

    MIDIHDR hdr{};
    hdr.lpData         = reinterpret_cast<LPSTR>(const_cast<uint8_t*>(msg.data()));
    hdr.dwBufferLength = static_cast<DWORD>(msg.size());
    hdr.dwBytesRecorded= static_cast<DWORD>(msg.size());

    MMRESULT r = midiOutPrepareHeader(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
    if (r != MMSYSERR_NOERROR) { err = "prepare: " + mmError(r); return false; }

    r = midiOutLongMsg(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
    if (r != MMSYSERR_NOERROR) {
        midiOutUnprepareHeader(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
        err = "send: " + mmError(r);
        return false;
    }

    // midiOutLongMsg is asynchronous: the buffer must stay valid until the
    // driver is done with it, which MHDR_DONE announces.
    while (!(hdr.dwFlags & MHDR_DONE)) Sleep(1);
    midiOutUnprepareHeader(static_cast<HMIDIOUT>(h_), &hdr, sizeof(hdr));
    return true;
}

// ----------------------------------------------------------------- MidiIn --

static void CALLBACK inProc(HMIDIIN, UINT msg, DWORD_PTR inst,
                            DWORD_PTR p1, DWORD_PTR)
{
    InCtx* ctx = reinterpret_cast<InCtx*>(inst);
    if (!ctx) return;

    if (msg == MIM_LONGDATA) {
        MIDIHDR* h = reinterpret_cast<MIDIHDR*>(p1);
        if (h->dwBytesRecorded > 0) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(h->lpData);
            ctx->assembly.insert(ctx->assembly.end(), p, p + h->dwBytesRecorded);
            // a message is complete only on its F7 - long SysEx can be split
            // across several buffers
            if (!ctx->assembly.empty() && ctx->assembly.back() == 0xf7) {
                if (ctx->cb) ctx->cb(ctx->assembly);
                ctx->assembly.clear();
            }
        }
        if (!ctx->closing)
            midiInAddBuffer(ctx->h, h, sizeof(MIDIHDR));   // re-queue
    }
}

MidiIn::~MidiIn() { close(); }

bool MidiIn::open(unsigned index,
                  std::function<void(const std::vector<uint8_t>&)> cb,
                  std::string& err)
{
    close();
    InCtx* ctx = new InCtx();
    ctx->cb = std::move(cb);
    ctx->raw.resize(SYSEX_BUF);

    HMIDIIN h = nullptr;
    MMRESULT r = midiInOpen(&h, index, reinterpret_cast<DWORD_PTR>(inProc),
                            reinterpret_cast<DWORD_PTR>(ctx), CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        delete ctx;
        err = "cannot open MIDI in (" + mmError(r) +
              ") - is another application using this port?";
        return false;
    }
    ctx->h = h;

    ctx->hdr.lpData         = ctx->raw.data();
    ctx->hdr.dwBufferLength = SYSEX_BUF;
    if ((r = midiInPrepareHeader(h, &ctx->hdr, sizeof(MIDIHDR))) != MMSYSERR_NOERROR ||
        (r = midiInAddBuffer   (h, &ctx->hdr, sizeof(MIDIHDR))) != MMSYSERR_NOERROR) {
        midiInClose(h); delete ctx;
        err = "buffer: " + mmError(r);
        return false;
    }
    midiInStart(h);

    h_ = h; buf_ = ctx;
    return true;
}

void MidiIn::close()
{
    if (!h_) return;
    InCtx* ctx = static_cast<InCtx*>(buf_);
    ctx->closing = true;
    midiInStop(static_cast<HMIDIIN>(h_));
    midiInReset(static_cast<HMIDIIN>(h_));               // returns the buffer
    midiInUnprepareHeader(static_cast<HMIDIIN>(h_), &ctx->hdr, sizeof(MIDIHDR));
    midiInClose(static_cast<HMIDIIN>(h_));
    delete ctx;
    h_ = nullptr; buf_ = nullptr;
}

} // namespace tr5x6
