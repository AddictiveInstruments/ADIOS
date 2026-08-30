#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QRadioButton>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "Uploader.h"
#include "decode.h"
#include "ui_MainWindow.h"
#include "../../5x6_upgrader/core/sysex.h"

namespace {
// A small square button painting an equilateral disclosure triangle - pointing
// down when expanded (checked), right when collapsed. The side equals the text
// height, so it scales with the font.
class TriangleButton : public QToolButton {
public:
    explicit TriangleButton(QWidget* parent = nullptr) : QToolButton(parent) {
        setCheckable(true);
        setAutoRaise(true);
        setChecked(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }
    QSize sizeHint() const override { return QSize(16, fontMetrics().height()); }
protected:
    void paintEvent(QPaintEvent*) override {
        const double s = qMax(7.0, fontMetrics().ascent() * 0.60);   // side, kept small
        const double h = s * 0.86602540378;               // equilateral height (s*sqrt3/2)
        const double cx = width() / 2.0, cy = height() / 2.0;
        QPointF tri[3];                                   // centred by bounding box
        if (isChecked()) {                                // pointing down
            tri[0] = QPointF(cx - s / 2, cy - h / 2);
            tri[1] = QPointF(cx + s / 2, cy - h / 2);
            tri[2] = QPointF(cx,         cy + h / 2);
        } else {                                          // pointing right
            tri[0] = QPointF(cx - h / 2, cy - s / 2);
            tri[1] = QPointF(cx - h / 2, cy + s / 2);
            tri[2] = QPointF(cx + h / 2, cy);
        }
        QColor col = isChecked() ? QColor(0xdf, 0xe4, 0xec) : QColor(0x8a, 0x94, 0xa6);
        if (underMouse()) col = QColor(0xff, 0xff, 0xff);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPolygon(tri, 3);
    }
};

// A filter checkbox drawn by hand: a coloured box border (green = on, red = off,
// grey = disabled) with NO fill, a white tick when on, and text that dims when
// off (the "sub-filter is inactive" cue).
class FilterCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
    void setSub(bool s) { sub_ = s; update(); }   // child sub-filter -> dimmer text
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const int box = 12;
        const double top = (height() - box) / 2.0;
        // Border shows the STATE (green on / red off); text colour shows the TREE
        // LEVEL (parent brighter, child dimmer) - not the checked state.
        QColor border = isChecked() ? QColor(0x55, 0xb5, 0x6a) : QColor(0xe0, 0x66, 0x6b);
        QColor txt    = sub_ ? QColor(0x5a, 0x61, 0x72) : QColor(0x8a, 0x94, 0xa6);
        if (!isEnabled()) { border = QColor(0x3a, 0x3f, 0x4a); txt = QColor(0x41, 0x46, 0x50); }
        p.setPen(QPen(border, 1.0));
        p.setBrush(Qt::NoBrush);                         // no fill, ever
        p.drawRoundedRect(QRectF(0.5, top + 0.5, box - 1, box - 1), 2, 2);
        if (isChecked()) {                               // white tick
            QColor chk = isEnabled() ? QColor(0xff, 0xff, 0xff) : QColor(0x6a, 0x70, 0x7c);
            QPen pen(chk, 1.6); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            QPointF tick[3] = { QPointF(2.6, top + 6.2), QPointF(4.8, top + 8.6), QPointF(9.2, top + 3.0) };
            p.drawPolyline(tick, 3);
        }
        p.setPen(txt);
        p.drawText(QRect(box + 6, 0, width() - box - 6, height()),
                   Qt::AlignVCenter | Qt::AlignLeft, text());
    }
    QSize sizeHint() const override {
        return QSize(12 + 6 + fontMetrics().horizontalAdvance(text()) + 2,
                     qMax(16, fontMetrics().height()));
    }
private:
    bool sub_ = false;
};

