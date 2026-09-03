#include "MainWindow.h"

#include <QApplication>
#include <QScreen>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QFile>
#include <algorithm>
#include <memory>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QStyle>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QButtonGroup>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QVector>
#include <QWheelEvent>
#include <functional>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QImage>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPointer>
#include <cmath>
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
#include "sysex.h"

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
    void setNeutral(bool n) { neutral_ = n; update(); }  // off-state gray, not red
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const int box = 12;
        const double top = (height() - box) / 2.0;
        // Border shows the STATE (green on / red off); text colour shows the TREE
        // LEVEL (parent brighter, child dimmer) - not the checked state.
        QColor border = neutral_ ? QColor(0x5a, 0x61, 0x72)                    // neutral: gray both states, no red/green
                                 : (isChecked() ? QColor(0x55, 0xb5, 0x6a) : QColor(0xe0, 0x66, 0x6b));
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
    bool neutral_ = false;
};

// A QMenu that STAYS OPEN when a checkable item is clicked, so several toggles can
// be flipped in one go. Submenus and plain items behave normally.
class StayOpenMenu : public QMenu {
public:
    using QMenu::QMenu;
protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        QAction* a = activeAction();
        if (a && a->isEnabled() && a->isCheckable()) { a->trigger(); return; }   // toggle, don't close
        QMenu::mouseReleaseEvent(e);
    }
};

// Per-monitor-item data roles. UserRole/+1 drive the delegate (dim the recreated
// running-status byte); +2..+4 keep the three column parts so the View column
// toggles can rebuild a line already on screen; +5 marks a running-status repeat
// even while Raw Data is hidden (so re-showing it dims the right byte again).
enum {
    Role_Dim    = Qt::UserRole,      // bool: draw dimmed status byte(s) NOW
    Role_HexPos = Qt::UserRole + 1,  // QVariantList<int>: their offsets in the current text
    Role_Stamp  = Qt::UserRole + 2,  // QString: timestamp column
    Role_Label  = Qt::UserRole + 3,  // QString: decoded column
    Role_Hex    = Qt::UserRole + 4,  // QString: raw-data column
    Role_Run    = Qt::UserRole + 5,  // bool: this line repeats running status
    Role_RunPos = Qt::UserRole + 6,  // QVariantList<int>: a merged line (NRPN) - offsets IN THE RAW
                                     // COLUMN of the status bytes that were recreated
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
        QList<int> dims;                                   // every recreated status byte on the line
        if (idx.data(Qt::UserRole).toBool())
            for (const QVariant& v : idx.data(Qt::UserRole + 1).toList()) {
                const int pos = v.toInt();
                if (pos >= 0 && pos + 2 <= text.size()) dims << pos;
            }
        std::sort(dims.begin(), dims.end());
        int at = 0;
        for (int pos : dims) {
            if (pos < at) continue;
            seg(text.mid(at, pos - at), base);
            seg(text.mid(pos, 2), dim);
            at = pos + 2;
        }
        seg(text.mid(at), base);
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

static QString noteName(int m)
{
    static const char* N[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return QString("%1%2").arg(N[m % 12]).arg(m / 12 - 1);
}

// A resizable on-screen MIDI keyboard: keys are a FIXED width, so a wider window
// shows MORE keys, never wider ones. Custom-painted, no Q_OBJECT - it reports
// notes through two std::function callbacks the owner sets.
class PianoKeyboard : public QWidget {
public:
    std::function<void(int note, int vel)> onNoteOn;
    std::function<void(int note)>          onNoteOff;
    std::function<void()>                  onView;   // fires when the visible slice changes
    static constexpr int KW = 24, BW = 15;
    explicit PianoKeyboard(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(72);
        for (int m = 0; m <= 127; ++m) if (!isBlack(m)) allWhite_.push_back(m);   // the WHOLE range
        center_ = qMax(0, allWhite_.indexOf(60));                                 // start on middle C
    }
    int  totalWhite()   const { return int(allWhite_.size()); }
    int  visibleWhite() const { return qBound(1, width() / KW, int(allWhite_.size())); }
    int  fullWidth()    const { return int(allWhite_.size()) * KW; }              // width that shows all keys
    int  scroll()       const { return scroll_; }
    int  maxScroll()    const { return qMax(0, int(allWhite_.size()) - visibleWhite()); }
    void setScroll(int s) {
        s = qBound(0, s, maxScroll());
        if (s == scroll_) return;
        scroll_ = s; center_ = scroll_ + visibleWhite() / 2;   // remember where we are
        update(); if (onView) onView();
    }
protected:
    void resizeEvent(QResizeEvent*) override {
        // Keep the CENTER fixed: widening/narrowing reveals/hides keys on BOTH sides.
        scroll_ = qBound(0, center_ - visibleWhite() / 2, maxScroll());
        if (onView) onView();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing, true);
        const int vis = visibleWhite(), h = height(), bh = h * 62 / 100, n = int(allWhite_.size());
        for (int i = 0; i < vis && scroll_ + i < n; ++i) {          // white keys
            const int m = allWhite_[scroll_ + i];
            const QRect r(i * KW, 0, KW, h);
            p.setPen(QColor(0xaa, 0xb4, 0xc2));
            p.setBrush(m == cur_ ? hitColor(false) : QColor(0xe9, 0xed, 0xf3));
            p.drawRoundedRect(QRectF(r).adjusted(0.5, -3, -0.5, -0.5), 3, 3);      // top clipped -> bottom rounded
            if (m % 12 == 0) {                                                     // label each C
                p.setPen(QColor(0x5a, 0x61, 0x72));
                p.drawText(r.adjusted(0, 0, 0, -3), Qt::AlignHCenter | Qt::AlignBottom, noteName(m));
            }
        }
        for (int i = 0; i + 1 < vis && scroll_ + i + 1 < n; ++i) {  // black keys, between visible whites
            const int bm = allWhite_[scroll_ + i] + 1;
            if (!isBlack(bm)) continue;
            const QRect r((i + 1) * KW - BW / 2, 0, BW, bh);
            p.setPen(QColor(0x0c, 0x11, 0x18));
            p.setBrush(bm == cur_ ? hitColor(true) : QColor(0x20, 0x25, 0x2f));
            p.drawRoundedRect(QRectF(r).adjusted(0.5, -3, -0.5, -0.5), 3, 3);
        }
    }
    void mousePressEvent(QMouseEvent* e) override { press(noteAt(e->pos()), e->pos().y()); }
    void mouseMoveEvent(QMouseEvent* e) override  { if (e->buttons() & Qt::LeftButton) press(noteAt(e->pos()), e->pos().y()); }
    void mouseReleaseEvent(QMouseEvent*) override { release(); }
    void wheelEvent(QWheelEvent* e) override { setScroll(scroll_ - e->angleDelta().y() / 120); e->accept(); }
private:
    static bool isBlack(int m) { int n = m % 12; return n==1 || n==3 || n==6 || n==8 || n==10; }
    int noteAt(QPoint pt) const {
        const int vis = visibleWhite(), h = height(), bh = h * 62 / 100, n = int(allWhite_.size());
        for (int i = 0; i + 1 < vis && scroll_ + i + 1 < n; ++i) {   // blacks first (on top)
            const int bm = allWhite_[scroll_ + i] + 1;
            if (isBlack(bm) && QRect((i + 1) * KW - BW / 2, 0, BW, bh).contains(pt)) return bm;
        }
        for (int i = 0; i < vis && scroll_ + i < n; ++i)
            if (QRect(i * KW, 0, KW, h).contains(pt)) return allWhite_[scroll_ + i];
        return -1;
    }
    // The pressed key lights with its velocity: a soft touch is a pale tint, a hard
    // one the full accent. The low end stays clearly visible - a press must show.
    QColor hitColor(bool black) const {
        const double t = qBound(0, curVel_, 127) / 127.0;
        const QColor lo = black ? QColor(0x2c, 0x44, 0x66) : QColor(0xb9, 0xcf, 0xe8);
        const QColor hi = black ? QColor(0x5a, 0x8a, 0xc8) : QColor(0x4a, 0x7a, 0xb8);
        return QColor(qRound(lo.red()   + (hi.red()   - lo.red())   * t),
                      qRound(lo.green() + (hi.green() - lo.green()) * t),
                      qRound(lo.blue()  + (hi.blue()  - lo.blue())  * t));
    }
    // Velocity from WHERE on the key it was pressed: lower = louder. A few-pixel
    // band at the bottom reaches 127 easily; the top never yields 0 (vel 0 = Note Off).
    int velFromY(int note, int y) const {
        const int h = height(), bh = h * 62 / 100;
        const int kh = isBlack(note) ? bh : h;
        const double f = qBound(0.0, double(y) / qMax(1, kh - 6), 1.0);
        return 1 + qRound(f * 126);   // 1..127
    }
    void press(int note, int y) {
        if (note < 0 || note == cur_) return;
        release(); cur_ = note; curVel_ = velFromY(note, y);   // kept: the highlight is drawn from it
        if (onNoteOn) onNoteOn(note, curVel_);
        update();
    }
    void release() { if (cur_ < 0) return; const int n = cur_; cur_ = -1; if (onNoteOff) onNoteOff(n); update(); }
    QVector<int> allWhite_;
    int scroll_ = 0, center_ = 0, cur_ = -1, curVel_ = 0;
};

// A pitch-bend / modulation wheel: the black cylinder validated in the mock-up,
// drawn with QPainter and dragged vertically. Pitch springs back to centre
// (8192); Mod has no spring and rests at the bottom (0). Every move (and every
// spring step) reports the raw MIDI value through onChange. Ridges are optional -
// a future View toggle in the Controller flips them with setRidges().
class Wheel : public QWidget {
public:
    enum Type { Pitch, Mod };
    std::function<void(int)> onChange;
    explicit Wheel(Type t, int h, QWidget* parent = nullptr) : QWidget(parent), type_(t), h_(h) {
        setFixedSize(TW, h_);
        val_ = (type_ == Pitch) ? 8192 : 0;
        buildHighlight();
    }
    int  value() const { return val_; }
    void setValue(int v, bool notify) {            // snapshot recall lands here
        stopSpring();
        val_ = (type_ == Pitch) ? qBound(0, v, 16383) : qBound(0, v, 127);
        update();
        if (notify && onChange) onChange(val_);
    }
    void setRidges(bool on) { ridges_ = on; update(); }
    void set7bit(bool on) { bit7_ = on; quantizeIf(); update(); }   // Pitch: coarse 7-bit (LSB 0); no MIDI on mode change
    static constexpr int TW = 38;   // total widget width (narrow body + tick margins)

protected:
    void paintEvent(QPaintEvent*) override {
        constexpr double PI = 3.14159265358979323846;
        const double cx = OX + W / 2.0, cy = h_ / 2.0, R = h_ / 2.0 - 3.0,
                     RX = W / 2.0 - 3.0, MAXRY = W / 2.0 - 3.0;   // margin scaled with width -> same ellipse proportion
        const double STEP = PI / 22.0, SPAN = PI / 2.0 * 0.82, EDGE = PI / 2.0 - 0.03;
        const double roll = (type_ == Pitch) ? (val_ - 8192) / 8192.0 * SPAN
                                             : (val_ / 127.0 - 0.5) * 2.0 * SPAN;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath path = bodyPath();
        p.save();
        p.setClipPath(path);
        p.fillRect(QRectF(OX, 0, W, h_), QColor(0x0b, 0x0d, 0x15));   // blue-black, to match the slate theme
        if (ridges_)
            for (int k = -40; k <= 40; ++k) {                 // surface ridges, bunching toward the ends
                const double a = (k + 0.5) * STEP + roll;
                if (a <= -EDGE || a >= EDGE) continue;
                const double y = cy - R * std::sin(a), c = std::cos(a);
                p.setPen(QPen(QColor(255, 255, 255, int(255 * 0.08 * c)), 1));
                p.drawLine(QLineF(OX + 2, y - 0.5, OX + W - 2, y - 0.5));
                p.setPen(QPen(QColor(0, 0, 0, int(255 * 0.42 * c)), 1));
                p.drawLine(QLineF(OX + 2, y + 0.5, OX + W - 2, y + 0.5));
            }
        QLinearGradient hg(OX, 0, OX + W, 0);                 // cross-cylinder shading: dark sides only
        hg.setColorAt(0.0, QColor(0, 0, 0, 217));  hg.setColorAt(0.24, QColor(0, 0, 0, 20));
        hg.setColorAt(0.5, QColor(0, 0, 0, 0));    hg.setColorAt(0.76, QColor(0, 0, 0, 20));
        hg.setColorAt(1.0, QColor(0, 0, 0, 217));
        p.fillRect(QRectF(OX, 0, W, h_), hg);
        p.drawImage(QPointF(OX, 0), highlight_);              // sine-windowed vertical reflet
        QLinearGradient vg(0, 0, 0, h_);                       // extra darkening at the two ends
        vg.setColorAt(0.0, QColor(0, 0, 0, 230)); vg.setColorAt(0.11, QColor(0, 0, 0, 0));
        vg.setColorAt(0.89, QColor(0, 0, 0, 0));  vg.setColorAt(1.0, QColor(0, 0, 0, 230));
        p.fillRect(QRectF(OX, 0, W, h_), vg);
        const double ym = cy - R * std::sin(roll), ry = std::max(1.0, MAXRY * std::cos(roll));
        QLinearGradient mg(0, ym - ry, 0, ym + ry);           // opaque marker, reflet at the bottom
        mg.setColorAt(0.0, QColor(0x06, 0x07, 0x0d)); mg.setColorAt(0.5, QColor(0x0c, 0x0e, 0x17));   // bluer grays (theme)
        mg.setColorAt(0.78, QColor(0x28, 0x2e, 0x3c)); mg.setColorAt(1.0, QColor(0x3c, 0x46, 0x58));
        p.setPen(Qt::NoPen); p.setBrush(mg);
        p.drawEllipse(QPointF(cx, ym), RX, ry);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(0, 0, 0, 153), 1));
        p.drawEllipse(QPointF(cx, ym), RX, ry);
        p.restore();
        p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(0, 0, 0), 1));
        p.drawPath(path);
        p.setRenderHint(QPainter::Antialiasing, false);       // crisp 1px reference ticks
        p.setPen(Qt::NoPen); p.setBrush(QColor(0x8a, 0x94, 0xa6));
        auto tick = [&](double y, double len) {
            p.drawRect(QRectF(OX - 1 - len, y - 0.5, len, 1));
            p.drawRect(QRectF(OX + W + 1, y - 0.5, len, 1));
        };
        if (type_ == Pitch) { tick(cy, 6); tick(CH + 2, 4); tick(h_ - CH - 2, 4); }   // long tick = rest
        else                { tick(h_ - CH - 2, 6); tick(cy, 4); tick(CH + 2, 4); }   // mod rests low
    }
    void mousePressEvent(QMouseEvent* e) override { pressed_ = true; stopSpring(); fromY(e->position().y()); }
    void mouseMoveEvent(QMouseEvent* e) override  { if (pressed_) fromY(e->position().y()); }
    void mouseReleaseEvent(QMouseEvent*) override { if (!pressed_) return; pressed_ = false; if (type_ == Pitch) startSpring(); }

private:
    static constexpr int W = 22, OX = 8, CH = 8;   // width cut ~1/3 (33->22), ends scaled to match

    QPainterPath bodyPath() const {                           // straight sides, full-width elliptical ends
        const double rx = W / 2.0 - 0.5, ry = CH, cxw = OX + W / 2.0;
        const QRectF topR(cxw - rx, 0.5, 2 * rx, 2 * ry);
        const QRectF botR(cxw - rx, h_ - 0.5 - 2 * ry, 2 * rx, 2 * ry);
        QPainterPath p;
        p.arcMoveTo(topR, 180);
        p.arcTo(topR, 180, -180);                             // left -> top -> right
        p.lineTo(cxw + rx, h_ - 0.5 - ry);
        p.arcTo(botR, 0, -180);                               // right -> bottom -> left
        p.closeSubpath();
        return p;
    }
    void buildHighlight() {                                   // built once: it does not move as the wheel rolls
        constexpr double PI = 3.14159265358979323846;
        highlight_ = QImage(W, h_, QImage::Format_ARGB32_Premultiplied);
        highlight_.fill(Qt::transparent);
        QPainter g(&highlight_);
        QLinearGradient band(0, 0, W, 0);
        band.setColorAt(0.0, QColor(255, 255, 255, 0)); band.setColorAt(0.5, QColor(255, 255, 255, 43));
        band.setColorAt(1.0, QColor(255, 255, 255, 0));
        g.fillRect(QRectF(0, 0, W, h_), band);
        g.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QLinearGradient vmask(0, 0, 0, h_);
        for (int i = 0; i <= 24; ++i) {
            const double q = i / 24.0;
            vmask.setColorAt(q, QColor(0, 0, 0, int(std::lround(255 * std::sin(PI * q)))));
        }
        g.fillRect(QRectF(0, 0, W, h_), vmask);
    }
    void quantizeIf() { if (bit7_ && type_ == Pitch) val_ = qBound(0, qRound(val_ / 128.0), 127) * 128; }   // snap to MSB grid
    void fromY(double y) {
        double t = (y - 9.0) / (h_ - 18.0);
        t = std::max(0.0, std::min(1.0, t));
        val_ = (type_ == Pitch) ? int(std::lround((1 - t) * 16383)) : int(std::lround((1 - t) * 127));
        quantizeIf();
        update();
        if (onChange) onChange(val_);
    }
    void startSpring() {                                      // ease back to centre over 180 ms
        stopSpring();
        auto* a = new QVariantAnimation(this);
        a->setStartValue(val_); a->setEndValue(8192);
        a->setDuration(180); a->setEasingCurve(QEasingCurve::OutQuad);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            val_ = v.toInt(); quantizeIf(); update(); if (onChange) onChange(val_);
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
        spring_ = a;
    }
    void stopSpring() { if (spring_) { spring_->stop(); spring_ = nullptr; } }

    Type type_;
    const int h_;                   // height, handed in: the keybed is built on the same figure
    int  val_ = 8192;
    bool ridges_ = true, pressed_ = false, bit7_ = false;
    QImage highlight_;
    QPointer<QVariantAnimation> spring_;
};

class CcPanel;

