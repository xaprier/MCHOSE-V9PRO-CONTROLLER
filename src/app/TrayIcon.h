#pragma once

#include <QObject>
#include <QPointer>

class EqualizerEngine;
class HeadsetController;
class MicMonitor;
class NoiseSuppressor;
class QAction;
class QMenu;
class QQuickWindow;
class QSystemTrayIcon;

// System tray entry. While it exists, closing the window only hides it so the audio chains
// (equalizer, noise suppression, monitor) and battery tracking keep running; "Quit" ends the app.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    TrayIcon(HeadsetController *headset, EqualizerEngine *equalizer, NoiseSuppressor *noiseSuppressor, MicMonitor *monitor,
             QObject *parent = nullptr);
    ~TrayIcon() override;

    static bool isAvailable();
    void setWindow(QQuickWindow *window);

signals:
    void quitRequested();

public slots:
    void showWindow();
    void toggleWindow();

private:
    void updateStatus();
    void updateToggles();

    HeadsetController *m_headset;
    EqualizerEngine *m_equalizer;
    NoiseSuppressor *m_noiseSuppressor;
    MicMonitor *m_monitor;
    QPointer<QQuickWindow> m_window;
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    QAction *m_status;
    QAction *m_equalizerAction;
    QAction *m_noiseAction;
    QAction *m_monitorAction;
    bool m_lowBatteryWarned = false;
};