// Renders a monitor line, drawing the two hex chars of a RECREATED running-status
// byte in a dimmer colour. The item carries UserRole = true and UserRole+1 = the
// character index of that byte; otherwise the line paints as one colour.
class MonitorDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        const QWidget* w = o.widget;
        QStyle* st = w ? w->style() : QApplication::style();
        p->save();
        st->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, p, w);   // background + selection
        const QString text = o.text;
        const QRect tr = st->subElementRect(QStyle::SE_ItemViewItemText, &o, w);
        const QFontMetrics fm(o.font);
        p->setFont(o.font);
        const QColor base = (o.state & QStyle::State_Selected)
                            ? o.palette.highlightedText().color() : QColor(0xdf, 0xe4, 0xec);
        const QColor dim = QColor(0x80, 0x8a, 0x99);      // recreated status byte
        int x = tr.left();
        auto seg = [&](const QString& s, const QColor& c) {
            if (s.isEmpty()) return;
            p->setPen(c);
            p->drawText(QRect(x, tr.top(), fm.horizontalAdvance(s) + 2, tr.height()),
                        Qt::AlignVCenter | Qt::AlignLeft, s);
            x += fm.horizontalAdvance(s);
        };
        const int pos = idx.data(Qt::UserRole + 1).toInt();
        if (idx.data(Qt::UserRole).toBool() && pos >= 0 && pos + 2 <= text.size()) {
            seg(text.left(pos), base);
            seg(text.mid(pos, 2), dim);
            seg(text.mid(pos + 2), base);
        } else {
            seg(text, base);
        }
        p->restore();
    }
};

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
    ui_ = new Ui::MainWindow;
    ui_->setupUi(this);

    // The .ui gives the static tree; the code keeps these pointers so the rest
    // of the logic reads unchanged, and applies the runtime-only properties a
    // designer file cannot express.
    inBox_        = ui_->inBox;
    outBox_       = ui_->outBox;
    idBox_        = ui_->idBox;
    queryBtn_     = ui_->queryBtn;
    devInfo_      = ui_->devInfo;
    hexPath_      = ui_->hexPath;
    browseBtn_    = ui_->browseBtn;
    uploadBtn_    = ui_->uploadBtn;
    progress_     = ui_->progress;
    uploadStatus_ = ui_->uploadStatus;
    term_         = ui_->term;
    sysexBox_     = ui_->sysexBox;
    sendBtn_      = ui_->sendBtn;
    monIn_        = ui_->monIn;
    monOut_       = ui_->monOut;
    monIn_->setItemDelegate(new MonitorDelegate(monIn_));
    monOut_->setItemDelegate(new MonitorDelegate(monOut_));

    // A .ui cannot carry a splitter's stretch factor, so both splitters get the
    // SAME recipe here: the first pane keeps its size (stretch 0), the second
    // takes the rest (stretch 1), and only the handle moves them - never a window
    // resize. Neither pane can be dragged away to nothing.
    //   rowsSplit (vertical):   Device-Info row on top | MIDI Monitor below
    //   colsSplit (horizontal): Device Info | Terminal / Upload-Status column
    for (QSplitter* sp : {ui_->rowsSplit, ui_->colsSplit}) {
        sp->setStretchFactor(0, 0);
        sp->setStretchFactor(1, 1);
        sp->setChildrenCollapsible(false);
    }
    // monSplit is the exception: BOTH monitor groups stretch EQUALLY, so their
    // current ratio is preserved on a window resize (50/50 by default).
    ui_->monSplit->setStretchFactor(0, 1);
    ui_->monSplit->setStretchFactor(1, 1);
    ui_->monSplit->setChildrenCollapsible(false);
    // Splitter layout, once they have a real size (a splitter ignores setSizes
    // given before it is shown). A remembered layout wins; otherwise the built-in
    // defaults - 220 px for the panels row, 300 px for Device Info (snug around
    // its widest line, the serial). stretch(0) then pins each first pane on a
    // window resize, so only the handle changes those sizes - and whatever the
    // user drags is saved on close and comes back next launch.
    QTimer::singleShot(0, this, [this] {
        QSettings s;
        const QByteArray rs = s.value("rowsSplitState").toByteArray();
        const QByteArray cs = s.value("colsSplitState").toByteArray();
        const QByteArray ms = s.value("monSplitState").toByteArray();
        if (rs.isEmpty()) ui_->rowsSplit->setSizes({220, qMax(0, ui_->rowsSplit->height() - 220)});
        else              ui_->rowsSplit->restoreState(rs);
        if (cs.isEmpty()) ui_->colsSplit->setSizes({300, qMax(0, ui_->colsSplit->width() - 300)});
        else              ui_->colsSplit->restoreState(cs);
        if (ms.isEmpty()) {                                  // 50/50: two equal
            int hw = ui_->monSplit->width() / 2;             // halves, above the
            ui_->monSplit->setSizes({hw, hw});               // group minimums so
        } else                                               // nothing gets clamped
            ui_->monSplit->restoreState(ms);
    });

    // A QSplitter handle does not repaint on mouse-over unless hover events are
    // enabled on it, so the ::handle:hover stylesheet rule never fires. Turn
    // them on by hand, on every splitter.
    for (QSplitter* sp : {ui_->rowsSplit, ui_->colsSplit, ui_->monSplit})
        for (int i = 0; i < sp->count(); ++i)
            if (QSplitterHandle* h = sp->handle(i)) h->setAttribute(Qt::WA_Hover, true);

    buildInputFilter();   // the collapsible Filter above the Input monitor list

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
    connect(uploader_, &Uploader::finished, this, [this](bool ok) {
        uploadBtn_->setEnabled(connected_ && !hexPath_->text().isEmpty());
        queryBtn_->setEnabled(connected_);
        // finished() is shared with the Ping/query worker; only an actual upload
        // runs the post-upload sequence.
        if (!wasUpload_) return;
        wasUpload_ = false;
        // Same timing as MIOS Studio: the uploader already waited 3 s after the
        // reboot command (Uploader::run), and the window adds 5 s more here
        // (UploadWindow.cpp TIMER_DELAYED_PROGRESS_OFF = 5000) so the app is sure
        // to have booted before we query it. Upload Status stays up the whole
        // time (the startup debug still fills the hidden Terminal); then the
        // Terminal comes back and the delayed re-read fires, refreshing Device
        // Info with what now runs.
        QTimer::singleShot(5000, this, [this, ok] {
            progress_->setValue(0);
            ui_->panelStack->setCurrentWidget(ui_->termGroup);
            if (ok && connected_) uploader_->queryInfo();
        });
    });

    // No Connect button: the ports open by themselves, and re-open whenever
    // the selection changes.
    connect(inBox_,  QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::connectPorts);
    connect(outBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::connectPorts);
    connect(queryBtn_,   &QPushButton::clicked, this, [this] {
        if (!connected_ || uploader_->busy()) return;
        uploader_->setDeviceId(uint8_t(idBox_->value()));
        // Do NOT disable the button here: disabling the focused widget makes Qt
        // hand focus to the next tab stop (the hex field), which then selects
        // all its text. The busy_ guard above already blocks a second Ping.
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

    // Fill and restore WITHOUT firing the combo signal (which would reconnect
    // on every added item); then open the remembered ports exactly once.
    {
        QSignalBlocker b1(inBox_), b2(outBox_);
        for (const auto& p : adios::inPorts())  inBox_->addItem(QString::fromStdString(p));
        for (const auto& p : adios::outPorts()) outBox_->addItem(QString::fromStdString(p));
        restoreSettings();
    }
    connectPorts();
}

MainWindow::~MainWindow()
{
    in_.close();
    out_.close();
    delete ui_;
}

// Last ports (by name, so a re-enumeration still finds them), device id, hex
// file and its folder come back on the next launch.
void MainWindow::restoreSettings()
{
    QSettings s;
    int i = inBox_->findText(s.value("inPort").toString());   if (i >= 0) inBox_->setCurrentIndex(i);
    int o = outBox_->findText(s.value("outPort").toString());  if (o >= 0) outBox_->setCurrentIndex(o);
    idBox_->setValue(s.value("deviceId", 0).toInt());
    hexPath_->setText(s.value("hexFile").toString());
    lastDir_ = s.value("browseDir").toString();
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("inPort",   inBox_->currentText());
    s.setValue("outPort",  outBox_->currentText());
    s.setValue("deviceId", idBox_->value());
    s.setValue("hexFile",  hexPath_->text());
    s.setValue("browseDir", lastDir_);
    s.setValue("rowsSplitState", ui_->rowsSplit->saveState());
    s.setValue("colsSplitState", ui_->colsSplit->saveState());
    s.setValue("monSplitState",  ui_->monSplit->saveState());

    // Filter: flags, channel mask, the Output "Apply Filter" toggle, and which
    // triangles are expanded - so the whole panel comes back as the user left it.
    s.setValue("filter/saved", true);
    for (const auto& f : filterFields()) s.setValue("filter/" + f.first, *f.second);
    s.setValue("filter/channelMask", filter_.channelMask);
    s.setValue("filter/applyOut", applyOutFilter_);
    if (filterBtn_) s.setValue("filter/expOuter", filterBtn_->isChecked());
    if (triVoice_)  s.setValue("filter/expVoice", triVoice_->isChecked());
    if (triSys_)    s.setValue("filter/expSys", triSys_->isChecked());
    if (triRt_)     s.setValue("filter/expRt", triRt_->isChecked());
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    saveSettings();
    e->accept();
}

void MainWindow::setConnected(bool on)
{
    connected_ = on;
    uploadBtn_->setEnabled(on && !hexPath_->text().isEmpty());
    sendBtn_->setEnabled(on);
    queryBtn_->setEnabled(on);
}

// Opens the selected In/Out ports. Called at startup and whenever a combo
// changes - there is no Connect button. A failure is reported in the Terminal
// (the status label is gone) and simply leaves the actions disabled.
void MainWindow::connectPorts()
{
    in_.close();
    out_.close();
    setConnected(false);

    if (inBox_->count() == 0 || outBox_->count() == 0) return;

    std::string err;
    if (!out_.open(unsigned(outBox_->currentIndex()), err)) {
        appendCapped(term_, new QListWidgetItem("! MIDI Out: " + QString::fromStdString(err)));
        return;
    }
    if (!in_.open(unsigned(inBox_->currentIndex()),
                  [this](const adios::Bytes& m, uint64_t t) { onMidiIn(m, t); }, err)) {
        out_.close();
        appendCapped(term_, new QListWidgetItem("! MIDI In: " + QString::fromStdString(err)));
        return;
    }
    uploader_->setDeviceId(uint8_t(idBox_->value()));
    clock_.restart();
    setConnected(true);
}

// ---- MIDI thread ---------------------------------------------------------
void MainWindow::onMidiIn(const adios::Bytes& msg, uint64_t t_us)
{
    // Wake a waiting Ping/upload RIGHT HERE, on the MIDI thread, the instant
    // the board answers - do NOT make the reply wait for the display timer.
    // That detour is what made a Ping miss the answer it had already received.
    // feedReply is mutex+condvar, safe to call from this thread.
    if (msg.size() >= 7 && msg[0] == 0xf0 && msg[1] == 0x00 &&
        msg[2] == 0x22 && msg[3] == 0x15) {
        tr5x6::Reply r = tr5x6::parse(msg.data(), msg.size());
        if (r.valid) uploader_->feedReply(r);
    }
    // The display still goes through the queue + timer.
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

    // Debug Terminal: the core sends console text as a "debug string" SysEx
    // (cmd 0x0D, sub 0x40) - F0 00 22 15 32 <id> 0D 40 <7-bit ascii...> F7.
    // Decode the ASCII and print it, one entry per embedded line. (Ping/query
    // ACKs are handled by the uploader and shown in Device Info, not here.)
    if (msg.size() >= 9 && msg[0] == 0xf0 && msg[1] == 0x00 && msg[2] == 0x22 &&
        msg[3] == 0x15 && msg[4] == 0x32 && msg[6] == 0x0d &&
        (msg[7] == 0x40 || msg[7] == 0x00)) {
        QString text;
        for (size_t i = 8; i < msg.size() && msg[i] != 0xf7; ++i)
            if (msg[i] < 0x80) text += QChar(msg[i]);
        const QStringList lines = text.split('\n');
        for (int k = 0; k < lines.size(); ++k) {
            if (k == lines.size() - 1 && lines[k].isEmpty()) break;   // drop the final newline
            appendCapped(term_, new QListWidgetItem(lines[k]));
        }
    }
}

void MainWindow::monitorLine(bool out, const adios::Bytes& msg)
{
    // The Filter always gates the Input side; it gates the Output side only when
    // its "Apply Filter" toggle is on (same settings, reused).
    if (!passesInFilter(msg) && (!out || applyOutFilter_)) return;

    // Running status: a channel-voice message repeating the previous status byte
    // travelled WITHOUT it on the wire, so its status byte here is recreated.
    uint8_t& last = out ? lastStatusOut_ : lastStatusIn_;
    bool running = false;
    if (!msg.empty()) {
        const uint8_t stb = msg[0];
        if (stb >= 0x80 && stb <= 0xef)      { running = (stb == last); last = stb; }
        else if (stb >= 0xf0 && stb <= 0xf7) { last = 0; }   // system common cancels it
        // 0xf8..0xff (real time) is interleaved and leaves running status untouched
    }

    adios::Decoded d = adios::decode(msg);
    const QString hex = hexFull(msg);
    const QString line = QString("%1   %2  %3").arg(nowStamp()).arg(d.label.c_str(), -38).arg(hex);
    auto* it = new QListWidgetItem(line);
    if (running) {
        it->setData(Qt::UserRole, true);
        it->setData(Qt::UserRole + 1, int(line.length() - hex.length()));   // status byte offset
    }
    appendCapped(out ? monOut_ : monIn_, it);
}

// Every on/off flag in filter_, paired with a stable key, so save and restore
// stay in sync from one list.
QList<QPair<QString, bool*>> MainWindow::filterFields()
{
    return {
        {"voice", &filter_.voice}, {"noteOnOff", &filter_.noteOnOff}, {"aftertouch", &filter_.aftertouch},
        {"control", &filter_.control}, {"program", &filter_.program}, {"chanPressure", &filter_.chanPressure},
        {"pitch", &filter_.pitch},
        {"sysCommon", &filter_.sysCommon}, {"timeCode", &filter_.timeCode}, {"songPos", &filter_.songPos},
        {"songSel", &filter_.songSel}, {"tuneReq", &filter_.tuneReq},
        {"realTime", &filter_.realTime}, {"clock", &filter_.clock}, {"startStop", &filter_.startStop},
        {"activeSense", &filter_.activeSense}, {"reset", &filter_.reset},
        {"sysex", &filter_.sysex}, {"invalid", &filter_.invalid},
    };
}

// Builds the collapsible Filter (a triangle header + a grid of checkboxes) and
// inserts it above the Input monitor's list. Each checkbox writes straight into
// filter_; passesInFilter() reads it for every incoming message.
void MainWindow::buildInputFilter()
{
    filterBtn_   = new TriangleButton;   // outer disclosure triangle
    filterPanel_ = new QWidget;
    // Hug the content vertically so collapsing categories leaves no dead space -
    // the monitor list (below, Expanding) takes back whatever the filter frees.
    filterPanel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // Restore the saved filter; on the very first launch (nothing saved) start
    // with everything collapsed and the default flags.
    QSettings fs;
    const bool haveFilter = fs.value("filter/saved", false).toBool();
    bool expOuter = false, expVoice = false, expSys = false, expRt = false;
    if (haveFilter) {
        for (const auto& f : filterFields()) *f.second = fs.value("filter/" + f.first, *f.second).toBool();
        filter_.channelMask = uint16_t(fs.value("filter/channelMask", filter_.channelMask).toUInt());
        applyOutFilter_ = fs.value("filter/applyOut", applyOutFilter_).toBool();
        expOuter = fs.value("filter/expOuter", true).toBool();
        expVoice = fs.value("filter/expVoice", true).toBool();
        expSys   = fs.value("filter/expSys", true).toBool();
        expRt    = fs.value("filter/expRt", true).toBool();
    }

    auto cb = [this](const QString& text, bool checked, bool* field, bool sub = true) {
        auto* c = new FilterCheckBox(text);
        c->setSub(sub);                          // children are sub-filters (dimmer)
        c->setChecked(checked);
        connect(c, &QCheckBox::toggled, this, [field](bool v) { *field = v; });
        return c;
    };

    auto* col = new QVBoxLayout(filterPanel_);
    col->setContentsMargins(0, 4, 0, 6);
    col->setSpacing(2);

    // A collapsible category: [triangle(16)][master cb]; children are indented by
    // exactly the 16 px triangle gutter, so every checkbox lines up in one column.
    auto addCategory = [this, col](const QString& title, bool* master, bool checked, bool expanded,
                                   QVBoxLayout* body, QList<QWidget*> kids) -> QToolButton* {
        auto* tri = new TriangleButton; tri->setFixedWidth(16);
        auto* mcb = new FilterCheckBox(title);
        mcb->setChecked(checked);
        auto* head = new QHBoxLayout; head->setContentsMargins(0, 0, 0, 0); head->setSpacing(0);
        head->addWidget(tri); head->addWidget(mcb); head->addStretch();
        col->addLayout(head);
        auto* w = new QWidget; w->setLayout(body);
        col->addWidget(w);
        for (auto* k : kids) k->setEnabled(checked);
        connect(mcb, &QCheckBox::toggled, this, [master, kids](bool v) { *master = v; for (auto* k : kids) k->setEnabled(v); });
        connect(tri, &QToolButton::toggled, w, [w](bool on) { w->setVisible(on); });
        tri->setChecked(expanded);
        return tri;
    };

    // ---- Voice Messages: the six types on the left, Channels block on the
    //      right (level with Note On/Off) ----
    {
        auto* body = new QVBoxLayout; body->setContentsMargins(16, 0, 0, 4); body->setSpacing(2);
        auto* twoCol = new QHBoxLayout; twoCol->setContentsMargins(0, 0, 0, 0); twoCol->setSpacing(28);
        body->addLayout(twoCol);

        auto* left = new QVBoxLayout; left->setContentsMargins(0, 0, 0, 0); left->setSpacing(2);
        QList<QWidget*> kids;
        kids << cb("Note On/Off", filter_.noteOnOff, &filter_.noteOnOff)
             << cb("Aftertouch (Poly)", filter_.aftertouch, &filter_.aftertouch)
             << cb("Control", filter_.control, &filter_.control)
             << cb("Program", filter_.program, &filter_.program)
             << cb("Channel Pressure", filter_.chanPressure, &filter_.chanPressure)
             << cb("Pitch Wheel", filter_.pitch, &filter_.pitch);
        for (auto* k : kids) left->addWidget(k);
        left->addStretch();

        auto* right = new QVBoxLayout; right->setContentsMargins(0, 0, 0, 0); right->setSpacing(2);
        auto* lblCh = new QLabel("Channels"); lblCh->setObjectName("filterSub");   // child of Voice
        right->addWidget(lblCh);

        auto* chBody = new QWidget;
        auto* chRow = new QHBoxLayout(chBody); chRow->setContentsMargins(0, 2, 0, 0); chRow->setSpacing(10);
        auto* grid = new QGridLayout; grid->setContentsMargins(0, 0, 0, 0); grid->setSpacing(3);
        QList<QPushButton*> chBtns;
        for (int i = 0; i < 16; ++i) {
            auto* pb = new QPushButton(QString::number(i + 1));
            pb->setObjectName("chanBtn"); pb->setCheckable(true); pb->setFixedSize(18, 18);
            pb->setChecked(filter_.channelMask & (1u << i));
            connect(pb, &QPushButton::toggled, this, [this, i](bool v) {
                filter_.channelMask = uint16_t(v ? (filter_.channelMask | (1u << i))
                                                 : (filter_.channelMask & ~(1u << i)));
            });
            chBtns << pb; grid->addWidget(pb, i / 4, i % 4);
        }
        auto* setAll = new QPushButton("Set all");   setAll->setObjectName("miniBtn"); setAll->setFixedHeight(18);
        auto* clrAll = new QPushButton("Clear all"); clrAll->setObjectName("miniBtn"); clrAll->setFixedHeight(18);
        connect(setAll, &QPushButton::clicked, this, [chBtns] { for (auto* pb : chBtns) pb->setChecked(true); });
        connect(clrAll, &QPushButton::clicked, this, [chBtns] { for (auto* pb : chBtns) pb->setChecked(false); });
        // spacing 3 like the grid rows so Set all lines up with 1-4, Clear all with 5-8
        auto* btnCol = new QVBoxLayout; btnCol->setContentsMargins(0, 0, 0, 0); btnCol->setSpacing(3);
        btnCol->addWidget(setAll); btnCol->addWidget(clrAll); btnCol->addStretch();
        chRow->addLayout(grid); chRow->addLayout(btnCol); chRow->addStretch();
        right->addWidget(chBody);
        right->addStretch();

        twoCol->addLayout(left);
        twoCol->addLayout(right);
        twoCol->addStretch();

        kids << lblCh << setAll << clrAll;
        for (auto* pb : chBtns) kids << pb;
        triVoice_ = addCategory("Voice Messages", &filter_.voice, filter_.voice, expVoice, body, kids);
    }

    // ---- System Common ----
    {
        auto* body = new QVBoxLayout; body->setContentsMargins(16, 0, 0, 4); body->setSpacing(2);
        QList<QWidget*> kids;
        kids << cb("Time Code", filter_.timeCode, &filter_.timeCode)
             << cb("Song Position Pointer", filter_.songPos, &filter_.songPos)
             << cb("Song Select", filter_.songSel, &filter_.songSel)
             << cb("Tune Request", filter_.tuneReq, &filter_.tuneReq);
        for (auto* k : kids) body->addWidget(k);
        triSys_ = addCategory("System Common", &filter_.sysCommon, filter_.sysCommon, expSys, body, kids);
    }

    // ---- Real Time (off by default) ----
    {
        auto* body = new QVBoxLayout; body->setContentsMargins(16, 0, 0, 4); body->setSpacing(2);
        QList<QWidget*> kids;
        kids << cb("Clock", filter_.clock, &filter_.clock)
             << cb("Start/Stop/Continue", filter_.startStop, &filter_.startStop)
             << cb("Active Sense", filter_.activeSense, &filter_.activeSense)
             << cb("Reset", filter_.reset, &filter_.reset);
        for (auto* k : kids) body->addWidget(k);
        triRt_ = addCategory("Real Time", &filter_.realTime, filter_.realTime, expRt, body, kids);
    }

    // ---- standalone, aligned in the same checkbox column (16 px gutter) ----
    auto standalone = [col](QCheckBox* c) {
        auto* row = new QHBoxLayout; row->setContentsMargins(0, 0, 0, 0); row->setSpacing(0);
        row->addSpacing(16); row->addWidget(c); row->addStretch();
        col->addLayout(row);
    };
    standalone(cb("System Exclusive", filter_.sysex, &filter_.sysex, false));
    standalone(cb("Invalid", filter_.invalid, &filter_.invalid, false));

    // ---- outer collapse: triangle + "Filter" label, above the list ----
    filterBtn_->setChecked(expOuter);
    connect(filterBtn_, &QToolButton::toggled, this, [this](bool on) { filterPanel_->setVisible(on); });
    filterPanel_->setVisible(filterBtn_->isChecked());

    auto* outerHead = new QWidget;
    auto* oh = new QHBoxLayout(outerHead); oh->setContentsMargins(0, 0, 0, 0); oh->setSpacing(4);
    auto* flabel = new QLabel("Filter"); flabel->setObjectName("filterLabel");
    oh->addWidget(filterBtn_); oh->addWidget(flabel); oh->addStretch();

    auto* lay = qobject_cast<QVBoxLayout*>(ui_->inGroup->layout());
    lay->insertWidget(0, outerHead);
    lay->insertWidget(1, filterPanel_);

    // The Output monitor gets an "Apply Filter" toggle, level with the Input's
    // "Filter" header. On, the OUT side reuses the very same filter settings.
    auto* applyOut = new FilterCheckBox("Apply Filter");
    applyOut->setChecked(applyOutFilter_);
    connect(applyOut, &QCheckBox::toggled, this, [this](bool v) { applyOutFilter_ = v; });
    if (auto* ol = qobject_cast<QVBoxLayout*>(ui_->outGroup->layout()))
        ol->insertWidget(0, applyOut, 0, Qt::AlignLeft);
}

// Does an incoming message pass the Input filter? Classified by MIDI status byte.
bool MainWindow::passesInFilter(const adios::Bytes& m) const
{
    if (m.empty()) return true;
    uint8_t st = m[0];
    if (st < 0x80) return filter_.invalid;               // stray data byte, no status
    auto chan = [&] { return (filter_.channelMask & (1u << (st & 0x0f))) != 0; };
    switch (st & 0xf0) {
    case 0x80: case 0x90: return filter_.voice && filter_.noteOnOff && chan();
    case 0xA0:            return filter_.voice && filter_.aftertouch && chan();
    case 0xB0:            return filter_.voice && filter_.control && chan();
    case 0xC0:            return filter_.voice && filter_.program && chan();
    case 0xD0:            return filter_.voice && filter_.chanPressure && chan();
    case 0xE0:            return filter_.voice && filter_.pitch && chan();
    case 0xF0:
        switch (st) {
        case 0xF0: case 0xF7: return filter_.sysex;
        case 0xF1:           return filter_.sysCommon && filter_.timeCode;
        case 0xF2:           return filter_.sysCommon && filter_.songPos;
        case 0xF3:           return filter_.sysCommon && filter_.songSel;
        case 0xF6:           return filter_.sysCommon && filter_.tuneReq;
        case 0xF8:           return filter_.realTime && filter_.clock;
        case 0xFA: case 0xFB: case 0xFC: return filter_.realTime && filter_.startStop;
        case 0xFE:           return filter_.realTime && filter_.activeSense;
        case 0xFF:           return filter_.realTime && filter_.reset;
        default:             return filter_.invalid;     // F4/F5/F9/FD undefined
        }
    }
    return filter_.invalid;
}

QString MainWindow::nowStamp()
{
    return QTime::currentTime().toString("HH:mm:ss.zzz");   // absolute time of day
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
    const QString cmd = sysexBox_->text();
    // Terminal "input string": F0 00 22 15 32 <id> 0D 00 <7-bit ascii...> 0A F7.
    // The trailing '\n' is what makes the core dispatch the accumulated line.
    adios::Bytes m = { 0xf0, 0x00, 0x22, 0x15, 0x32, uint8_t(idBox_->value()), 0x0d, 0x00 };
    for (QChar c : cmd) m.push_back(uint8_t(c.toLatin1()) & 0x7f);
    m.push_back('\n');
    m.push_back(0xf7);
    sendRaw(m);
    appendCapped(term_, new QListWidgetItem("> " + cmd));   // echo the typed line
    sysexBox_->clear();
}

void MainWindow::chooseHex()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Firmware .hex"), lastDir_,
                                             tr("Intel HEX (*.hex);;All files (*)"));
    if (!f.isEmpty()) {
        hexPath_->setText(f);
        lastDir_ = QFileInfo(f).absolutePath();
    }
}

void MainWindow::doUpload()
{
    if (!connected_ || uploader_->busy()) return;
    wasUpload_ = true;
    ui_->panelStack->setCurrentWidget(ui_->statusGroup);   // show Upload Status
    uploader_->setDeviceId(uint8_t(idBox_->value()));
    progress_->setValue(0);
    uploadBtn_->setEnabled(false);
    uploadStatus_->clear();
    appendCapped(uploadStatus_, new QListWidgetItem(
        QString("== upload %1 ==").arg(QFileInfo(hexPath_->text()).fileName())));
    uploader_->start(hexPath_->text());
}
