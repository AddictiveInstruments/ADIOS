// The capture tool.
//
// One button - or the space bar - runs the whole ceremony:
//   MIRROR_HALT 1 over MIDI: the instrument parks its screen task at a frame
//   boundary and the panel stops changing;
//   the 32-bit helper opens an EPHEMERAL SWD session and drains the panel's
//   GDRAM - the real pixels, read back over the panel's own bus, not a
//   reconstruction of what was once sent to it;
//   MIRROR_HALT 0: the instrument resumes exactly where it parked;
//   the PNG lands in the folder below and the view shows it 1:1.
//
// Between captures NOTHING runs: no MIDI port held, no SWD session, no
// polling. The instrument cannot tell this tool exists.

#pragma once
#include <QImage>
#include <QProcess>
#include <QString>
#include <QWidget>

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QListWidget;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

private slots:
    void onCapture();
    void onChooseFolder();

private:
    // The two MIDI moments of a capture. The port is opened for the one
    // message and closed again - nothing is held between captures.
    bool sendHalt(quint8 on, QString* err);
    void finishCapture(int exitCode, QProcess::ExitStatus st);
    bool save(const QImage& img);
    void logRow(const QString& what, bool ok);

    QComboBox*   outBox_    = nullptr;
    QSpinBox*    idBox_     = nullptr;
    QLabel*      link_      = nullptr;

    QLabel*      view_      = nullptr;   // the 480x320 panel, 1:1
    QLabel*      status_    = nullptr;
    QPushButton* captureBtn_= nullptr;
    QLabel*      counter_   = nullptr;
    QPushButton* folderBtn_ = nullptr;
    QListWidget* shots_     = nullptr;

    QProcess*    helper_    = nullptr;
    QString      rawPath_;
    QString      folder_;
    int          shotNum_ = 0;
};
