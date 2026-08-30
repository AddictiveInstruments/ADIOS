// Firmware upload over the ADIOS bootloader protocol, on a worker thread so
// the window stays alive. Reuses the shared protocol encoder (core/sysex) and
// the shared Intel-HEX loader (core/hexfile); only the request/response timing
// lives here.
//
// The flow is the plain one, not the two-stage boundary-migration dance the
// betatester upgrader does: enter the bootloader, then stream write-memory
// blocks for every hex segment, one acknowledge per block, a few retries on
// silence. A .hex that carries the embedded bootloader copy simply has a
// segment at 0x08000000 too - it is written like any other.

#pragma once
#include <QObject>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "../../5x6_upgrader/core/sysex.h"

namespace adios { class Out; }

class Uploader : public QObject {
    Q_OBJECT
public:
    // `sendGuard` serialises access to the one shared MIDI Out with the GUI
    // thread; the uploader locks it around every send.
    Uploader(adios::Out* out, std::mutex* sendGuard, QObject* parent = nullptr);
    ~Uploader() override;

    void setDeviceId(uint8_t id) { deviceId_ = id; }

    // Fed by the window's reply router, on the GUI thread, for every ADIOS
    // reply that arrives. Wakes a waiting exchange.
    void feedReply(const tr5x6::Reply& r);

    // Loads the file and runs the upload on a detached worker. Emits progress
    // and then finished exactly once.
    void start(const QString& hexPath);

    // Interrogates the connected board - OS name, processor, flash, RAM,
    // version - and emits each answer through log(). Also on a worker thread,
    // sharing the same reply latch, so it cannot run during an upload.
    void queryInfo();

    bool busy() const { return busy_.load(); }

signals:
    void log(QString line, bool ok);         // upload status lines
    void progress(int pct);
    void finished(bool ok);
    // Every message the uploader puts on the wire, so the window can echo it
    // into the MIDI OUT monitor - queued to the GUI thread as a QByteArray.
    void sent(QByteArray msg);
    // Structured device-info lines from queryInfo(), for the Device Info panel.
    // colour: 0 normal (app running), 1 orange (updater), 2 red (bootloader).
    void infoClear();
    void infoLine(QString text, int colour);

private:
    void         sendMsg(const tr5x6::Bytes& msg);   // send + echo to monitor
    tr5x6::Reply exchange(const tr5x6::Bytes& msg, int timeoutMs, int tries = 1);
    bool enterBootloader();
    bool run(const QString& hexPath);

    adios::Out* out_;
    std::mutex* sendGuard_;
    uint8_t     deviceId_ = 0;

    std::mutex              replyMx_;
    std::condition_variable replyCv_;
    tr5x6::Reply            reply_;
    bool                    haveReply_ = false;

    std::atomic<bool> busy_{false};
};
