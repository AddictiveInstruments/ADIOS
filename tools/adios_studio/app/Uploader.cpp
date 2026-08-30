#include "Uploader.h"

#include <QThread>
#include <chrono>

#include "hexfile.h"
#include "../midi/midi.h"

using namespace tr5x6;

Uploader::Uploader(adios::Out* out, std::mutex* sendGuard, QObject* parent)
    : QObject(parent), out_(out), sendGuard_(sendGuard) {}

Uploader::~Uploader() = default;

void Uploader::feedReply(const Reply& r)
{
    {
        std::lock_guard<std::mutex> lk(replyMx_);
        reply_ = r;
        haveReply_ = true;
    }
    replyCv_.notify_one();
}

void Uploader::sendMsg(const Bytes& msg)
{
    { std::lock_guard<std::mutex> g(*sendGuard_); std::string err; out_->send(msg, err); }
    emit sent(QByteArray(reinterpret_cast<const char*>(msg.data()), int(msg.size())));
}

Reply Uploader::exchange(const Bytes& msg, int timeoutMs)
{
    // Arm the latch BEFORE sending. The reply is delivered straight from the
    // MIDI thread (MainWindow::onMidiIn) into feedReply, not routed through the
    // display timer, so the board's first answer wakes this wait at once - one
    // send, one reply.
    {
        std::lock_guard<std::mutex> lk(replyMx_);
        haveReply_ = false;
    }
    sendMsg(msg);
    std::unique_lock<std::mutex> lk(replyMx_);
    if (replyCv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                          [this] { return haveReply_; }))
        return reply_;
    return Reply{};   // valid == false: a real timeout
}

bool Uploader::enterBootloader()
{
    // Core type (query 0x0b) is the clean discriminator the firmware added for
    // exactly this flow: "APP" while the application runs, "BSL" once the
    // bootloader is resident. Legacy cores DISACK it - treated as "not BSL".
    auto coreType = [&]() -> std::string {
        Reply r = exchange(query(deviceId_, 0x0b), 250);
        return (r.valid && r.cmd == ACK) ? r.text() : std::string();
    };

    if (coreType() == "BSL") { emit log("already in bootloader", true); return true; }

    // The firmware reboots into its bootloader on QUERY 0x7f (not a top-level
    // command): it acks with arg 0x7f - the "wait, I'm rebooting" handshake -
    // then resets with the stay-resident flag set. See adios_midi.c case 0x7f.
    emit log("requesting bootloader entry (query 0x7f)...", true);
    Reply ack = exchange(query(deviceId_, 0x7f), 500);
    if (ack.valid && ack.cmd == ACK)
        emit log("board acknowledged, rebooting...", true);

    // The board resets straight into a RESIDENT bootloader (stay-resident flag),
    // so it comes up within a poll or two - 3 tries is plenty. A disconnected or
    // silent board then fails fast instead of hammering the port for ~18 s.
    for (int i = 0; i < 3; ++i) {
        QThread::msleep(100);
        std::string ct = coreType();
        if (ct == "BSL") { emit log("bootloader detected", true); return true; }
    }
    emit log("no bootloader appeared - board absent or not answering", false);
    return false;
}

