#include "ui/MainWindow.h"
#include "control/AppSettings.h"
#include "control/ControlRegistry.h"
#include "engine/AudioEngine.h"
#include "library/Library.h"
#include "controllers/ControllerManager.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFile>
#include <QCommandLineParser>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("stemboks"));
    app.setApplicationDisplayName(QStringLiteral("Stemboks"));
    app.setOrganizationName(QStringLiteral("stemboks"));
    app.setApplicationVersion(QStringLiteral("1.8.9"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Stemboks — stem DJ player"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption loadOpt(QStringList() << "load", "Load track to deck 1", "file");
    parser.addOption(loadOpt);
    parser.process(app);

    for (const char* f : {":/fonts/OpenSans-Regular.ttf", ":/fonts/OpenSans-Bold.ttf", ":/fonts/OpenSans-SemiBold.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(f));
    app.setFont(QFont(QStringLiteral("Open Sans"), 11));

    AppSettings::instance().load();
    bool light = AppSettings::instance().getInt(QStringLiteral("[Stembox]"), QStringLiteral("theme"), 0) == 1;
    QFile qss(light ? QStringLiteral(":/theme/style-stemboks-light.qss")
                    : QStringLiteral(":/theme/style-stemboks.qss"));
    if (qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    Library::instance().open();
    AudioEngine::instance().start();
    ControllerManager::instance().start();

    MainWindow w;
    w.show();
    if (parser.isSet(loadOpt))
        w.loadTrackToDeck(0, parser.value(loadOpt));

    int rc = app.exec();
    AppSettings::instance().save();
    AudioEngine::instance().stop();
    return rc;
}
