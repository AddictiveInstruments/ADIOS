#include "Uploader.h"

#include <QThread>
#include <chrono>

#include "../../5x6_upgrader/core/hexfile.h"
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
    {
        std::lock_guard<std::mutex> lk(replyMx_);
        haveReply_ = false;
    }
    sendMsg(msg);
    std::unique_lock<std::mutex> lk(replyMx_);
    if (replyCv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                          [this] { return haveReply_; }))
        return reply_;
    return Reply{};   // valid == false: a timeout
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

    if (coreType() == "BSL") { emit log("déjà en bootloader", true); return true; }

    // The firmware reboots into its bootloader on QUERY 0x7f (not a top-level
    // command): it acks with arg 0x7f - the "wait, I'm rebooting" handshake -
    // then resets with the stay-resident flag set. See adios_midi.c case 0x7f.
    emit log("demande de passage en bootloader (query 0x7f)...", true);
    Reply ack = exchange(query(deviceId_, 0x7f), 500);
    if (ack.valid && ack.cmd == ACK)
        emit log("la carte accuse réception, elle redémarre...", true);

    for (int i = 0; i < 50; ++i) {   // ~5 s: the reboot plus the 2 s BSL window
        QThread::msleep(100);
        std::string ct = coreType();
        if (ct == "BSL") { emit log("bootloader détecté", true); return true; }
    }
    emit log("aucun bootloader n'est apparu — LAST+UP tenus à l'allumage ?", false);
    return false;
}

bool Uploader::run(const QString& hexPath)
{
    HexImage img;
    std::string err;
    if (!loadHex(hexPath.toStdString(), img, err)) {
        emit log(QString("hex illisible : %1").arg(QString::fromStdString(err)), false);
        return false;
    }
    size_t totalBytes = 0;
    for (auto& s : img.segments) totalBytes += s.data.size();
    if (!totalBytes) { emit log("hex vide", false); return false; }
    emit log(QString("%1 segment(s), %2 octets").arg(img.segments.size()).arg(totalBytes), true);

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
            const int MAX = 8;
            do {
                r = exchange(writeMem(deviceId_, addr, chunk.data(), chunk.size()), 1500);
                if (r.valid && r.cmd == ACK) break;
                if (r.valid && r.cmd == DISACK) {
                    emit log(QString("bloc 0x%1 refusé (erreur 0x%2)")
                                 .arg(addr, 8, 16, QChar('0')).arg(r.arg, 2, 16, QChar('0')),
                             false);
                    return false;
                }
                ++tries;   // silence: retry
            } while (tries < MAX);
            if (!(r.valid && r.cmd == ACK)) {
                emit log(QString("bloc 0x%1 sans réponse après %2 essais")
                             .arg(addr, 8, 16, QChar('0')).arg(MAX), false);
                return false;
            }
            done += n;
            emit progress(int(done * 100 / totalBytes));
        }
    }
    emit log("upload terminé, sortie du bootloader", true);
    // Query 0x7f to a resident bootloader releases its halt state and jumps to
    // the freshly written application (adios_midi.c, BSL branch).
    sendMsg(query(deviceId_, 0x7f));
    return true;
}

void Uploader::queryInfo()
{
    if (busy_.exchange(true)) return;
    QThread* t = QThread::create([this] {
        // OS name first: it is the one sub-command numbered the same on both
        // cores, and its answer ("ADIOS" vs "MIOS32") says which numbering the
        // rest uses.
        Reply os = exchange(query(deviceId_, Q_OS_NAME), 300);
        if (!(os.valid && os.cmd == ACK)) {
            emit log("aucune reponse - carte absente, mauvais id, ou pas ADIOS ?", false);
            busy_.store(false);
            emit finished(false);
            return;
        }
        const bool legacy = os.text() != "ADIOS";
        emit log(QString("OS         : %1").arg(QString::fromStdString(os.text())), true);

        static const QueryItem items[] = {
            QueryItem::Processor, QueryItem::ChipId, QueryItem::Serial,
            QueryItem::Flash, QueryItem::Ram, QueryItem::AppName1,
            QueryItem::AppName2, QueryItem::Version, QueryItem::CoreType,
        };
        for (QueryItem it : items) {
            uint8_t sub = querySub(it, legacy);
            if (!sub) continue;   // this core has no such entry
            Reply r = exchange(query(deviceId_, sub), 250);
            QString val = (r.valid && r.cmd == ACK)
                              ? QString::fromStdString(r.text())
                              : QString("-");
            emit log(QString("%1: %2")
                         .arg(queryLabel(it), -11)
                         .arg(val), true);
        }
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
