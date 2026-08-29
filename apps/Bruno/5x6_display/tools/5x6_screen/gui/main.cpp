#include "MainWindow.h"
#include <QApplication>
#include <QFile>
#include <QtGlobal>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("5x6 Screen"));

    // The look is a deliberate choice, not the system's: this tool is meant to
    // be recognisable next to the ROM editor and the updater, so it carries the
    // same charter on Windows and on macOS rather than imitating either.
    // ":/gui/style.qss", not ":/style.qss" - qt_add_resources KEEPS the file's
    // relative path under the prefix, so the folder name is part of it. A
    // missing stylesheet fails silently (the window just comes up in the system
    // theme), which is exactly how it went unnoticed the first time on the
    // upgrader.
    QFile qss(QStringLiteral(":/gui/style.qss"));
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    else
        qWarning("stylesheet not found - the window will use the system theme");

    MainWindow w;
    w.show();
    return app.exec();
}
