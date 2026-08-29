#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include "Uploader.h"
#include "decode.h"
#include "../../5x6_upgrader/core/sysex.h"

namespace {
// Colour codes shared with Uploader::infoLine: 0 normal, 1 updater, 2 boot.
QColor infoColour(int c)
{
    switch (c) {
    case 1:  return QColor(0xe0, 0x91, 0x3f);   // orange - BSL update tool
    case 2:  return QColor(0xe0, 0x66, 0x6b);   // red    - the bootloader itself
    default: return QColor(0xdf, 0xe4, 0xec);   // normal - an application runs
    }
}

QString hexFull(const adios::Bytes& m)
{
    QString s;
    s.reserve(int(m.size()) * 3);
    for (size_t i = 0; i < m.size(); ++i) {
        if (i) s += ' ';
        s += QString("%1").arg(m[i], 2, 16, QChar('0')).toUpper();
    }
    return s;
}

// A monitor / log list: one message per line, no wrap, no elision, horizontal
// scroll, individually selectable, capped so it cannot grow without bound.
QListWidget* makeList(int minH)
{
    auto* w = new QListWidget;
    w->setObjectName("mono");
    w->setUniformItemSizes(true);
    w->setWordWrap(false);
    w->setTextElideMode(Qt::ElideNone);
    w->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    w->setSelectionMode(QAbstractItemView::ExtendedSelection);
    w->setMinimumHeight(minH);
    return w;
}

void appendCapped(QListWidget* w, QListWidgetItem* it)
{
    // Keep auto-scroll only when the view already sits at the bottom, so a
    // manual scroll-up to inspect a line is not yanked away on the next event.
    QScrollBar* sb = w->verticalScrollBar();
    const bool atBottom = sb->value() >= sb->maximum() - 2;
    w->addItem(it);
    while (w->count() > 5000) delete w->takeItem(0);
    if (atBottom) w->scrollToBottom();
}
} // namespace

