#include "TrayIcon.h"

#include "EqualizerEngine.h"
#include "HeadsetController.h"
#include "MicMonitor.h"
#include "NoiseSuppressor.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QQuickWindow>
#include <QSystemTrayIcon>

namespace {

constexpr int LowBatteryPercent = 15;

} // namespace

TrayIcon::TrayIcon(HeadsetController *headset, EqualizerEngine *equalizer, NoiseSuppressor *noiseSuppressor, MicMonitor *monitor,
                   QObject *parent)
    : QObject(parent)
    , m_headset(headset)
    , m_equalizer(equalizer)
    , m_noiseSuppressor(noiseSuppressor)
    , m_monitor(monitor)
    , m_tray(new QSystemTrayIcon(QApplication::windowIcon(), this))
    , m_menu(new QMenu)
{
    m_status = m_menu->addAction(QString());
    m_status->setEnabled(false);
    m_menu->addSeparator();
    m_menu->addAction(tr("Open V9 Pro Control"), this, &TrayIcon::showWindow);
    m_menu->addSeparator();

    m_equalizerAction = m_menu->addAction(tr("Equalizer"));
    m_equalizerAction->setCheckable(true);
    connect(m_equalizerAction, &QAction::triggered, m_equalizer, &EqualizerEngine::setEnabled);
    m_noiseAction = m_menu->addAction(tr("Noise suppression"));
    m_noiseAction->setCheckable(true);
    connect(m_noiseAction, &QAction::triggered, m_noiseSuppressor, &NoiseSuppressor::setEnabled);
    m_monitorAction = m_menu->addAction(tr("Microphone monitor"));
    m_monitorAction->setCheckable(true);
    connect(m_monitorAction, &QAction::triggered, m_monitor, &MicMonitor::setEnabled);

    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), this, &TrayIcon::quitRequested);

    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            toggleWindow();
    });

    connect(m_headset, &HeadsetController::batteryChanged, this, &TrayIcon::updateStatus);
    connect(m_headset, &HeadsetController::connectionChanged, this, &TrayIcon::updateStatus);
    connect(m_equalizer, &EqualizerEngine::enabledChanged, this, &TrayIcon::updateToggles);
    connect(m_noiseSuppressor, &NoiseSuppressor::enabledChanged, this, &TrayIcon::updateToggles);
    connect(m_noiseSuppressor, &NoiseSuppressor::activeChanged, this, &TrayIcon::updateToggles);
    connect(m_monitor, &MicMonitor::enabledChanged, this, &TrayIcon::updateToggles);

    updateStatus();
    updateToggles();
    m_tray->show();
}

TrayIcon::~TrayIcon()
{
    // QMenu is a widget and cannot take a QObject parent.
    delete m_menu;
}

bool TrayIcon::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::setWindow(QQuickWindow *window)
{
    m_window = window;
}

void TrayIcon::showWindow()
{
    if (!m_window)
        return;
    m_window->show();
    if (m_window->visibility() == QWindow::Minimized)
        m_window->showNormal();
    m_window->raise();
    m_window->requestActivate();
}

void TrayIcon::toggleWindow()
{
    if (m_window && m_window->isVisible() && m_window->visibility() != QWindow::Minimized)
        m_window->hide();
    else
        showWindow();
}

void TrayIcon::updateStatus()
{
    QString status;
    const int percent = m_headset->batteryPercent();
    const auto state = m_headset->chargeState();
    if (!m_headset->dongleConnected()) {
        status = tr("Dongle not found");
    } else if (!m_headset->headsetOnline() && state != HeadsetController::ChargeState::Asleep) {
        status = tr("Headset offline");
    } else if (percent < 0) {
        status = tr("Headset connected");
    } else if (state == HeadsetController::ChargeState::Charging) {
        status = tr("Battery %1% · charging").arg(percent);
    } else if (state == HeadsetController::ChargeState::Full) {
        status = tr("Battery full");
    } else if (state == HeadsetController::ChargeState::Asleep) {
        status = tr("Battery %1% · asleep").arg(percent);
    } else {
        status = tr("Battery %1%").arg(percent);
    }
    m_status->setText(status);
    m_tray->setToolTip(tr("V9 Pro Control — %1").arg(status));

    // One warning per discharge; charging re-arms it.
    const bool discharging = state == HeadsetController::ChargeState::Discharging;
    if (discharging && percent >= 0 && percent <= LowBatteryPercent && !m_lowBatteryWarned) {
        m_lowBatteryWarned = true;
        m_tray->showMessage(tr("Headset battery low"), tr("MCHOSE V9 PRO is at %1%.").arg(percent), QSystemTrayIcon::Warning);
    } else if (!discharging || percent > LowBatteryPercent) {
        m_lowBatteryWarned = false;
    }
}

void TrayIcon::updateToggles()
{
    m_equalizerAction->setChecked(m_equalizer->enabled());
    m_equalizerAction->setEnabled(m_equalizer->available());
    m_noiseAction->setChecked(m_noiseSuppressor->enabled());
    m_noiseAction->setEnabled(m_noiseSuppressor->available());
    m_monitorAction->setChecked(m_monitor->enabled());
}
