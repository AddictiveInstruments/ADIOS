#include "MainWindow.h"
#include "../core/sysex.h"
#include "../platform/midi_win.h"
#include "../cli/upgrade.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

#include <chrono>
#include <mutex>

using namespace tr5x6;

// ---------------------------------------------------------------------------
// The transport, shared by the worker and by Query. Only one of the two ever
// runs at a time, so a single pair of ports is enough.
// ---------------------------------------------------------------------------
namespace {

MidiIn             g_in;
MidiOut            g_out;
std::mutex         g_mutex;
std::vector<Reply> g_replies;
bool               g_open = false;

void onSysex(const std::vector<uint8_t>& msg)
{
    Reply r = parse(msg.data(), msg.size());
    if (!r.valid) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_replies.push_back(std::move(r));
}

bool openPorts(unsigned in, unsigned out, QString& err)
{
    std::string e;
    g_in.close(); g_out.close(); g_open = false;
    if (!g_in.open(in, onSysex, e))  { err = QString::fromStdString(e); return false; }
    if (!g_out.open(out, e))         { err = QString::fromStdString(e); return false; }
    g_open = true;
    return true;
}

Reply exchangeRaw(const Bytes& msg, int ms)
{
    { std::lock_guard<std::mutex> lk(g_mutex); g_replies.clear(); }
    std::string err;
    if (!g_out.sendSysex(msg, err)) return Reply{};
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (!g_replies.empty()) return g_replies.front();
        }
        QThread::msleep(1);
    }
    return Reply{};
}

// The worker's report hooks. They are plain function pointers, so they cannot
// carry a `this` - the task pointer lives here for the duration of one run.
UpgradeTask* g_task = nullptr;

void hookSend(Link& lk, const Bytes& b)     { (void)lk; exchangeRaw(b, 0); }
Reply hookExchange(Link& lk, const Bytes& b, int ms) { (void)lk; return exchangeRaw(b, ms); }

} // namespace

// ---------------------------------------------------------------------------
// UpgradeTask
// ---------------------------------------------------------------------------

UpgradeTask::UpgradeTask(unsigned inPort, unsigned outPort, uint8_t deviceId,
                         QString migrationHex, QString applicationHex)
    : in_(inPort), out_(outPort), id_(deviceId),
      mig_(std::move(migrationHex)), app_(std::move(applicationHex)) {}

void UpgradeTask::run()
{
    g_task = this;

    Link lk;
    lk.out        = &g_out;
    lk.deviceId   = id_;
    lk.sendFn     = hookSend;
    lk.exchangeFn = hookExchange;

    Log log;
    log.infoFn     = [](const char* s) { if (g_task) emit g_task->info(QString::fromUtf8(s)); };
    log.warnFn     = [](const char* s) { if (g_task) emit g_task->warn(QString::fromUtf8(s)); };
    log.okFn       = [](const char* s) { if (g_task) emit g_task->ok(QString::fromUtf8(s)); };
    log.stepFn     = [](int i, int st)  { if (g_task) emit g_task->step(i, st); };
    log.errFn      = [](const char* s) { if (g_task) emit g_task->error(QString::fromUtf8(s)); };
    log.statusFn   = [](const char* s) { if (g_task) emit g_task->status(QString::fromUtf8(s)); };
    log.lockFn     = [](bool on)       { if (g_task) emit g_task->locked(on); };
    log.progressFn = [](size_t d, size_t t) {
        if (g_task) emit g_task->progress(static_cast<int>(d), static_cast<int>(t));
    };

    UpgradeImages img;
    img.migrationHex   = mig_.toStdString();
    img.applicationHex = app_.toStdString();

    const bool ok = upgrade(lk, img, log);
    g_task = nullptr;
    emit finished(ok);
}

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------