MainWindow::MainWindow()
{
    setWindowTitle(tr("ADIOS Studio"));
    resize(960, 820);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(9);

    // ---- port bar --------------------------------------------------------
    auto* ports = new QHBoxLayout;
    ports->setSpacing(8);
    inBox_  = new QComboBox;  inBox_->setMinimumWidth(150);
    inBox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    outBox_ = new QComboBox;  outBox_->setMinimumWidth(150);
    outBox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    idBox_  = new QSpinBox;   idBox_->setRange(0, 127);
    connectBtn_ = new QPushButton(tr("Connect"));
    connectBtn_->setObjectName("primary");
    queryBtn_ = new QPushButton(tr("Query"));
    queryBtn_->setToolTip(tr("Read the connected board: OS, processor, flash, version"));
    link_ = new QLabel; link_->setObjectName("dim");
    ports->addWidget(new QLabel(tr("In")));
    ports->addWidget(inBox_, 1);
    ports->addWidget(new QLabel(tr("Out")));
    ports->addWidget(outBox_, 1);
    ports->addWidget(new QLabel(tr("Device ID")));
    ports->addWidget(idBox_);
    ports->addWidget(connectBtn_);
    ports->addWidget(queryBtn_);
    root->addLayout(ports);
    root->addWidget(link_);

    // ---- device info -----------------------------------------------------
    auto* dg = new QGroupBox(tr("Device Info"));
    auto* dl = new QVBoxLayout(dg);
    devInfo_ = makeList(120);
    dl->addWidget(devInfo_);
    root->addWidget(dg);

    // ---- upload ----------------------------------------------------------
    auto* ug = new QGroupBox(tr("Upload Firmware"));
    auto* ul = new QVBoxLayout(ug);
    auto* urow = new QHBoxLayout;
    hexPath_ = new QLineEdit; hexPath_->setPlaceholderText(tr(".hex file"));
    browseBtn_ = new QPushButton(tr("Browse")); browseBtn_->setObjectName("flat");
    uploadBtn_ = new QPushButton(tr("Upload")); uploadBtn_->setObjectName("primary");
    uploadBtn_->setEnabled(false);
    urow->addWidget(hexPath_, 1);
    urow->addWidget(browseBtn_);
    urow->addWidget(uploadBtn_);
    ul->addLayout(urow);
    progress_ = new QProgressBar; progress_->setRange(0, 100); progress_->setValue(0);
    ul->addWidget(progress_);
    ul->addWidget(new QLabel(tr("Upload status")));
    uploadStatus_ = makeList(70);
    ul->addWidget(uploadStatus_);
    root->addWidget(ug);

    // ---- terminal --------------------------------------------------------
    auto* tg = new QGroupBox(tr("Terminal (SysEx)"));
    auto* tl = new QVBoxLayout(tg);
    term_ = makeList(90);
    tl->addWidget(term_);
    auto* srow = new QHBoxLayout;
    sysexBox_ = new QLineEdit;
    sysexBox_->setPlaceholderText(tr("F0 00 22 15 32 00 0F F7   (Enter to send)"));
    sendBtn_ = new QPushButton(tr("Send")); sendBtn_->setObjectName("flat");
    srow->addWidget(sysexBox_, 1);
    srow->addWidget(sendBtn_);
    tl->addLayout(srow);
    root->addWidget(tg);

    // ---- monitor ---------------------------------------------------------
    auto* mg = new QGroupBox(tr("MIDI Monitor"));
    auto* ml = new QVBoxLayout(mg);
    auto* split = new QSplitter(Qt::Horizontal);
    monIn_  = makeList(120);
    monOut_ = makeList(120);
    muteRt_ = new QCheckBox(tr("hide clock / realtime"));
    muteRt_->setChecked(true);
    auto* inPane = new QWidget; auto* inV = new QVBoxLayout(inPane);
    inV->setContentsMargins(0, 0, 0, 0); inV->setSpacing(3);
    auto* inHdr = new QHBoxLayout;
    auto* inLbl = new QLabel(tr("IN")); inLbl->setObjectName("dim");
    inHdr->addWidget(inLbl);
    inHdr->addStretch(1);
    inHdr->addWidget(muteRt_);
    inV->addLayout(inHdr);
    inV->addWidget(monIn_);
    auto* outPane = new QWidget; auto* outV = new QVBoxLayout(outPane);
    outV->setContentsMargins(0, 0, 0, 0); outV->setSpacing(3);
    auto* outLbl = new QLabel(tr("OUT")); outLbl->setObjectName("dim");
    outV->addWidget(outLbl);
    outV->addWidget(monOut_);
    split->addWidget(inPane);
    split->addWidget(outPane);
    ml->addWidget(split, 1);
    root->addWidget(mg, 1);

    // Right-click menus (select all / copy / clear) on every list.
    auto wireMenu = [this](QListWidget* w) {
        w->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(w, &QWidget::customContextMenuRequested, this, [w](const QPoint& p) {
            QMenu m;
            QAction* sa = m.addAction(tr("Select All"));
            QAction* co = m.addAction(tr("Copy"));
            m.addSeparator();
            QAction* cl = m.addAction(tr("Clear"));
            QAction* a = m.exec(w->viewport()->mapToGlobal(p));
            if (a == sa) w->selectAll();
            else if (a == co) {
                QString t;
                for (auto* it : w->selectedItems()) t += it->text() + '\n';
                if (!t.isEmpty()) QApplication::clipboard()->setText(t);
            } else if (a == cl) w->clear();
        });
    };
    wireMenu(monIn_);
    wireMenu(monOut_);
    wireMenu(term_);
    wireMenu(devInfo_);
    wireMenu(uploadStatus_);

    // ---- uploader wiring -------------------------------------------------
    uploader_ = new Uploader(&out_, &outGuard_, this);
    connect(uploader_, &Uploader::log, this, [this](QString l, bool ok) {
        auto* it = new QListWidgetItem((ok ? "  " : "! ") + l);
        if (!ok) it->setForeground(infoColour(2));
        appendCapped(uploadStatus_, it);
    });
    connect(uploader_, &Uploader::progress, this, [this](int p) { progress_->setValue(p); });
    connect(uploader_, &Uploader::sent, this, [this](QByteArray b) {
        monitorLine(true, adios::Bytes(b.begin(), b.end()));
    });
    connect(uploader_, &Uploader::infoClear, this, [this] { devInfo_->clear(); });
    connect(uploader_, &Uploader::infoLine, this, [this](QString t, int c) {
        auto* it = new QListWidgetItem(t);
        it->setForeground(infoColour(c));
        appendCapped(devInfo_, it);
    });
    connect(uploader_, &Uploader::finished, this, [this](bool) {
        uploadBtn_->setEnabled(connected_ && !hexPath_->text().isEmpty());
        queryBtn_->setEnabled(connected_);
    });

    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::toggleConnect);
    connect(queryBtn_,   &QPushButton::clicked, this, [this] {
        if (!connected_ || uploader_->busy()) return;
        uploader_->setDeviceId(uint8_t(idBox_->value()));
        queryBtn_->setEnabled(false);
        uploader_->queryInfo();
    });
    connect(browseBtn_,  &QPushButton::clicked, this, &MainWindow::chooseHex);
    connect(uploadBtn_,  &QPushButton::clicked, this, &MainWindow::doUpload);
    connect(sendBtn_,    &QPushButton::clicked, this, &MainWindow::sendSysex);
    connect(sysexBox_,   &QLineEdit::returnPressed, this, &MainWindow::sendSysex);
    connect(hexPath_,    &QLineEdit::textChanged, this,
            [this](const QString& t) { uploadBtn_->setEnabled(connected_ && !t.isEmpty()); });

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTick);
    timer_->start(20);

    for (const auto& p : adios::inPorts())  inBox_->addItem(QString::fromStdString(p));
    for (const auto& p : adios::outPorts()) outBox_->addItem(QString::fromStdString(p));
    setConnected(false);
}

