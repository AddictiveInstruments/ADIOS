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
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QButtonGroup>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
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
    Role_Dim    = Qt::UserRole,      // bool: draw the status byte dimmer NOW
    Role_HexPos = Qt::UserRole + 1,  // int:  its offset in the current text
    Role_Stamp  = Qt::UserRole + 2,  // QString: timestamp column
    Role_Label  = Qt::UserRole + 3,  // QString: decoded column
    Role_Hex    = Qt::UserRole + 4,  // QString: raw-data column
    Role_Run    = Qt::UserRole + 5,  // bool: this line repeats running status
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
            p.setBrush(m == cur_ ? QColor(0x4a, 0x7a, 0xb8) : QColor(0xe9, 0xed, 0xf3));
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
            p.setBrush(bm == cur_ ? QColor(0x4a, 0x7a, 0xb8) : QColor(0x20, 0x25, 0x2f));
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
    // Velocity from WHERE on the key it was pressed: lower = louder. A few-pixel
    // band at the bottom reaches 127 easily; the top never yields 0 (vel 0 = Note Off).
    int velFromY(int note, int y) const {
        const int h = height(), bh = h * 62 / 100;
        const int kh = isBlack(note) ? bh : h;
        const double f = qBound(0.0, double(y) / qMax(1, kh - 6), 1.0);
        return 1 + qRound(f * 126);   // 1..127
    }
    void press(int note, int y) { if (note < 0 || note == cur_) return; release(); cur_ = note; if (onNoteOn) onNoteOn(note, velFromY(note, y)); update(); }
    void release() { if (cur_ < 0) return; const int n = cur_; cur_ = -1; if (onNoteOff) onNoteOff(n); update(); }
    QVector<int> allWhite_;
    int scroll_ = 0, center_ = 0, cur_ = -1;
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
    explicit Wheel(Type t, QWidget* parent = nullptr) : QWidget(parent), type_(t) {
        setFixedSize(TW, H);
        val_ = (type_ == Pitch) ? 8192 : 0;
        buildHighlight();
    }
    int  value() const { return val_; }
    void setRidges(bool on) { ridges_ = on; update(); }
    void set7bit(bool on) { bit7_ = on; quantizeIf(); update(); if (onChange) onChange(val_); }   // Pitch: coarse 7-bit (LSB 0)
    static constexpr int TW = 38;   // total widget width (narrow body + tick margins)

protected:
    void paintEvent(QPaintEvent*) override {
        constexpr double PI = 3.14159265358979323846;
        const double cx = OX + W / 2.0, cy = H / 2.0, R = H / 2.0 - 3.0,
                     RX = W / 2.0 - 3.0, MAXRY = W / 2.0 - 3.0;   // margin scaled with width -> same ellipse proportion
        const double STEP = PI / 22.0, SPAN = PI / 2.0 * 0.82, EDGE = PI / 2.0 - 0.03;
        const double roll = (type_ == Pitch) ? (val_ - 8192) / 8192.0 * SPAN
                                             : (val_ / 127.0 - 0.5) * 2.0 * SPAN;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath path = bodyPath();
        p.save();
        p.setClipPath(path);
        p.fillRect(QRectF(OX, 0, W, H), QColor(0x0c, 0x0d, 0x11));
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
        p.fillRect(QRectF(OX, 0, W, H), hg);
        p.drawImage(QPointF(OX, 0), highlight_);              // sine-windowed vertical reflet
        QLinearGradient vg(0, 0, 0, H);                       // extra darkening at the two ends
        vg.setColorAt(0.0, QColor(0, 0, 0, 230)); vg.setColorAt(0.11, QColor(0, 0, 0, 0));
        vg.setColorAt(0.89, QColor(0, 0, 0, 0));  vg.setColorAt(1.0, QColor(0, 0, 0, 230));
        p.fillRect(QRectF(OX, 0, W, H), vg);
        const double ym = cy - R * std::sin(roll), ry = std::max(1.0, MAXRY * std::cos(roll));
        QLinearGradient mg(0, ym - ry, 0, ym + ry);           // opaque marker, reflet at the bottom
        mg.setColorAt(0.0, QColor(0x06, 0x06, 0x09)); mg.setColorAt(0.5, QColor(0x0c, 0x0d, 0x0f));
        mg.setColorAt(0.78, QColor(0x2b, 0x2d, 0x31)); mg.setColorAt(1.0, QColor(0x45, 0x48, 0x4d));
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
        if (type_ == Pitch) { tick(cy, 6); tick(CH + 2, 4); tick(H - CH - 2, 4); }   // long tick = rest
        else                { tick(H - CH - 2, 6); tick(cy, 4); tick(CH + 2, 4); }   // mod rests low
    }
    void mousePressEvent(QMouseEvent* e) override { pressed_ = true; stopSpring(); fromY(e->position().y()); }
    void mouseMoveEvent(QMouseEvent* e) override  { if (pressed_) fromY(e->position().y()); }
    void mouseReleaseEvent(QMouseEvent*) override { if (!pressed_) return; pressed_ = false; if (type_ == Pitch) startSpring(); }