MainWindow::MainWindow()
{
    // The title lives in the window bar and NOWHERE else - repeating it inside
    // the window is a line of chrome that says the same thing twice.
    setWindowTitle(tr("5x6 firmware update  (Beta-Testers Only)"));
    setFixedWidth(430);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(13, 12, 13, 13);
    root->setSpacing(9);

    // ---- the four controls ------------------------------------------------
    auto row = [&](const QString& label, QWidget* w) {
        auto* h = new QHBoxLayout;
        auto* l = new QLabel(label);
        l->setObjectName("dim");
        l->setFixedWidth(64);
        h->addWidget(l);
        h->addWidget(w, 1);
        root->addLayout(h);
        return h;
    };

    inBox_  = new QComboBox;
    outBox_ = new QComboBox;
    for (const auto& s : midiInPorts())  inBox_->addItem(QString::fromStdString(s));
    for (const auto& s : midiOutPorts()) outBox_->addItem(QString::fromStdString(s));
    row(tr("MIDI in"), inBox_);
    row(tr("MIDI out"), outBox_);

    idBox_ = new QSpinBox;
    idBox_->setRange(0, 15);
    idBox_->setFixedWidth(52);
    queryBtn_ = new QPushButton(tr("Query"));
    {
        auto* h = new QHBoxLayout;
        auto* l = new QLabel(tr("Device ID"));
        l->setObjectName("dim");
        l->setFixedWidth(64);
        h->addWidget(l);
        h->addWidget(idBox_);
        h->addStretch(1);
        h->addWidget(queryBtn_);
        root->addLayout(h);
    }

    auto* rule = new QFrame;
    rule->setFrameShape(QFrame::HLine);
    rule->setObjectName("rule");
    root->addWidget(rule);

    // ---- everything below is inert until the board answers -----------------
    auto* detail = new QVBoxLayout;
    detail->setSpacing(2);
    auto field = [&](const QString& name, QLabel*& value) {
        auto* h = new QHBoxLayout;
        auto* l = new QLabel(name);
        l->setObjectName("dim");
        value = new QLabel(QStringLiteral("-"));
        value->setAlignment(Qt::AlignRight);
        h->addWidget(l);
        h->addWidget(value, 1);
        detail->addLayout(h);
    };
    field(tr("Machine"), machine_);
    field(tr("Firmware"), firmware_);
    field(tr("Banks"), banks_);
    auto* detailBox = new QFrame;
    detailBox->setObjectName("inset");
    detailBox->setLayout(detail);
    root->addWidget(detailBox);

    // The three steps, always visible: a tester who is told to wait deserves to
    // see WHAT is being waited for, and an error deserves to name its step.
    {
        static const char* names[3] = { "Install migrating tool",
                                        "Bootloader update",
                                        "Application update" };
        auto* steps = new QVBoxLayout;
        steps->setSpacing(5);
        for (int i = 0; i < 3; ++i) {
            stepBar_[i] = new QProgressBar;
            stepBar_[i]->setRange(0, 100);
            stepBar_[i]->setValue(0);
            stepBar_[i]->setFixedHeight(20);
            stepBar_[i]->setTextVisible(true);
            stepBar_[i]->setAlignment(Qt::AlignCenter);
            stepBar_[i]->setFormat(tr(names[i]));
            steps->addWidget(stepBar_[i]);
        }
        root->addLayout(steps);
    }

    status_ = new QLabel;
    status_->setObjectName("dim");
    root->addWidget(status_);

    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setFixedHeight(150);
    root->addWidget(log_);

    {
        auto* h = new QHBoxLayout;
        warning_ = new QLabel;
        warning_->setObjectName("warn");
        closeBtn_ = new QPushButton(tr("Close"));
        h->addWidget(warning_, 1);
        h->addWidget(closeBtn_);
        root->addLayout(h);
    }

    connect(queryBtn_, &QPushButton::clicked, this, &MainWindow::onQuery);
    connect(closeBtn_, &QPushButton::clicked, this, &QWidget::close);

    setGateEnabled(false);
}

MainWindow::~MainWindow()
{
    if (thread_) { thread_->quit(); thread_->wait(3000); }
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    // The lock is not advisory: closing mid-sequence leaves a board with no
    // usable bootloader, and only a SWD probe gets it back.
    if (locked_) { e->ignore(); return; }
    e->accept();
}

