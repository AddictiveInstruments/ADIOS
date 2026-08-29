// ADIOS Studio - a focused clone of MIOS Studio's main window: pick the MIDI
// ports, read the connected board, upload firmware, watch the traffic, talk
// SysEx. NONE of the Tools menu's device applications - just the workbench.
//
// One MIDI In and one MIDI Out are shared by every panel. Incoming messages
// are marshalled off the MIDI thread onto a queue, drained by a timer, then
// routed to the monitor, the terminal and the uploader in turn.

#pragma once
#include <QElapsedTimer>
#include <QWidget>
#include <cstdint>
#include <mutex>
#include <vector>

#include "../midi/midi.h"

class QComboBox;
class QSpinBox;
class QPushButton;
class QLineEdit;
class QLabel;
class QProgressBar;
class QCheckBox;
class QListWidget;
class QTimer;
class Uploader;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

private slots:
    void toggleConnect();
    void chooseHex();
    void doUpload();
    void sendSysex();
    void onTick();

private:
    struct RxMsg { adios::Bytes bytes; uint64_t t_us; };

    void onMidiIn(const adios::Bytes& msg, uint64_t t_us);   // MIDI thread
    void routeIn(const adios::Bytes& msg, uint64_t t_us);    // GUI thread
    void monitorLine(bool out, const adios::Bytes& msg);
    bool sendRaw(const adios::Bytes& msg);                   // GUI thread, echoes to monitor
    void setConnected(bool on);
    QString nowStamp();

    // --- ports ---
    QComboBox*   inBox_  = nullptr;
    QComboBox*   outBox_ = nullptr;
    QSpinBox*    idBox_  = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QPushButton* queryBtn_ = nullptr;
    QLabel*      link_ = nullptr;

    // --- device info ---
    QListWidget* devInfo_ = nullptr;

    // --- upload ---
    QLineEdit*    hexPath_ = nullptr;
    QPushButton*  browseBtn_ = nullptr;
    QPushButton*  uploadBtn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QListWidget*  uploadStatus_ = nullptr;

    // --- terminal / sysex ---
    QListWidget* term_ = nullptr;
    QLineEdit*   sysexBox_ = nullptr;
    QPushButton* sendBtn_ = nullptr;

    // --- monitor ---
    QListWidget* monIn_ = nullptr;
    QListWidget* monOut_ = nullptr;
    QCheckBox*   muteRt_ = nullptr;

    adios::In   in_;
    adios::Out  out_;
    std::mutex  outGuard_;
    bool        connected_ = false;

    Uploader*   uploader_ = nullptr;

    std::mutex           rxMx_;
    std::vector<RxMsg>   rxQueue_;
    QTimer*              timer_ = nullptr;
    QElapsedTimer        clock_;
};
