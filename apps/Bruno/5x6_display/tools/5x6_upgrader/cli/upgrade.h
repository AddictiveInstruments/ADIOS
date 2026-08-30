#pragma once
#include "sysex.h"
#include "../platform/midi_win.h"
#include <string>

namespace tr5x6 {

// One request/response channel to the board. The Qt build will implement the
// same two calls on top of its own transport; upgrade() never sees an OS.
struct Link {
    MidiOut* out = nullptr;
    uint8_t  deviceId = 0;
    void  (*sendFn)(Link&, const Bytes&) = nullptr;
    Reply (*exchangeFn)(Link&, const Bytes&, int) = nullptr;

    void  send(const Bytes& b)                 { sendFn(*this, b); }
    Reply exchange(const Bytes& b, int ms)     { return exchangeFn(*this, b, ms); }
};

// Where the tool reports. The console prints; a window would fill a list and
// move a bar.
struct Log {
    void (*infoFn)(const char*) = nullptr;
    void (*errFn)(const char*) = nullptr;
    void (*progressFn)(size_t, size_t) = nullptr;
    // A single line the caller OVERWRITES rather than appends - a countdown
    // has no business filling a log with 120 identical entries.
    void (*statusFn)(const char*) = nullptr;
    // Raised once, at the point of no return: from here the board has no
    // valid bootloader until the sequence finishes. The window uses it to
    // take Close away.
    void (*lockFn)(bool) = nullptr;

    // Something the OPERATOR must act on - red. Not an error: the sequence is
    // fine, it is waiting for a pair of hands.
    void (*warnFn)(const char*) = nullptr;
    // Good news - green. Mostly "you can stop holding the buttons now".
    void (*okFn)(const char*) = nullptr;
    // Step index 0..2, state 0 pending / 1 running / 2 done.
    void (*stepFn)(int, int) = nullptr;

    void info(const char* fmt, ...);
    void err(const char* fmt, ...);
    void warn(const char* fmt, ...);
    void ok(const char* fmt, ...);
    void status(const char* fmt, ...);
    void step(int index, int state) { if (stepFn) stepFn(index, state); }
    void progress(size_t done, size_t total) { if (progressFn) progressFn(done, total); }
    void lock(bool on) { if (lockFn) lockFn(on); }
};

struct UpgradeImages {
    std::string migrationHex;    // the two-stage tool + new bootloader
    std::string applicationHex;  // the rev 1 firmware
    uint32_t    legacyBoundary = 0x2800;  // THIS fleet's boundary - carried, not guessed
    uint32_t    newBoundary    = 0x2000;  // where the fresh bootloader ends; the
                                          // whole region below it is rewritten
};

bool upgrade(Link& lk, const UpgradeImages& img, Log& log);

} // namespace tr5x6