void MainWindow::setGateEnabled(bool on)
{
    for (QWidget* w : { static_cast<QWidget*>(machine_), static_cast<QWidget*>(firmware_),
                        static_cast<QWidget*>(banks_),
                        static_cast<QWidget*>(stepBar_[0]),
                        static_cast<QWidget*>(stepBar_[1]),
                        static_cast<QWidget*>(stepBar_[2]) })
        w->setEnabled(on);
}

void MainWindow::appendLog(const QString& line, const char* colour)
{
    if (line.isEmpty()) return;
    log_->appendHtml(QStringLiteral("<span style='color:%1'>%2</span>")
                         .arg(QString::fromLatin1(colour), line.toHtmlEscaped()));
}

void MainWindow::onQuery()
{
    QString err;
    if (!openPorts(static_cast<unsigned>(inBox_->currentIndex()),
                   static_cast<unsigned>(outBox_->currentIndex()), err)) {
        appendLog(err, "#e06c6c");
        return;
    }

    const uint8_t id = static_cast<uint8_t>(idBox_->value());

    Reply r = exchangeRaw(ping(Target::App, id), 400);
    if (!(r.valid && r.cmd == ACK)) {
        appendLog(tr("no answer - check the ports and the device ID"), "#e06c6c");
        return;
    }
    unitType_ = r.arg;
    machine_->setText(unitType_ == 0x62 ? QStringLiteral("TR-626")
                    : unitType_ == 0x50 ? QStringLiteral("TR-505")
                                        : QStringLiteral("unknown"));
    banks_->setText(unitType_ == 0x62 ? QStringLiteral("8 to preserve")
                                      : QStringLiteral("16 to preserve"));

    r = exchangeRaw(query(id, Q_OS_NAME), 300);
    const QString os = (r.valid && r.cmd == ACK) ? QString::fromStdString(r.text()) : QString();
    const bool legacy = (os != QLatin1String("ADIOS"));

    // AppName1, not AppName2: line 1 carries the application NAME
    // ("5x6 Display/ROM"), line 2 carries the copyright.
    r = exchangeRaw(query(id, querySub(QueryItem::AppName1, legacy)), 300);
    firmware_->setText((r.valid && r.cmd == ACK)
                           ? QString::fromStdString(r.text())
                           : QStringLiteral("-"));

    setGateEnabled(true);

    if (!legacy) {
        appendLog(tr("this board already runs the current firmware - nothing to do"), "#6fcf8f");
        return;
    }

    appendLog(tr("legacy board detected - starting"), "#6f9ad6");
    startSequence();
}

