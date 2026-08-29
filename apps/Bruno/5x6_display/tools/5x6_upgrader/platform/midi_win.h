// The MIDI transport interface - the ONLY part of this tool that knows an OS.
//
// One header, two implementations: platform/midi_win.cpp (winmm) and
// platform/midi_mac.cpp (CoreMIDI). Nothing above this file changes between
// them, which is the whole point of the split.
//
// Both are written against the system API directly rather than pulling RtMidi
// in: those ARE the APIs RtMidi wraps, so the dependency would buy nothing but
// a build step. A third platform means one more file here and one line in
// CMakeLists - nothing else in the tool moves.

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace tr5x6 {

std::vector<std::string> midiInPorts();
std::vector<std::string> midiOutPorts();

class MidiOut {
public:
    ~MidiOut();
    bool open(unsigned index, std::string& err);
    void close();
    // Sends one complete F0..F7 message.
    bool sendSysex(const std::vector<uint8_t>& msg, std::string& err);
private:
    void* h_ = nullptr;
    // CoreMIDI needs BOTH the port and the destination endpoint at send time;
    // winmm needs only the handle and leaves this null.
    void* dest_ = nullptr;
};

class MidiIn {
public:
    ~MidiIn();
    // The callback runs on a MIDI thread, never on the caller's.
    bool open(unsigned index, std::function<void(const std::vector<uint8_t>&)> cb,
              std::string& err);
    void close();
private:
    void* h_ = nullptr;
    void* buf_ = nullptr;
};

} // namespace tr5x6
