#include "QmlTypes.h"
#include "TrayIcon.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSocketNotifier>
#include <QTimer>

#include <csignal>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcApp, "v9.app")

namespace {

int signalPipe[2] = {-1, -1};

void onTerminationSignal(int)
{
    const char byte = 1;
    [[maybe_unused]] const auto written = ::write(signalPipe[0], &byte, 1);
}

// SIGTERM (logout, systemd) and SIGINT must go through a normal quit: the destructors hand the
// default devices and running streams back to the headset before the virtual nodes disappear.
void quitOnTerminationSignals()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, signalPipe) != 0)
        return;
    auto *notifier = new QSocketNotifier(signalPipe[1], QSocketNotifier::Read, qApp);
    QObject::connect(notifier, &QSocketNotifier::activated, qApp, [] {
        char byte;
        [[maybe_unused]] const auto read = ::read(signalPipe[1], &byte, 1);
        QCoreApplication::quit();
    });
    struct sigaction action {};
    action.sa_handler = onTerminationSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    ::sigaction(SIGTERM, &action, nullptr);
    ::sigaction(SIGINT, &action, nullptr);
    ::sigaction(SIGHUP, &action, nullptr);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("v9pro-control"));
    QApplication::setApplicationDisplayName(QStringLiteral("V9 Pro Control"));
    QApplication::setOrganizationName(QStringLiteral("v9pro-control"));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("v9pro-control"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("v9pro-control"), QIcon(QStringLiteral(":/icons/v9pro-control.svg"))));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Save a screenshot of the window to <file> after startup and quit."),
                                              QStringLiteral("file"));
    const QCommandLineOption pageOption(QStringLiteral("page"), QStringLiteral("Start on page <index>."), QStringLiteral("index"));
    parser.addOption(screenshotOption);
    const QCommandLineOption hiddenOption(QStringLiteral("hidden"), QStringLiteral("Start in the system tray without showing the window."));
    parser.addOption(pageOption);
    parser.addOption(hiddenOption);
    parser.process(app);
    quitOnTerminationSignals();

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
    if (!window)
        return 1;
    if (parser.isSet(pageOption))
        window->setProperty("currentPage", parser.value(pageOption).toInt());

    // Without a tray (no StatusNotifier host) closing the window has to quit, or the app would
    // keep running with no way back.
    std::unique_ptr<TrayIcon> tray;
    qCInfo(lcApp) << "system tray available:" << TrayIcon::isAvailable();
    if (TrayIcon::isAvailable() && !parser.isSet(screenshotOption)) {
        tray = std::make_unique<TrayIcon>(&headset, &equalizer, &noiseSuppressor, &monitor);
        tray->setWindow(window);
        QApplication::setQuitOnLastWindowClosed(false);
        window->setProperty("hideOnClose", true);
    }
    if (!tray || !parser.isSet(hiddenOption))
        window->show();

    if (parser.isSet(screenshotOption)) {
        // Long enough for device discovery, the first HID exchanges and the firmware read.
        QTimer::singleShot(3500, window, [window, path = parser.value(screenshotOption)] {
            window->grabWindow().save(path);
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