void MainWindow::startSequence()
{
    // The machine decides WHICH image: the tool that runs on the board cannot
    // be told the type, because the application that answered the ping is gone
    // by then. So it is baked in, and we pick the file.
    // The images sit beside the executable on Windows, but inside
    // Contents/Resources on macOS - Contents/MacOS is code-only, and codesign
    // refuses to seal a bundle that has data in it. applicationDirPath() is
    // Contents/MacOS there, hence the step up.
#ifdef Q_OS_MACOS
    const QString dir = QCoreApplication::applicationDirPath() +
                        QStringLiteral("/../Resources/images/");
#else
    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/images/");
#endif
    const QString mig = dir + (unitType_ == 0x62 ? QStringLiteral("5x6_migration_626.hex")
                                                 : QStringLiteral("5x6_migration_505.hex"));
    const QString app = dir + QStringLiteral("5x6_display_rev1_app_only.hex");

    for (const QString& f : { mig, app })
        if (!QFile::exists(f)) { appendLog(tr("missing image: %1").arg(f), "#e06c6c"); return; }

    queryBtn_->setEnabled(false);
    inBox_->setEnabled(false);
    outBox_->setEnabled(false);
    idBox_->setEnabled(false);

    auto* task = new UpgradeTask(static_cast<unsigned>(inBox_->currentIndex()),
                                 static_cast<unsigned>(outBox_->currentIndex()),
                                 static_cast<uint8_t>(idBox_->value()), mig, app);
    // NO PARENT, deliberately. As a child of this window the thread was
    // destroyed by ~QWidget's deleteChildren() on the way out, and Qt answers
    // the destruction of a thread that has not stopped with qFatal - the whole
    // process aborts. It owns itself now and goes when it has really finished.
    thread_ = new QThread;
    task->moveToThread(thread_);

    connect(thread_, &QThread::started,  task, &UpgradeTask::run);
    connect(task, &UpgradeTask::info,     this, &MainWindow::onInfo);
    connect(task, &UpgradeTask::warn,     this, &MainWindow::onWarn);
    connect(task, &UpgradeTask::ok,       this, &MainWindow::onOk);
    connect(task, &UpgradeTask::step,     this, &MainWindow::onStep);
    connect(task, &UpgradeTask::error,    this, &MainWindow::onError);
    connect(task, &UpgradeTask::status,   this, &MainWindow::onStatus);
    connect(task, &UpgradeTask::progress, this, &MainWindow::onProgress);
    connect(task, &UpgradeTask::locked,   this, &MainWindow::onLocked);
    connect(task, &UpgradeTask::finished, this, &MainWindow::onFinished);
    connect(task, &UpgradeTask::finished, thread_, &QThread::quit);
    connect(thread_, &QThread::finished,  task, &QObject::deleteLater);
    // And the thread disposes of ITSELF, once stopped for real. This is the
    // line that makes the unparented thread safe rather than merely leaked.
    connect(thread_, &QThread::finished,  thread_, &QObject::deleteLater);

    thread_->start();
}

void MainWindow::onInfo(const QString& line)   { appendLog(line, "#8a94a6"); }
void MainWindow::onWarn(const QString& line)   { appendLog(line, "#e06c6c"); }
void MainWindow::onOk(const QString& line)     { appendLog(line, "#6fcf8f"); }

void MainWindow::onStep(int index, int state)
{
    if (index < 0 || index > 2) return;
    // pending grey, running blue, done green - the same three colours the log
    // uses, so a glance at either tells the same story
    static const char* fill[3] = { "#2e3644", "#4a7ab8", "#3f7d5c" };
    stepBar_[index]->setStyleSheet(
        QStringLiteral("QProgressBar::chunk { background:%1; border-radius:2px; }")
            .arg(QLatin1String(fill[state])));
    if (state == 1) curStep_ = index;
    if (state == 2) stepBar_[index]->setValue(100);
}
void MainWindow::onError(const QString& line)  { appendLog(line, "#e06c6c"); }
void MainWindow::onStatus(const QString& line) { status_->setText(line); }

void MainWindow::onProgress(int done, int total)
{
    // Progress always belongs to the step that is running. A step with no
    // transfer of its own - waiting for the operator, say - simply never
    // reports any, and its bar stays where onStep put it.
    if (curStep_ < 0 || curStep_ > 2) return;
    stepBar_[curStep_]->setValue(total > 0 ? done * 100 / total : 0);
}

void MainWindow::onLocked(bool on)
{
    locked_ = on;
    closeBtn_->setEnabled(!on);
    warning_->setText(on ? tr("Do not disconnect the board") : QString());
}

void MainWindow::onFinished(bool ok)
{
    onLocked(false);
    status_->clear();
    // Nothing to add on success: the sequence already checked the board and
    // said so. Only a failure needs a closing line.
    if (!ok) appendLog(tr("stopped"), "#e06c6c");
    queryBtn_->setEnabled(true);
    inBox_->setEnabled(true);
    outBox_->setEnabled(true);
    idBox_->setEnabled(true);
    // thread_ is NOT cleared here any more. It used to be, and that was the
    // bug: the pointer went to zero while the thread object lived on, so the
    // destructor's "stop it first" guard tested zero and did nothing - and the
    // window then destroyed a running thread. QPointer now zeroes it at the
    // one moment that is true, when the object is actually gone.
}