// One control on the Control Change surface. Phase 2: a potentiometer knob that
// can be dropped (right-click the panel), dragged around (edit mode, snapped to
// the grid) and removed (right-click it). Parameters and MIDI come next.
class CcControl : public QWidget {
public:
    explicit CcControl(CcPanel* panel);
    // The kind is serialised as an int, so these numbers must never move: 1 was the
    // fader, now dropped, and its slot stays EMPTY rather than shifting the others.
    enum Kind { Knob = 0, SliderSwitch = 2, PushSwitch = 3, Label = 4, Line = 5 };
    static Kind kindOf(int v) {
        return (v == SliderSwitch || v == PushSwitch || v == Label || v == Line) ? Kind(v) : Knob;
    }
    // Decoration: no MIDI, no CC number, and no label bands - it IS the drawing.
    bool isDeco() const { return kind == Label || kind == Line; }
    Kind kind = Knob;
    QString name;                                 // shown on top; empty -> "CC<n>"
    int cc = 1, rmin = 0, rmax = 127, def = 0, val = 0;
    int id = 0;                                    // stable identity, never reused: what a snapshot refers to
    // Address: a plain CC (cc), or an NRPN parameter 0..16383 (nrpnNum) - some synths
    // mix both. A 14-bit NRPN value sends CC#38 with its low seven bits as well.
    bool nrpn = false;
    int  nrpnNum = 0;
    bool wide = false;
    bool absolute = false;                         // false: value maps to MIDI 0..127; true: value IS the MIDI value (7-bit)
    // --- Slider Switch: detents, what each one is called and what it sends -----
    int     positions = 4;                         // 2..8
    QString posNames;                              // "tri,sqr,sin"  - a missing entry prints nothing
    QString posValues;                             // "0,42,85,127"  - a missing entry falls back to an even spread
    int     posIdx = 0;                            // current detent, 0 = bottom
    // --- Push Switch: a PB86 cap with its LED ---------------------------------
    bool latching = false;                         // false: momentary, held down; true: bistable
    int  capColor = 0;                             // index in capColorAt()
    // --- Label and Line -------------------------------------------------------
    int  fontPx = 12;                              // Label: text size
    int  lineW  = 1;                               // Line: thickness
    static const QColor& capColorAt(int i) {       // one base tone per cap, every shade derives from it
        static const QColor c[4] = { QColor(0x33, 0x38, 0x3f), QColor(0x5a, 0x8a, 0xc8),
                                     QColor(0xa6, 0x3a, 0x33), QColor(0x9a, 0xa1, 0xab) };
        return c[qBound(0, i, 3)];
    }
    static QStringList splitCsv(const QString& s) {
        QStringList out;
        const QStringList raw = s.split(QLatin1Char(','));
        for (const QString& t : raw) { const QString v = t.trimmed(); if (!v.isEmpty()) out << v; }
        return out;
    }
    // What a snapshot keeps of this control - the knob's value, the switch's
    // detent, the button's on/off - and how it is put back, sent or not.
    int  stateValue() const { return kind == SliderSwitch ? posIdx : val; }
    void applyState(int v, bool send);
    bool applyMidi(int midi);                      // from the wire: on screen only, nothing sent;
                                                   // true when the value actually moved
    int     posCount() const { return qBound(2, positions, 8); }
    QString nameAt(int i) const { const QStringList l = splitCsv(posNames); return i < l.size() ? l.at(i) : QString(); }
    int     valueAt(int i) const {                 // explicit value if given, else spread over 0..127
        const QStringList l = splitCsv(posValues);
        if (i < l.size()) { bool ok = false; const int v = l.at(i).toInt(&ok); if (ok) return v; }
        const int n = posCount();
        return n < 2 ? 0 : qRound(double(i) * 127.0 / (n - 1));
    }
    // Every kind resizes FREELY: width and height are independent.
    int    minW()     const { return isDeco() ? 16 : 48; }   // a dial, a groove or a cap need 48
    int    minH()     const {                  // the floor is what the drawing really needs, no more
        if (isDeco()) return 16;               // one grid cell
        // A switch takes (detents + 3) grid steps, and the count is exact, not padded:
        // the two label bands with their gaps eat 40 px, the stem with its guards 24,
        // and every gap between two detents is one grid step -
        //     16 * (n + 3)  ==  40 + 24 + 16 * (n - 1)
        // so the floor stays on the grid whatever the detent count.
        if (kind == SliderSwitch) return int(LBL_PITCH * (posCount() + 3));
        if (kind == PushSwitch) return 80;         // the cap keeps its 42x60 ratio, this leaves it 40 px
        return 48;                             // knob: both label bands + a drawable dial
    }
    // Everything the size must NOT change is frozen in PIXELS at the reference tile
    // 64x80 (what a fresh knob gets): the labels, their distance to the top and the
    // bottom edge, and every stroke width. Resizing plays on the DIAMETERS only.
    static constexpr int    LBL_PX  = 11;     // label font
    static constexpr double NAME_H  = 16.0;   // name band, under the top edge
    static constexpr double TOP_GAP = 4.8;    // name band -> dial
    static constexpr double VAL_H   = 14.4;   // value band, over the bottom edge
    static constexpr double RING_W  = 3.6;    // ring stroke
    static constexpr double PTR_W   = 2.5;    // pointer stroke
    static constexpr double ZERO_W  = 1.1;    // zero mark stroke
    static constexpr double RING_GAP = 6.27;  // ring radius -> body radius: a CONSTANT gap, not a ratio
    // Slider Switch, same rule: the tile size changes the TRAVEL and nothing else.
    // Brightness and edge radius were settled on the mock-up (60 %, radius 4).
    static constexpr double BOT_GAP   = 4.8;   // groove -> value band (the knob needs none, its dial is round)
    // Groove and stem: the mock-up figures taken down to 90 %.
    static constexpr double SLOT_W    = 21.6;  // groove width
    static constexpr double STEM_W    = 18.0;  // moving part: 1.8 px of groove left visible each side
    static constexpr double STEM_H    = 21.6;
    static constexpr int    TEETH     = 6;     // triangular teeth, edge to edge of the stem
    static constexpr double STEM_LUM  = 0.60;  // black plastic: the whole profile is dimmed
    static constexpr double EDGE_RAD  = 0.10;  // crest/valley blend half-width (0 = sharp edge)
    // Travel guard: the stem never reaches the ends of the groove. 1.2 is not free -
    // it is what makes stem + guards come to exactly 24, which is what lands the tile
    // floor on a whole number of grid steps (see minH).
    static constexpr double SW_INSET  = 1.2;
    // A disc reads lighter than an arc of the same width - the arc is long, the disc
    // is not - so the position dot is wider than the ring it has to match.
    static constexpr double DOT_D     = 5.0;
    static constexpr double DOT_GAP   = 4.0;   // dot -> groove
    static constexpr double LBL_GAP   = 5.0;   // groove -> position labels
    static constexpr double LBL_PITCH = 16.0;  // detent pitch, FIXED at the grid step whatever the tile height
    static constexpr double SW_MARGIN = 3.0;   // keeps the whole block inside the tile
    // Push Switch. Unlike the knob, whose strokes stay in pixels, the CAP scales as
    // a whole: it is drawn at a reference of 42x60 and blown up to the room it gets,
    // so every figure below is a ratio of that reference. Only the outline and the
    // edge fade stay in pixels. Values settled on the mock-up.
    static constexpr double PB_W = 42.0, PB_H = 60.0;   // reference cap
    static constexpr double PB_R      = 2.0;   // corner radius
    static constexpr double PB_SLOPE  = 7.8;   // chamfer, from the edge to the plateau
    static constexpr double PB_SMOOTH = 0.45;  // how far a slope melts into its neighbours (0 = knife edge)
    static constexpr double PB_EDGE   = 1.0;   // outer edge fade, in PIXELS, never scaled
    static constexpr double PB_SEAM   = 0.38;  // where the chamfer starts, in cap heights
    static constexpr double PB_LED    = 10.2;  // lens diameter
    static constexpr double PB_HOLE   = 12.2;  // the hole it sits in
    static constexpr double PB_LEDY   = 0.21;  // lens centre, in cap heights
    static constexpr double PB_DARK   = 0.45;  // pressed: black at the bottom edge, nothing at the top
    // Every shade of the cap is the base tone times one of these.
    static constexpr double PB_T_TOP = 1.22, PB_T_LEFT = 1.07, PB_T_RIGHT = 0.68,
                            PB_T_BOT = 0.57, PB_T_PLAT = 0.90, PB_T_BASE2 = 0.81;
    void drawCap(QPainter& p, const QRectF& box, bool down) const;
    struct SwLayout { double sx, top, bot, cyTop, cyBot; };
    SwLayout swLayout() const;                 // groove position and travel, for the current size
    double   detentY(int i, const SwLayout& l) const;
    int midiOf(int v, int span = 127) const {      // displayed value -> MIDI, 7 or 14 bits wide
        if (absolute) return qBound(0, v - rmin, span);        // transpose: MIDI = value - min
        if (rmax == rmin) return 0;
        return qBound(0, qRound(double(v - rmin) / (rmax - rmin) * span), span);
    }
    int  valueSpan() const { return (nrpn && wide) ? 16383 : 127; }
    bool blocked() const;                          // its number is reserved right now (Bank / NRPN)
    void sendValue();                              // the ONE way a control puts its value on the wire
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
private:
    int cornerAt(QPoint pt) const;                // 0=TL 1=TR 2=BL 3=BR, -1 = body
    void editProperties();                        // right-click > Properties... modal dialog
    void setPosFromY(int y);                      // locked switch: nearest detent to cursor Y
    void drawStem(QPainter& p, QPointF at) const; // the switch's moving part
    CcPanel* panel_;
    QPoint   grab_;                               // move: cursor offset inside the widget
    QPoint   rzAnchor_;                           // resize: the fixed (opposite) corner, parent coords
    bool     dragging_ = false, resizing_ = false, rzRight_ = false, rzBottom_ = false, valuing_ = false;
    bool     pressing_ = false;                   // push switch: cap held down (never saved)
    int      dragStartY_ = 0, dragStartVal_ = 0;  // locked: vertical drag adjusts the value
    QPoint   dragPrev_;                            // last snapped drag target (incremental group move)
};

// The Control Change surface: a dark canvas hosting CcControls, with an optional
// snap grid shown while editing. Right-click empty space adds a knob; the panel
// grows (scrollbars) rather than clip a control pushed past the visible area.
class CcPanel : public QWidget {
public:
    explicit CcPanel(QWidget* parent = nullptr) : QWidget(parent) { setFocusPolicy(Qt::StrongFocus); }
    void setLocked(bool on) { locked_ = on; update(); for (CcControl* c : controls_) c->update(); notifyState(); }
    void setGrid(bool on)   { if (gridOn_ != on && onEdited) onEdited(); gridOn_ = on; update(); }
    void setGridStep(int s) { gridStep_ = qMax(2, s); update(); }
    bool locked() const { return locked_; }
    std::function<void(bool)> onLockChange;       // menu Lock/Unlock routes through the header checkbox
    std::function<void(int, int)> onCc;           // (cc, 7-bit value) -> owner sends MIDI on the keyboard channel
    void emitCc(int cc, int midi) { if (onCc) onCc(cc, midi); }
    std::function<void(int, int, bool)> onNrpn;   // (parameter, value, 14-bit) -> owner sends CC#99/98/6[/38]
    void emitNrpn(int param, int v, bool wide) { if (onNrpn) onNrpn(param, v, wide); }
    bool bankMsbArmed() const { return bankMsb_; }
    bool bankLsbArmed() const { return bankLsb_; }
    void setBankArmed(bool msb, bool lsb) { bankMsb_ = msb; bankLsb_ = lsb; refreshAll(); }
    bool nrpnPresent() const { for (CcControl* c : controls_) if (c->nrpn) return true; return false; }
    void refreshAll() { for (CcControl* c : controls_) c->update(); }
    bool isSelected(CcControl* c) const { return sel_.contains(c); }
    void clearSelection() { const auto old = sel_; sel_.clear(); for (CcControl* c : old) c->update(); notifyState(); }
    void selectOnly(CcControl* c) {
        const auto old = sel_; sel_.clear(); if (c) sel_.append(c);
        for (CcControl* o : old) if (!sel_.contains(o)) o->update();
        if (c) c->update();
        notifyState();
    }
    void toggleSelection(CcControl* c) { if (!c) return; if (sel_.removeAll(c) == 0) sel_.append(c); c->update(); notifyState(); }
    void selectInRect(const QRect& r) {
        const auto old = sel_; sel_.clear();
        for (CcControl* c : controls_) if (r.intersects(c->geometry())) sel_.append(c);
        for (CcControl* c : old) if (!sel_.contains(c)) c->update();
        for (CcControl* c : sel_) if (!old.contains(c)) c->update();
        notifyState();
    }
    void deleteSelected() {
        if (locked_ || sel_.isEmpty()) return;
        for (CcControl* c : sel_) { controls_.removeAll(c); c->hide(); c->setParent(nullptr); c->deleteLater(); }
        sel_.clear(); refreshExtent(); update();
    }
    int  snap1(int v) const { return gridOn_ ? qRound(double(v) / gridStep_) * gridStep_ : v; }
    QPoint snap(QPoint p) const { return QPoint(snap1(p.x()), snap1(p.y())); }
    // Identity: a counter that only ever climbs. A copy, a paste or an undo all get
    // or keep their own id; a legacy surface (saved before ids) is numbered on load.
    int  takeId() { return nextId_++; }
    int  nextId() const { return nextId_; }
    void setNextId(int n) { nextId_ = qMax(nextId_, n); }
    bool gridOn() const { return gridOn_; }
    const QList<CcControl*>& controls() const { return controls_; }
    std::function<void()> onEdited;               // a REAL change to the surface (config goes dirty)
    void addControl(CcControl::Kind k, QPoint at) {
        auto* c = new CcControl(this);
        c->kind = k;
        c->id = takeId();
        CcControl* model = nullptr;                    // model on the last control OF THE SAME kind
        for (int i = controls_.size() - 1; i >= 0; --i) if (controls_[i]->kind == k) { model = controls_[i]; break; }
        if (model) {
            c->name = model->name; c->rmin = model->rmin; c->rmax = model->rmax; c->def = model->def; c->val = model->def;
            c->absolute = model->absolute; c->resize(model->size());
            c->positions = model->positions; c->posNames = model->posNames; c->posValues = model->posValues;
            c->latching = model->latching; c->capColor = model->capColor;
            c->fontPx = model->fontPx; c->lineW = model->lineW;
            c->nrpn = model->nrpn; c->nrpnNum = model->nrpnNum; c->wide = model->wide;
        } else if (k == CcControl::SliderSwitch) {
            c->resize(96, 160);                        // default switch: room for the position labels
        } else if (k == CcControl::PushSwitch) {
            c->resize(64, 100);                        // the size at which the cap comes out 42x60 exactly
        } else if (k == CcControl::Label) {
            c->resize(96, 16);
        } else if (k == CcControl::Line) {
            c->resize(128, 16);
        }
        if (k == CcControl::SliderSwitch) c->val = c->valueAt(c->posIdx);
        if (!c->isDeco()) c->cc = nextFreeCc(1);        // first free CC; decoration takes none
        controls_.append(c);
        const QPoint pos = snap(at - QPoint(c->width() / 2, c->height() / 2));
        c->move(qMax(0, pos.x()), qMax(0, pos.y()));
        c->show(); selectOnly(c); refreshExtent();
    }
    void forget(CcControl* c) { controls_.removeAll(c); sel_.removeAll(c); }
    QPoint moveSelectionBy(QPoint d) {            // drag one selected control -> the whole group follows
        for (CcControl* c : sel_) { d.setX(qMax(d.x(), -c->x())); d.setY(qMax(d.y(), -c->y())); }  // keep all >= 0
        for (CcControl* c : sel_) c->move(c->pos() + d);
        refreshExtent();
        return d;                                 // actual (clamped) delta -> caller advances its reference
    }
    void duplicateSelection() { duplicateSelection(gridStep_, gridStep_); }   // menu: one grid cell down-right
    void duplicateSelection(int dx, int dy) {     // duplicate EVERY selected control (fresh CC), select the copies
        if (locked_ || sel_.isEmpty()) return;
        QSet<int> used;
        for (CcControl* c : controls_) used.insert(c->cc);
        const auto src = sel_;
        QList<CcControl*> fresh;
        for (CcControl* s : src) {
            auto* c = new CcControl(this);
            c->id = takeId();
            c->name = s->name; c->rmin = s->rmin; c->rmax = s->rmax; c->def = s->def; c->val = s->val; c->absolute = s->absolute; c->kind = s->kind;
            c->positions = s->positions; c->posNames = s->posNames; c->posValues = s->posValues; c->posIdx = s->posIdx;
            c->latching = s->latching; c->capColor = s->capColor; c->fontPx = s->fontPx; c->lineW = s->lineW;
            c->nrpn = s->nrpn; c->nrpnNum = s->nrpnNum; c->wide = s->wide;
            if (!c->isDeco()) {
                int n = qBound(0, s->cc, 127);
                while (n < 127 && used.contains(n)) ++n;
                used.insert(n); c->cc = n;
            }
            c->resize(s->size());
            controls_.append(c);
            const QPoint pos = snap(s->pos() + QPoint(dx, dy));
            c->move(qMax(0, pos.x()), qMax(0, pos.y()));
            c->show();
            fresh.append(c);
        }
        refreshExtent();
        clearSelection(); sel_ = fresh; for (CcControl* c : fresh) c->update();
    }
    void copySelected() {
        clips_.clear();
        for (CcControl* c : sel_) {
            Clip cl; cl.name = c->name; cl.cc = c->cc; cl.rmin = c->rmin; cl.rmax = c->rmax;
            cl.def = c->def; cl.val = c->val; cl.absolute = c->absolute; cl.kind = int(c->kind); cl.size = c->size(); cl.pos = c->pos();
            cl.positions = c->positions; cl.posNames = c->posNames; cl.posValues = c->posValues; cl.posIdx = c->posIdx;
            cl.latching = c->latching; cl.capColor = c->capColor; cl.fontPx = c->fontPx; cl.lineW = c->lineW;
            cl.nrpn = c->nrpn; cl.nrpnNum = c->nrpnNum; cl.wide = c->wide;
            clips_.append(cl);
        }
    }
    void pasteClipboard() {
        if (clips_.isEmpty() || locked_) return;
        QSet<int> used;                                        // CCs already taken (existing + this batch)
        for (CcControl* c : controls_) used.insert(c->cc);
        QList<CcControl*> fresh;
        for (Clip& cl : clips_) {
            auto* c = new CcControl(this);
            c->id = takeId();
            c->name = cl.name; c->rmin = cl.rmin; c->rmax = cl.rmax; c->def = cl.def; c->val = cl.val; c->absolute = cl.absolute; c->kind = CcControl::kindOf(cl.kind);
            c->positions = cl.positions; c->posNames = cl.posNames; c->posValues = cl.posValues; c->posIdx = cl.posIdx;
            c->latching = cl.latching; c->capColor = cl.capColor; c->fontPx = cl.fontPx; c->lineW = cl.lineW;
            c->nrpn = cl.nrpn; c->nrpnNum = cl.nrpnNum; c->wide = cl.wide;
            if (!c->isDeco()) {                                // decoration takes no CC number
                int n = qBound(0, cl.cc, 127);
                while (n < 127 && used.contains(n)) ++n;        // next free CC
                used.insert(n); c->cc = n; cl.cc = n;           // remember -> repeated pastes keep climbing
            }
            if (cl.size.isValid()) c->resize(cl.size);
            controls_.append(c);
            const QPoint pos = snap(cl.pos + QPoint(gridStep_, gridStep_));
            c->move(qMax(0, pos.x()), qMax(0, pos.y()));
            c->show();
            cl.pos = c->pos();                                 // cascade position too
            fresh.append(c);
        }
        refreshExtent();
        clearSelection(); sel_ = fresh; for (CcControl* c : fresh) c->update();
    }
    int nextFreeCc(int from) const {
        for (int n = qBound(0, from, 127); n <= 127; ++n) {
            bool used = false;
            for (CcControl* c : controls_) if (!c->isDeco() && !c->nrpn && c->cc == n) { used = true; break; }
            if (!used) return n;
        }
        return 127;
    }
    void refreshExtent() {                        // min size = the EXACT bounding box, so the scrollbars
        QRect box(0, 0, 1, 1);                    // show up only when a control is really clipped, never before
        for (CcControl* c : controls_) box = box.united(c->geometry());
        setMinimumSize(box.right() + 1, box.bottom() + 1);   // right()/bottom() are inclusive -> +1 = last pixel
    }
    // --- undo/redo of construction: snapshot the whole surface; a gesture (drag,
    // resize, alt-dup) coalesces into ONE entry; a no-op change pushes nothing. ---
    void beginGesture() { if (gestureDepth_++ == 0) pending_ = snapshot(); }
    void endGesture() {
        if (gestureDepth_ == 0) return;
        if (--gestureDepth_ != 0) return;
        if (snapshot() != pending_) {
            undo_.append(pending_); if (undo_.size() > 64) undo_.removeFirst(); redo_.clear();
            if (onEdited) onEdited();
        }
        notifyState();
    }
    void clearHistory() { undo_.clear(); redo_.clear(); notifyState(); }
    void undo() { if (undo_.isEmpty()) return; redo_.append(snapshot()); restore(undo_.takeLast()); notifyState(); }
    void redo() { if (redo_.isEmpty()) return; undo_.append(snapshot()); restore(redo_.takeLast()); notifyState(); }
    bool canUndo() const { return !locked_ && !undo_.isEmpty(); }
    bool canRedo() const { return !locked_ && !redo_.isEmpty(); }
    bool hasSelection() const { return !sel_.isEmpty(); }
    int  controlCount() const { return controls_.size(); }
    std::function<void()> onStateChanged;         // -> owner refreshes the Edit menu enabled states
    void notifyState() { if (onStateChanged) onStateChanged(); }
    void selectAll() {
        const auto old = sel_; sel_ = controls_;
        for (CcControl* c : controls_) if (!old.contains(c)) c->update();
        notifyState();
    }
    void removeAll() {
        if (locked_ || controls_.isEmpty()) return;
        beginGesture();
        for (CcControl* c : controls_) { c->hide(); c->setParent(nullptr); c->deleteLater(); }
        controls_.clear(); sel_.clear(); refreshExtent(); update();
        endGesture();
    }
    // --- surface persistence (serialise every control's params + geometry) ---
    // Format 2 appends the switch fields. The version marker had to fit WITHOUT
    // breaking surfaces already saved: format 1 opened with a positive control
    // count, so a negative lead value can only be a version, and the count follows.
    QByteArray saveState() const {
        QByteArray ba; QDataStream ds(&ba, QIODevice::WriteOnly);
        const auto st = snapshot();
        ds << qint32(-4) << qint32(st.size());
        for (const Desc& d : st)
            ds << d.name << d.cc << d.rmin << d.rmax << d.def << d.val << d.absolute << d.x << d.y << d.w << d.h << d.kind
               << d.positions << d.posNames << d.posValues << d.posIdx << d.latching << d.capColor
               << d.fontPx << d.lineW;
        return ba;
    }
    void loadState(const QByteArray& ba) {
        QVector<Desc> st; QDataStream ds(ba); qint32 n = 0; ds >> n;
        int ver = 1;
        if (n < 0) { ver = -n; ds >> n; }              // pre-switch surfaces have no marker
        for (int i = 0; i < n; ++i) {
            Desc d;
            ds >> d.name >> d.cc >> d.rmin >> d.rmax >> d.def >> d.val >> d.absolute >> d.x >> d.y >> d.w >> d.h >> d.kind;
            if (ver >= 2) ds >> d.positions >> d.posNames >> d.posValues >> d.posIdx;
            if (ver >= 3) ds >> d.latching >> d.capColor;
            if (ver >= 4) ds >> d.fontPx >> d.lineW;
            if (ds.status() != QDataStream::Ok) return;
            st.append(d);
        }
        restore(st);
        undo_.clear(); redo_.clear(); notifyState();
    }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        setFocus(Qt::MouseFocusReason);
        if (locked_ || e->button() != Qt::LeftButton) return;
        if (!(e->modifiers() & Qt::ControlModifier)) clearSelection();
        band_ = true; bandOrigin_ = e->pos(); bandRect_ = QRect(bandOrigin_, bandOrigin_); update();   // start marquee
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!band_) return;
        bandRect_ = QRect(bandOrigin_, e->pos()).normalized();
        selectInRect(bandRect_);
        update();
    }
    void mouseReleaseEvent(QMouseEvent*) override { if (band_) { band_ = false; update(); } }
    void keyPressEvent(QKeyEvent* e) override {       // Copy/Paste only; the rest are Edit-menu QAction shortcuts
        if (!locked_ && e->matches(QKeySequence::Copy))       { copySelected(); e->accept(); }
        else if (!locked_ && e->matches(QKeySequence::Paste)) { beginGesture(); pasteClipboard(); endGesture(); e->accept(); }
        else QWidget::keyPressEvent(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0x12, 0x15, 0x1c));        // inset canvas ground
        if (!locked_ && gridOn_) {                            // snap grid, edit mode only
            p.setPen(QColor(0x2c, 0x32, 0x3e));
            for (int y = gridStep_; y < height(); y += gridStep_)
                for (int x = gridStep_; x < width(); x += gridStep_)
                    p.drawPoint(x, y);
        }
        if (band_) {                                          // rubber-band marquee
            p.setPen(QPen(QColor(0x5a, 0x8a, 0xc8), 1));
            p.setBrush(QColor(0x5a, 0x8a, 0xc8, 40));
            p.drawRect(bandRect_);
        }
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        QMenu m(this);
        QAction* lk = m.addAction(locked_ ? QStringLiteral("Unlock") : QStringLiteral("Lock"));   // first
        m.addSeparator();
        QMenu* addM = m.addMenu(QStringLiteral("Add"));              addM->setEnabled(!locked_);
        QAction* aKnob = addM->addAction(QStringLiteral("Knob"));
        QAction* aSlid = addM->addAction(QStringLiteral("Slider Switch"));
        QAction* aPush = addM->addAction(QStringLiteral("Push Switch"));
        addM->addSeparator();
        QAction* aLbl  = addM->addAction(QStringLiteral("Label"));
        QAction* aLin  = addM->addAction(QStringLiteral("Line"));
        QAction* selAll = m.addAction(QStringLiteral("Select All")); selAll->setEnabled(!locked_ && !controls_.isEmpty());
        QAction* remAll = m.addAction(QStringLiteral("Remove All")); remAll->setEnabled(!locked_ && !controls_.isEmpty());
        QAction* chosen = m.exec(e->globalPos());
        if (chosen == lk && onLockChange) onLockChange(!locked_);
        else if (chosen == aKnob) { beginGesture(); addControl(CcControl::Knob, e->pos()); endGesture(); }
        else if (chosen == aSlid) { beginGesture(); addControl(CcControl::SliderSwitch, e->pos()); endGesture(); }
        else if (chosen == aPush) { beginGesture(); addControl(CcControl::PushSwitch, e->pos()); endGesture(); }
        else if (chosen == aLbl)  { beginGesture(); addControl(CcControl::Label, e->pos()); endGesture(); }
        else if (chosen == aLin)  { beginGesture(); addControl(CcControl::Line, e->pos()); endGesture(); }
        else if (chosen == selAll) selectAll();
        else if (chosen == remAll) removeAll();
    }
