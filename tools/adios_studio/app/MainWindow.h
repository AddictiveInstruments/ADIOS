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
class QButtonGroup;
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
    // `when` is stamped in the MIDI callback, not when the queue is drained: the two
    // monitors have to be comparable, or the delay they show is the display's and not
    // the wire's.
    struct RxMsg { adios::Bytes bytes; uint64_t t_us; QString when; };

    void onMidiIn(const adios::Bytes& msg, uint64_t t_us);   // MIDI thread
    void routeIn(const adios::Bytes& msg, uint64_t t_us, const QString& when = QString());   // GUI thread
    // `stamp` empty = stamp it now. Forward sends from the MIDI thread and the GUI
    // shows the line later, so it hands the time the bytes ACTUALLY left: otherwise
    // the monitor would report its own delay as the delay of the thru.
    void monitorLine(bool out, const adios::Bytes& msg, const QString& stamp = QString());
    // ONE rule for everything that leaves: the bytes go on the wire, and the monitor is
    // merely TOLD - it draws on its own timer, at its own (lower) priority. Nothing
    // that plays waits on something that only shows.
    bool sendRaw(const adios::Bytes& msg);                   // wire, then note it for the monitor
    bool sendWire(const adios::Bytes& msg, std::string* err = nullptr);   // ANY thread, no widget
    void echoToMonitor(const adios::Bytes& msg);             // ANY thread: queue, never draw
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
    struct CtrlSnap {
        bool used = false;
        int msb = 0, lsb = 0, program = 1, mod = 0;
        QMap<int, int> values;
        // The trigger address is WIRING, not content: it survives Clear and Clear All,
        // and only New config wipes it. -1 = none.
        int trigCh = -1, trigNote = -1;
        bool hasTrigger() const { return trigCh >= 0 && trigNote >= 0; }
    };
    QVector<CtrlSnap> ctrlSnaps_;
    // ---- Snapshot MIDI triggers ------------------------------------------------
    // A cell listens to a CHANNEL and a NOTE. Those notes reach it either through a
    // port of its own - strictly for this, never monitored, never forwarded, never
    // seen by the uploader - or, when there is none, through the main input. They do
    // NOT depend on "MIDI Input": these have their own switch.
    QCheckBox* snapTrigChk_ = nullptr;   // "Use MIDI Triggers", the master switch
    QCheckBox* snapAuxChk_  = nullptr;   // "Use Auxiliary Input Port"
    QComboBox* snapPortBox_ = nullptr;   // that port; the main input is greyed out in the list
    QCheckBox* snapOmniChk_ = nullptr;
    QSpinBox*  snapChanSp_  = nullptr;
    adios::In  snapIn_;                            // the auxiliary port itself
    std::mutex snapMx_;
    std::vector<adios::Bytes> snapQueue_;          // filled on ITS thread, drained by onTick
    void ctrlFillSnapPorts();                      // rebuild the list, grey the main input
    void ctrlGreyMainPorts();                      // and grey the trigger port in Studio's own
    void ctrlOpenSnapPort();                       // open or close it to match the settings
    void ctrlSnapTrigger(const adios::Bytes& msg); // GUI thread: match an address, recall
    void ctrlEditSnapshot(int n);                  // right-click > Parameters...
    // MIDI Learn. Armed by a dialog, disarmed by the FIRST message that is both valid
    // and not already taken - the hook returns true only then, and an address already
    // in use is simply ignored so the button stays lit and the wait goes on.
    std::function<bool(const adios::Bytes&)> learnHook_;
    bool learnFromAux_ = false;                    // which source that hook listens to
    bool ctrlLearnEat(const adios::Bytes& msg, bool aux);
    // Found once, at build time. These sit on the path every incoming message walks,
    // and findChild() rescans the whole window each time it is called.
    QButtonGroup* snapGroup_  = nullptr;
    QSpinBox*  bankMsbSp_  = nullptr; QSpinBox*  bankLsbSp_  = nullptr; QSpinBox*  pgmSp_  = nullptr;
    QCheckBox* bankMsbChk_ = nullptr; QCheckBox* bankLsbChk_ = nullptr; QCheckBox* pgmChk_ = nullptr;
    // Preferences window (Config > Preferences...): the ticks live there, as
    // children of the controller window so they persist with the others.
    QDialog*   prefsDlg_     = nullptr;
    QCheckBox* snapOnlyAct_  = nullptr; // "Snapshot Send only changes"
    QCheckBox* snapModAct_   = nullptr; // "Snapshot includes Mod"
    QCheckBox* midiInChk_    = nullptr; // "MIDI Input": incoming CC/NRPN drive the surface
    QCheckBox* omniChk_      = nullptr; // "MIDI Input Omni": ticked = any channel, else one
    QComboBox* inChanBox_    = nullptr; // that channel: index 0 = "As Output", which follows the
                                        // keyboard's own TX channel; 1..16 = that channel
    QCheckBox* fwdChk_       = nullptr; // "MIDI Forward": what comes in goes out again
    void ctrlMidiIn(const adios::Bytes& msg);   // GUI thread, from onTick
    QComboBox* fwdModeBox_   = nullptr; // "Forward": Config (only what this controller makes) / All
    QCheckBox* constVelChk_  = nullptr; // "Constant velocity": the keyboard ignores the press height
    QSpinBox*  constVelSp_   = nullptr; // that velocity, 1..127
    // The .cpp-local widgets the wire has to reach. Kept as QWidget* and cast at the
    // point of use, exactly as the Mod wheel already was.
    QWidget* modWheel_  = nullptr;      // the Mod wheel
    QWidget* bendWheel_ = nullptr;      // the Bend wheel
    QWidget* kbWidget_  = nullptr;      // the on-screen keyboard
    // ---- Forward, and why it does NOT live on the GUI thread ------------------
    // A thru that waits for the display timer is not a thru: it inherits the timer's
    // period and its jitter. So Forward runs in the MIDI input callback, the instant
    // the bytes land, and reads this picture of the rules instead of the widgets -
    // which belong to the other thread. The GUI rebuilds it whenever anything it
    // holds changes, which is rare next to the note traffic.
    struct FwdRules {
        bool on = false;                  // Forward on, and MIDI Input with it
        bool all = false;                 // false = Config
        bool omni = true;
        int  inCh = 0;                    // the channel listened to when Omni is off
        int  outCh = 0;                   // the keyboard's channel: what Config re-stamps onto
        bool bankMsb = false, bankLsb = false, program = false;
        uint32_t cc[4] = {0, 0, 0, 0};    // 128 bits: the CC numbers live on the surface
        std::vector<int> nrpn;            // sorted: the NRPN parameters on the surface
    };
    std::mutex fwdMx_;
    FwdRules   fwd_;
    void ctrlRefreshFwdRules();                  // GUI thread: rebuild the picture
    void ctrlForward(const adios::Bytes& msg);   // MIDI thread: decide, and send at once
    // The run held on the MIDI thread; that thread alone ever touches these.
    std::vector<adios::Bytes> fwdHold_;
    int fwdMsb_ = -1, fwdLsb_ = -1, fwdCh_ = -1;
    // What Forward already sent, waiting to be SHOWN in the Out monitor, each with the
    // time it left.
    std::mutex txMx_;
    std::vector<std::pair<adios::Bytes, QString>> txEcho_;
    void ctrlStoreSnapshot(int n);
    void ctrlRecallSnapshot(int n);
    void ctrlClearSnapshot(int n);
    void ctrlClearAllSnapshots();
    void ctrlRefreshSnapCells();
    // A lit cell means "the screen shows what this slot holds". Change any value the
    // slot holds and that stops being true, so the selection drops. A recall writes
    // those same values, hence the flag it raises while it works.
    void ctrlSnapDeselect();
    void ctrlSnapTouched() { if (!ctrlSnapApplying_) ctrlSnapDeselect(); }
    bool ctrlSnapApplying_ = false;
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

    // The screen is drained on one timer and the MONITOR on another. Moving a knob and
    // building a monitor line are not the same job: done in the same pass, the widgets
    // could not repaint until every line of the batch had been built, because a repaint
    // only happens once the handler RETURNS. That is why the keyboard lagged exactly as
    // the surface did - two unrelated drawings, one queue in front of both.
    QTimer*              monTimer_ = nullptr;
    std::vector<RxMsg>   monQueue_;      // GUI thread only: waiting to be SHOWN
    void onMonDrain();
    std::mutex           rxMx_;
    std::vector<RxMsg>   rxQueue_;
    QTimer*              timer_ = nullptr;
    QElapsedTimer        clock_;
    QString              lastDir_;   // remembered browse folder
};
