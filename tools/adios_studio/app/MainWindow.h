// ADIOS Studio - a focused clone of MIOS Studio's main window: pick the MIDI
// ports, read the connected board, upload firmware, watch the traffic, talk
// SysEx. NONE of the Tools menu's device applications - just the workbench.
//
// One MIDI In and one MIDI Out are shared by every panel. Incoming messages
// are marshalled off the MIDI thread onto a queue, drained by a timer, then
// routed to the monitor, the terminal and the uploader in turn.

#pragma once
class QCheckBox; class QComboBox; class QSpinBox; class QDialog;
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
    struct MonCols { bool stamp = true, decode = true, raw = true; bool nrpn = false; };   // nrpn: one line per run
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

    // Controller configuration: one XML file holding the layout, the Bank/Program
    // settings, the surface with its identified objects, and (later) the snapshots.
    QString ctrlConfigPath_;            // empty = untitled, the legacy QSettings state drives
    bool    ctrlDirty_ = false;
    QMenu*  ctrlRecentMenu_ = nullptr;
    void ctrlNewConfig();
    void ctrlOpenConfig(const QString& path = QString());
    bool ctrlSaveConfig(bool saveAs);
    bool ctrlWriteConfig(const QString& path);
    bool ctrlReadConfig(const QString& path);   // open a named file: load + bookkeeping
    bool ctrlLoadXml(const QString& path);      // just the loading, no bookkeeping
    QString ctrlSessionPath() const;            // the working copy, in the app data dir
    void ctrlSetDirty(bool on);
    void ctrlUpdateTitle();
    void ctrlAddRecent(const QString& path);
    void ctrlRebuildRecent();

    // Snapshots: 64 slots, each a picture of Bank/Program, every surface object's
    // state by id, and the Mod wheel. Stored in the config file with two options.
    struct CtrlSnap { bool used = false; int msb = 0, lsb = 0, program = 1, mod = 0; QMap<int, int> values; };
    QVector<CtrlSnap> ctrlSnaps_;
    // Preferences window (Config > Preferences...): the ticks live there, as
    // children of the controller window so they persist with the others.
    QDialog*   prefsDlg_     = nullptr;
    QCheckBox* snapOnlyAct_  = nullptr; // "Snapshot Send only changes"
    QCheckBox* snapModAct_   = nullptr; // "Snapshot includes Mod"
    QCheckBox* midiInChk_    = nullptr; // "MIDI Input": incoming CC/NRPN drive the surface
    QCheckBox* omniChk_      = nullptr; // "MIDI Input Omni": ticked = any channel, else one
    QSpinBox*  inChanSp_     = nullptr; // that channel, 1..16
    QCheckBox* fwdChk_       = nullptr; // "MIDI Forward": what comes in goes out again
    void ctrlMidiIn(const adios::Bytes& msg);   // GUI thread, from routeIn
    QWidget* modWheel_ = nullptr;       // the Mod wheel (a .cpp-local class, kept as a QWidget)
    void ctrlStoreSnapshot(int n);
    void ctrlRecallSnapshot(int n);
    void ctrlClearSnapshot(int n);
    void ctrlClearAllSnapshots();
    void ctrlRefreshSnapCells();
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
    // Decode NRPN: the CC#99/98/6[/38] run of an NRPN collapses into ONE monitor
    // line. The run in progress, per direction.
    struct NrpnRun {
        int ch = -1, msb = -1, lsb = -1, value = 0;
        QString raw;                    // the raw column: the messages of the run, "|"-separated
        QList<int> runOffsets;          // where, in raw, a RECREATED status byte sits (running status)
        QListWidgetItem* item = nullptr;
    };
    NrpnRun      nrpnIn_, nrpnOut_;
    NrpnRun      nrpnCtl_;              // an NRPN run arriving for the controller's surface

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
