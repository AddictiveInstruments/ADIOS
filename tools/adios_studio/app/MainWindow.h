// ADIOS Studio - a focused clone of MIOS Studio's main window: pick the MIDI
// ports, read the connected board, upload firmware, watch the traffic, talk
// SysEx. NONE of the Tools menu's device applications - just the workbench.
//
// One MIDI In and one MIDI Out are shared by every panel. Incoming messages
// are marshalled off the MIDI thread onto a queue, drained by a timer, then
// routed to the monitor, the terminal and the uploader in turn.

#pragma once
#include <QElapsedTimer>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <cstdint>
#include <mutex>
#include <vector>

#include "../midi/midi.h"

class QComboBox;
class QSpinBox;
class QPushButton;
class QToolButton;
class QLineEdit;
class QLabel;
class QProgressBar;
class QCheckBox;
class QAction;
class QListWidget;
class QListWidgetItem;
class QMenuBar;
class QTimer;
class Uploader;
namespace Ui { class MainWindow; }

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;   // Up/Down history in the terminal input

private slots:
    void connectPorts();   // auto: opens the selected In/Out, no Connect button
    void chooseHex();
    void doUpload();
    void sendSysex();
    void onTick();

private:
    void saveSettings();
    void restoreSettings();
    struct RxMsg { adios::Bytes bytes; uint64_t t_us; };

    void onMidiIn(const adios::Bytes& msg, uint64_t t_us);   // MIDI thread
    void routeIn(const adios::Bytes& msg, uint64_t t_us);    // GUI thread
    void monitorLine(bool out, const adios::Bytes& msg);
    bool sendRaw(const adios::Bytes& msg);                   // GUI thread, echoes to monitor
    void setConnected(bool on);

    // View menu (menu bar built in the ctor). Each monitor line keeps its three
    // parts so the column toggles can rebuild what is already on screen.
    void buildMenus();
    void openController();             // Tools > Controller: open (or raise) its own window
    void resetLayout();               // Reset Layout: re-check every View toggle + default splitters
    void syncMonSplit();              // hide the monitor row entirely when both monitors are off
    struct MonCols { bool stamp = true, decode = true, raw = true; };
    void rebuildMonRow(QListWidgetItem* it, const MonCols& c);
    void rerenderMonitor(QListWidget* list, const MonCols& c);
    MonCols inCols_, outCols_;
    QMenuBar* menuBar_ = nullptr;
    QList<QAction*> viewToggles_;     // every checkable View action, for Reset Layout
    QAction* inShowAct_ = nullptr;    // the two monitor "Show" toggles, read by syncMonSplit
    QAction* outShowAct_ = nullptr;
    int      monRowH_ = 0;            // monitor-row height removed while both are hidden
    QWidget* controllerWin_ = nullptr;  // Tools > Controller, an independent window
    QWidget* ccSurface_ = nullptr;      // the Control Change CcPanel (saved on close)
    int      kbChannel_ = 0;            // keyboard MIDI TX channel (0..15)
    QString nowStamp();

    // Collapsible message filter above the Input monitor list.
    void buildInputFilter();
    bool passesInFilter(const adios::Bytes& msg) const;
    QList<QPair<QString, bool*>> filterFields();   // named on/off flags, for save/restore

    // What the Input monitor shows, by MIDI message type. Defaults mirror the
    // old behaviour: everything except Real Time (clock/realtime hidden).
    struct InFilter {
        bool voice = true;
        bool noteOnOff = true, aftertouch = true, control = true,
             program = true, chanPressure = true, pitch = true;
        uint16_t channelMask = 0xFFFF;   // one bit per MIDI channel 1..16
        bool sysCommon = true, timeCode = true, songPos = true, songSel = true, tuneReq = true;
        bool realTime = false, clock = false, startStop = false, activeSense = false, reset = false;
        bool sysex = true, invalid = true;
    } filter_;
    QToolButton* filterBtn_ = nullptr;    // the triangle header
    QWidget*     filterHead_ = nullptr;   // triangle + "Filter" label row (View hides it whole)
    QWidget*     filterPanel_ = nullptr;  // the checkbox grid it shows/hides
    QCheckBox*   applyFilterBox_ = nullptr;  // "Apply Filter" (Output); hidden with the Filter
    QToolButton* triVoice_ = nullptr;     // category triangles, kept for save/restore
    QToolButton* triSys_ = nullptr;
    QToolButton* triRt_ = nullptr;

    Ui::MainWindow* ui_ = nullptr;   // the static layout, from MainWindow.ui

    // --- ports --- (pointers grabbed from ui_ after setupUi)
    QComboBox*   inBox_  = nullptr;
    QComboBox*   outBox_ = nullptr;
    QSpinBox*    idBox_  = nullptr;
    QPushButton* queryBtn_ = nullptr;   // labelled "Ping"

    // --- device info ---
    QListWidget* devInfo_ = nullptr;

    // --- upload ---
    QComboBox*    hexPath_ = nullptr;
    QPushButton*  browseBtn_ = nullptr;
    QPushButton*  uploadBtn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QListWidget*  uploadStatus_ = nullptr;

    // --- terminal / sysex ---
    QListWidget* term_ = nullptr;
    QLineEdit*   sysexBox_ = nullptr;
    QPushButton* sendBtn_ = nullptr;
    QCheckBox*   clearOnGreeting_ = nullptr;   // "Clear on Greeting" toggle in the terminal
    QStringList  cmdHistory_;        // terminal input history, walked with Up/Down
    int          histIdx_ = 0;
    bool         termBarOpen_ = false;   // a live "\r" progress bar owns the last line

    // --- monitor ---
    QListWidget* monIn_ = nullptr;
    QListWidget* monOut_ = nullptr;
    bool         applyOutFilter_ = false;   // reuse the Input filter on the Output side
    uint8_t      lastStatusIn_ = 0;         // running-status tracking, per direction
    uint8_t      lastStatusOut_ = 0;

    adios::In   in_;
    adios::Out  out_;
    std::mutex  outGuard_;
    bool        connected_ = false;
    bool        wasUpload_ = false;   // tells the shared finished() an upload just ran

    Uploader*   uploader_ = nullptr;

    std::mutex           rxMx_;
    std::vector<RxMsg>   rxQueue_;
    QTimer*              timer_ = nullptr;
    QElapsedTimer        clock_;
    QString              lastDir_;   // remembered browse folder
};
