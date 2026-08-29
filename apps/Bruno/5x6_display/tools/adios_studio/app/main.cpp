#include "MainWindow.h"
#include <QApplication>
#include <QFile>
#include <QtGlobal>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ADIOS Studio"));
    app.setOrganizationName(QStringLiteral("Addictive Instruments"));
    // so QSettings has a home for the remembered ports, id and hex file

    // Same charter as the ROM editor, the updater and the screen tool - one
    // recognisable family on both systems. A missing stylesheet is not fatal;
    // the window simply comes up in the system theme.
    QFile qss(QStringLiteral(":/app/style.qss"));
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    else
        qWarning("stylesheet not found - system theme");

    MainWindow w;
    w.show();
    return app.exec();
}