private:
    struct Clip { QString name; int cc = 1, rmin = 0, rmax = 127, def = 0, val = 0; bool absolute = false; int kind = 0; QSize size; QPoint pos;
                            int positions = 4; QString posNames, posValues; int posIdx = 0;
                  bool latching = false; int capColor = 0; int fontPx = 12; int lineW = 1;
                  bool nrpn = false; int nrpnNum = 0; bool wide = false; };
public:
    struct Desc { QString name; int cc, rmin, rmax, def, val; bool absolute; int x, y, w, h, kind;
        int positions = 4; QString posNames, posValues; int posIdx = 0;          // Slider Switch only
        bool latching = false; int capColor = 0;                                 // Push Switch only
        int fontPx = 12; int lineW = 1;                                          // Label / Line
        int id = 0;                                                              // 0 = not numbered yet
        bool nrpn = false; int nrpnNum = 0; bool wide = false;                   // address
        bool operator==(const Desc& o) const {
            return name == o.name && cc == o.cc && rmin == o.rmin && rmax == o.rmax && def == o.def && val == o.val
                && absolute == o.absolute && x == o.x && y == o.y && w == o.w && h == o.h && kind == o.kind
                && positions == o.positions && posNames == o.posNames && posValues == o.posValues && posIdx == o.posIdx
                && latching == o.latching && capColor == o.capColor
                && fontPx == o.fontPx && lineW == o.lineW && id == o.id
                && nrpn == o.nrpn && nrpnNum == o.nrpnNum && wide == o.wide; } };
    QVector<Desc> snapshot() const {
        QVector<Desc> st;
        for (CcControl* c : controls_)
            st.append({ c->name, c->cc, c->rmin, c->rmax, c->def, c->val, c->absolute, c->x(), c->y(), c->width(), c->height(), int(c->kind),
                        c->positions, c->posNames, c->posValues, c->posIdx, c->latching, c->capColor,
                        c->fontPx, c->lineW, c->id, c->nrpn, c->nrpnNum, c->wide });
        return st;
    }
    void restore(const QVector<Desc>& st) {
        for (CcControl* c : controls_) { c->hide(); c->setParent(nullptr); c->deleteLater(); }
        controls_.clear(); sel_.clear();
        for (const Desc& d : st) {
            auto* c = new CcControl(this);
            c->name = d.name; c->cc = d.cc; c->rmin = d.rmin; c->rmax = d.rmax; c->def = d.def; c->val = d.val; c->absolute = d.absolute; c->kind = CcControl::kindOf(d.kind);
            c->positions = d.positions; c->posNames = d.posNames; c->posValues = d.posValues; c->posIdx = d.posIdx;
            c->latching = d.latching; c->capColor = d.capColor; c->fontPx = d.fontPx; c->lineW = d.lineW;
            c->nrpn = d.nrpn; c->nrpnNum = d.nrpnNum; c->wide = d.wide;
            c->id = d.id > 0 ? d.id : takeId();        // a surface saved before ids gets numbered here
            setNextId(c->id + 1);
            c->resize(d.w, d.h); c->move(d.x, d.y); c->show();
            controls_.append(c);
        }
        refreshExtent(); update();
    }
private:
    int  nextId_ = 1;
    bool bankMsb_ = false, bankLsb_ = false;      // Bank Select armed -> CC#0 / CC#32 reserved
    bool locked_ = false, gridOn_ = true;
    int  gridStep_ = 16;
    QList<CcControl*> controls_, sel_;
    QList<Clip> clips_;
    bool   band_ = false;
    QPoint bandOrigin_;
    QRect  bandRect_;
    QVector<QVector<Desc>> undo_, redo_;          // stacks of snapshots
    QVector<Desc>          pending_;              // the gesture's pre-state
    int    gestureDepth_ = 0;
};

CcControl::CcControl(CcPanel* panel) : QWidget(panel), panel_(panel) {
    resize(64, 80);                                             // default 4x5 grid cells
    setMouseTracking(true);
    setObjectName(QStringLiteral("ccControl"));
    // SCOPED to this widget on purpose: an unscoped rule descends on every child,
    // and the properties dialog is one - its drop-downs came out transparent, which
    // on screen means white.
    setStyleSheet(QStringLiteral("#ccControl { background: transparent; }"));
}

int CcControl::cornerAt(QPoint pt) const
{
    const int hs = 10, w = width(), h = height();
    const bool l = pt.x() < hs, r = pt.x() >= w - hs, tp = pt.y() < hs, bt = pt.y() >= h - hs;
    if (l && tp) return 0;
    if (r && tp) return 1;
    if (l && bt) return 2;
    if (r && bt) return 3;
    return -1;
}
void CcControl::paintEvent(QPaintEvent*)
{
    constexpr double PI = 3.14159265358979323846;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool edit = !panel_->locked();
    const double W = width(), H = height();
    if (edit) {                                            // movable/resizable tile; transparent once locked
        const bool sel = panel_->isSelected(this);            // selection = lighter (accent) border, 1px
        p.setPen(QPen(sel ? QColor(0x5a, 0x8a, 0xc8) : QColor(0x3d, 0x47, 0x57), 1));
        p.setBrush(QColor(0x1a, 0x1f, 0x28, sel ? 180 : 140));
        p.drawRoundedRect(QRectF(0.5, 0.5, W - 1, H - 1), 4, 4);
    }
    const double nameH = NAME_H, topGap = TOP_GAP, valueH = VAL_H;   // fixed bands -> fixed insets
    QFont f = p.font(); f.setPixelSize(LBL_PX); p.setFont(f);        // fixed label size
    if (isDeco()) {                                        // decoration: no bands, no value
        if (kind == Label) {
            QFont lf = p.font(); lf.setPixelSize(qBound(6, fontPx, 96)); p.setFont(lf);
            // An unnamed label keeps showing its placeholder even once locked, dimmed:
            // an object one can neither see nor select would just be a trap.
            const bool empty = name.isEmpty();
            p.setPen(empty ? QColor(0x6b, 0x74, 0x84) : QColor(0xdf, 0xe4, 0xec));
            p.drawText(QRectF(2, 0, W - 4, H), Qt::AlignCenter,
                       empty ? QStringLiteral("Label") : name);
        } else {                                           // Line: the tile's long side decides which way
            const double th = qBound(1, lineW, 8);
            p.setPen(Qt::NoPen); p.setBrush(QColor(0x3d, 0x47, 0x57));
            if (W >= H) p.drawRect(QRectF(0, (H - th) / 2, W, th));
            else        p.drawRect(QRectF((W - th) / 2, 0, th, H));
        }
        if (edit && panel_->isSelected(this)) {            // same four handles as everything else
            p.setPen(Qt::NoPen); p.setBrush(QColor(0x5a, 0x8a, 0xc8));
            const int hs = 6, w = width(), h = height();
            p.drawRect(0, 0, hs, hs); p.drawRect(w - hs, 0, hs, hs);
            p.drawRect(0, h - hs, hs, hs); p.drawRect(w - hs, h - hs, hs, hs);
        }
        return;
    }
    // Both labels are centred on the GRAPHIC: the tile centre everywhere, except on
    // a switch whose groove has slid left to make room for its position labels.
    const double cx = (kind == SliderSwitch) ? swLayout().sx + SLOT_W / 2 : W / 2;
    p.setPen(QColor(0xdf, 0xe4, 0xec));                    // name on top (else CC<n>)
    p.drawText(QRectF(cx - W / 2, 2, W, nameH), Qt::AlignHCenter | Qt::AlignVCenter,
               name.isEmpty() ? QStringLiteral("CC%1").arg(cc) : name);
    p.setPen(QColor(0x8a, 0x94, 0xa6));                    // value below
    p.drawText(QRectF(cx - W / 2, H - valueH - 2, W, valueH), Qt::AlignHCenter | Qt::AlignVCenter, QString::number(val));
    const double t     = (rmax > rmin) ? qBound(0.0, double(val - rmin) / (rmax - rmin), 1.0) : 0.0;
    const double tZero = (rmax > rmin) ? qBound(0.0, double(0   - rmin) / (rmax - rmin), 1.0) : 0.0;
    const double regTop = nameH + topGap, regBot = H - valueH;            // control area between the labels
    if (kind == Knob) {
        const double cx = W / 2.0;
        const double R  = qMax(6.0, qMin(W - 8, regBot - regTop) / 2.0);   // the ring diameter follows the tile
        const double br = qMax(3.0, R - RING_GAP);         // the body follows it, keeping the gap unchanged
        const double cy = (regTop + regBot) / 2.0;         // centred: width and height are independent now
        const QRectF arcR(cx - R, cy - R, 2 * R, 2 * R);
        const double aw = RING_W;
        QPen tp(QColor(0x3a, 0x41, 0x4e), aw); tp.setCapStyle(Qt::RoundCap);
        p.setPen(tp); p.drawArc(arcR, 225 * 16, -270 * 16);                    // track (7:30 -> over top -> 4:30)
        QPen vp(QColor(0x5a, 0x8a, 0xc8), aw); vp.setCapStyle(Qt::FlatCap);    // value fill: ALWAYS from zero
        p.setPen(vp); p.drawArc(arcR, int((225 - 270 * tZero) * 16), int(-270 * (t - tZero) * 16));
        if (t != tZero) {                                                     // rounded tip on the moving end
            const double av = (-135 + t * 270) * PI / 180.0;
            p.setPen(Qt::NoPen); p.setBrush(QColor(0x5a, 0x8a, 0xc8));
            p.drawEllipse(QPointF(cx + R * std::sin(av), cy - R * std::cos(av)), aw / 2, aw / 2);
        }
        const double azero = (-135 + tZero * 270) * PI / 180.0;               // zero mark on the ring
        p.setPen(QPen(QColor(0xe9, 0xed, 0xf3), ZERO_W));
        p.drawLine(QPointF(cx + (R - aw * 0.5) * std::sin(azero), cy - (R - aw * 0.5) * std::cos(azero)),
                   QPointF(cx + (R + aw * 0.5) * std::sin(azero), cy - (R + aw * 0.5) * std::cos(azero)));
        QLinearGradient dg(cx, cy - br, cx, cy + br);
        dg.setColorAt(0, QColor(0x30, 0x37, 0x45)); dg.setColorAt(1, QColor(0x1a, 0x1f, 0x28));
        p.setPen(QPen(QColor(0x0c, 0x0d, 0x11), 1)); p.setBrush(dg);
        p.drawEllipse(QPointF(cx, cy), br, br);                                // knob body
        const double a = (-135 + t * 270) * PI / 180.0;
        p.setPen(QPen(QColor(0xdf, 0xe4, 0xec), PTR_W));
        p.drawLine(QPointF(cx + br * 0.45 * std::sin(a), cy - br * 0.45 * std::cos(a)),
                   QPointF(cx + (br - 1) * std::sin(a), cy - (br - 1) * std::cos(a)));   // pointer
    } else if (kind == PushSwitch) {                                          // Push Switch
        drawCap(p, QRectF(4, regTop, W - 8, regBot - BOT_GAP - regTop), pressing_);
    } else {                                                                  // Slider Switch
        const SwLayout l = swLayout();
        QLinearGradient sg(l.sx, 0, l.sx + SLOT_W, 0);                        // groove, lit from the right
        sg.setColorAt(0, QColor(0x04, 0x05, 0x07));
        sg.setColorAt(0.4, QColor(0x09, 0x0b, 0x0f));
        sg.setColorAt(1, QColor(0x12, 0x16, 0x1c));
        p.setPen(QPen(QColor(0x2b, 0x31, 0x3a), 1)); p.setBrush(sg);
        p.drawRoundedRect(QRectF(l.sx + 0.5, l.top + 0.5, SLOT_W - 1, l.bot - l.top - 1), 3, 3);
        const int np = posCount();
        for (int i = 0; i < np; ++i) {                                        // dot on the left, label on the right
            const double y = detentY(i, l);
            const bool on = (i == posIdx);
            p.setPen(Qt::NoPen); p.setBrush(on ? QColor(0x5a, 0x8a, 0xc8) : QColor(0x3a, 0x41, 0x4e));
            p.drawEllipse(QPointF(l.sx - DOT_GAP - DOT_D / 2, y), DOT_D / 2, DOT_D / 2);
            const QString nm = nameAt(i);
            if (nm.isEmpty()) continue;
            const double lx = l.sx + SLOT_W + LBL_GAP;
            p.setPen(on ? QColor(0x5a, 0x8a, 0xc8) : QColor(0x6b, 0x74, 0x84));
            p.drawText(QRectF(lx, y - 8, qMax(1.0, W - lx), 16), Qt::AlignLeft | Qt::AlignVCenter, nm);
        }
        drawStem(p, QPointF(l.sx + (SLOT_W - STEM_W) / 2, detentY(posIdx, l) - STEM_H / 2));
    }
    if (blocked()) {                                       // reserved number: veiled, and mute
        p.setPen(Qt::NoPen); p.setBrush(QColor(0x12, 0x15, 0x1c, 165));
        p.drawRoundedRect(QRectF(0, 0, W, H), 4, 4);
    }
    if (edit && panel_->isSelected(this)) {                // four corner resize handles: SELECTED only
        p.setPen(Qt::NoPen); p.setBrush(QColor(0x5a, 0x8a, 0xc8));
        const int hs = 6, w = width(), h = height();
        p.drawRect(0, 0, hs, hs); p.drawRect(w - hs, 0, hs, hs);
        p.drawRect(0, h - hs, hs, hs); p.drawRect(w - hs, h - hs, hs, hs);
    }
}
// Where the groove sits and how far the stem may travel. The groove is centred in
// the tile; when the position labels would not fit on its right the whole block
// slides left, but never past the tile's own margin.
// A field switched off by a tick must LOOK off. setEnabled alone is not enough:
// the stylesheet's :disabled state does not reach a QSpinBox's inner line edit,
// so a "dim" property is set as well, and the style keys on it (re-polished so it
// takes at once, children included).
static void setFieldOn(QWidget* w, bool on)
{
    w->setEnabled(on);
    w->setProperty("dim", !on);
    w->style()->unpolish(w); w->style()->polish(w);
    for (QWidget* c : w->findChildren<QWidget*>()) { c->style()->unpolish(c); c->style()->polish(c); }
    w->update();
}

