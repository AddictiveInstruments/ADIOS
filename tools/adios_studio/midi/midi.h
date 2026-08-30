// Full MIDI transport for ADIOS Studio - short messages AND SysEx, both
// directions. A superset of the upgrader's SysEx-only transport (which stays
// as it is): a monitor has to see note-ons and clocks, not just system
// exclusive, so this one delivers everything and timestamps it.
//
// One header, one implementation per OS (midi_win.cpp / midi_mac.cpp). The
// GUI above never names winmm or CoreMIDI.

#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace adios {

using Bytes = std::vector<uint8_t>;

std::vector<std::string> inPorts();
std::vector<std::string> outPorts();

class Out {
public:
    ~Out();
    bool open(unsigned index, std::string& err);
    void close();
    bool isOpen() const { return h_ != nullptr; }
    // Any complete message: a 1-3 byte channel/system message, or a whole
    // F0..F7 SysEx. The transport picks short vs long by the status byte.
    bool send(const Bytes& msg, std::string& err);

private:
    void* h_ = nullptr;
    void* dest_ = nullptr;   // CoreMIDI destination endpoint; null on winmm
};

class In {
public:
    ~In();
    // cb runs on a MIDI thread, never the caller's. Each call is ONE complete
    // message; t_us is a monotonic microsecond stamp for the monitor's clock.
    using Callback = std::function<void(const Bytes& msg, uint64_t t_us)>;
    bool open(unsigned index, Callback cb, std::string& err);
    void close();
    bool isOpen() const { return h_ != nullptr; }

private:
    void* h_ = nullptr;
    void* ctx_ = nullptr;
};

} // namespace adios