bool Uploader::run(const QString& hexPath)
{
    HexImage img;
    std::string err;
    if (!loadHex(hexPath.toStdString(), img, err)) {
        emit log(QString("cannot read hex: %1").arg(QString::fromStdString(err)), false);
        return false;
    }
    size_t totalBytes = 0;
    for (auto& s : img.segments) totalBytes += s.data.size();
    if (!totalBytes) { emit log("empty hex", false); return false; }
    emit log(QString("%1 segment(s), %2 bytes").arg(img.segments.size()).arg(totalBytes), true);

    if (!enterBootloader()) return false;

    const size_t BLOCK = 256;   // a multiple of 16, well under BSL_SYSEX_MAX
    size_t done = 0;
    for (const auto& seg : img.segments) {
        // Blocks must be 16-aligned in length; the loader's segments already
        // start aligned, and the last short block is padded to 16 here.
        for (size_t off = 0; off < seg.data.size(); off += BLOCK) {
            size_t n = std::min(BLOCK, seg.data.size() - off);
            size_t padded = (n + 15) & ~size_t(15);
            std::vector<uint8_t> chunk(seg.data.begin() + off, seg.data.begin() + off + n);
            chunk.resize(padded, 0xff);   // erased-flash filler for the tail

            uint32_t addr = seg.addr + uint32_t(off);
            Reply r;
            int tries = 0;
            const int MAX = 16;
            do {
                r = exchange(writeMem(deviceId_, addr, chunk.data(), chunk.size()), 1500);
                if (r.valid && r.cmd == ACK) break;
                if (r.valid && r.cmd == DISACK) {
                    emit log(QString("block 0x%1 rejected (error 0x%2)")
                                 .arg(addr, 8, 16, QChar('0')).arg(r.arg, 2, 16, QChar('0')),
                             false);
                    return false;
                }
                ++tries;   // silence: retry
            } while (tries < MAX);
            if (!(r.valid && r.cmd == ACK)) {
                emit log(QString("block 0x%1 no response after %2 tries")
                             .arg(addr, 8, 16, QChar('0')).arg(MAX), false);
                return false;
            }
            done += n;
            emit progress(int(done * 100 / totalBytes));
        }
    }
    emit log("upload complete, leaving bootloader", true);
    // Query 0x7f to a resident bootloader releases its halt state and jumps to
    // the freshly written application (adios_midi.c, BSL branch).
    sendMsg(query(deviceId_, 0x7f));
    // Mirror MIOS Studio: after the reboot-into-app command, give the core time
    // to reset and start before anyone queries it (UploadHandler.cpp wait(3000)).
    // The window then adds its own 5 s (UploadWindow.cpp TIMER_DELAYED_PROGRESS_OFF)
    // before the post-upload re-read.
    emit log("waiting for the board to reboot...", true);
    QThread::msleep(3000);
    return true;
}

void Uploader::queryInfo()
{
    if (busy_.exchange(true)) return;
    QThread* t = QThread::create([this] {
        emit infoClear();

        // OS name first: the one sub-command numbered the same on both cores,
        // and its answer ("ADIOS" vs "MIOS32") says which numbering the rest
        // uses.
        Reply os = exchange(query(deviceId_, Q_OS_NAME), 500);
        if (!(os.valid && os.cmd == ACK)) {
            emit infoLine("No response - board absent, wrong device id, or not ADIOS?", 2);
            busy_.store(false);
            emit finished(false);
            return;
        }
        const bool legacy = os.text() != "ADIOS";

        auto ask = [&](QueryItem it) -> QString {
            uint8_t sub = querySub(it, legacy);
            if (!sub) return QString();               // this core has no such entry
            Reply r = exchange(query(deviceId_, sub), 400);
            return (r.valid && r.cmd == ACK) ? QString::fromStdString(r.text()) : QString();
        };

        // Core type decides the colour of the "running program" lines, exactly
        // as the MIOS Studio charter: normal = an application is running,
        // orange = the BSL-update tool sits in its place, red = the bootloader
        // itself, with no application to run.
        const QString core = ask(QueryItem::CoreType);
        const int col = core == "UPDATER" ? 1 : core == "BSL" ? 2 : 0;

        emit infoLine("Operating System: " + QString::fromStdString(os.text()), 0);
        QString s;
        if (!(s = ask(QueryItem::Processor)).isEmpty()) emit infoLine("Processor: " + s, 0);
        if (!(s = ask(QueryItem::ChipId)).isEmpty())    emit infoLine("Chip ID: 0x" + s, 0);
        if (!(s = ask(QueryItem::Serial)).isEmpty())    emit infoLine("Serial: #" + s, 0);
        if (!(s = ask(QueryItem::Flash)).isEmpty())     emit infoLine("Flash Memory Size: " + s + " bytes", 0);
        if (!(s = ask(QueryItem::Ram)).isEmpty())       emit infoLine("RAM Size: " + s + " bytes", 0);
        if (!(s = ask(QueryItem::Boundary)).isEmpty())
            emit infoLine((col == 1 ? "Uploader/BSL boundary: 0x" : "App/BSL boundary: 0x") + s, col);

        // First app line NAMES the running program - coloured. The second is a
        // copyright, the same whatever runs, so it stays neutral.
        const QString app1 = ask(QueryItem::AppName1);
        const QString ver  = ask(QueryItem::Version);
        if (!app1.isEmpty()) emit infoLine(ver.isEmpty() ? app1 : (app1 + " " + ver), col);
        const QString app2 = ask(QueryItem::AppName2);
        if (!app2.isEmpty()) emit infoLine(app2, 0);

        busy_.store(false);
        emit finished(true);
    });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void Uploader::start(const QString& hexPath)
{
    if (busy_.exchange(true)) return;
    QThread* t = QThread::create([this, hexPath] {
        bool ok = run(hexPath);
        busy_.store(false);
        emit finished(ok);
    });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}
