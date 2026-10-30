import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import V9Control

ScrollView {
    id: root

    readonly property bool online: Headset.dongleConnected && Headset.headsetOnline
    readonly property string chargeCaption: {
        switch (Headset.chargeState) {
        case Headset.Charging:
            return qsTr("Charging")
        case Headset.Full:
            return qsTr("Fully charged")
        case Headset.Asleep:
            return qsTr("Asleep")
        case Headset.Discharging:
            return qsTr("On battery")
        }
        return Headset.dongleConnected ? qsTr("Headset off") : qsTr("No dongle")
    }

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        RowLayout {
            spacing: Theme.spacing
            Layout.fillWidth: true

            Card {
                title: qsTr("Battery")
                Layout.preferredWidth: 280
                Layout.fillHeight: true

                // Centred inside a full-width holder: the ring's own Layout.alignment is not
                // honoured when the card's body column is narrower than the card.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: ring.implicitHeight

                    BatteryRing {
                        id: ring
                        anchors.horizontalCenter: parent.horizontalCenter
                        percent: root.online || Headset.chargeState === Headset.Asleep ? Headset.batteryPercent : -1
                        charging: Headset.chargeState === Headset.Charging
                        caption: root.chargeCaption
                    }
                }
                Text {
                    visible: Headset.lastBatteryUpdate.getTime() > 0
                    text: qsTr("Updated %1").arg(Qt.formatTime(Headset.lastBatteryUpdate, "HH:mm:ss"))
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }

            Card {
                title: qsTr("MCHOSE V9 PRO")
                subtitle: qsTr("2.4 GHz wireless gaming headset")
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flow {
                    spacing: Theme.spacingSmall
                    Layout.fillWidth: true

                    StatusPill {
                        text: Headset.dongleConnected ? qsTr("Dongle connected") : qsTr("Dongle not found")
                        tone: Headset.dongleConnected ? Theme.success : Theme.danger
                    }
                    StatusPill {
                        visible: Headset.dongleConnected
                        text: Headset.headsetOnline ? qsTr("Headset online") : qsTr("Headset offline")
                        tone: Headset.headsetOnline ? Theme.success : Theme.warning
                    }
                    StatusPill {
                        visible: Mixer.outputAvailable
                        text: Mixer.outputIsDefault ? qsTr("Default output") : qsTr("Not default output")
                        tone: Mixer.outputIsDefault ? Theme.info : Theme.textMuted
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                LevelSlider {
                    label: qsTr("Headphones")
                    icon: "../icons/volume.svg"
                    mutedIcon: "../icons/volume-off.svg"
                    available: Mixer.outputAvailable
                    value: Mixer.outputVolume
                    muted: Mixer.outputMuted
                    Layout.fillWidth: true
                    onMoved: value => Mixer.outputVolume = value
                    onMuteToggled: Mixer.outputMuted = !Mixer.outputMuted
                }
                LevelSlider {
                    label: qsTr("Microphone")
                    icon: "../icons/mic.svg"
                    mutedIcon: "../icons/mic-off.svg"
                    available: Mixer.micAvailable
                    value: Mixer.micVolume
                    muted: Mixer.micMuted
                    Layout.fillWidth: true
                    onMoved: value => Mixer.micVolume = value
                    onMuteToggled: Mixer.micMuted = !Mixer.micMuted
                }
            }
        }

        Card {
            title: qsTr("Sound mode")
            subtitle: qsTr("Preset stored and processed inside the headset. Also switchable with the headset button.")
            enabled: root.online
            opacity: enabled ? 1.0 : 0.5
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                ButtonGroup {
                    id: modeGroup
                }

                ModeTile {
                    text: qsTr("Game 1")
                    description: qsTr("Footsteps and positional cues")
                    icon.source: Qt.resolvedUrl("../icons/target.svg")
                    checked: Headset.soundMode === Headset.GameOne
                    ButtonGroup.group: modeGroup
                    Layout.fillWidth: true
                    onClicked: Headset.soundMode = Headset.GameOne
                }
                ModeTile {
                    text: qsTr("Game 2")
                    description: qsTr("Immersive, wider stage")
                    icon.source: Qt.resolvedUrl("../icons/gamepad.svg")
                    checked: Headset.soundMode === Headset.GameTwo
                    ButtonGroup.group: modeGroup
                    Layout.fillWidth: true
                    onClicked: Headset.soundMode = Headset.GameTwo
                }
                ModeTile {
                    text: qsTr("Music")
                    description: qsTr("Balanced listening")
                    icon.source: Qt.resolvedUrl("../icons/music.svg")
                    checked: Headset.soundMode === Headset.Music
                    ButtonGroup.group: modeGroup
                    Layout.fillWidth: true
                    onClicked: Headset.soundMode = Headset.Music
                }
            }
        }

        Card {
            title: qsTr("Quick toggles")
            subtitle: qsTr("Software features running in PipeWire. Details on the Sound and Equalizer pages.")
            Layout.fillWidth: true

            Flow {
                spacing: 28
                Layout.fillWidth: true

                ToggleSwitch {
                    text: qsTr("Equalizer")
                    checked: Equalizer.enabled
                    enabled: Equalizer.available
                    onToggled: Equalizer.enabled = checked
                }
                ToggleSwitch {
                    text: qsTr("Noise suppression")
                    checked: NoiseSuppression.enabled
                    enabled: NoiseSuppression.available
                    onToggled: NoiseSuppression.enabled = checked
                }
                ToggleSwitch {
                    text: qsTr("Mic monitor")
                    checked: Monitor.enabled
                    onToggled: Monitor.enabled = checked
                }
            }
        }
    }
}