MainWindow::~MainWindow()
{
    in_.close();
    out_.close();
}

void MainWindow::setConnected(bool on)
{
    connected_ = on;
    connectBtn_->setText(on ? tr("Disconnect") : tr("Connect"));
    inBox_->setEnabled(!on);
    outBox_->setEnabled(!on);
    uploadBtn_->setEnabled(on && !hexPath_->text().isEmpty());
    sendBtn_->setEnabled(on);
    queryBtn_->setEnabled(on);
}

void MainWindow::toggleConnect()
{
    if (connected_) {
        in_.close();
        out_.close();
        setConnected(false);
        link_->setText(tr("disconnected"));
        return;
    }
    if (inBox_->count() == 0 || outBox_->count() == 0) {
        link_->setText(tr("no MIDI port"));
        return;
    }
    std::string err;
    if (!out_.open(unsigned(outBox_->currentIndex()), err)) {
        link_->setText(QString::fromStdString(err));
        return;
    }
    if (!in_.open(unsigned(inBox_->currentIndex()),
                  [this](const adios::Bytes& m, uint64_t t) { onMidiIn(m, t); }, err)) {
        out_.close();
        link_->setText(QString::fromStdString(err));
        return;
    }
    uploader_->setDeviceId(uint8_t(idBox_->value()));
    clock_.restart();
    setConnected(true);
    link_->setText(tr("connected  —  %1  →  %2")
                       .arg(inBox_->currentText(), outBox_->currentText()));
}

// ---- MIDI thread: park the message, the timer drains it ------------------
void MainWindow::onMidiIn(const adios::Bytes& msg, uint64_t t_us)
{
    std::lock_guard<std::mutex> lk(rxMx_);
    rxQueue_.push_back({msg, t_us});
}

void MainWindow::onTick()
{
    std::vector<RxMsg> batch;
    { std::lock_guard<std::mutex> lk(rxMx_); batch.swap(rxQueue_); }
    for (auto& r : batch) routeIn(r.bytes, r.t_us);
}

void MainWindow::routeIn(const adios::Bytes& msg, uint64_t)
{
    monitorLine(false, msg);

    // ADIOS-family SysEx also feeds the terminal (as text) and the uploader.
    if (msg.size() >= 7 && msg[0] == 0xf0 && msg[1] == 0x00 &&
        msg[2] == 0x22 && msg[3] == 0x15) {
        tr5x6::Reply r = tr5x6::parse(msg.data(), msg.size());
        if (r.valid) {
            uploader_->feedReply(r);
            QString txt = QString::fromStdString(r.text());
            QString kind = r.cmd == tr5x6::ACK ? "ACK" : r.cmd == tr5x6::DISACK ? "DISACK"
                          : QString("cmd 0x%1").arg(r.cmd, 2, 16, QChar('0'));
            appendCapped(term_, new QListWidgetItem(
                QString("< %1   %2").arg(kind, txt.isEmpty() ? hexFull(r.payload) : txt)));
        }
    }
}

void MainWindow::monitorLine(bool out, const adios::Bytes& msg)
{
    adios::Decoded d = adios::decode(msg);
    if (d.isRealtime && muteRt_->isChecked()) return;
    QString line = QString("%1  %2   %3")
                       .arg(nowStamp())
                       .arg(d.label.c_str(), -34)
                       .arg(hexFull(msg));
    appendCapped(out ? monOut_ : monIn_, new QListWidgetItem(line));
}

QString MainWindow::nowStamp()
{
    double s = clock_.isValid() ? clock_.elapsed() / 1000.0 : 0.0;
    return QString("%1").arg(s, 9, 'f', 3);
}

bool MainWindow::sendRaw(const adios::Bytes& msg)
{
    if (!connected_) return false;
    std::string err;
    bool ok;
    { std::lock_guard<std::mutex> g(outGuard_); ok = out_.send(msg, err); }
    if (ok) monitorLine(true, msg);
    else    appendCapped(term_, new QListWidgetItem("! send: " + QString::fromStdString(err)));
    return ok;
}

void MainWindow::sendSysex()
{
    if (!connected_) return;
    adios::Bytes m;
    if (!adios::parseHexLine(sysexBox_->text().toStdString(), m)) {
        appendCapped(term_, new QListWidgetItem(tr("! invalid hex")));
        return;
    }
    sendRaw(m);
}

void MainWindow::chooseHex()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Firmware .hex"), QString(),
                                             tr("Intel HEX (*.hex);;All files (*)"));
    if (!f.isEmpty()) hexPath_->setText(f);
}

void MainWindow::doUpload()
{
    if (!connected_ || uploader_->busy()) return;
    uploader_->setDeviceId(uint8_t(idBox_->value()));
    progress_->setValue(0);
    uploadBtn_->setEnabled(false);
    uploadStatus_->clear();
    appendCapped(uploadStatus_, new QListWidgetItem(
        QString("== upload %1 ==").arg(QFileInfo(hexPath_->text()).fileName())));
    uploader_->start(hexPath_->text());
}