// One base tone per cap, every shade derived from it: a new colour is ONE value,
// not seven.
static QColor pbTone(const QColor& c, double f)
{
    return QColor(qBound(0, qRound(c.red()   * f), 255),
                  qBound(0, qRound(c.green() * f), 255),
                  qBound(0, qRound(c.blue()  * f), 255));
}
// The PB86 cap: a single moulded piece of CONSTANT width, whose lower part is made
// of four slopes running from the very edges down to a central plateau - there is no
// flat band between a slope and an edge - with the LED on the surface above them.
// Pressed, the cap does not move an inch: a shadow builds up towards its bottom edge.
void CcControl::drawCap(QPainter& p, const QRectF& box, bool down) const
{
    const double k = qMin(box.width() / PB_W, box.height() / PB_H);
    if (k <= 0.0) return;
    const double W = PB_W * k, H = PB_H * k;
    const double x0 = box.x() + (box.width() - W) / 2, y0 = box.y() + (box.height() - H) / 2;
    const double c = PB_SLOPE * k, r = PB_R * k, sy = y0 + H * PB_SEAM;
    const QColor base = capColorAt(capColor), base2 = pbTone(base, PB_T_BASE2), plat = pbTone(base, PB_T_PLAT);

    QPainterPath shape;
    shape.addRoundedRect(QRectF(x0, y0, W, H), r, r);
    p.save();
    p.setClipPath(shape);
    p.setPen(Qt::NoPen);

    QLinearGradient bg(0, y0, 0, y0 + H);
    bg.setColorAt(0, base); bg.setColorAt(1, base2);
    p.setBrush(bg); p.drawRect(QRectF(x0, y0, W, H));

    auto face = [&](const QPolygonF& poly, QPointF a, QPointF b, double f) {
        QLinearGradient g(a, b);                   // across the slope: neighbour, own tone, plateau
        const double t = PB_SMOOTH / 2;
        g.setColorAt(0, base2);
        g.setColorAt(t, pbTone(base, f));
        g.setColorAt(1 - t, pbTone(base, f));
        g.setColorAt(1, plat);
        p.setBrush(g); p.drawPolygon(poly);
    };
    face(QPolygonF({ QPointF(x0, sy), QPointF(x0 + W, sy), QPointF(x0 + W - c, sy + c), QPointF(x0 + c, sy + c) }),
         QPointF(0, sy), QPointF(0, sy + c), PB_T_TOP);
    face(QPolygonF({ QPointF(x0, sy), QPointF(x0 + c, sy + c), QPointF(x0 + c, y0 + H - c), QPointF(x0, y0 + H) }),
         QPointF(x0, 0), QPointF(x0 + c, 0), PB_T_LEFT);
    face(QPolygonF({ QPointF(x0 + W, sy), QPointF(x0 + W, y0 + H), QPointF(x0 + W - c, y0 + H - c), QPointF(x0 + W - c, sy + c) }),
         QPointF(x0 + W, 0), QPointF(x0 + W - c, 0), PB_T_RIGHT);
    face(QPolygonF({ QPointF(x0, y0 + H), QPointF(x0 + c, y0 + H - c), QPointF(x0 + W - c, y0 + H - c), QPointF(x0 + W, y0 + H) }),
         QPointF(0, y0 + H), QPointF(0, y0 + H - c), PB_T_BOT);
    p.setBrush(plat);
    p.drawRect(QRectF(x0 + c, sy + c, W - 2 * c, y0 + H - c - sy - c));

    for (int j = 0; j < 12; ++j) {                 // edge fade in twelve layers whose TOTAL width is
        const double t = j / 12.0, ins = 0.75 + t * PB_EDGE;   // the setting - so every tenth of a pixel shows
        QColor e(0x0a, 0x0c, 0x10); e.setAlphaF(0.5 * (1 - t));
        p.setPen(QPen(e, PB_EDGE / 12.0 + 0.35)); p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(x0 + ins, y0 + ins, W - 2 * ins, H - 2 * ins),
                          qMax(0.0, r - t * PB_EDGE), qMax(0.0, r - t * PB_EDGE));
    }
    if (down) {                                    // pressed: no travel at all, only shadow
        QLinearGradient sh(0, y0, 0, y0 + H);
        sh.setColorAt(0, QColor(0, 0, 0, 0));
        sh.setColorAt(1, QColor(0, 0, 0, int(PB_DARK * 255)));
        p.setPen(Qt::NoPen); p.setBrush(sh); p.drawRect(QRectF(x0, y0, W, H));
    }
    p.restore();

    // Outline LAST, over everything: painted before, the slopes would cover its inner
    // half and the lower part would come out a hair wider than the upper one.
    p.setPen(QPen(QColor(0x0a, 0x0c, 0x10), 1.5)); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(x0, y0, W, H), r, r);

    // Only a LATCHED switch carries a LED. A momentary one has no state to show, so
    // its cap comes out bare - no lens, no hole - and the press reads on the shadow.
    if (!latching) return;
    const bool lit = (val != rmin);
    const QPointF lc(x0 + W / 2, y0 + H * PB_LEDY);
    const double lr = PB_LED * k / 2, hr = PB_HOLE * k / 2;
    p.setPen(Qt::NoPen);
    if (lit) {
        const double hal = PB_LED * k * 1.9;
        QRadialGradient ha(lc, hal);
        ha.setColorAt(0,    QColor(0xff, 0x4a, 0x34, 128));
        ha.setColorAt(0.55, QColor(0xff, 0x4a, 0x34, 36));
        ha.setColorAt(1,    QColor(0xff, 0x4a, 0x34, 0));
        p.setBrush(ha); p.drawEllipse(lc, hal, hal);
    }
    p.setBrush(QColor(0x0b, 0x0d, 0x11)); p.drawEllipse(lc, hr, hr);
    QRadialGradient lg(lc, lr, lc + QPointF(-lr * 0.24, -lr * 0.36));
    if (lit) { lg.setColorAt(0, QColor(0xff, 0xf2, 0xec)); lg.setColorAt(0.28, QColor(0xff, 0x6a, 0x52));
               lg.setColorAt(0.70, QColor(0xee, 0x2d, 0x20)); lg.setColorAt(1, QColor(0x8e, 0x17, 0x12)); }
    else     { lg.setColorAt(0, QColor(0x5c, 0x2a, 0x28)); lg.setColorAt(0.55, QColor(0x3a, 0x16, 0x18));
               lg.setColorAt(1, QColor(0x20, 0x0c, 0x0e)); }
    p.setBrush(lg); p.drawEllipse(lc, lr, lr);
    QColor spec(0xff, 0xff, 0xff); spec.setAlphaF(lit ? 0.55 : 0.10);
    p.setBrush(spec);
    p.drawEllipse(QPointF(lc.x() - PB_LED * k * 0.16, lc.y() - PB_LED * k * 0.20),
                  PB_LED * k * 0.15, PB_LED * k * 0.10);
}
CcControl::SwLayout CcControl::swLayout() const
{
    const double W = width(), H = height();
    QFont f = font(); f.setPixelSize(LBL_PX);
    const QFontMetricsF fm(f);
    double nw = 0;
    for (int i = 0; i < posCount(); ++i) nw = qMax(nw, fm.horizontalAdvance(nameAt(i)));
    const double leftW  = DOT_D + DOT_GAP;
    const double rightW = nw > 0 ? LBL_GAP + nw : 0;
    double sx = (W - SLOT_W) / 2, shift = 0;
    if (sx + SLOT_W + rightW > W - SW_MARGIN) shift = (W - SW_MARGIN) - (sx + SLOT_W + rightW);
    if (sx - leftW + shift < SW_MARGIN)       shift = SW_MARGIN - (sx - leftW);
    sx += shift;
    // The detent pitch is fixed, so the TRAVEL is fixed too, and the groove is only
    // as long as the travel it has to serve - a groove longer than the stem can run
    // would be a lie. Spare height therefore goes around it: the groove is centred
    // in what the two label bands leave. Under the floor (a surface saved before
    // this rule) the groove is clamped and the detents squeeze together.
    const double regTop = NAME_H + TOP_GAP, regBot = H - VAL_H - BOT_GAP;
    const double want = STEM_H + 2 * SW_INSET + LBL_PITCH * (posCount() - 1);
    const double gh = qMin(want, regBot - regTop);
    SwLayout l;
    l.sx    = sx;
    l.top   = regTop + (regBot - regTop - gh) / 2;
    l.bot   = l.top + gh;
    l.cyTop = l.top + SW_INSET + STEM_H / 2;
    l.cyBot = qMax(l.cyTop, l.bot - SW_INSET - STEM_H / 2);
    return l;
}
double CcControl::detentY(int i, const SwLayout& l) const     // detent 0 = bottom = first value
{
    const int n = posCount();
    return n < 2 ? l.cyBot : l.cyBot - (l.cyBot - l.cyTop) * double(i) / (n - 1);
}
// The moving part: a black piece whose face carries TEETH triangular ridges. One
// ridge = one gradient period - valley, lit face, crest, shadow face, valley - so
// the radius only widens the blend at the crest and at the bottom of the valley,
// and the brightness multiplies the whole profile at once.
void CcControl::drawStem(QPainter& p, QPointF at) const
{
    auto dim = [](int r, int g, int b) {
        return QColor(qRound(r * STEM_LUM), qRound(g * STEM_LUM), qRound(b * STEM_LUM));
    };
    const QColor lit = dim(58, 64, 73), crest = dim(80, 88, 100), shadow = dim(18, 21, 27), valley = dim(8, 10, 14);
    const double th = STEM_H / TEETH, h = EDGE_RAD;
    p.setPen(Qt::NoPen);
    for (int k = 0; k < TEETH; ++k) {
        const double y = at.y() + k * th;
        QLinearGradient g(0, y, 0, y + th);
        g.setColorAt(0, valley);      g.setColorAt(h, lit);        g.setColorAt(0.5 - h, lit);
        g.setColorAt(0.5, crest);
        g.setColorAt(0.5 + h, shadow); g.setColorAt(1 - h, shadow); g.setColorAt(1, valley);
        p.setBrush(g);
        p.drawRect(QRectF(at.x(), y, STEM_W, th));
    }
    p.setPen(QPen(QColor(0, 0, 0), 0.7)); p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(at.x(), at.y(), STEM_W, STEM_H));
}
// Locked switch: nearest detent to the cursor, sent as soon as it changes. Press
// and drag both land here, so the switch follows the mouse like a fader does.
void CcControl::applyState(int v, bool send)
{
    if (isDeco()) return;
    if (kind == SliderSwitch) { posIdx = qBound(0, v, posCount() - 1); val = valueAt(posIdx); }
    else if (kind == PushSwitch) { val = (v == rmax) ? rmax : rmin; }
    else { val = qBound(qMin(rmin, rmax), v, qMax(rmin, rmax)); }
    update();
    if (!send) return;
    sendValue();
}
// A number can be spoken for: CC#0 while Bank MSB is armed, CC#32 while Bank LSB
// is, and the four NRPN controllers as soon as any control on the surface is an
// NRPN. A control sitting on one of them is dimmed and mute until that lifts.
bool CcControl::blocked() const
{
    if (isDeco() || nrpn) return false;
    if (cc == 0  && panel_->bankMsbArmed()) return true;
    if (cc == 32 && panel_->bankLsbArmed()) return true;
    if ((cc == 98 || cc == 99 || cc == 6 || cc == 38) && panel_->nrpnPresent()) return true;
    return false;
}
void CcControl::sendValue()
{
    if (isDeco() || blocked()) return;
    const int span = valueSpan();
    const int v = (kind == Knob) ? midiOf(val, span) : qBound(0, val, span);
    if (nrpn) panel_->emitNrpn(nrpnNum, v, wide);
    else      panel_->emitCc(cc, v);
}
bool CcControl::applyMidi(int midi)
{
    if (isDeco()) return false;
    const int span = valueSpan();
    midi = qBound(0, midi, span);
    if (kind == Knob) {
        const int lo = qMin(rmin, rmax), hi = qMax(rmin, rmax);
        const int nv = absolute ? qBound(lo, rmin + midi, hi)                       // the inverse of midiOf
                                : qBound(lo, rmin + qRound(double(midi) / span * (rmax - rmin)), hi);
        if (nv == val) return false;
        val = nv;
    } else if (kind == SliderSwitch) {
        int best = 0, bd = 1 << 20;                                                 // the detent nearest that value
        for (int i = 0; i < posCount(); ++i) { const int d = qAbs(valueAt(i) - midi); if (d < bd) { bd = d; best = i; } }
        if (best == posIdx) return false;
        posIdx = best; val = valueAt(posIdx);
    } else {
        const int nv = (midi == rmax) ? rmax : (midi == rmin) ? rmin : (midi >= 64 ? rmax : rmin);
        if (nv == val) return false;
        val = nv;
    }
    update();
    return true;
}
void CcControl::setPosFromY(int y)
{
    const SwLayout l = swLayout();
    const int n = posCount();
    int best = 0; double bd = 1e18;
    for (int i = 0; i < n; ++i) { const double d = qAbs(detentY(i, l) - y); if (d < bd) { bd = d; best = i; } }
    if (best == posIdx) return;
    posIdx = best;
    val = valueAt(posIdx);
    update();
    sendValue();
}
void CcControl::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    if (panel_->locked()) {                             // locked: vertical drag sets the value + sends MIDI
        if (isDeco() || blocked()) return;              // decoration is inert; so is a reserved number
        valuing_ = true;
        if (kind == SliderSwitch) setPosFromY(e->pos().y());       // switch: jump to the nearest detent
        else if (kind == PushSwitch) {                             // push: bistable toggles, momentary goes down
            pressing_ = true;
            val = latching ? (val == rmax ? rmin : rmax) : rmax;
            update();
            sendValue();
        }
        else { dragStartY_ = e->pos().y(); dragStartVal_ = val; }
        return;
    }
    panel_->setFocus(Qt::MouseFocusReason);             // so Copy/Paste/Delete reach the panel
    panel_->beginGesture();                             // one undo entry per edit gesture (committed on release)
    if (e->modifiers() & Qt::AltModifier) {             // Alt+drag = duplicate in place, then drag the copies
        if (!panel_->isSelected(this)) panel_->selectOnly(this);
        panel_->duplicateSelection(0, 0);
        dragging_ = true; grab_ = e->pos(); dragPrev_ = pos();
        return;
    }
    if (e->modifiers() & Qt::ControlModifier) panel_->toggleSelection(this);
    else if (!panel_->isSelected(this)) panel_->selectOnly(this);
    const int corner = cornerAt(e->pos());
    if (corner >= 0) {                                   // resize from a corner; the OPPOSITE corner stays put
        resizing_ = true;
        rzRight_  = (corner == 0 || corner == 2);
        rzBottom_ = (corner == 0 || corner == 1);
        rzAnchor_ = pos() + QPoint(rzRight_ ? width() : 0, rzBottom_ ? height() : 0);
    } else { dragging_ = true; grab_ = e->pos(); dragPrev_ = pos(); raise(); }
}
void CcControl::mouseMoveEvent(QMouseEvent* e)
{
    if (panel_->locked()) {                              // locked: value drag (up = more) or hover hint
        if (isDeco() || blocked()) { setCursor(Qt::ArrowCursor); return; }
        if (!valuing_) { setCursor(kind == PushSwitch ? Qt::PointingHandCursor : Qt::SizeVerCursor); return; }
        if (kind == PushSwitch) return;                              // a button ignores the drag
        if (kind == SliderSwitch) { setPosFromY(e->pos().y()); return; }   // switch: the detent follows the drag
        const double span = (rmax != rmin) ? qAbs(rmax - rmin) : 1;   // knob: relative
        const int lo = qMin(rmin, rmax), hi = qMax(rmin, rmax);
        const int nv = qBound(lo, dragStartVal_ + qRound((dragStartY_ - e->pos().y()) * span / 150.0), hi);
        if (nv != val) { val = nv; update(); sendValue(); }
        return;
    }
    if (!dragging_ && !resizing_) {                      // hover: move cursor on the body, resize on a corner
        const int c = cornerAt(e->pos());
        setCursor(c < 0 ? Qt::SizeAllCursor
                        : (c == 0 || c == 3) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
        return;
    }
    if (resizing_) {                                    // corner drag, each side on its own axis
        const QPoint cur = mapToParent(e->pos());
        const int w = qMax(minW(), panel_->snap1(qAbs(cur.x() - rzAnchor_.x())));
        const int h = qMax(minH(), panel_->snap1(qAbs(cur.y() - rzAnchor_.y())));
        setGeometry(qMax(0, rzRight_ ? rzAnchor_.x() - w : rzAnchor_.x()),
                    qMax(0, rzBottom_ ? rzAnchor_.y() - h : rzAnchor_.y()), w, h);
        panel_->refreshExtent();
        return;
    }
    const QPoint target = panel_->snap(mapToParent(e->pos()) - grab_);   // move the whole selection together
    dragPrev_ += panel_->moveSelectionBy(target - dragPrev_);            // incremental: works whether or not 'this' is in the group
}
void CcControl::mouseReleaseEvent(QMouseEvent*)
{
    if (pressing_) {                                   // a momentary switch falls back when let go
        pressing_ = false;
        if (!latching && val != rmin) { val = rmin; sendValue(); }
        update();
    }
    dragging_ = resizing_ = valuing_ = false;
    panel_->endGesture();
}
void CcControl::contextMenuEvent(QContextMenuEvent* e)
{
    if (panel_->locked()) return;
    panel_->setFocus(Qt::MouseFocusReason);             // keep key focus so Ctrl+Z works after a menu action
    if (!panel_->isSelected(this)) panel_->selectOnly(this);
    QMenu m(this);
    QAction* props = m.addAction(QStringLiteral("Properties..."));
    QAction* dup   = m.addAction(QStringLiteral("Duplicate"));
    m.addSeparator();
    QAction* rem   = m.addAction(QStringLiteral("Remove"));
    QAction* chosen = m.exec(e->globalPos());
    if (chosen == props) editProperties();
    else if (chosen == dup) { panel_->beginGesture(); panel_->duplicateSelection(); panel_->endGesture(); }
    else if (chosen == rem) { panel_->beginGesture(); panel_->forget(this); hide(); setParent(nullptr); panel_->refreshExtent(); deleteLater(); panel_->endGesture(); }
    e->accept();
}
void CcControl::editProperties()
{
    QDialog d(window());                       // the window, not the control: nothing of ours leaks into it
    d.setWindowTitle(kind == SliderSwitch ? QStringLiteral("Slider Switch properties")
                   : kind == PushSwitch   ? QStringLiteral("Push Switch properties")
                   : kind == Label        ? QStringLiteral("Label properties")
                   : kind == Line         ? QStringLiteral("Line properties")
                                          : QStringLiteral("Knob properties"));
    auto* form = new QFormLayout(&d);
    auto* typeBox = new QComboBox;                              // the Kind rides in the item DATA:
    typeBox->addItem(QStringLiteral("Knob"),          int(Knob));    // the rows no longer match the enum
    typeBox->addItem(QStringLiteral("Slider Switch"), int(SliderSwitch));
    typeBox->addItem(QStringLiteral("Push Switch"),   int(PushSwitch));
    typeBox->addItem(QStringLiteral("Label"),         int(Label));
    typeBox->addItem(QStringLiteral("Line"),          int(Line));
    typeBox->setCurrentIndex(qMax(0, typeBox->findData(int(kind))));
    auto* modeBox = new QComboBox; modeBox->addItem(QStringLiteral("Relative")); modeBox->addItem(QStringLiteral("Absolute"));
    modeBox->setCurrentIndex(absolute ? 1 : 0);
    auto* nameEd = new QLineEdit(name);
    auto* ccSp = new QSpinBox;  ccSp->setRange(0, 127);  ccSp->setValue(cc);
    // Address: a CC number, or an NRPN parameter as MSB / LSB / one number - three
    // LINKED fields, because the manuals use either notation (Novation and Roland
    // give the pair, Sequential the number). Fill in whichever the doc gives.
    auto* addrBox = new QComboBox; addrBox->addItem(QStringLiteral("CC")); addrBox->addItem(QStringLiteral("NRPN"));
    addrBox->setCurrentIndex(nrpn ? 1 : 0);
    const int nn = qBound(0, nrpnNum, 16383);
    auto* msbNr = new QSpinBox; msbNr->setRange(0, 127);   msbNr->setValue(nn / 128);
    auto* lsbNr = new QSpinBox; lsbNr->setRange(0, 127);   lsbNr->setValue(nn % 128);
    auto* numNr = new QSpinBox; numNr->setRange(0, 16383); numNr->setValue(nn);
    auto* wideChk = new FilterCheckBox(QStringLiteral("14-bit (CC#6 + CC#38)")); wideChk->setNeutral(true); wideChk->setChecked(wide);
    auto linking = std::make_shared<bool>(false);
    auto fromPair = [msbNr, lsbNr, numNr, linking] {
        if (*linking) return; *linking = true; numNr->setValue(msbNr->value() * 128 + lsbNr->value()); *linking = false; };
    auto fromNum  = [msbNr, lsbNr, numNr, linking] {
        if (*linking) return; *linking = true; msbNr->setValue(numNr->value() / 128); lsbNr->setValue(numNr->value() % 128); *linking = false; };
    connect(msbNr, &QSpinBox::valueChanged, &d, [fromPair](int) { fromPair(); });
    connect(lsbNr, &QSpinBox::valueChanged, &d, [fromPair](int) { fromPair(); });
    connect(numNr, &QSpinBox::valueChanged, &d, [fromNum](int)  { fromNum(); });
    auto* minSp = new QSpinBox; minSp->setRange(-16384, 16383); minSp->setValue(rmin);
    auto* maxSp = new QSpinBox; maxSp->setRange(-16384, 16383); maxSp->setValue(rmax);
    auto* defSp = new QSpinBox; defSp->setRange(-16384, 16383); defSp->setValue(def);
    // Absolute keeps max-min <= 127 (a 7-bit MIDI window, may sit anywhere incl. negative):
    // pushing one bound past the span drags the other; a smaller span stays allowed.
    auto spanOf = [addrBox, wideChk] { return (addrBox->currentIndex() == 1 && wideChk->isChecked()) ? 16383 : 127; };
    auto capFromMin = [minSp, maxSp, modeBox, spanOf] { if (modeBox->currentIndex() == 1 && maxSp->value() - minSp->value() > spanOf()) maxSp->setValue(minSp->value() + spanOf()); };
    auto capFromMax = [minSp, maxSp, modeBox, spanOf] { if (modeBox->currentIndex() == 1 && maxSp->value() - minSp->value() > spanOf()) minSp->setValue(maxSp->value() - spanOf()); };
    connect(minSp, &QSpinBox::valueChanged, &d, [capFromMin](int) { capFromMin(); });
    connect(maxSp, &QSpinBox::valueChanged, &d, [capFromMax](int) { capFromMax(); });
    connect(modeBox, &QComboBox::currentIndexChanged, &d, [capFromMin](int) { capFromMin(); });
    form->addRow(QStringLiteral("Type"),    typeBox);
    form->addRow(QStringLiteral("Mode"),    modeBox);
    form->addRow(QStringLiteral("Name"),    nameEd);
    form->addRow(QStringLiteral("Address"), addrBox);
    form->addRow(QStringLiteral("CC #"),    ccSp);
    form->addRow(QStringLiteral("NRPN MSB"), msbNr);
    form->addRow(QStringLiteral("NRPN LSB"), lsbNr);
    form->addRow(QStringLiteral("NRPN #"),   numNr);
    form->addRow(QStringLiteral("Value"), wideChk);
    form->addRow(QStringLiteral("Min"),     minSp);
    form->addRow(QStringLiteral("Max"),     maxSp);
    form->addRow(QStringLiteral("Default"), defSp);
    // Slider Switch: how many detents, what each is called, what each one sends. A
    // switch carries its own values, so Mode/Min/Max/Default are meaningless for it
    // and the two sets of rows swap with the type.
    auto* posSp   = new QSpinBox; posSp->setRange(2, 8); posSp->setValue(posCount());
    auto* namesEd = new QLineEdit(posNames);  namesEd->setPlaceholderText(QStringLiteral("On,Off"));
    auto* valsEd  = new QLineEdit(posValues); valsEd->setPlaceholderText(QStringLiteral("127,0"));
    form->addRow(QStringLiteral("Positions"),       posSp);
    form->addRow(QStringLiteral("Position names"),  namesEd);
    form->addRow(QStringLiteral("Position values"), valsEd);
    // Push Switch: what a press does, and the colour of the cap.
    auto* actBox = new QComboBox;
    actBox->addItem(QStringLiteral("Momentary")); actBox->addItem(QStringLiteral("Latched"));
    actBox->setCurrentIndex(latching ? 1 : 0);
    auto* colBox = new QComboBox;
    colBox->addItem(QStringLiteral("Black")); colBox->addItem(QStringLiteral("Blue"));
    colBox->addItem(QStringLiteral("Red"));   colBox->addItem(QStringLiteral("Light grey"));
    colBox->setCurrentIndex(qBound(0, capColor, 3));
    form->addRow(QStringLiteral("Action"), actBox);
    form->addRow(QStringLiteral("Cap"),    colBox);
    auto* fontSp = new QSpinBox; fontSp->setRange(6, 96); fontSp->setValue(qBound(6, fontPx, 96));
    auto* lineSp = new QSpinBox; lineSp->setRange(1, 8);  lineSp->setValue(qBound(1, lineW, 8));
    form->addRow(QStringLiteral("Text size"), fontSp);
    form->addRow(QStringLiteral("Thickness"), lineSp);
    auto swRows = [form, posSp, namesEd, valsEd, actBox, colBox, modeBox, minSp, maxSp, defSp,
                   fontSp, lineSp, nameEd, ccSp, addrBox, msbNr, lsbNr, numNr, wideChk](int t) {
        const bool sl = (t == int(SliderSwitch)), pb = (t == int(PushSwitch));
        const bool la = (t == int(Label)), li = (t == int(Line)), deco = la || li;
        const bool useN = !deco && addrBox->currentIndex() == 1;
        form->setRowVisible(fontSp, la); form->setRowVisible(lineSp, li);
        form->setRowVisible(nameEd, !li);                  // a line has nothing to say
        form->setRowVisible(addrBox, !deco);               // decoration sends nothing
        form->setRowVisible(ccSp, !deco && !useN);
        form->setRowVisible(msbNr, useN); form->setRowVisible(lsbNr, useN);
        form->setRowVisible(numNr, useN); form->setRowVisible(wideChk, useN);
        form->setRowVisible(posSp, sl);   form->setRowVisible(namesEd, sl); form->setRowVisible(valsEd, sl);
        form->setRowVisible(actBox, pb);  form->setRowVisible(colBox, pb);
        form->setRowVisible(modeBox, !sl && !pb && !deco);
        form->setRowVisible(minSp, !sl && !deco);  form->setRowVisible(maxSp, !sl && !deco);
        // A push switch has no range, it has two values - so its two rows are named
        // for what they are instead of Min and Max.
        if (auto* l = qobject_cast<QLabel*>(form->labelForField(minSp)))
            l->setText(pb ? QStringLiteral("Value Off") : QStringLiteral("Min"));
        if (auto* l = qobject_cast<QLabel*>(form->labelForField(maxSp)))
            l->setText(pb ? QStringLiteral("Value On")  : QStringLiteral("Max"));
        form->setRowVisible(defSp, !sl && !pb && !deco);
    };
    swRows(typeBox->currentData().toInt());
    connect(typeBox, &QComboBox::currentIndexChanged, &d, [swRows, typeBox](int) { swRows(typeBox->currentData().toInt()); });
    connect(addrBox, &QComboBox::currentIndexChanged, &d, [swRows, typeBox](int) { swRows(typeBox->currentData().toInt()); });
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() == QDialog::Accepted) {
        panel_->beginGesture();
        const Kind nk = kindOf(typeBox->currentData().toInt());
        if (nk != kind) {                          // type change: honour the new kind's floors
            kind = nk;
            resize(qMax(width(), minW()), qMax(height(), minH()));
            panel_->refreshExtent();
        }
        positions = posSp->value();
        posNames  = namesEd->text().trimmed();
        posValues = valsEd->text().trimmed();
        posIdx    = qBound(0, posIdx, posCount() - 1);
        latching  = (actBox->currentIndex() == 1);
        capColor  = colBox->currentIndex();
        fontPx    = fontSp->value();
        lineW     = lineSp->value();
        if (height() < minH()) {                   // more detents need a taller tile, else the labels collide
            resize(width(), minH());
            panel_->refreshExtent();
        }
        absolute = (modeBox->currentIndex() == 1);
        name = nameEd->text().trimmed();
        cc = ccSp->value();
        nrpn = (addrBox->currentIndex() == 1);
        nrpnNum = numNr->value();
        wide = wideChk->isChecked();
        rmin = minSp->value(); rmax = maxSp->value();
        if (rmax < rmin) qSwap(rmin, rmax);
        if (absolute && rmax - rmin > valueSpan()) rmax = rmin + valueSpan();   // 7- or 14-bit window
        def = qBound(rmin, defSp->value(), rmax);
        val = (kind == SliderSwitch) ? valueAt(posIdx)          // reset to the rest state on edit
            : (kind == PushSwitch) ? rmin : def;
        update();
        panel_->refreshAll();                      // an NRPN appearing or leaving changes who is reserved
        panel_->endGesture();
    }
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
    hexPath_->lineEdit()->setPlaceholderText(".hex file");
    hexPath_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);   // fill the row like the old field
    browseBtn_    = ui_->browseBtn;
    uploadBtn_    = ui_->uploadBtn;
    progress_     = ui_->progress;
    uploadStatus_ = ui_->uploadStatus;
    term_         = ui_->term;
    sysexBox_     = ui_->sysexBox;
    sendBtn_      = ui_->sendBtn;
    { auto* cop = new FilterCheckBox("Clear on Greeting"); cop->setNeutral(true);   // gray, no colored border
      clearOnGreeting_ = cop; ui_->sendRow->insertWidget(0, cop); }
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
    buildMenus();         // Tools / View menu bar, reserved at the top of root

    // Right-click menus (select all / copy / [clear]) on every list. Copy takes
    // the whole selection (multi-line: Ctrl/Shift-click), not just the clicked
    // row. Device Info and Upload Status are read-only mirrors: no Clear.
    auto wireMenu = [this](QListWidget* w, bool allowClear) {
        w->setContextMenuPolicy(Qt::CustomContextMenu);
        w->setSelectionMode(QAbstractItemView::ExtendedSelection);
        connect(w, &QWidget::customContextMenuRequested, this, [w, allowClear](const QPoint& p) {
            QMenu m;
            QAction* sa = m.addAction(tr("Select All"));
            QAction* co = m.addAction(tr("Copy"));
            QAction* cl = nullptr;
            if (allowClear) { m.addSeparator(); cl = m.addAction(tr("Clear")); }
            QAction* a = m.exec(w->viewport()->mapToGlobal(p));
            if (a == sa) w->selectAll();
            else if (a == co) {
                QString t;
                for (auto* it : w->selectedItems()) t += it->text() + '\n';
                if (!t.isEmpty()) QApplication::clipboard()->setText(t);
            } else if (cl && a == cl) w->clear();
        });
    };
    wireMenu(monIn_,        true);
    wireMenu(monOut_,       true);
    wireMenu(term_,         true);
    wireMenu(devInfo_,      false);   // read-only, no Clear
    wireMenu(uploadStatus_, false);   // read-only, no Clear

    // ---- uploader wiring -------------------------------------------------
    uploader_ = new Uploader(&out_, &outGuard_, this);
    connect(uploader_, &Uploader::log, this, [this](QString l, int level) {
        // 0 info (normal), 1 success (green), 2 error (red). Only the error gets
        // the "! " gutter; green is reserved for the final "upload complete".
        auto* it = new QListWidgetItem((level == 2 ? "! " : "  ") + l);
        if (level == 1)      it->setForeground(QColor(0x55, 0xb5, 0x6a));   // green
        else if (level == 2) it->setForeground(infoColour(2));             // red
        appendCapped(uploadStatus_, it);
    });
    connect(uploader_, &Uploader::progress, this, [this](int p) { progress_->setValue(p); });
    connect(uploader_, &Uploader::sent, this, [this](QByteArray b) {
        monitorLine(true, adios::Bytes(b.begin(), b.end()));
    });
    connect(uploader_, &Uploader::infoClear, this, [this] { devInfo_->clear(); });
    connect(uploader_, &Uploader::greetingRequested, this,
            [this] { if (clearOnGreeting_->isChecked()) term_->clear(); });
    connect(uploader_, &Uploader::infoLine, this, [this](QString t, int c) {
        auto* it = new QListWidgetItem(t);
        it->setForeground(infoColour(c));
        appendCapped(devInfo_, it);
    });
    connect(uploader_, &Uploader::finished, this, [this](bool ok) {
        uploadBtn_->setEnabled(connected_ && !hexPath_->currentText().isEmpty());
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
        uploader_->queryInfo();   // a valid ping pulls the greeting; the terminal
                                  // is cleared then (Clear on Greeting), not here
    });
    connect(browseBtn_,  &QPushButton::clicked, this, &MainWindow::chooseHex);
    connect(uploadBtn_,  &QPushButton::clicked, this, &MainWindow::doUpload);
    connect(sendBtn_,    &QPushButton::clicked, this, &MainWindow::sendSysex);
    connect(sysexBox_,   &QLineEdit::returnPressed, this, &MainWindow::sendSysex);
    sysexBox_->installEventFilter(this);   // Up/Down walk the command history
    connect(hexPath_,    &QComboBox::currentTextChanged, this,
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

    // Bring the Controller back if it was open when the app last closed, at its
    // remembered size and position. Deferred so it appears after the main window.
    if (QSettings().value("ctrlOpen", false).toBool())
        QTimer::singleShot(0, this, [this] { openController(); });

    // Auto-ping at startup: read the device and pull its terminal greeting, just
    // like the Ping button, once the event loop is running and the ports have
    // settled. Silent (no-op) when nothing is connected or a job is already busy.
    QTimer::singleShot(300, this, [this] {
        if (connected_ && !uploader_->busy()) {
            uploader_->setDeviceId(uint8_t(idBox_->value()));
            uploader_->queryInfo();
        }
    });
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
    hexPath_->addItems(s.value("hexHistory").toStringList());
    hexPath_->setCurrentText(s.value("hexFile").toString());
    clearOnGreeting_->setChecked(s.value("clearOnGreeting", false).toBool());
    lastDir_ = s.value("browseDir").toString();
    // View toggles first - each may hide a panel and resize the window - THEN the
    // saved geometry, so the remembered size (not the toggle's resize) is final.
    for (QAction* a : viewToggles_) {
        const QVariant v = s.value("view/" + a->objectName());
        if (v.isValid()) a->setChecked(v.toBool());
    }
    if (s.contains("winGeometry")) restoreGeometry(s.value("winGeometry").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("inPort",   inBox_->currentText());
    s.setValue("outPort",  outBox_->currentText());
    s.setValue("deviceId", idBox_->value());
    s.setValue("hexFile",  hexPath_->currentText());
    {
        QStringList hist;
        for (int i = 0; i < hexPath_->count(); ++i) hist << hexPath_->itemText(i);
        s.setValue("hexHistory", hist);
    }
    s.setValue("clearOnGreeting", clearOnGreeting_->isChecked());
    s.setValue("browseDir", lastDir_);
    s.setValue("rowsSplitState", ui_->rowsSplit->saveState());
    s.setValue("colsSplitState", ui_->colsSplit->saveState());
    s.setValue("monSplitState",  ui_->monSplit->saveState());
    s.setValue("winGeometry",    saveGeometry());   // position + size of the main window
    for (QAction* a : viewToggles_) s.setValue("view/" + a->objectName(), a->isChecked());

    // Controller window: remember whether it was open and its size/position, so it
    // returns exactly as left on the next launch.
    s.setValue("ctrlOpen", controllerWin_ && controllerWin_->isVisible());
    if (controllerWin_) {
        s.setValue("ctrlGeometry", controllerWin_->saveGeometry());
        for (QAction* a : controllerWin_->findChildren<QAction*>())    // View toggles, keyed by objectName
            if (!a->objectName().isEmpty()) s.setValue("ctrl/" + a->objectName(), a->isChecked());
        for (QCheckBox* c : controllerWin_->findChildren<QCheckBox*>())  // No Note Off
            if (!c->objectName().isEmpty()) s.setValue("ctrl/" + c->objectName(), c->isChecked());
        for (QSpinBox* sp : controllerWin_->findChildren<QSpinBox*>())   // Bank MSB/LSB, Program
            if (!sp->objectName().isEmpty()) s.setValue("ctrl/" + sp->objectName(), sp->value());
        for (QComboBox* cb : controllerWin_->findChildren<QComboBox*>())  // MIDI Input Channel
            if (!cb->objectName().isEmpty()) s.setValue("ctrl/" + cb->objectName(), cb->currentIndex());
        for (QSplitter* sp : controllerWin_->findChildren<QSplitter*>()) // the Bank | surface divider
            if (!sp->objectName().isEmpty()) s.setValue("ctrl/" + sp->objectName(), sp->saveState());
        s.setValue("ctrl/channel", kbChannel_);
        if (auto* sg = controllerWin_->findChild<QButtonGroup*>(QStringLiteral("snapGroup")))
            s.setValue("ctrl/snapshot", sg->checkedId());
        if (ccSurface_) s.setValue("ctrl/surface", static_cast<CcPanel*>(ccSurface_)->saveState());
        // The working copy goes to the session file, with the name of the config it
        // came from and whether it differs from it. The named file is NOT touched:
        // only Save writes it.
        s.setValue("ctrl/configPath", ctrlConfigPath_);
        s.setValue("ctrl/dirty", ctrlDirty_);
        ctrlWriteConfig(ctrlSessionPath());
    }

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
    uploadBtn_->setEnabled(on && !hexPath_->currentText().isEmpty());
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
        msg[2] == 0x22 && msg[3] == 0x15 && msg[6] != 0x0d) {   // 0x0d = terminal debug string, NOT a query/upload reply
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
    ctrlMidiIn(msg);

    // Debug Terminal: the core sends console text as a "debug string" SysEx
    // (cmd 0x0D, sub 0x40) - F0 00 22 15 32 <id> 0D 40 <7-bit ascii...> F7.
    // Decode the ASCII and print it, one entry per embedded line. (Ping/query
    // ACKs are handled by the uploader and shown in Device Info, not here.)
    if (msg.size() >= 9 && msg[0] == 0xf0 && msg[1] == 0x00 && msg[2] == 0x22 &&
        msg[3] == 0x15 && msg[4] == 0x32 && msg[6] == 0x0d &&
        (msg[7] == 0x40 || msg[7] == 0x00)) {
        QString text;
        for (size_t i = 8; i < msg.size() && msg[i] != 0xf7; ++i)
            // printable + newline + CR: the OS debug packer pads the last packet
            // with 0x00 (dropped here, else an invisible "blank line"); CR is kept
            // because the format progress bar refreshes its line with it.
            if (msg[i] == '\n' || msg[i] == '\r' || (msg[i] >= 0x20 && msg[i] < 0x80))
                text += QChar(msg[i]);

        // A carriage return means "redraw the current line": the firmware streams
        // the format progress as "\r[|||   ] NN%". Overwrite the live bar line, or
        // open one under the phase label the previous (plain) message printed.
        if (text.contains('\r')) {
            const QString bar = text.section('\r', -1);
            if (bar.isEmpty()) return;
            if (termBarOpen_ && term_->count())
                term_->item(term_->count() - 1)->setText(bar);
            else { appendCapped(term_, new QListWidgetItem(bar)); termBarOpen_ = true; }
            return;
        }
        termBarOpen_ = false;   // any ordinary line closes the live bar

        const QStringList lines = text.split('\n');
        for (int k = 0; k < lines.size(); ++k) {
            if (lines[k].isEmpty()) continue;   // no gratuitous blank lines in the terminal
            // don't repeat a line identical to the current bottom one: the core
            // re-greets on every Ping and after a reboot.
            if (term_->count() && term_->item(term_->count() - 1)->text() == lines[k]) continue;
            appendCapped(term_, new QListWidgetItem(lines[k]));
            // A device_id change from the board ("device_id: M -> N"): we do NOT
            // switch the host id silently - prompt the user to do it and re-Ping.
            if (lines[k].startsWith("device_id: ") && lines[k].contains(" -> ")) {
                const QString n = lines[k].section(" -> ", 1, 1).trimmed();
                auto* hint = new QListWidgetItem("set Device ID to " + n + " and Ping");
                hint->setForeground(infoColour(1));   // orange
                appendCapped(term_, hint);
            }
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

    // Decode NRPN. CC#99 opens a run, CC#98 completes the parameter, CC#6 writes
    // the ONE line, and a CC#38 right behind widens the value on that same line.
    // Any other message ends the run - except real time, which interleaves with
    // everything and must not.
    {
        const MonCols& cols = out ? outCols_ : inCols_;
        NrpnRun& r = out ? nrpnOut_ : nrpnIn_;
        QListWidget* list = out ? monOut_ : monIn_;
        const bool isCc = msg.size() == 3 && (msg[0] & 0xf0) == 0xb0;
        // No "(14-bit)" tag: it pushed the label past its column and broke the raw
        // data alignment. A 14-bit value shows by being above 127 and by its four
        // segments in the raw column.
        auto lineFor = [](int ch, int msb, int lsb, int value) {
            const std::string chs = QString::asprintf("ch %2d", ch + 1).toStdString();
            const std::string prm = QString::asprintf("#%d", msb * 128 + lsb).toStdString();
            const std::string val = QString::asprintf("val %d", value).toStdString();
            return QString::asprintf("%-9s %-5s %-7s %s", "NRPN", chs.c_str(), prm.c_str(), val.c_str());
        };
        // One message more in the raw column; a recreated status byte keeps its mark.
        auto addSeg = [&r, &running, &msg] {
            if (!r.raw.isEmpty()) r.raw += QStringLiteral(" | ");
            if (running) r.runOffsets << r.raw.length();
            r.raw += hexFull(msg);
        };
        if (cols.nrpn && isCc) {
            const int ch = msg[0] & 0x0f, cc = msg[1], v = msg[2];
            if (cc == 99) { r = NrpnRun(); r.ch = ch; r.msb = v; addSeg(); return; }
            if (cc == 98 && r.ch == ch && r.msb >= 0 && r.lsb < 0) { r.lsb = v; addSeg(); return; }
            if (cc == 6 && r.ch == ch && r.msb >= 0 && r.lsb >= 0 && !r.item) {
                r.value = v; addSeg();
                auto* one = new QListWidgetItem;
                one->setData(Role_Stamp, nowStamp());
                one->setData(Role_Label, lineFor(ch, r.msb, r.lsb, r.value));
                one->setData(Role_Hex,   r.raw);
                one->setData(Role_Run,   false);
                QVariantList offs; for (int o : r.runOffsets) offs << o;
                one->setData(Role_RunPos, offs);
                rebuildMonRow(one, cols);
                appendCapped(list, one);
                r.item = one;
                return;
            }
            if (cc == 38 && r.ch == ch && r.item && list->row(r.item) >= 0) {
                r.value = (r.value << 7) | v; addSeg();
                r.item->setData(Role_Label, lineFor(ch, r.msb, r.lsb, r.value));
                r.item->setData(Role_Hex,   r.raw);
                QVariantList offs; for (int o : r.runOffsets) offs << o;
                r.item->setData(Role_RunPos, offs);
                rebuildMonRow(r.item, cols);
                r = NrpnRun();
                return;
            }
            r = NrpnRun();                                // some other CC: not a run
        } else if (!(msg.size() == 1 && msg[0] >= 0xf8)) {
            r = NrpnRun();
        }
    }

    adios::Decoded d = adios::decode(msg);
    auto* it = new QListWidgetItem;
    it->setData(Role_Stamp, nowStamp());
    it->setData(Role_Label, QString::fromStdString(d.label));
    it->setData(Role_Hex,   hexFull(msg));
    it->setData(Role_Run,   running);
    rebuildMonRow(it, out ? outCols_ : inCols_);
    appendCapped(out ? monOut_ : monIn_, it);
}

// Rebuild one monitor line's visible text from its stored parts and the current
// column choices; keeps the timestamp/decoded/raw layout of the original line
// and re-places the running-status dim only while Raw Data is shown.
void MainWindow::rebuildMonRow(QListWidgetItem* it, const MonCols& c)
{
    const QString stamp = it->data(Role_Stamp).toString();
    const QString label = it->data(Role_Label).toString();
    const QString hex   = it->data(Role_Hex).toString();
    QString line;
    if (c.stamp)  line = stamp;
    if (c.decode) { if (!line.isEmpty()) line += QStringLiteral("   ");
                    line += c.raw ? label.leftJustified(38) : label; }
    if (c.raw)    { if (!line.isEmpty()) line += QStringLiteral("  "); line += hex; }
    it->setText(line);
    // A plain line dims its one status byte when it ran on running status; a merged
    // line (NRPN) carries the offsets of every recreated byte within its raw column.
    QVariantList offs = it->data(Role_RunPos).toList();
    if (offs.isEmpty() && it->data(Role_Run).toBool()) offs << 0;
    const bool dim = c.raw && !offs.isEmpty();
    QVariantList pos;
    if (dim) { const int hexStart = int(line.length() - hex.length()); for (const QVariant& o : offs) pos << hexStart + o.toInt(); }
    it->setData(Role_Dim, dim);
    it->setData(Role_HexPos, pos);
}

void MainWindow::rerenderMonitor(QListWidget* list, const MonCols& c)
{
    for (int i = 0; i < list->count(); ++i) rebuildMonRow(list->item(i), c);
}

// Menu bar, reserved at the top of the root layout (a QWidget has no native one).
void MainWindow::buildMenus()
{
    menuBar_ = new QMenuBar(this);

    // Tools: structure only for now (Controller + Screen Capture), wired later.
    QMenu* mTools = menuBar_->addMenu(tr("Tools"));
    connect(mTools->addAction(tr("Controller")), &QAction::triggered, this, &MainWindow::openController);
    QMenu* mCap = mTools->addMenu(tr("Screen Capture"));
    mCap->addAction(tr("5x6 Display"))->setEnabled(false);
    mCap->addAction(tr("Dr Display"))->setEnabled(false);

    // View: reset the splitters, show/hide either monitor and its columns.
    QMenu* mView = menuBar_->addMenu(tr("View"));
    mView->addAction(tr("Reset Layout"), this, &MainWindow::resetLayout);
    mView->addSeparator();

    // A monitor's sub-menu: an optional Filter toggle, then the three columns.
    // The column toggles rebuild what is already on screen, not just new lines.
    auto addCols = [this](QMenu* sub, MonCols* cols, QListWidget* list, bool withFilter, const QString& prefix) {
        if (withFilter) {
            QAction* f = sub->addAction(tr("Filter"));
            f->setObjectName(prefix + "filter");   // QSettings key for persistence
            f->setCheckable(true); f->setChecked(true);
            connect(f, &QAction::toggled, this, [this](bool v) {
                filterHead_->setVisible(v);                              // triangle + "Filter" label
                filterPanel_->setVisible(v && filterBtn_->isChecked());  // panel only if expanded
                applyFilterBox_->setVisible(v);                          // Output's Apply Filter follows
            });
            viewToggles_ << f;
            sub->addSeparator();
        }
        struct Col { const char* name; bool MonCols::* field; };
        const Col cols3[] = { {"TimeStamp", &MonCols::stamp},
                              {"Decoding",  &MonCols::decode},
                              {"Raw Data",  &MonCols::raw} };
        for (const Col& e : cols3) {
            QAction* a = sub->addAction(tr(e.name));
            a->setObjectName(prefix + e.name);
            a->setCheckable(true); a->setChecked(cols->*e.field);
            auto field = e.field;
            connect(a, &QAction::toggled, this, [this, cols, list, field](bool v) {
                cols->*field = v;
                rerenderMonitor(list, *cols);
            });
            viewToggles_ << a;
        }
        // Decode NRPN is a READING, not a column: the CC#99/98/6[/38] run of an NRPN
        // collapses into one line, parameter and value, instead of three or four.
        sub->addSeparator();
        QAction* n = sub->addAction(tr("Decode NRPN"));
        n->setObjectName(prefix + "DecodeNRPN");
        n->setCheckable(true); n->setChecked(cols->nrpn);
        connect(n, &QAction::toggled, this, [cols](bool v) { cols->nrpn = v; });
        viewToggles_ << n;                        // persisted with the others
    };

    // The panel show/hide lives on a "Show" item INSIDE the sub-menu: a checkable
    // action that also owns a sub-menu opens it on click instead of toggling, so
    // the parent cannot be the switch (Qt limitation).
    QMenu* inSub = mView->addMenu(tr("Input Monitor"));
    QAction* inShow = inSub->addAction(tr("Show"));
    inShow->setCheckable(true); inShow->setChecked(true);
    inShow->setObjectName("in.show");
    inShowAct_ = inShow;
    connect(inShow, &QAction::toggled, this, [this](bool v) { ui_->inGroup->setVisible(v); syncMonSplit(); });
    viewToggles_ << inShow;
    inSub->addSeparator();
    addCols(inSub, &inCols_, monIn_, true, "in.");

    QMenu* outSub = mView->addMenu(tr("Output Monitor"));
    QAction* outShow = outSub->addAction(tr("Show"));
    outShow->setObjectName("out.show");
    outShow->setCheckable(true); outShow->setChecked(true);
    outShowAct_ = outShow;
    connect(outShow, &QAction::toggled, this, [this](bool v) { ui_->outGroup->setVisible(v); syncMonSplit(); });
    viewToggles_ << outShow;
    outSub->addSeparator();
    addCols(outSub, &outCols_, monOut_, false, "out.");

    ui_->root->setMenuBar(menuBar_);
}

// Tools > Controller: an independent top-level window (its own frame, styled by
// the app-wide sheet). Built on first use, then just raised on later opens.
void MainWindow::openController()
{
    if (!controllerWin_) {
        // Parentless top-level: an INDEPENDENT window (own z-order, can sit BEHIND
        // the main window when it regains focus). WA_QuitOnClose=false so closing
        // the main window still quits the app even while this one is open.
        controllerWin_ = new QWidget(nullptr);
        controllerWin_->setAttribute(Qt::WA_QuitOnClose, false);
        controllerWin_->setWindowTitle(tr("ADIOS Controller"));   // window title only; Tools menu entry stays "Controller"
        // Come back at the size/position it had last time; first ever open matches
        // the main window. (Width is still capped below to where all keys show.)
        const QByteArray cg = QSettings().value("ctrlGeometry").toByteArray();
        if (!cg.isEmpty()) controllerWin_->restoreGeometry(cg);
        else               controllerWin_->resize(size());

        auto* col = new QVBoxLayout(controllerWin_);
        col->setContentsMargins(12, 12, 12, 12);   // like the main window's root layout
        // The window can never be dragged under what its content needs: the layout
        // minimum is enforced whatever explicit minimum the window may carry.
        col->setSizeConstraint(QLayout::SetMinimumSize);
        col->setSpacing(0);
        // --- Control Change: a canvas of CC controls ABOVE the keyboard, taking the
        // leftover height. Locked/Grid live in the group header (outside the panel);
        // the panel sits in a scroll area so an object left outside the visible zone
        // after a resize brings scrollbars rather than being clipped away.
        auto* ccGrp = new QGroupBox(tr("Control Change"));
        auto* ccl = new QVBoxLayout(ccGrp); ccl->setContentsMargins(0, 0, 0, 0); ccl->setSpacing(8);
        auto* ccHead = new QHBoxLayout; ccHead->setContentsMargins(0, 0, 0, 0); ccHead->setSpacing(14);
        auto* ccLocked = new FilterCheckBox(tr("Locked")); ccLocked->setObjectName("ccLocked"); ccLocked->setNeutral(true);
        auto* ccGrid = new FilterCheckBox(tr("Grid")); ccGrid->setObjectName("ccGrid"); ccGrid->setNeutral(true); ccGrid->setChecked(true);
        ccHead->addWidget(ccLocked); ccHead->addWidget(ccGrid); ccHead->addStretch();
        ccl->addLayout(ccHead);
        auto* ccPanel = new CcPanel;
        auto* ccScroll = new QScrollArea; ccScroll->setWidget(ccPanel); ccScroll->setWidgetResizable(true);
        ccScroll->setFrameShape(QFrame::NoFrame);
        ccl->addWidget(ccScroll, 1);
        connect(ccLocked, &QCheckBox::toggled, ccPanel, &CcPanel::setLocked);
        connect(ccGrid,   &QCheckBox::toggled, ccPanel, &CcPanel::setGrid);
        ccPanel->onLockChange = [ccLocked](bool on) { ccLocked->setChecked(on); };   // panel menu <-> header case
        // Every locked gesture on the surface ends up here, so this is also where the
        // snapshot selection is dropped: what is on screen is no longer what the lit
        // cell holds.
        ccPanel->onCc = [this](int cc, int v) {
            ctrlSnapTouched();
            sendRaw({ uint8_t(0xB0 | kbChannel_), uint8_t(cc & 0x7f), uint8_t(v & 0x7f) });
        };
        // The NRPN run goes out WHOLE every time - parameter MSB, LSB, then the value -
        // rather than trusting the receiver to remember the last parameter: that
        // memory breaks as soon as anything else talks on the channel in between.
        ccPanel->onNrpn = [this](int param, int v, bool wide) {
            ctrlSnapTouched();
            const uint8_t st = uint8_t(0xB0 | kbChannel_);
            sendRaw({ st, uint8_t(99), uint8_t((param >> 7) & 0x7f) });
            sendRaw({ st, uint8_t(98), uint8_t(param & 0x7f) });
            sendRaw({ st, uint8_t(6),  uint8_t(wide ? (v >> 7) & 0x7f : v & 0x7f) });
            if (wide) sendRaw({ st, uint8_t(38), uint8_t(v & 0x7f) });
        };
        // --- Bank / Program Change, to the left of the surface -----------------
        // Bank Select is two controllers, CC0 (MSB) and CC32 (LSB), and they only ARM
        // a bank: the Program Change that follows is what actually picks the sound.
        // Hence Send firing the three in that order, and a bank field on its own
        // merely re-arming.
        auto* bkGrp  = new QGroupBox(tr("Bank / Program Change"));
        auto* bkCol  = new QVBoxLayout(bkGrp); bkCol->setContentsMargins(0, 0, 0, 0); bkCol->setSpacing(8);
        // A GRID, not a form: QFormLayout deliberately tops the label when the field
        // is taller than it - meant for multi-line fields, and no setLabelAlignment
        // undoes it. Here each cell states its own vertical centring.
        auto* bkGrid = new QGridLayout;
        bkGrid->setContentsMargins(0, 0, 0, 0);
        bkGrid->setHorizontalSpacing(10); bkGrid->setVerticalSpacing(6);
        // A tick in front of each: unticked, that message simply does not go out -
        // green border when it will, red when it will not, the same checkbox as
        // Locked and Grid. The fields are kept narrow, three digits is all they hold.
        auto* msbChk = new FilterCheckBox(tr("Bank MSB")); msbChk->setObjectName(QStringLiteral("bankMsbOn"));
        auto* lsbChk = new FilterCheckBox(tr("Bank LSB")); lsbChk->setObjectName(QStringLiteral("bankLsbOn"));
        auto* pgmChk = new FilterCheckBox(tr("Program"));  pgmChk->setObjectName(QStringLiteral("programOn"));
        for (FilterCheckBox* c : { msbChk, lsbChk, pgmChk }) { c->setNeutral(true); c->setChecked(false); }   // armed by hand
        auto* msbSp = new QSpinBox; msbSp->setObjectName(QStringLiteral("bankMsb")); msbSp->setRange(0, 127);
        auto* lsbSp = new QSpinBox; lsbSp->setObjectName(QStringLiteral("bankLsb")); lsbSp->setRange(0, 127);
        auto* pgmSp = new QSpinBox; pgmSp->setObjectName(QStringLiteral("program"));
        pgmSp->setRange(1, 128);                       // shown 1..128, sent 0..127
        for (QSpinBox* sp : { msbSp, lsbSp, pgmSp }) sp->setFixedWidth(62);
        auto* sendBtn = new QPushButton(tr("Send")); sendBtn->setObjectName(QStringLiteral("bankSend"));
        const Qt::Alignment mid = Qt::AlignLeft | Qt::AlignVCenter;
        bkGrid->addWidget(msbChk, 0, 0, mid); bkGrid->addWidget(msbSp, 0, 1, mid);   // tick, then its box
        bkGrid->addWidget(lsbChk, 1, 0, mid); bkGrid->addWidget(lsbSp, 1, 1, mid);
        bkGrid->addWidget(pgmChk, 2, 0, mid); bkGrid->addWidget(pgmSp, 2, 1, mid);
        bkCol->addLayout(bkGrid);
        bkCol->addWidget(sendBtn);
        bkCol->addStretch();                           // fields stay at the top of the group
        auto sendBank = [this, msbSp, lsbSp, msbChk, lsbChk] {
            if (msbChk->isChecked()) sendRaw({ uint8_t(0xb0 | kbChannel_), uint8_t(0),  uint8_t(msbSp->value()) });
            if (lsbChk->isChecked()) sendRaw({ uint8_t(0xb0 | kbChannel_), uint8_t(32), uint8_t(lsbSp->value()) });
        };
        auto sendPgm = [this, pgmSp, pgmChk] {
            if (pgmChk->isChecked()) sendRaw({ uint8_t(0xc0 | kbChannel_), uint8_t(pgmSp->value() - 1) });
        };
        auto armSend = [sendBtn, msbChk, lsbChk, pgmChk] {   // nothing ticked, nothing to send
            sendBtn->setEnabled(msbChk->isChecked() || lsbChk->isChecked() || pgmChk->isChecked());
        };
        for (FilterCheckBox* c : { msbChk, lsbChk, pgmChk })
            connect(c, &QCheckBox::toggled, controllerWin_, [armSend](bool) { armSend(); });
        armSend();
        // An unticked line goes grey: the field it will not send has no business
        // looking editable.
        connect(msbChk, &QCheckBox::toggled, msbSp, [msbSp](bool on) { setFieldOn(msbSp, on); });
        // Armed, a bank tick reserves its controller on the surface: CC#0 for MSB,
        // CC#32 for LSB, each on its own.
        auto armBank = [ccPanel, msbChk, lsbChk] { ccPanel->setBankArmed(msbChk->isChecked(), lsbChk->isChecked()); };
        connect(msbChk, &QCheckBox::toggled, controllerWin_, [armBank](bool) { armBank(); });
        connect(lsbChk, &QCheckBox::toggled, controllerWin_, [armBank](bool) { armBank(); });
        armBank();
        connect(lsbChk, &QCheckBox::toggled, lsbSp, [lsbSp](bool on) { setFieldOn(lsbSp, on); });
        connect(pgmChk, &QCheckBox::toggled, pgmSp, [pgmSp](bool on) { setFieldOn(pgmSp, on); });
        setFieldOn(msbSp, msbChk->isChecked()); setFieldOn(lsbSp, lsbChk->isChecked()); setFieldOn(pgmSp, pgmChk->isChecked());
        // Turning a field sends NOTHING: Send is the only thing that puts bytes on
        // the wire, and the ticks arm what it will carry.
        connect(sendBtn, &QPushButton::clicked, controllerWin_, [sendBank, sendPgm] { sendBank(); sendPgm(); });

        // A splitter rather than a fixed width: the two groups share the room and the
        // user decides where the line sits. Only the surface grows with the window.
        // --- Snapshot: 4 x 16 cells, the channel cells one size up -------------
        // Four columns rather than sixteen: this side of the splitter is the narrow
        // one, a row of sixteen would have pushed the surface out of the window.
        auto* snapGrp  = new QGroupBox(tr("Snapshot"));
        auto* snapCol  = new QVBoxLayout(snapGrp); snapCol->setContentsMargins(0, 0, 0, 0); snapCol->setSpacing(0);
        auto* snapGrid = new QGridLayout; snapGrid->setContentsMargins(0, 0, 0, 0); snapGrid->setSpacing(4);
        auto* snapGroup = new QButtonGroup(controllerWin_);
        snapGroup->setObjectName(QStringLiteral("snapGroup"));
        for (int i = 0; i < 64; ++i) {
            auto* b = new QPushButton(QString::number(i + 1));
            b->setObjectName(QStringLiteral("snapCell"));
            b->setCheckable(true);
            b->setFixedSize(26, 26);                   // 18x18 for a channel cell, 26 here
            snapGroup->addButton(b, i);
            snapGrid->addWidget(b, i / 4, i % 4);
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(b, &QWidget::customContextMenuRequested, this, [this, b, i](QPoint at) {
                QMenu m(b);
                QAction* st = m.addAction(tr("Store"));
                QAction* cl = m.addAction(tr("Clear"));     cl->setEnabled(ctrlSnaps_.value(i).used);
                m.addSeparator();
                QAction* ca = m.addAction(tr("Clear All"));
                QAction* chosen = m.exec(b->mapToGlobal(at));
                if (chosen == st) ctrlStoreSnapshot(i);
                else if (chosen == cl) ctrlClearSnapshot(i);
                else if (chosen == ca) ctrlClearAllSnapshots();
            });
        }
        ctrlSnaps_ = QVector<CtrlSnap>(64);
        connect(snapGroup, &QButtonGroup::idClicked, this, [this](int n) { ctrlRecallSnapshot(n); });
        auto* snapHost = new QWidget;                  // the grid rides inside the scroll area
        auto* snapHostLay = new QVBoxLayout(snapHost);
        snapHostLay->setContentsMargins(0, 0, 0, 0); snapHostLay->setSpacing(0);
        snapHostLay->addLayout(snapGrid);
        snapHostLay->addStretch();
        auto* snapScroll = new QScrollArea;
        snapScroll->setWidget(snapHost);
        snapScroll->setWidgetResizable(true);
        snapScroll->setFrameShape(QFrame::NoFrame);
        snapScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // four columns, never more
        snapScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        snapCol->addWidget(snapScroll, 1);

        auto* leftCol = new QWidget;                   // the two groups share the left side
        auto* leftLay = new QVBoxLayout(leftCol);
        leftLay->setContentsMargins(0, 0, 0, 0); leftLay->setSpacing(10);
        leftLay->addWidget(bkGrp);
        leftLay->addWidget(snapGrp, 1);
        // Static column, and no wider than its content asks for: Fixed means the
        // layout hands it exactly its size hint, and every spare pixel goes to the
        // surface. The splitter is gone with it.
        leftCol->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        auto* topRow = new QHBoxLayout;
        topRow->setContentsMargins(0, 0, 0, 0); topRow->setSpacing(10);
        topRow->addWidget(leftCol);
        topRow->addWidget(ccGrp, 1);
        col->addLayout(topRow, 1);             // fills the room above the keyboard
        col->addSpacing(10);

        auto* grp = new QGroupBox(tr("MIDI Keyboard"));
        auto* gl  = new QVBoxLayout(grp); gl->setContentsMargins(0, 0, 0, 0); gl->setSpacing(0);   // QSS gives the 15px padding
        const int TOP_GAP = 6;   // gap under the Channel row (mirrored in Left Hand under Bend/Mod)

        // "Channel:" + 16 single-select cells (blue), the keyboard's TX channel. Same
        // 18x18 and spacing 3 as the filter's channel matrix, but a single row.
        auto* chRow = new QHBoxLayout; chRow->setContentsMargins(0, 0, 0, 0); chRow->setSpacing(3);
        chRow->addWidget(new QLabel(tr("Channel:")));
        chRow->addSpacing(6);
        auto* chGroup = new QButtonGroup(controllerWin_);
        chGroup->setObjectName(QStringLiteral("chGroup"));
        for (int i = 0; i < 16; ++i) {
            auto* b = new QPushButton(QString::number(i + 1));
            b->setObjectName("kbChan");
            b->setCheckable(true);
            b->setFixedSize(18, 18);
            b->setChecked(i == 0);
            chGroup->addButton(b, i);
            chRow->addWidget(b);
        }
        chRow->addStretch();
        auto* noNoteOff = new FilterCheckBox(tr("No Note Off"));   // on: release sends Note On vel 0 (running status)
        noNoteOff->setObjectName("noNoteOff");                     // persisted with the View toggles
        noNoteOff->setNeutral(true);                               // same gray box + white tick as "Apply Filter"
        noNoteOff->setText(tr("Keyboard No Note Off"));            // shown in Preferences, not on the keyboard row
        connect(chGroup, &QButtonGroup::idToggled, this, [this](int id, bool on) { if (on) kbChannel_ = id; });
        gl->addLayout(chRow);
        gl->addSpacing(TOP_GAP);

        // The strip and its gap are named because Left Hand mirrors them: a wheel runs
        // from the TOP of the strip to the BOTTOM of the white keys.
        const int STRIP_H = 4, STRIP_GAP = 2;
        auto* keybed = new QWidget; keybed->setFixedHeight(STRIP_H);   // thin accent strip above the keys
        keybed->setStyleSheet("background:#4a7ab8; border-radius:2px;");
        gl->addWidget(keybed);
        gl->addSpacing(STRIP_GAP);

        // 112 is a PHYSICAL height - the way the author measures it, with a ruler on
        // a screen shot - so it is divided by the display scaling: 112 on screen at
        // 100 % and at 125 % alike. Keys and wheels are built on this one figure.
        const int keyH = qRound(112.0 / QGuiApplication::primaryScreen()->devicePixelRatio());
        auto* kb = new PianoKeyboard;
        kb->setFixedHeight(keyH);
        gl->addWidget(kb);

        gl->addSpacing(10);                    // gap: the scrollbar sits away from the keys

        // Scrollbar in a fixed-height row so hiding it (all keys fit) keeps its space:
        // the keys never move, it just disappears.
        auto* sbRow = new QWidget; sbRow->setFixedHeight(14);
        auto* sbLay = new QVBoxLayout(sbRow); sbLay->setContentsMargins(0, 0, 0, 0); sbLay->setSpacing(0);
        auto* sb = new QScrollBar(Qt::Horizontal);
        sbLay->addWidget(sb);
        gl->addWidget(sbRow);

        connect(sb, &QScrollBar::valueChanged, kb, [kb](int v) { kb->setScroll(v); });
        kb->onView = [kb, sb] {
            QSignalBlocker block(sb);
            sb->setRange(0, kb->maxScroll());
            sb->setPageStep(kb->visibleWhite());
            sb->setValue(kb->scroll());
            sb->setVisible(kb->maxScroll() > 0);   // all keys fit -> no scrollbar (row keeps its height)
        };
        kb->onNoteOn  = [this](int n, int vel) { sendRaw({ uint8_t(0x90 | kbChannel_), uint8_t(n), uint8_t(vel) }); };
        kb->onNoteOff = [this, noNoteOff](int n) {
            const uint8_t status = noNoteOff->isChecked() ? uint8_t(0x90 | kbChannel_)    // Note On vel 0
                                                          : uint8_t(0x80 | kbChannel_);   // real Note Off
            sendRaw({ status, uint8_t(n), uint8_t(0) });
        };

        // --- Left Hand: pitch-bend + modulation wheels, to the LEFT of the keyboard.
        // Its three rows line up with the keyboard group beside it: the Bend/Mod
        // captions on the Channel row, the wheels on the blue strip + keys (same 126
        // px), and the value read-outs on the scrollbar row. Same row heights and
        // spacings, same group chrome -> everything aligns.
        const int WHEEL_GAP = 6;   // keep the two wheels close, not spread out
        auto* lhGrp = new QGroupBox(tr("Left Hand"));
        lhGrp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        auto* lgl = new QVBoxLayout(lhGrp); lgl->setContentsMargins(0, 0, 0, 0); lgl->setSpacing(0);
        auto mkCentered = [](const QString& t, const char* obj, int h) {
            auto* l = new QLabel(t); l->setObjectName(obj);
            l->setAlignment(Qt::AlignCenter); l->setFixedSize(Wheel::TW, h); return l;
        };
        auto* labRow = new QHBoxLayout; labRow->setContentsMargins(0, 0, 0, 0); labRow->setSpacing(WHEEL_GAP);
        // Row for row with the keyboard group: the labels take the Channel row's own
        // height, the wheels span strip + gap + keys, the values sit in a row the
        // height of the scrollbar's.
        const int chH = chRow->sizeHint().height();
        auto* lblBend = mkCentered(tr("Bend"), "wheelLbl", chH);
        auto* lblMod  = mkCentered(tr("Mod"),  "wheelLbl", chH);
        labRow->addWidget(lblBend); labRow->addWidget(lblMod);
        lgl->addLayout(labRow);
        lgl->addSpacing(TOP_GAP);
        auto* whRow = new QHBoxLayout; whRow->setContentsMargins(0, 0, 0, 0); whRow->setSpacing(WHEEL_GAP);
        const int wheelH = STRIP_H + STRIP_GAP + keyH;  // top of the strip to the foot of the keys
        auto* pitch = new Wheel(Wheel::Pitch, wheelH);
        auto* mod   = new Wheel(Wheel::Mod, wheelH); modWheel_ = mod;   // reachable by the snapshot code
        whRow->addWidget(pitch); whRow->addWidget(mod);
        lgl->addLayout(whRow);
        lgl->addSpacing(10);
        auto* valRow = new QHBoxLayout; valRow->setContentsMargins(0, 0, 0, 0); valRow->setSpacing(WHEEL_GAP);
        auto* vBend = mkCentered(QStringLiteral("0"), "wheelVal", 14);   // 14 = the scrollbar row height
        auto* vMod  = mkCentered(QStringLiteral("0"), "wheelVal", 14); modValLabel_ = vMod;
        valRow->addWidget(vBend); valRow->addWidget(vMod);
        lgl->addLayout(valRow);

        pitch->onChange = [this, vBend](int v) {
            const int s = v - 8192;   // 0 at rest, signed offset otherwise
            vBend->setText(QString(s > 0 ? "+" : "") + QString::number(s));
            sendRaw({ uint8_t(0xE0 | kbChannel_), uint8_t(v & 0x7f), uint8_t((v >> 7) & 0x7f) });
        };
        mod->onChange = [this, vMod](int v) {
            vMod->setText(QString::number(v));
            ctrlSnapTouched();                     // Mod is one of the values a snapshot holds
            sendRaw({ uint8_t(0xB0 | kbChannel_), uint8_t(1), uint8_t(v & 0x7f) });   // CC 1
        };

        auto* handsRow = new QHBoxLayout; handsRow->setContentsMargins(0, 0, 0, 0); handsRow->setSpacing(10);
        handsRow->addWidget(lhGrp);
        handsRow->addWidget(grp, 1);   // the keyboard group takes the slack
        col->addLayout(handsRow);

        // Menu bar: Config (presets later), Edit, View. A plain QWidget has no
        // native menu bar, so the layout hosts it - same trick as the main window.
        // Floors, so the window cannot be dragged shorter than what these can draw:
        // squashed to a band, a surface shows the top of its knobs and nothing else.
        // A hidden group is ignored by the layout, so masking one still frees its room.
        ccGrp->setMinimumHeight(160);
        snapGrp->setMinimumHeight(140);
        // Nothing to freeze down here: the keybed is setFixedHeight(120), the strip
        // and the scrollbar row are fixed too, and a wheel is setFixedSize - both
        // groups already take their height from their content and never stretch.
        // Pinning them again only carved whatever height they happened to have.

        auto* mb = new QMenuBar(controllerWin_);
        // Config: the whole controller as ONE file - New / Open / Save / Save As / Recent.
        auto* cfgMenu = mb->addMenu(tr("Config"));
        auto* cfgNew  = cfgMenu->addAction(tr("New"));      cfgNew->setShortcut(QKeySequence::New);
        auto* cfgOpen = cfgMenu->addAction(tr("Open..."));  cfgOpen->setShortcut(QKeySequence::Open);
        auto* cfgSave = cfgMenu->addAction(tr("Save"));     cfgSave->setShortcut(QKeySequence::Save);
        auto* cfgAs   = cfgMenu->addAction(tr("Save As...")); cfgAs->setShortcut(QKeySequence::SaveAs);
        cfgMenu->addSeparator();
        ctrlRecentMenu_ = cfgMenu->addMenu(tr("Recent"));
        cfgMenu->addSeparator();
        auto* prefAct = cfgMenu->addAction(tr("Preferences..."));

        // The Preferences window. Built now, shown on demand, and a CHILD of the
        // controller window: its ticks are found by the same objectName loops that
        // persist every other tick, and they live in the config file like the rest.
        prefsDlg_ = new QDialog(controllerWin_);
        prefsDlg_->setWindowTitle(tr("Preferences"));
        prefsDlg_->setModal(false);
        auto* pf = new QFormLayout(prefsDlg_);
        pf->setHorizontalSpacing(14); pf->setVerticalSpacing(8);
        auto mkTick = [](const QString& t, const char* obj, bool on) {
            auto* c = new FilterCheckBox(t); c->setObjectName(QLatin1String(obj)); c->setNeutral(true); c->setChecked(on); return c;
        };
        midiInChk_   = mkTick(tr("MIDI Input"),                 "midiIn",      false);
        fwdChk_      = mkTick(tr("MIDI Forward"),               "midiForward", false);
        snapOnlyAct_ = mkTick(tr("Snapshot Send only changes"), "snapOnly",    true);
        snapModAct_  = mkTick(tr("Snapshot includes Mod"),      "snapMod",     false);
        omniChk_ = mkTick(tr("MIDI Input Omni"), "midiOmni", true);   // ticked: any channel
        // "As Output" first and by default: the input then listens on whatever channel
        // the keyboard transmits on, so the two follow each other instead of being set twice.
        inChanBox_ = new QComboBox; inChanBox_->setObjectName(QStringLiteral("midiInChannel"));
        inChanBox_->addItem(tr("As Output"));
        for (int i = 1; i <= 16; ++i) inChanBox_->addItem(QString::number(i));
        inChanBox_->setCurrentIndex(0);
        // Three groups, two rules between them: a 1 px line in the panel's border tone.
        auto mkSep = [] { auto* f = new QFrame; f->setObjectName(QStringLiteral("prefSep")); f->setFixedHeight(1); return f; };
        pf->addRow(noNoteOff);                                   // moved here from the keyboard row
        pf->addRow(mkSep());
        pf->addRow(midiInChk_);
        pf->addRow(omniChk_);
        pf->addRow(tr("MIDI Input Channel"), inChanBox_);
        if (QWidget* l = pf->labelForField(inChanBox_)) l->setObjectName(QStringLiteral("filterLabel"));   // the ticks' grey, not label white
        pf->addRow(fwdChk_);
        pf->addRow(mkSep());
        pf->addRow(snapOnlyAct_);
        pf->addRow(snapModAct_);
        // Everything under MIDI Input follows it: Omni and Forward only with Input on,
        // the channel only with Input on AND Omni off.
        auto prefEnable = [this, pf] {
            const bool in = midiInChk_->isChecked();
            omniChk_->setEnabled(in);
            fwdChk_->setEnabled(in);
            const bool chan = in && !omniChk_->isChecked();
            setFieldOn(inChanBox_, chan);
            if (QWidget* l = pf->labelForField(inChanBox_)) l->setEnabled(chan);   // the label greys with its field
        };
        connect(omniChk_,   &QCheckBox::toggled, this, [prefEnable](bool) { prefEnable(); });
        connect(midiInChk_, &QCheckBox::toggled, this, [prefEnable](bool) { prefEnable(); });
        prefEnable();
        connect(prefAct, &QAction::triggered, this, [this] { prefsDlg_->show(); prefsDlg_->raise(); prefsDlg_->activateWindow(); });
        connect(cfgNew,  &QAction::triggered, this, [this] { ctrlNewConfig(); });
        connect(cfgOpen, &QAction::triggered, this, [this] { ctrlOpenConfig(); });
        connect(cfgSave, &QAction::triggered, this, [this] { ctrlSaveConfig(false); });
        connect(cfgAs,   &QAction::triggered, this, [this] { ctrlSaveConfig(true); });
        ctrlRebuildRecent();
        auto* editMenu = mb->addMenu(tr("Edit"));
        auto* view = mb->addMenu(tr("View"));

        // --- View: Reset Layout (top) then the Left Hand toggles ---
        auto* resetAct = view->addAction(tr("Reset Layout"));
        view->addSeparator();
        auto addToggle = [](QMenu* m, const QString& t, const char* obj, bool on) {
            auto* a = m->addAction(t); a->setObjectName(obj); a->setCheckable(true); a->setChecked(on); return a;
        };
        // One sub-menu per group, in the order they sit on screen. Hiding both groups
        // of the left column empties it, and the surface takes the whole width.
        // With the three top groups hidden there is nothing left that WANTS height -
        // only the keyboard, which has a fixed one. The window then snaps to that
        // height and refuses to be stretched vertically, instead of being draggable
        // over an empty band. The height it had is kept in a property, and handed
        // back the moment a group returns.
        auto fitToKeyboard = [this, bkGrp, snapGrp, ccGrp] {
            if (!controllerWin_ || !controllerWin_->layout()) return;
            // isHidden(), NOT isVisible(): before the window is shown, isVisible() is
            // false for every child, hidden or not - and the toggles restored from the
            // settings fire exactly then. With a group saved hidden, the window came up
            // with its maximum locked, and could never be dragged taller.
            const bool any = !bkGrp->isHidden() || !snapGrp->isHidden() || !ccGrp->isHidden();
            // Only the MAXIMUM is ever touched: an explicit minimum on the window would
            // override the one the layout computes, and let the window be dragged under
            // its content - which is exactly how the groups came to overlap once.
            controllerWin_->setMaximumHeight(QWIDGETSIZE_MAX);      // unlock before measuring
            if (!any) {
                if (!controllerWin_->property("freeH").isValid())
                    controllerWin_->setProperty("freeH", controllerWin_->height());
                controllerWin_->layout()->activate();               // the hide must be accounted for
                const int h = controllerWin_->sizeHint().height();
                controllerWin_->resize(controllerWin_->width(), h);
                controllerWin_->setMaximumHeight(h);                // locked: min (layout) == max
            } else if (const QVariant fh = controllerWin_->property("freeH"); fh.isValid()) {
                controllerWin_->resize(controllerWin_->width(), fh.toInt());
                controllerWin_->setProperty("freeH", QVariant());
            }
        };
        auto* bpmM = new StayOpenMenu(tr("Bank/Program Change"), view); view->addMenu(bpmM);
        connect(addToggle(bpmM, tr("Show"), "bpmShow", true), &QAction::toggled, controllerWin_,
                [bkGrp, fitToKeyboard](bool on) { bkGrp->setVisible(on); fitToKeyboard(); });
        auto* snapM = new StayOpenMenu(tr("Snapshot"), view); view->addMenu(snapM);
        connect(addToggle(snapM, tr("Show"), "snapShow", true), &QAction::toggled, controllerWin_,
                [snapGrp, fitToKeyboard](bool on) { snapGrp->setVisible(on); fitToKeyboard(); });
        auto* ccM = new StayOpenMenu(tr("Control Change"), view); view->addMenu(ccM);
        connect(addToggle(ccM, tr("Show"), "ccShow", true), &QAction::toggled, controllerWin_,
                [ccGrp, fitToKeyboard](bool on) { ccGrp->setVisible(on); fitToKeyboard(); });
        auto* lh = new StayOpenMenu(tr("Left Hand"), view); view->addMenu(lh);   // StayOpen: toggles don't close it
        auto* aShow = addToggle(lh, tr("Show"), "lhShow", true);
        connect(aShow, &QAction::toggled, lhGrp, &QWidget::setVisible);
        auto* aStripped = addToggle(lh, tr("Stripped"), "lhStripped", true);
        connect(aStripped, &QAction::toggled, this, [pitch, mod](bool on) { pitch->setRidges(on); mod->setRidges(on); });
        auto* bendM = new StayOpenMenu(tr("Bend"), lh); lh->addMenu(bendM);
        auto* aBendShow = addToggle(bendM, tr("Show"), "bendShow", true);
        connect(aBendShow, &QAction::toggled, this, [lblBend, pitch, vBend](bool on) { lblBend->setVisible(on); pitch->setVisible(on); vBend->setVisible(on); });
        auto* aBend7 = addToggle(bendM, tr("7 bits"), "bend7bit", false);
        connect(aBend7, &QAction::toggled, pitch, [pitch](bool on) { pitch->set7bit(on); });
        auto* modM = new StayOpenMenu(tr("Mod"), lh); lh->addMenu(modM);
        auto* aModShow = addToggle(modM, tr("Show"), "modShow", true);
        connect(aModShow, &QAction::toggled, this, [lblMod, mod, vMod](bool on) { lblMod->setVisible(on); mod->setVisible(on); vMod->setVisible(on); });
        connect(resetAct, &QAction::triggered, this,     // every View toggle back to default + default size
                [this, aShow, aStripped, aBendShow, aBend7, aModShow] {
                    aShow->setChecked(true); aStripped->setChecked(true); aBendShow->setChecked(true);
                    aBend7->setChecked(false); aModShow->setChecked(true);
                    controllerWin_->resize(size());
                });

        // --- Edit: undo/redo, Add (knob, both switches), duplicate/remove,
        // select all / remove all. Shortcuts on the actions; enabled states kept
        // live by the panel (onStateChanged). ---
        auto* undoAct = editMenu->addAction(tr("Undo")); undoAct->setShortcut(QKeySequence::Undo);
        auto* redoAct = editMenu->addAction(tr("Redo")); redoAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Z")));
        editMenu->addSeparator();
        auto* addMenu = editMenu->addMenu(tr("Add"));
        auto* addKnobAct  = addMenu->addAction(tr("Knob"));
        auto* addSwAct    = addMenu->addAction(tr("Slider Switch"));
        auto* addPbAct    = addMenu->addAction(tr("Push Switch"));
        addMenu->addSeparator();
        auto* addLblAct   = addMenu->addAction(tr("Label"));
        auto* addLinAct   = addMenu->addAction(tr("Line"));
        editMenu->addSeparator();
        auto* dupAct = editMenu->addAction(tr("Duplicate")); dupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
        auto* remAct = editMenu->addAction(tr("Remove"));    remAct->setShortcut(QKeySequence::Delete);
        editMenu->addSeparator();
        auto* selAllAct = editMenu->addAction(tr("Select All")); selAllAct->setShortcut(QKeySequence::SelectAll);
        auto* remAllAct = editMenu->addAction(tr("Remove All"));
        connect(undoAct,    &QAction::triggered, ccPanel, [ccPanel] { ccPanel->undo(); });
        connect(redoAct,    &QAction::triggered, ccPanel, [ccPanel] { ccPanel->redo(); });
        connect(addKnobAct, &QAction::triggered, ccPanel, [ccPanel] {
            ccPanel->beginGesture(); ccPanel->addControl(CcControl::Knob, ccPanel->visibleRegion().boundingRect().center()); ccPanel->endGesture(); });
        connect(addSwAct, &QAction::triggered, ccPanel, [ccPanel] {
            ccPanel->beginGesture(); ccPanel->addControl(CcControl::SliderSwitch, ccPanel->visibleRegion().boundingRect().center()); ccPanel->endGesture(); });
        connect(addPbAct, &QAction::triggered, ccPanel, [ccPanel] {
            ccPanel->beginGesture(); ccPanel->addControl(CcControl::PushSwitch, ccPanel->visibleRegion().boundingRect().center()); ccPanel->endGesture(); });
        connect(addLblAct, &QAction::triggered, ccPanel, [ccPanel] {
            ccPanel->beginGesture(); ccPanel->addControl(CcControl::Label, ccPanel->visibleRegion().boundingRect().center()); ccPanel->endGesture(); });
        connect(addLinAct, &QAction::triggered, ccPanel, [ccPanel] {
            ccPanel->beginGesture(); ccPanel->addControl(CcControl::Line, ccPanel->visibleRegion().boundingRect().center()); ccPanel->endGesture(); });
        connect(dupAct,     &QAction::triggered, ccPanel, [ccPanel] { ccPanel->beginGesture(); ccPanel->duplicateSelection(); ccPanel->endGesture(); });
        connect(remAct,     &QAction::triggered, ccPanel, [ccPanel] { ccPanel->beginGesture(); ccPanel->deleteSelected(); ccPanel->endGesture(); });
        connect(selAllAct,  &QAction::triggered, ccPanel, [ccPanel] { ccPanel->selectAll(); });
        connect(remAllAct,  &QAction::triggered, ccPanel, [ccPanel] { ccPanel->removeAll(); });
        auto updateEdit = [ccPanel, undoAct, redoAct, addMenu, dupAct, remAct, selAllAct, remAllAct] {
            const bool ed = !ccPanel->locked();
            undoAct->setEnabled(ccPanel->canUndo());
            redoAct->setEnabled(ccPanel->canRedo());
            addMenu->setEnabled(ed);
            dupAct->setEnabled(ed && ccPanel->hasSelection());
            remAct->setEnabled(ed && ccPanel->hasSelection());
            selAllAct->setEnabled(ed && ccPanel->controlCount() > 0);
            remAllAct->setEnabled(ed && ccPanel->controlCount() > 0);
        };
        ccPanel->onStateChanged = updateEdit;
        connect(editMenu, &QMenu::aboutToShow, editMenu, [updateEdit] { updateEdit(); });   // fresh states on open
        updateEdit();
        col->setMenuBar(mb);

        // Restore what was saved at the last close: View toggles, No Note Off, and
        // the channel. Setting each fires its handler -> the panels/state come back
        // exactly as left.
        QSettings cs;
        for (QAction* a : controllerWin_->findChildren<QAction*>()) {
            if (a->objectName().isEmpty()) continue;
            const QVariant v = cs.value("ctrl/" + a->objectName());
            if (v.isValid() && v.toBool() != a->isChecked()) a->setChecked(v.toBool());
        }
        for (QCheckBox* c : controllerWin_->findChildren<QCheckBox*>()) {
            if (c->objectName().isEmpty()) continue;
            const QVariant v = cs.value("ctrl/" + c->objectName());
            if (v.isValid()) c->setChecked(v.toBool());
        }
        for (QSpinBox* sp : controllerWin_->findChildren<QSpinBox*>()) {
            if (sp->objectName().isEmpty()) continue;
            const QVariant v = cs.value("ctrl/" + sp->objectName());
            if (!v.isValid()) continue;
            const QSignalBlocker block(sp);              // no signal storm while restoring
            sp->setValue(v.toInt());
        }
        for (QComboBox* cb : controllerWin_->findChildren<QComboBox*>()) {
            if (cb->objectName().isEmpty()) continue;
            const QVariant v = cs.value("ctrl/" + cb->objectName());
            if (!v.isValid()) continue;
            const QSignalBlocker block(cb);
            cb->setCurrentIndex(qBound(0, v.toInt(), cb->count() - 1));
        }
        for (QSplitter* sp : controllerWin_->findChildren<QSplitter*>()) {
            if (sp->objectName().isEmpty()) continue;
            const QByteArray st = cs.value("ctrl/" + sp->objectName()).toByteArray();
            if (!st.isEmpty()) sp->restoreState(st);
        }
        if (auto* b = chGroup->button(cs.value("ctrl/channel", 0).toInt())) b->setChecked(true);
        if (auto* b = snapGroup->button(cs.value("ctrl/snapshot", 0).toInt())) b->setChecked(true);
        ccSurface_ = ccPanel;                                // for saveSettings
        if (const QByteArray surf = cs.value("ctrl/surface").toByteArray(); !surf.isEmpty())
            ccPanel->loadState(surf);                        // the legacy state (no config file yet)
        // What comes back, in order of preference: the SESSION copy written at the
        // last close (the exact working state, dirty flag included), else the named
        // config that was open, else the legacy state restored above. Whatever the
        // source, a session ALWAYS starts locked.
        const QString cfg = cs.value("ctrl/configPath").toString();
        ctrlConfigPath_ = (!cfg.isEmpty() && QFileInfo::exists(cfg)) ? cfg : QString();
        bool dirty = false;
        if (QFileInfo::exists(ctrlSessionPath()) && ctrlLoadXml(ctrlSessionPath()))
            dirty = cs.value("ctrl/dirty", false).toBool();
        else if (!ctrlConfigPath_.isEmpty())
            ctrlLoadXml(ctrlConfigPath_);
        ccLocked->setChecked(true);
        // From here on, any change to what the config holds marks it dirty. Wired
        // last so the restores above do not count.
        ccPanel->onEdited = [this] { ctrlSetDirty(true); };
        // The named spin boxes ARE the three Bank/Program values a snapshot holds, so
        // moving one both dirties the config and drops the snapshot selection.
        for (QSpinBox* sp : controllerWin_->findChildren<QSpinBox*>())
            if (!sp->objectName().isEmpty())
                connect(sp, &QSpinBox::valueChanged, this, [this](int) { ctrlSetDirty(true); ctrlSnapTouched(); });
        for (QComboBox* cb : controllerWin_->findChildren<QComboBox*>())
            if (!cb->objectName().isEmpty())
                connect(cb, &QComboBox::currentIndexChanged, this, [this](int) { ctrlSetDirty(true); });
        for (QCheckBox* c : controllerWin_->findChildren<QCheckBox*>())
            if (!c->objectName().isEmpty() && c->objectName() != QLatin1String("ccLocked"))
                connect(c, &QCheckBox::toggled, this, [this](bool) { ctrlSetDirty(true); });
        for (QAction* a : controllerWin_->findChildren<QAction*>())
            if (!a->objectName().isEmpty() && a->isCheckable()) connect(a, &QAction::toggled, this, [this](bool) { ctrlSetDirty(true); });
        connect(chGroup, &QButtonGroup::idToggled, this, [this](int, bool on) { if (on) ctrlSetDirty(true); });
        ctrlDirty_ = dirty;
        ctrlUpdateTitle();

        // Cap the width where EVERY key shows: once laid out, the window/keyboard
        // width difference IS the exact chrome (margins + group padding + border),
        // so no magic number can drift if the stylesheet changes.
        QWidget* win = controllerWin_;
        QTimer::singleShot(0, win, [win, kb] {
            if (kb->width() > 0) win->setMaximumWidth(kb->fullWidth() + win->width() - kb->width());
        });
    }
    controllerWin_->show();
    controllerWin_->raise();
    controllerWin_->activateWindow();
}

// ---------------------------------------------------------------------------
// Controller configuration file. One XML document holds what the window IS -
// its layout, the Bank/Program settings, the surface with its identified
// objects - and will hold the snapshots. Locked is deliberately not in it: a
// session always starts locked. An unknown element is skipped with a warning,
// never fatal, so a file from a newer version still opens.
// ---------------------------------------------------------------------------
namespace {
const char* const kCapNames[4] = { "black", "blue", "red", "grey" };
int capIndex(const QString& s) { for (int i = 0; i < 4; ++i) if (s == QLatin1String(kCapNames[i])) return i; return s.toInt(); }
QString b01(bool v) { return v ? QStringLiteral("1") : QStringLiteral("0"); }
bool attrBool(const QXmlStreamAttributes& a, const char* n, bool d) { return a.hasAttribute(n) ? a.value(n) != QLatin1String("0") : d; }
int  attrInt (const QXmlStreamAttributes& a, const char* n, int d)  { return a.hasAttribute(n) ? a.value(n).toInt() : d; }
}

bool MainWindow::ctrlWriteConfig(const QString& path)
{
    if (!controllerWin_ || !ccSurface_) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    auto* panel = static_cast<CcPanel*>(ccSurface_);
    auto spin = [this](const char* n) { auto* s = controllerWin_->findChild<QSpinBox*>(QLatin1String(n)); return s ? s->value() : 0; };
    auto tick = [this](const char* n) { auto* c = controllerWin_->findChild<QCheckBox*>(QLatin1String(n)); return c && c->isChecked(); };
    auto num  = [](int v) { return QString::number(v); };

    QXmlStreamWriter x(&f);
    x.setAutoFormatting(true); x.setAutoFormattingIndent(2);
    x.writeStartDocument();
    // Version 2: midiIn/@channel counts from 0, where 0 is "As Output". Version 1
    // only knew 1..16, so a file that old cannot mean anything but "not chosen".
    x.writeStartElement("adios-controller"); x.writeAttribute("version", "2");

    x.writeStartElement("window");
    x.writeAttribute("w", num(controllerWin_->width())); x.writeAttribute("h", num(controllerWin_->height()));
    x.writeEndElement();

    x.writeStartElement("view");                          // every checkable View action, by name
    for (QAction* a : controllerWin_->findChildren<QAction*>())
        if (!a->objectName().isEmpty() && a->isCheckable()) x.writeAttribute(a->objectName(), b01(a->isChecked()));
    x.writeEndElement();

    x.writeStartElement("keyboard");
    x.writeAttribute("channel", num(kbChannel_ + 1)); x.writeAttribute("noNoteOff", b01(tick("noNoteOff")));
    x.writeEndElement();

    x.writeStartElement("midiIn");
    x.writeAttribute("on", b01(tick("midiIn"))); x.writeAttribute("omni", b01(tick("midiOmni")));
    x.writeAttribute("channel", num(inChanBox_ ? inChanBox_->currentIndex() : 0));   // 0 = As Output, 1..16 = that channel
    x.writeAttribute("forward", b01(tick("midiForward")));
    x.writeEndElement();

    x.writeStartElement("bank");
    x.writeAttribute("msb", num(spin("bankMsb"))); x.writeAttribute("lsb", num(spin("bankLsb"))); x.writeAttribute("program", num(spin("program")));
    x.writeAttribute("sendMsb", b01(tick("bankMsbOn"))); x.writeAttribute("sendLsb", b01(tick("bankLsbOn"))); x.writeAttribute("sendProgram", b01(tick("programOn")));
    x.writeEndElement();

    x.writeStartElement("surface");
    x.writeAttribute("grid", b01(panel->gridOn())); x.writeAttribute("nextId", num(panel->nextId()));
    for (const CcPanel::Desc& d : panel->snapshot()) {
        const auto k = CcControl::kindOf(d.kind);
        x.writeStartElement(k == CcControl::Knob ? "knob" : k == CcControl::SliderSwitch ? "slider"
                          : k == CcControl::PushSwitch ? "push" : k == CcControl::Label ? "label" : "line");
        x.writeAttribute("id", num(d.id));
        if (k == CcControl::Line) {
            x.writeAttribute("thickness", num(d.lineW));
        } else if (k == CcControl::Label) {
            x.writeAttribute("text", d.name); x.writeAttribute("size", num(d.fontPx));
        } else {
            x.writeAttribute("name", d.name); x.writeAttribute("cc", num(d.cc));
            if (d.nrpn) { x.writeAttribute("nrpn", "1"); x.writeAttribute("param", num(d.nrpnNum)); x.writeAttribute("wide", b01(d.wide)); }
            if (k == CcControl::Knob) {
                x.writeAttribute("min", num(d.rmin)); x.writeAttribute("max", num(d.rmax)); x.writeAttribute("default", num(d.def));
                x.writeAttribute("value", num(d.val)); x.writeAttribute("absolute", b01(d.absolute));
            } else if (k == CcControl::SliderSwitch) {
                x.writeAttribute("positions", num(d.positions)); x.writeAttribute("names", d.posNames);
                x.writeAttribute("values", d.posValues); x.writeAttribute("position", num(d.posIdx));
            } else {
                x.writeAttribute("off", num(d.rmin)); x.writeAttribute("on", num(d.rmax)); x.writeAttribute("value", num(d.val));
                x.writeAttribute("latched", b01(d.latching)); x.writeAttribute("cap", QLatin1String(kCapNames[qBound(0, d.capColor, 3)]));
            }
        }
        x.writeAttribute("x", num(d.x)); x.writeAttribute("y", num(d.y)); x.writeAttribute("w", num(d.w)); x.writeAttribute("h", num(d.h));
        x.writeEndElement();
    }
    x.writeEndElement();

    x.writeStartElement("snapshots");
    x.writeAttribute("sendOnlyChanges", b01(snapOnlyAct_ && snapOnlyAct_->isChecked()));
    x.writeAttribute("includeMod",      b01(snapModAct_  && snapModAct_->isChecked()));
    for (int i = 0; i < ctrlSnaps_.size(); ++i) {
        const CtrlSnap& sn = ctrlSnaps_[i];
        if (!sn.used) continue;
        x.writeStartElement("snapshot"); x.writeAttribute("n", num(i + 1));
        x.writeStartElement("bank");
        x.writeAttribute("msb", num(sn.msb)); x.writeAttribute("lsb", num(sn.lsb)); x.writeAttribute("program", num(sn.program));
        x.writeEndElement();
        for (auto it = sn.values.cbegin(); it != sn.values.cend(); ++it) {
            x.writeStartElement("v"); x.writeAttribute("id", num(it.key())); x.writeAttribute("value", num(it.value())); x.writeEndElement();
        }
        x.writeStartElement("mod"); x.writeAttribute("value", num(sn.mod)); x.writeEndElement();
        x.writeEndElement();
    }
    x.writeEndElement();

    x.writeEndElement();
    x.writeEndDocument();
    return f.error() == QFileDevice::NoError;
}

bool MainWindow::ctrlReadConfig(const QString& path)
{
    if (!ctrlLoadXml(path)) return false;
    ctrlConfigPath_ = path;
    ctrlAddRecent(path);
    ctrlDirty_ = false;
    ctrlUpdateTitle();
    return true;
}

// The working copy lives beside the settings, not in the user's file: what changed
// in a session survives a relaunch WITH its asterisk, and the named file on disk
// moves only on Save.
QString MainWindow::ctrlSessionPath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QLatin1String("/controller-session.xml");
}

bool MainWindow::ctrlLoadXml(const QString& path)
{
    if (!controllerWin_ || !ccSurface_) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    auto* panel = static_cast<CcPanel*>(ccSurface_);
    QXmlStreamReader x(&f);
    QVector<CcPanel::Desc> st;
    int nextId = 1;
    bool gridOn = true, seenSurface = false;
    int fileVersion = 1;                                  // decides how midiIn/@channel reads
    QSize win;
    QVector<CtrlSnap> snaps(64);
    int inSnap = -1;                                      // index of the <snapshot> being read
    bool onlyChanges = true, includeMod = false;

    while (!x.atEnd()) {
        const auto tok = x.readNext();
        if (tok == QXmlStreamReader::EndElement && x.name() == QLatin1String("snapshot")) { inSnap = -1; continue; }
        if (tok != QXmlStreamReader::StartElement) continue;
        const QString tag = x.name().toString();
        const QXmlStreamAttributes a = x.attributes();
        if (tag == QLatin1String("adios-controller")) { fileVersion = attrInt(a, "version", 1); continue; }
        if (tag == QLatin1String("snapshots")) { onlyChanges = attrBool(a, "sendOnlyChanges", true); includeMod = attrBool(a, "includeMod", false); continue; }
        if (tag == QLatin1String("snapshot")) {
            inSnap = attrInt(a, "n", 0) - 1;
            if (inSnap >= 0 && inSnap < 64) snaps[inSnap].used = true; else inSnap = -1;
            continue;
        }
        if (inSnap >= 0) {                                // inside a snapshot: its bank, values, mod
            if (tag == QLatin1String("bank")) { snaps[inSnap].msb = attrInt(a, "msb", 0); snaps[inSnap].lsb = attrInt(a, "lsb", 0); snaps[inSnap].program = attrInt(a, "program", 1); }
            else if (tag == QLatin1String("v")) snaps[inSnap].values.insert(attrInt(a, "id", 0), attrInt(a, "value", 0));
            else if (tag == QLatin1String("mod")) snaps[inSnap].mod = attrInt(a, "value", 0);
            continue;
        }
        if (tag == QLatin1String("window")) { win = QSize(attrInt(a, "w", 0), attrInt(a, "h", 0)); continue; }
        if (tag == QLatin1String("view")) {
            for (QAction* act : controllerWin_->findChildren<QAction*>())
                if (!act->objectName().isEmpty() && act->isCheckable() && a.hasAttribute(act->objectName()))
                    act->setChecked(attrBool(a, act->objectName().toUtf8().constData(), act->isChecked()));
            continue;
        }
        if (tag == QLatin1String("keyboard")) {
            if (auto* g = controllerWin_->findChild<QButtonGroup*>(QStringLiteral("chGroup")))
                if (auto* b = g->button(qBound(0, attrInt(a, "channel", 1) - 1, 15))) b->setChecked(true);
            if (auto* c = controllerWin_->findChild<QCheckBox*>(QStringLiteral("noNoteOff"))) c->setChecked(attrBool(a, "noNoteOff", c->isChecked()));
            continue;
        }
        if (tag == QLatin1String("midiIn")) {
            if (midiInChk_) midiInChk_->setChecked(attrBool(a, "on", false));
            if (omniChk_)   omniChk_->setChecked(attrBool(a, "omni", true));
            // A version 1 file carries the old spin box's 1..16 and had no way to say
            // "As Output" - which is now the default, so that is what it gets.
            if (inChanBox_) { const QSignalBlocker b(inChanBox_);
                inChanBox_->setCurrentIndex(fileVersion >= 2 ? qBound(0, attrInt(a, "channel", 0), 16) : 0); }
            if (fwdChk_)    fwdChk_->setChecked(attrBool(a, "forward", false));
            continue;
        }
        if (tag == QLatin1String("bank")) {
            auto setSpin = [this](const char* n, int v) { if (auto* s = controllerWin_->findChild<QSpinBox*>(QLatin1String(n))) { const QSignalBlocker b(s); s->setValue(v); } };
            auto setTick = [this](const char* n, bool v) { if (auto* c = controllerWin_->findChild<QCheckBox*>(QLatin1String(n))) c->setChecked(v); };
            setSpin("bankMsb", attrInt(a, "msb", 0)); setSpin("bankLsb", attrInt(a, "lsb", 0)); setSpin("program", attrInt(a, "program", 1));
            setTick("bankMsbOn", attrBool(a, "sendMsb", true)); setTick("bankLsbOn", attrBool(a, "sendLsb", true)); setTick("programOn", attrBool(a, "sendProgram", true));
            continue;
        }
        if (tag == QLatin1String("surface")) { seenSurface = true; gridOn = attrBool(a, "grid", true); nextId = attrInt(a, "nextId", 1); continue; }
        CcPanel::Desc d;
        d.cc = 1; d.rmin = 0; d.rmax = 127; d.def = 0; d.val = 0; d.absolute = false;
        if      (tag == QLatin1String("knob"))   d.kind = CcControl::Knob;
        else if (tag == QLatin1String("slider")) d.kind = CcControl::SliderSwitch;
        else if (tag == QLatin1String("push"))   d.kind = CcControl::PushSwitch;
        else if (tag == QLatin1String("label"))  d.kind = CcControl::Label;
        else if (tag == QLatin1String("line"))   d.kind = CcControl::Line;
        else { qWarning("config: unknown element <%s> skipped", qPrintable(tag)); continue; }
        d.id = attrInt(a, "id", 0);
        d.name = a.hasAttribute("text") ? a.value("text").toString() : a.value("name").toString();
        d.cc = attrInt(a, "cc", 1);
        d.nrpn = attrBool(a, "nrpn", false); d.nrpnNum = attrInt(a, "param", 0); d.wide = attrBool(a, "wide", false);
        d.rmin = attrInt(a, "min", attrInt(a, "off", 0)); d.rmax = attrInt(a, "max", attrInt(a, "on", 127));
        d.def = attrInt(a, "default", 0); d.val = attrInt(a, "value", 0); d.absolute = attrBool(a, "absolute", false);
        d.positions = attrInt(a, "positions", 4); d.posNames = a.value("names").toString(); d.posValues = a.value("values").toString();
        d.posIdx = attrInt(a, "position", 0);
        d.latching = attrBool(a, "latched", false); d.capColor = capIndex(a.value("cap").toString());
        d.fontPx = attrInt(a, "size", 12); d.lineW = attrInt(a, "thickness", 1);
        d.x = attrInt(a, "x", 0); d.y = attrInt(a, "y", 0); d.w = attrInt(a, "w", 64); d.h = attrInt(a, "h", 80);
        st.append(d);
    }
    if (x.hasError()) { qWarning("config: %s", qPrintable(x.errorString())); return false; }
    if (seenSurface) {
        panel->restore(st);
        panel->setNextId(nextId);
        panel->clearHistory();
        if (auto* g = controllerWin_->findChild<QCheckBox*>(QStringLiteral("ccGrid"))) g->setChecked(gridOn);
    }
    if (win.isValid() && win.width() > 0 && win.height() > 0) controllerWin_->resize(win);
    ctrlSnaps_ = snaps;
    if (snapOnlyAct_) snapOnlyAct_->setChecked(onlyChanges);
    if (snapModAct_)  snapModAct_->setChecked(includeMod);
    ctrlRefreshSnapCells();
    return true;
}

void MainWindow::ctrlSetDirty(bool on)
{
    if (ctrlDirty_ == on) return;
    ctrlDirty_ = on;
    ctrlUpdateTitle();
}

void MainWindow::ctrlUpdateTitle()
{
    if (!controllerWin_) return;
    const QString name = ctrlConfigPath_.isEmpty() ? tr("Untitled") : QFileInfo(ctrlConfigPath_).fileName();
    controllerWin_->setWindowTitle(QStringLiteral("ADIOS Controller - %1%2").arg(name, ctrlDirty_ ? QStringLiteral("*") : QString()));
}

void MainWindow::ctrlNewConfig()
{
    if (!controllerWin_ || !ccSurface_) return;
    if (ctrlDirty_ && QMessageBox::question(controllerWin_, tr("New config"),
            tr("Discard the changes to the current config?")) != QMessageBox::Yes) return;
    auto* panel = static_cast<CcPanel*>(ccSurface_);
    if (auto* l = controllerWin_->findChild<QCheckBox*>(QStringLiteral("ccLocked"))) l->setChecked(false);
    panel->removeAll();
    panel->clearHistory();
    auto setSpin = [this](const char* n, int v) { if (auto* s = controllerWin_->findChild<QSpinBox*>(QLatin1String(n))) { const QSignalBlocker b(s); s->setValue(v); } };
    setSpin("bankMsb", 0); setSpin("bankLsb", 0); setSpin("program", 1);   // program shows 1 = sends 0
    for (const char* n : { "bankMsbOn", "bankLsbOn", "programOn" })
        if (auto* c = controllerWin_->findChild<QCheckBox*>(QLatin1String(n))) c->setChecked(false);
    if (auto* g = controllerWin_->findChild<QButtonGroup*>(QStringLiteral("chGroup")))
        if (auto* b = g->button(0)) b->setChecked(true);          // the keyboard back to channel 1
    if (modWheel_) {                                              // Mod back to rest, on screen only
        static_cast<Wheel*>(modWheel_)->setValue(0, false);
        if (modValLabel_) modValLabel_->setText(QStringLiteral("0"));
    }
    ctrlSnaps_ = QVector<CtrlSnap>(64);
    ctrlRefreshSnapCells();
    ctrlSnapDeselect();                                           // every slot is empty: none is selected
    if (snapOnlyAct_) snapOnlyAct_->setChecked(true);
    if (snapModAct_)  snapModAct_->setChecked(false);
    ctrlConfigPath_.clear();
    ctrlDirty_ = false;
    ctrlUpdateTitle();
}

// ---- MIDI Input: the wire drives the surface ------------------------------------
// With MIDI Input on, an incoming Control Change - or an NRPN run - lands on the
// controls that carry that address, on screen only: nothing is sent back. Omni On
// listens on every channel, Omni Off on one. Forward re-sends what comes in, except
// SysEx, which is Studio's own dialogue with the core.
void MainWindow::ctrlMidiIn(const adios::Bytes& msg)
{
    if (!controllerWin_ || !ccSurface_ || !midiInChk_ || !midiInChk_->isChecked() || msg.empty()) return;
    const uint8_t st = msg[0];
    if (fwdChk_ && fwdChk_->isChecked() && st != 0xf0) sendRaw(msg);
    if (msg.size() != 3 || (st & 0xf0) != 0xb0) return;                  // only CC drive the surface
    const int ch = st & 0x0f;
    // Omni off means one channel only: the one named in the box, or the keyboard's own
    // when the box says "As Output".
    if (omniChk_ && !omniChk_->isChecked() && inChanBox_) {
        const int idx = inChanBox_->currentIndex();
        if (ch != (idx == 0 ? kbChannel_ : idx - 1)) return;
    }
    const int cc = msg[1], v = msg[2];
    auto* panel = static_cast<CcPanel*>(ccSurface_);
    NrpnRun& r = nrpnCtl_;
    if (cc == 99) { r = NrpnRun(); r.ch = ch; r.msb = v; return; }
    if (cc == 98) { if (r.ch == ch && r.msb >= 0) r.lsb = v; return; }
    if (cc == 6 || cc == 38) {
        if (r.ch != ch || r.msb < 0 || r.lsb < 0) return;
        const int param = r.msb * 128 + r.lsb;
        if (cc == 6) r.value = v; else r.value = (r.value << 7) | v;
        bool moved = false;
        for (CcControl* c : panel->controls())
            if (c->nrpn && c->nrpnNum == param && (cc == 6 ? !c->wide : c->wide)) moved = c->applyMidi(r.value) || moved;
        if (moved) ctrlSnapTouched();                      // the screen has left the lit cell behind
        return;
    }
    bool moved = false;
    for (CcControl* c : panel->controls())
        if (!c->nrpn && !c->isDeco() && c->cc == cc && !c->blocked()) moved = c->applyMidi(v) || moved;
    if (moved) ctrlSnapTouched();
}

// ---- snapshots ---------------------------------------------------------------
void MainWindow::ctrlRefreshSnapCells()
{
    auto* g = controllerWin_ ? controllerWin_->findChild<QButtonGroup*>(QStringLiteral("snapGroup")) : nullptr;
    if (!g) return;
    for (QAbstractButton* b : g->buttons()) {
        const bool used = ctrlSnaps_.value(g->id(b)).used;
        if (b->property("used").toBool() == used && b->property("used").isValid()) continue;
        b->setProperty("used", used);                     // the stylesheet keys on it
        b->style()->unpolish(b); b->style()->polish(b);
    }
}

// The group is exclusive, so "none of them checked" has to be asked for explicitly.
void MainWindow::ctrlSnapDeselect()
{
    auto* g = controllerWin_ ? controllerWin_->findChild<QButtonGroup*>(QStringLiteral("snapGroup")) : nullptr;
    if (!g) return;
    QAbstractButton* b = g->checkedButton();
    if (!b) return;
    g->setExclusive(false);
    b->setChecked(false);
    g->setExclusive(true);
}

void MainWindow::ctrlStoreSnapshot(int n)
{
    if (!controllerWin_ || !ccSurface_ || n < 0 || n >= ctrlSnaps_.size()) return;
    auto spin = [this](const char* k) { auto* s = controllerWin_->findChild<QSpinBox*>(QLatin1String(k)); return s ? s->value() : 0; };
    CtrlSnap sn;
    sn.used = true;
    sn.msb = spin("bankMsb"); sn.lsb = spin("bankLsb"); sn.program = spin("program");
    for (CcControl* c : static_cast<CcPanel*>(ccSurface_)->controls())
        if (!c->isDeco()) sn.values.insert(c->id, c->stateValue());
    if (modWheel_) sn.mod = static_cast<Wheel*>(modWheel_)->value();
    ctrlSnaps_[n] = sn;
    if (auto* g = controllerWin_->findChild<QButtonGroup*>(QStringLiteral("snapGroup")))
        if (auto* b = g->button(n)) b->setChecked(true);
    ctrlRefreshSnapCells();
    ctrlSetDirty(true);
}

void MainWindow::ctrlClearSnapshot(int n)
{
    if (n < 0 || n >= ctrlSnaps_.size() || !ctrlSnaps_[n].used) return;
    ctrlSnaps_[n] = CtrlSnap();
    ctrlRefreshSnapCells();
    ctrlSetDirty(true);
}

void MainWindow::ctrlClearAllSnapshots()
{
    bool any = false;
    for (const CtrlSnap& s : ctrlSnaps_) any = any || s.used;
    if (!any) return;
    if (QMessageBox::question(controllerWin_, tr("Clear All"), tr("Clear all 64 snapshots?")) != QMessageBox::Yes) return;
    ctrlSnaps_ = QVector<CtrlSnap>(64);
    ctrlRefreshSnapCells();
    ctrlSetDirty(true);
}

// Recall: the screen AND the wire, in the agreed order - Bank/Program, then the
// surface, then Mod if the option says so. With "Send only changes" a value equal
// to what is already there is not sent again.
void MainWindow::ctrlRecallSnapshot(int n)
{
    if (!controllerWin_ || !ccSurface_ || n < 0 || n >= ctrlSnaps_.size()) return;
    const CtrlSnap& sn = ctrlSnaps_[n];
    if (!sn.used) return;                                 // an empty cell only selects
    const bool onlyChanges = snapOnlyAct_ && snapOnlyAct_->isChecked();
    // A recall writes every value the slot holds. Without this the first one written
    // would report a change and put the cell out the instant it came on.
    ctrlSnapApplying_ = true;

    // 1. Bank / Program: the fields take the values, Send does the sending - so the
    //    ticks decide what goes out, as they do for a hand-pressed Send.
    bool bankChanged = false;
    auto setSpin = [this, &bankChanged](const char* k, int v) {
        auto* s = controllerWin_->findChild<QSpinBox*>(QLatin1String(k));
        if (!s) return;
        if (s->value() != v) { const QSignalBlocker b(s); s->setValue(v); bankChanged = true; }
    };
    setSpin("bankMsb", sn.msb); setSpin("bankLsb", sn.lsb); setSpin("program", sn.program);
    if (bankChanged || !onlyChanges)
        if (auto* btn = controllerWin_->findChild<QPushButton*>(QStringLiteral("bankSend")); btn && btn->isEnabled()) btn->click();

    // 2. The surface, object by object, by id.
    for (CcControl* c : static_cast<CcPanel*>(ccSurface_)->controls()) {
        if (c->isDeco() || !sn.values.contains(c->id)) continue;
        const int v = sn.values.value(c->id);
        const bool changed = (c->stateValue() != v);
        c->applyState(v, changed || !onlyChanges);
    }

    // 3. Mod, optional.
    if (snapModAct_ && snapModAct_->isChecked() && modWheel_) {
        auto* w = static_cast<Wheel*>(modWheel_);
        if (w->value() != sn.mod || !onlyChanges) w->setValue(sn.mod, true);
    }
    ctrlSnapApplying_ = false;
}

void MainWindow::ctrlOpenConfig(const QString& given)
{
    if (!controllerWin_) return;
    QString path = given;
    if (path.isEmpty()) {
        const QString dir = QSettings().value("ctrl/configDir").toString();
        path = QFileDialog::getOpenFileName(controllerWin_, tr("Open config"), dir, tr("ADIOS Controller config (*.xml)"));
        if (path.isEmpty()) return;
    }
    if (ctrlDirty_ && QMessageBox::question(controllerWin_, tr("Open config"),
            tr("Discard the changes to the current config?")) != QMessageBox::Yes) return;
    if (!ctrlReadConfig(path)) {
        QMessageBox::warning(controllerWin_, tr("Open config"), tr("Could not read %1").arg(path));
        return;
    }
    QSettings().setValue("ctrl/configDir", QFileInfo(path).absolutePath());
}

bool MainWindow::ctrlSaveConfig(bool saveAs)
{
    if (!controllerWin_) return false;
    QString path = ctrlConfigPath_;
    if (saveAs || path.isEmpty()) {
        const QString dir = QSettings().value("ctrl/configDir").toString();
        path = QFileDialog::getSaveFileName(controllerWin_, tr("Save config"), dir, tr("ADIOS Controller config (*.xml)"));
        if (path.isEmpty()) return false;
        if (!path.endsWith(QLatin1String(".xml"), Qt::CaseInsensitive)) path += QLatin1String(".xml");
    }
    if (!ctrlWriteConfig(path)) {
        QMessageBox::warning(controllerWin_, tr("Save config"), tr("Could not write %1").arg(path));
        return false;
    }
    ctrlConfigPath_ = path;
    QSettings().setValue("ctrl/configDir", QFileInfo(path).absolutePath());
    ctrlAddRecent(path);
    ctrlDirty_ = false;
    ctrlUpdateTitle();
    return true;
}

void MainWindow::ctrlAddRecent(const QString& path)
{
    QSettings s;
    QStringList rec = s.value("ctrl/recent").toStringList();
    rec.removeAll(path); rec.prepend(path);
    while (rec.size() > 8) rec.removeLast();
    s.setValue("ctrl/recent", rec);
    ctrlRebuildRecent();
}

void MainWindow::ctrlRebuildRecent()
{
    if (!ctrlRecentMenu_) return;
    ctrlRecentMenu_->clear();
    const QStringList rec = QSettings().value("ctrl/recent").toStringList();
    for (const QString& p : rec) {
        QAction* a = ctrlRecentMenu_->addAction(QFileInfo(p).fileName());
        a->setToolTip(p);
        connect(a, &QAction::triggered, this, [this, p] { ctrlOpenConfig(p); });
    }
    ctrlRecentMenu_->setEnabled(!rec.isEmpty());
}

// Reset Layout: re-check every View toggle (show all panels, columns and the
// filter) and put the three splitters back to the first-launch defaults.
void MainWindow::resetLayout()
{
    for (QAction* a : viewToggles_)                   // Decode NRPN is a reading, not layout: left alone
        if (!a->objectName().endsWith(QLatin1String("DecodeNRPN"))) a->setChecked(true);
    syncMonSplit();
    resize(960, 860);                                   // default main-window size
    // Big second value = "give it the rest": QSplitter clamps to what is left,
    // so these hold regardless of the (just-changed) window height.
    ui_->rowsSplit->setSizes({220, 1 << 20});
    ui_->colsSplit->setSizes({300, 1 << 20});
    ui_->monSplit->setSizes({1 << 20, 1 << 20});        // equal halves
}

// The monitor row shows while either monitor is enabled. Hiding BOTH removes the
// row and SHRINKS THE WINDOW by its height, so Device Info + Terminal keep their
// current size (the window gets smaller, they do not grow); bringing a monitor
// back grows the window again. Read the "Show" toggles, not isVisible(): a child
// of a hidden parent reports invisible, which would wedge the re-show.
void MainWindow::syncMonSplit()
{
    const bool any = (inShowAct_  && inShowAct_->isChecked())
                  || (outShowAct_ && outShowAct_->isChecked());
    if (any != ui_->monSplit->isHidden()) return;   // already in the wanted state
    if (!any) {
        monRowH_ = ui_->monSplit->height() + ui_->rowsSplit->handleWidth();
        ui_->monSplit->setVisible(false);
        resize(width(), qMax(minimumSizeHint().height(), height() - monRowH_));
    } else {
        ui_->monSplit->setVisible(true);
        resize(width(), height() + monRowH_);
    }
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
        c->setNeutral(true);                     // the general theme: no red/green state colours
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
        mcb->setNeutral(true);
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

    filterHead_ = new QWidget;
    QWidget* outerHead = filterHead_;
    auto* oh = new QHBoxLayout(outerHead); oh->setContentsMargins(0, 0, 0, 0); oh->setSpacing(4);
    auto* flabel = new QLabel("Filter"); flabel->setObjectName("filterLabel");
    oh->addWidget(filterBtn_); oh->addWidget(flabel); oh->addStretch();

    auto* lay = qobject_cast<QVBoxLayout*>(ui_->inGroup->layout());
    lay->insertWidget(0, outerHead);
    lay->insertWidget(1, filterPanel_);

    // The Output monitor gets an "Apply Filter" toggle, level with the Input's
    // "Filter" header. On, the OUT side reuses the very same filter settings.
    auto* applyOut = new FilterCheckBox("Apply Filter");
    applyFilterBox_ = applyOut;           // View's Filter toggle hides it with the filter
    applyOut->setNeutral(true);           // gray border when off (this box only), not red
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
    if (!cmd.isEmpty() && (cmdHistory_.isEmpty() || cmdHistory_.last() != cmd))
        cmdHistory_.append(cmd);
    histIdx_ = cmdHistory_.size();   // one past the end = the fresh empty line
    sysexBox_->clear();
}

bool MainWindow::eventFilter(QObject* o, QEvent* e)
{
    if (o == sysexBox_ && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Up) {          // older
            if (!cmdHistory_.isEmpty()) {
                if (histIdx_ > 0) --histIdx_;
                sysexBox_->setText(cmdHistory_.value(histIdx_));
            }
            return true;
        }
        if (ke->key() == Qt::Key_Down) {        // newer, past the end = empty line
            if (histIdx_ < cmdHistory_.size()) ++histIdx_;
            sysexBox_->setText(histIdx_ < cmdHistory_.size() ? cmdHistory_.at(histIdx_) : QString());
            return true;
        }
    }
    return QWidget::eventFilter(o, e);
}

void MainWindow::chooseHex()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Firmware .hex"), lastDir_,
                                             tr("Intel HEX (*.hex);;All files (*)"));
    if (!f.isEmpty()) {
        int i = hexPath_->findText(f);
        if (i >= 0) hexPath_->removeItem(i);   // an existing entry moves to the top
        hexPath_->insertItem(0, f);
        hexPath_->setCurrentIndex(0);
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
        QString("== upload %1 ==").arg(QFileInfo(hexPath_->currentText()).fileName())));
    uploader_->start(hexPath_->currentText());
}
