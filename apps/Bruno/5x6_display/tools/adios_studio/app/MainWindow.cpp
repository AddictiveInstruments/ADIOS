#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include "Uploader.h"
#include "decode.h"
#include "../../5x6_upgrader/core/sysex.h"

namespace {
QString hexOf(const adios::Bytes& m, int cap = 24)
{
    QString s;
    for (int i = 0; i < (int)m.size() && i < cap; ++i)
        s += QString("%1 ").arg(m[i], 2, 16, QChar('0')).toUpper();
    if ((int)m.size() > cap) s += "...";
    return s.trimmed();
}
} // namespace

MainWindow::MainWindow()
{
    setWindowTitle(tr("ADIOS Studio"));
    resize(920, 760);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // ---- port bar --------------------------------------------------------
    auto* ports = new QHBoxLayout;
    ports->setSpacing(8);
    inBox_  = new QComboBox;  inBox_->setMinimumWidth(130); inBox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    outBox_ = new QComboBox;  outBox_->setMinimumWidth(130); outBox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    idBox_  = new QSpinBox;   idBox_->setRange(0, 127);
    refreshBtn_ = new QPushButton(tr("↻"));
    refreshBtn_->setObjectName("flat");
    refreshBtn_->setFixedWidth(30);
    connectBtn_ = new QPushButton(tr("connecter"));
    connectBtn_->setObjectName("primary");
    queryBtn_ = new QPushButton(tr("interroger"));
    queryBtn_->setObjectName("flat");
    queryBtn_->setToolTip(tr("nom OS, processeur, flash, version de la carte connectée"));
    link_ = new QLabel; link_->setObjectName("dim");
    ports->addWidget(new QLabel(tr("In")));
    ports->addWidget(inBox_, 1);
    ports->addWidget(new QLabel(tr("Out")));
    ports->addWidget(outBox_, 1);
    ports->addWidget(new QLabel(tr("id")));
    ports->addWidget(idBox_);
    ports->addWidget(refreshBtn_);
    ports->addWidget(connectBtn_);
    ports->addWidget(queryBtn_);
    root->addLayout(ports);
    root->addWidget(link_);

    // ---- upload ----------------------------------------------------------
    auto* up = new QGroupBox(tr("Upload firmware"));
    auto* upl = new QVBoxLayout(up);
    auto* row = new QHBoxLayout;
    hexPath_ = new QLineEdit; hexPath_->setPlaceholderText(tr("fichier .hex"));
    browseBtn_ = new QPushButton(tr("parcourir")); browseBtn_->setObjectName("flat");
    uploadBtn_ = new QPushButton(tr("envoyer")); uploadBtn_->setObjectName("primary");
    uploadBtn_->setEnabled(false);
    row->addWidget(hexPath_, 1);
    row->addWidget(browseBtn_);
    row->addWidget(uploadBtn_);
    upl->addLayout(row);
    progress_ = new QProgressBar; progress_->setRange(0, 100); progress_->setValue(0);
    upl->addWidget(progress_);
    root->addWidget(up);

    // ---- terminal --------------------------------------------------------
    auto* tg = new QGroupBox(tr("Terminal ADIOS / SysEx"));
    auto* tl = new QVBoxLayout(tg);
    term_ = new QPlainTextEdit; term_->setReadOnly(true);
    term_->setMinimumHeight(120);
    term_->setObjectName("mono");
    term_->setMaximumBlockCount(5000);
    tl->addWidget(term_);
    auto* srow = new QHBoxLayout;
    sysexBox_ = new QLineEdit;
    sysexBox_->setPlaceholderText(tr("F0 00 22 15 32 00 0F F7   (Entrée pour envoyer)"));
    sendBtn_ = new QPushButton(tr("envoyer")); sendBtn_->setObjectName("flat");
    auto* clearTerm = new QPushButton(tr("effacer")); clearTerm->setObjectName("flat");
    srow->addWidget(sysexBox_, 1);
    srow->addWidget(sendBtn_);
    srow->addWidget(clearTerm);
    tl->addLayout(srow);
    root->addWidget(tg);
    connect(clearTerm, &QPushButton::clicked, this, [this] { term_->clear(); });

    // ---- monitor ---------------------------------------------------------
    auto* mg = new QGroupBox(tr("Moniteur MIDI"));
    auto* ml = new QVBoxLayout(mg);
    auto* split = new QSplitter(Qt::Horizontal);
    monIn_  = new QPlainTextEdit; monIn_->setReadOnly(true);  monIn_->setObjectName("mono");  monIn_->setMaximumBlockCount(5000);
    monOut_ = new QPlainTextEdit; monOut_->setReadOnly(true); monOut_->setObjectName("mono"); monOut_->setMaximumBlockCount(5000);
    auto wrap = [](const QString& title, QPlainTextEdit* e) {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0); v->setSpacing(3);
        auto* lab = new QLabel(title); lab->setObjectName("dim");
        v->addWidget(lab); v->addWidget(e);
        return w;
    };
    split->addWidget(wrap(tr("IN"), monIn_));
    split->addWidget(wrap(tr("OUT"), monOut_));
    ml->addWidget(split, 1);
    auto* mrow = new QHBoxLayout;
    muteRt_ = new QCheckBox(tr("masquer horloge / temps réel"));
    muteRt_->setChecked(true);
    auto* clearMon = new QPushButton(tr("effacer")); clearMon->setObjectName("flat");
    mrow->addWidget(muteRt_, 1);
    mrow->addWidget(clearMon);
    ml->addLayout(mrow);
    root->addWidget(mg, 1);
    connect(clearMon, &QPushButton::clicked, this, [this] { monIn_->clear(); monOut_->clear(); });

    uploader_ = new Uploader(&out_, &outGuard_, this);
    connect(uploader_, &Uploader::log, this, [this](QString l, bool ok) {
        term_->appendPlainText((ok ? "  " : "! ") + l);
    });
    connect(uploader_, &Uploader::progress, this, [this](int p) { progress_->setValue(p); });
    connect(uploader_, &Uploader::finished, this, [this](bool) {
        // Shared by upload and query; each already logs its own outcome, so
        // this only hands the controls back.
        uploadBtn_->setEnabled(connected_ && !hexPath_->text().isEmpty());
        queryBtn_->setEnabled(connected_);
    });

    connect(refreshBtn_, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::toggleConnect);
    connect(queryBtn_,   &QPushButton::clicked, this, [this] {
        if (!connected_ || uploader_->busy()) return;
        uploader_->setDeviceId(uint8_t(idBox_->value()));
        queryBtn_->setEnabled(false);
        term_->appendPlainText("== interrogation ==");
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

    refreshPorts();
    setConnected(false);
}

MainWindow::~MainWindow()
{
    in_.close();
    out_.close();
}

void MainWindow::refreshPorts()
{
    const QString keepIn  = inBox_->currentText();
    const QString keepOut = outBox_->currentText();
    inBox_->clear();
    outBox_->clear();
    for (const auto& p : adios::inPorts())  inBox_->addItem(QString::fromStdString(p));
    for (const auto& p : adios::outPorts()) outBox_->addItem(QString::fromStdString(p));
    int i = inBox_->findText(keepIn);   if (i >= 0) inBox_->setCurrentIndex(i);
    int o = outBox_->findText(keepOut); if (o >= 0) outBox_->setCurrentIndex(o);
}

void MainWindow::setConnected(bool on)
{
    connected_ = on;
    connectBtn_->setText(on ? tr("déconnecter") : tr("connecter"));
    inBox_->setEnabled(!on);
    outBox_->setEnabled(!on);
    refreshBtn_->setEnabled(!on);
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
        link_->setText(tr("déconnecté"));
        return;
    }
    if (inBox_->count() == 0 || outBox_->count() == 0) {
        link_->setText(tr("aucun port MIDI"));
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
    link_->setText(tr("connecté — %1 → %2")
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

void MainWindow::routeIn(const adios::Bytes& msg, uint64_t t_us)
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
            QString line = QString("< %1  %2").arg(kind, txt.isEmpty() ? hexOf(r.payload) : txt);
            term_->appendPlainText(line);
        }
    }
    (void)t_us;
}