private:
    static constexpr int H = 126, W = 22, OX = 8, CH = 8;   // width cut ~1/3 (33->22), ends scaled to match

    QPainterPath bodyPath() const {                           // straight sides, full-width elliptical ends
        const double rx = W / 2.0 - 0.5, ry = CH, cxw = OX + W / 2.0;
        const QRectF topR(cxw - rx, 0.5, 2 * rx, 2 * ry);
        const QRectF botR(cxw - rx, H - 0.5 - 2 * ry, 2 * rx, 2 * ry);
        QPainterPath p;
        p.arcMoveTo(topR, 180);
        p.arcTo(topR, 180, -180);                             // left -> top -> right
        p.lineTo(cxw + rx, H - 0.5 - ry);
        p.arcTo(botR, 0, -180);                               // right -> bottom -> left
        p.closeSubpath();
        return p;
    }
    void buildHighlight() {                                   // built once: it does not move as the wheel rolls
        constexpr double PI = 3.14159265358979323846;
        highlight_ = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
        highlight_.fill(Qt::transparent);
        QPainter g(&highlight_);
        QLinearGradient band(0, 0, W, 0);
        band.setColorAt(0.0, QColor(255, 255, 255, 0)); band.setColorAt(0.5, QColor(255, 255, 255, 43));
        band.setColorAt(1.0, QColor(255, 255, 255, 0));
        g.fillRect(QRectF(0, 0, W, H), band);
        g.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QLinearGradient vmask(0, 0, 0, H);
        for (int i = 0; i <= 24; ++i) {
            const double q = i / 24.0;
            vmask.setColorAt(q, QColor(0, 0, 0, int(std::lround(255 * std::sin(PI * q)))));
        }
        g.fillRect(QRectF(0, 0, W, H), vmask);
    }
    void quantizeIf() { if (bit7_ && type_ == Pitch) val_ = qBound(0, qRound(val_ / 128.0), 127) * 128; }   // snap to MSB grid
    void fromY(double y) {
        double t = (y - 9.0) / (H - 18.0);
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
    int  val_ = 8192;
    bool ridges_ = true, pressed_ = false, bit7_ = false;
    QImage highlight_;
    QPointer<QVariantAnimation> spring_;
};

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
        for (QAction* a : controllerWin_->findChildren<QAction*>())   // View toggles, keyed by objectName
            if (!a->objectName().isEmpty()) s.setValue("ctrl/" + a->objectName(), a->isChecked());
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
    const bool dim = c.raw && it->data(Role_Run).toBool();
    it->setData(Role_Dim, dim);
    it->setData(Role_HexPos, dim ? int(line.length() - hex.length()) : -1);
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
        controllerWin_->setWindowTitle(tr("Controller"));
        // Come back at the size/position it had last time; first ever open matches
        // the main window. (Width is still capped below to where all keys show.)
        const QByteArray cg = QSettings().value("ctrlGeometry").toByteArray();
        if (!cg.isEmpty()) controllerWin_->restoreGeometry(cg);
        else               controllerWin_->resize(size());

        auto* col = new QVBoxLayout(controllerWin_);
        col->setContentsMargins(12, 12, 12, 12);   // like the main window's root layout
        col->setSpacing(0);
        col->addStretch(1);                    // empty room above, for later controls

        auto* grp = new QGroupBox(tr("MIDI Keyboard"));
        auto* gl  = new QVBoxLayout(grp); gl->setContentsMargins(0, 0, 0, 0); gl->setSpacing(0);   // QSS gives the 15px padding
        const int TOP_GAP = 6;   // gap under the Channel row (mirrored in Left Hand under Bend/Mod)

        // "Channel:" + 16 single-select cells (blue), the keyboard's TX channel. Same
        // 18x18 and spacing 3 as the filter's channel matrix, but a single row.
        auto* chRow = new QHBoxLayout; chRow->setContentsMargins(0, 0, 0, 0); chRow->setSpacing(3);
        chRow->addWidget(new QLabel(tr("Channel:")));
        chRow->addSpacing(6);
        auto* chGroup = new QButtonGroup(controllerWin_);
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
        noNoteOff->setNeutral(true);                               // same gray box + white tick as "Apply Filter"
        chRow->addWidget(noNoteOff);
        connect(chGroup, &QButtonGroup::idToggled, this, [this](int id, bool on) { if (on) kbChannel_ = id; });
        gl->addLayout(chRow);
        gl->addSpacing(TOP_GAP);

        auto* keybed = new QWidget; keybed->setFixedHeight(4);   // thin accent strip above the keys
        keybed->setStyleSheet("background:#4a7ab8; border-radius:2px;");
        gl->addWidget(keybed);
        gl->addSpacing(2);

        auto* kb = new PianoKeyboard;
        kb->setFixedHeight(120);
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
        auto* lblBend = mkCentered(tr("Bend"), "wheelLbl", 18);      // 18 = the Channel row height
        auto* lblMod  = mkCentered(tr("Mod"),  "wheelLbl", 18);
        labRow->addWidget(lblBend); labRow->addWidget(lblMod);
        lgl->addLayout(labRow);
        lgl->addSpacing(TOP_GAP);
        auto* whRow = new QHBoxLayout; whRow->setContentsMargins(0, 0, 0, 0); whRow->setSpacing(WHEEL_GAP);
        auto* pitch = new Wheel(Wheel::Pitch);   // 49 x 126, top aligns with the blue strip
        auto* mod   = new Wheel(Wheel::Mod);
        whRow->addWidget(pitch); whRow->addWidget(mod);
        lgl->addLayout(whRow);
        lgl->addSpacing(10);
        auto* valRow = new QHBoxLayout; valRow->setContentsMargins(0, 0, 0, 0); valRow->setSpacing(WHEEL_GAP);
        auto* vBend = mkCentered(QStringLiteral("0"), "wheelVal", 14);   // 14 = the scrollbar row height
        auto* vMod  = mkCentered(QStringLiteral("0"), "wheelVal", 14);
        valRow->addWidget(vBend); valRow->addWidget(vMod);
        lgl->addLayout(valRow);

        pitch->onChange = [this, vBend](int v) {
            const int s = v - 8192;   // 0 at rest, signed offset otherwise
            vBend->setText(QString(s > 0 ? "+" : "") + QString::number(s));
            sendRaw({ uint8_t(0xE0 | kbChannel_), uint8_t(v & 0x7f), uint8_t((v >> 7) & 0x7f) });
        };
        mod->onChange = [this, vMod](int v) {
            vMod->setText(QString::number(v));
            sendRaw({ uint8_t(0xB0 | kbChannel_), uint8_t(1), uint8_t(v & 0x7f) });   // CC 1
        };

        auto* handsRow = new QHBoxLayout; handsRow->setContentsMargins(0, 0, 0, 0); handsRow->setSpacing(10);
        handsRow->addWidget(lhGrp);
        handsRow->addWidget(grp, 1);   // the keyboard group takes the slack
        col->addLayout(handsRow);

        // Menu bar: Layout (empty for now) and View. A plain QWidget has no native
        // menu bar, so the layout hosts it - same trick as the main window.
        auto* mb = new QMenuBar(controllerWin_);
        mb->addMenu(tr("Layout"));                       // empty placeholder (presets, later)
        auto* view = mb->addMenu(tr("View"));
        auto* lh = new StayOpenMenu(tr("Left Hand"), view); view->addMenu(lh);   // StayOpen: toggles don't close it
        auto addToggle = [](QMenu* m, const QString& t, const char* obj, bool on) {
            auto* a = m->addAction(t); a->setObjectName(obj); a->setCheckable(true); a->setChecked(on); return a;
        };
        connect(addToggle(lh, tr("Show"), "lhShow", true), &QAction::toggled, lhGrp, &QWidget::setVisible);
        connect(addToggle(lh, tr("Stripped"), "lhStripped", true), &QAction::toggled, this,
                [pitch, mod](bool on) { pitch->setRidges(on); mod->setRidges(on); });
        auto* bendM = new StayOpenMenu(tr("Bend"), lh); lh->addMenu(bendM);
        connect(addToggle(bendM, tr("Show"), "bendShow", true), &QAction::toggled, this,
                [lblBend, pitch, vBend](bool on) { lblBend->setVisible(on); pitch->setVisible(on); vBend->setVisible(on); });
        connect(addToggle(bendM, tr("7 bits"), "bend7bit", false), &QAction::toggled, pitch,
                [pitch](bool on) { pitch->set7bit(on); });
        auto* modM = new StayOpenMenu(tr("Mod"), lh); lh->addMenu(modM);
        connect(addToggle(modM, tr("Show"), "modShow", true), &QAction::toggled, this,
                [lblMod, mod, vMod](bool on) { lblMod->setVisible(on); mod->setVisible(on); vMod->setVisible(on); });
        col->setMenuBar(mb);

        // Restore the toggles saved at the last close (fires each handler -> the
        // panels come back hidden/shown, stripped, 7-bit exactly as left).
        QSettings cs;
        for (QAction* a : controllerWin_->findChildren<QAction*>()) {
            if (a->objectName().isEmpty()) continue;
            const QVariant v = cs.value("ctrl/" + a->objectName());
            if (v.isValid() && v.toBool() != a->isChecked()) a->setChecked(v.toBool());
        }

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

// Reset Layout: re-check every View toggle (show all panels, columns and the
// filter) and put the three splitters back to the first-launch defaults.
void MainWindow::resetLayout()
{
    for (QAction* a : viewToggles_) a->setChecked(true);
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
