// The upgrade window.
//
// One button: Close. There is no Update - a successful Query IS the go-ahead,
// so the operator never has to be told "now press the other one". Close is
// taken away the moment the machine's bootloader answers and given back when
// the sequence ends: its state alone says whether it is safe to walk off.
//
// The sequence runs on a worker thread. It knows nothing about Qt - it calls
// four plain function pointers, and those post to this window through queued
// signals, which is the only safe way across a thread boundary.

#pragma once
#include <QWidget>
#include <QThread>

class QComboBox;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QPlainTextEdit;

class UpgradeTask : public QObject {
    Q_OBJECT
public:
    UpgradeTask(unsigned inPort, unsigned outPort, uint8_t deviceId,
                QString migrationHex, QString applicationHex);
public slots:
    void run();
signals:
    void info(const QString& line);
    void error(const QString& line);
    void warn(const QString& line);
    void ok(const QString& line);
    void step(int index, int state);
    void status(const QString& line);
    void progress(int done, int total);
    void locked(bool on);
    void finished(bool ok);
private:
    unsigned in_, out_;
    uint8_t  id_;
    QString  mig_, app_;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onQuery();
    void onWarn(const QString& line);
    void onOk(const QString& line);
    void onStep(int index, int state);
    void onInfo(const QString& line);
    void onError(const QString& line);
    void onStatus(const QString& line);
    void onProgress(int done, int total);
    void onLocked(bool on);
    void onFinished(bool ok);

private:
    void appendLog(const QString& line, const char* colour);
    void setGateEnabled(bool on);
    void startSequence();

    QComboBox*      inBox_    = nullptr;
    QComboBox*      outBox_   = nullptr;
    QSpinBox*       idBox_    = nullptr;
    QPushButton*    queryBtn_ = nullptr;

    QLabel*         machine_  = nullptr;
    QLabel*         firmware_ = nullptr;
    QLabel*         banks_    = nullptr;

    // One bar per step, its name written INSIDE it. Three bars say what a
    // single anonymous one cannot: which stage is running, which are done, and
    // how far the current one has got - in the same glance.
    QProgressBar*   stepBar_[3] = { nullptr, nullptr, nullptr };
    int             curStep_    = -1;
    QLabel*         status_   = nullptr;
    QPlainTextEdit* log_      = nullptr;
    QLabel*         warning_  = nullptr;
    QPushButton*    closeBtn_ = nullptr;

    QThread*        thread_   = nullptr;
    bool            locked_   = false;
    int             unitType_ = 0;      // 0x50 or 0x62, from the ping
};