QString MainWindow::nowStamp()
{
    double s = clock_.isValid() ? clock_.elapsed() / 1000.0 : 0.0;
    return QString("%1").arg(s, 8, 'f', 3);
}

void MainWindow::monitorLine(bool out, const adios::Bytes& msg)
{
    adios::Decoded d = adios::decode(msg);
    if (d.isRealtime && muteRt_->isChecked()) return;
    QString line = QString("%1  %2   %3")
                       .arg(nowStamp())
                       .arg(d.label.c_str(), -34)
                       .arg(QString::fromStdString(d.hex));
    (out ? monOut_ : monIn_)->appendPlainText(line);
}

bool MainWindow::sendRaw(const adios::Bytes& msg)
{
    if (!connected_) return false;
    std::string err;
    bool ok;
    { std::lock_guard<std::mutex> g(outGuard_); ok = out_.send(msg, err); }
    if (ok) monitorLine(true, msg);
    else    term_->appendPlainText("! envoi : " + QString::fromStdString(err));
    return ok;
}

void MainWindow::sendSysex()
{
    if (!connected_) { term_->appendPlainText("! non connecté"); return; }
    adios::Bytes m;
    if (!adios::parseHexLine(sysexBox_->text().toStdString(), m)) {
        term_->appendPlainText("! hex invalide");
        return;
    }
    sendRaw(m);
}

void MainWindow::chooseHex()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Firmware .hex"), QString(),
                                             tr("Intel HEX (*.hex);;Tous (*)"));
    if (!f.isEmpty()) hexPath_->setText(f);
}

void MainWindow::doUpload()
{
    if (!connected_ || uploader_->busy()) return;
    uploader_->setDeviceId(uint8_t(idBox_->value()));
    progress_->setValue(0);
    uploadBtn_->setEnabled(false);
    term_->appendPlainText(QString("== upload %1 ==")
                               .arg(QFileInfo(hexPath_->text()).fileName()));
    uploader_->start(hexPath_->text());
}
