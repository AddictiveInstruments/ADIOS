#include "MainWindow.h"
#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <QtGlobal>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // Force the Fusion style on EVERY platform. It honours the stylesheet
    // identically; the native macOS style ignores QSS background/colour on
    // QPushButton, so the channel-filter buttons lost their green fill there.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
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
