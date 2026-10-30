#include "QmlTypes.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("v9pro-control"));
    QApplication::setApplicationDisplayName(QStringLiteral("V9 Pro Control"));
    QApplication::setOrganizationName(QStringLiteral("v9pro-control"));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("v9pro-control"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/V9Control/qml/icons/headset.svg")));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Save a screenshot of the window to <file> after startup and quit."),
                                              QStringLiteral("file"));
    const QCommandLineOption pageOption(QStringLiteral("page"), QStringLiteral("Start on page <index>."), QStringLiteral("index"));
    parser.addOption(screenshotOption);
    parser.addOption(pageOption);
    parser.process(app);

    HeadsetController headset;
    AudioMixer mixer;
    EqualizerEngine equalizer(&mixer);
    NoiseSuppressor noiseSuppressor(&mixer);
    MicMonitor monitor(&mixer, &noiseSuppressor);
    FirmwareManager firmware(&headset);
    ReportLogModel hidLog(&headset);
    HeadsetForeign::instance = &headset;
    MixerForeign::instance = &mixer;
    EqualizerForeign::instance = &equalizer;
    NoiseSuppressionForeign::instance = &noiseSuppressor;
    MonitorForeign::instance = &monitor;
    FirmwareForeign::instance = &firmware;
    HidLogForeign::instance = &hidLog;

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("V9Control", "Main");

    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window && parser.isSet(pageOption))
        window->setProperty("currentPage", parser.value(pageOption).toInt());

    if (window && parser.isSet(screenshotOption)) {
        // Long enough for device discovery, the first HID exchanges and the firmware read.
        QTimer::singleShot(3500, window, [window, path = parser.value(screenshotOption)] {
            window->grabWindow().save(path);
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
