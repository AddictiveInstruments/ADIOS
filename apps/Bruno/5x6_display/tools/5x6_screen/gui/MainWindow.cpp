#include "MainWindow.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "../../5x6_upgrader/platform/midi_win.h"

using namespace tr5x6;

namespace {
constexpr int PANEL_W = 480;
constexpr int PANEL_H = 320;
constexpr int RAW_SIZE = PANEL_W * PANEL_H * 3;
constexpr quint8 CMD_MIRROR_HALT = 0x0A;
} // namespace

MainWindow::MainWindow()
{
    setWindowTitle(tr("5x6 Screen  (Beta-Testers Only)"));

    folder_ = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (folder_.isEmpty()) folder_ = QDir::homePath();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(10);

    // ---- where MIRROR_HALT goes out -------------------------------------
    auto* link = new QHBoxLayout;
    link->setSpacing(8);
    outBox_ = new QComboBox;
    for (const auto& p : midiOutPorts()) outBox_->addItem(QString::fromStdString(p));
    idBox_ = new QSpinBox;
    idBox_->setRange(0, 127);
    link_ = new QLabel;
    link_->setObjectName("dim");
    link->addWidget(new QLabel(tr("MIDI Out")));
    link->addWidget(outBox_, 1);
    link->addWidget(new QLabel(tr("id")));
    link->addWidget(idBox_);
    link->addWidget(link_, 1);
    root->addLayout(link);

    // ---- the panel, 1:1 and fixed ----------------------------------------
    // No scaling and no zoom: a manual wants the pixel the operator sees, and
    // a resized view would quietly lie about it.
    view_ = new QLabel;
    view_->setFixedSize(PANEL_W, PANEL_H);
    view_->setObjectName("panel");
    root->addWidget(view_, 0, Qt::AlignHCenter);

    // ---- the one button ---------------------------------------------------
    auto* bar = new QHBoxLayout;
    bar->setSpacing(8);
    status_ = new QLabel(tr("prêt"));
    captureBtn_ = new QPushButton(tr("capturer"));
    captureBtn_->setObjectName("primary");
    bar->addWidget(status_, 1);
    bar->addWidget(captureBtn_);
    root->addLayout(bar);

    auto* dest = new QHBoxLayout;
    dest->setSpacing(8);
    counter_ = new QLabel;
    counter_->setObjectName("dim");
    folderBtn_ = new QPushButton(tr("changer le dossier"));
    folderBtn_->setObjectName("flat");
    dest->addWidget(counter_, 1);
    dest->addWidget(folderBtn_);
    root->addLayout(dest);

    // ---- what has been written, and when ----------------------------------
    shots_ = new QListWidget;
    shots_->setMinimumHeight(120);
    root->addWidget(shots_);

    connect(captureBtn_, &QPushButton::clicked, this, &MainWindow::onCapture);
    connect(folderBtn_,  &QPushButton::clicked, this, &MainWindow::onChooseFolder);

    // Hands stay on the instrument, not on the mouse.
    auto* sc = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(sc, &QShortcut::activated, this, &MainWindow::onCapture);

    counter_->setText(tr("0 capture · %1").arg(folder_));
}

MainWindow::~MainWindow() = default;

bool MainWindow::sendHalt(quint8 on, QString* err)
{
    MidiOut out;
    std::string e;
    if (!out.open(unsigned(outBox_->currentIndex()), e)) {
        if (err) *err = QString::fromStdString(e);
        return false;
    }
    const std::vector<uint8_t> msg = {
        0xf0, 0x00, 0x22, 0x15, 0x44,
        uint8_t(idBox_->value()), CMD_MIRROR_HALT, on, 0xf7
    };
    const bool ok = out.sendSysex(msg, e);
    if (!ok && err) *err = QString::fromStdString(e);
    return ok;
}

void MainWindow::onCapture()
{
    if (helper_) return;                     // one ceremony at a time

    QString err;
    if (!sendHalt(1, &err)) {
        logRow(tr("MIDI : %1").arg(err), false);
        return;
    }
    captureBtn_->setEnabled(false);
    status_->setText(tr("machine gelée — dump en cours..."));

    rawPath_ = QDir::temp().filePath(QStringLiteral("5x6_gdram.raw"));

    helper_ = new QProcess(this);
    connect(helper_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::finishCapture);
    connect(helper_, &QProcess::readyReadStandardOutput, this, [this]() {
        // "POS <bytes>" progress lines from the helper
        const QStringList lines =
            QString::fromLatin1(helper_->readAllStandardOutput()).split(QLatin1Char('\n'));
        for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
            if (it->startsWith(QLatin1String("POS "))) {
                const int pos = it->mid(4).toInt();
                status_->setText(tr("dump : %1 %").arg(pos * 100 / RAW_SIZE));
                break;
            }
        }
    });
    helper_->start(QCoreApplication::applicationDirPath()
                       + QStringLiteral("/5x6_dump_helper.exe"),
                   {rawPath_});
}

void MainWindow::finishCapture(int exitCode, QProcess::ExitStatus st)
{
    const QString helperErr =
        QString::fromLatin1(helper_->readAllStandardError()).trimmed();
    helper_->deleteLater();
    helper_ = nullptr;

    // The machine is released FIRST, whatever happened to the dump - a failed
    // capture must never leave the instrument parked.
    QString err;
    if (!sendHalt(0, &err))
        logRow(tr("libération MIDI : %1").arg(err), false);

    captureBtn_->setEnabled(true);

    if (st != QProcess::NormalExit || exitCode != 0) {
        status_->setText(tr("échec"));
        logRow(helperErr.isEmpty()
                   ? tr("aide de dump : code %1").arg(exitCode)
                   : helperErr,
               false);
        return;
    }

    QFile f(rawPath_);
    if (!f.open(QIODevice::ReadOnly) || f.size() < RAW_SIZE) {
        status_->setText(tr("échec"));
        logRow(tr("fichier de dump illisible"), false);
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    f.remove();

    // The raw bytes ARE the picture: R,G,B per pixel, six significant bits
    // each, exactly as RAMRD hands them out. Format_RGB888 takes them as-is.
    QImage img(reinterpret_cast<const uchar*>(raw.constData()),
               PANEL_W, PANEL_H, PANEL_W * 3, QImage::Format_RGB888);
    const QImage copy = img.copy();          // detach from the QByteArray

    view_->setPixmap(QPixmap::fromImage(copy));
    status_->setText(tr("prêt"));
    save(copy);
}

bool MainWindow::save(const QImage& img)
{
    const QString name = QStringLiteral("5x6-%1-%2.png")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"))
        .arg(++shotNum_, 3, 10, QLatin1Char('0'));
    const QString path = QDir(folder_).filePath(name);

    if (!img.save(path, "PNG")) {
        --shotNum_;
        logRow(tr("échec d'écriture — %1").arg(path), false);
        return false;
    }
    logRow(name, true);
    counter_->setText(tr("%1 capture(s) · %2").arg(shotNum_).arg(folder_));
    return true;
}

void MainWindow::logRow(const QString& what, bool ok)
{
    auto* it = new QListWidgetItem(
        QStringLiteral("%1  %2")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), what));
    it->setForeground(ok ? QBrush(QColor(0x9e, 0xd5, 0x72))
                         : QBrush(QColor(0xe0, 0x8f, 0x62)));
    shots_->insertItem(0, it);
}

void MainWindow::onChooseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Dossier des captures"), folder_);
    if (d.isEmpty()) return;
    folder_ = d;
    counter_->setText(tr("%1 capture(s) · %2").arg(shotNum_).arg(folder_));
}
