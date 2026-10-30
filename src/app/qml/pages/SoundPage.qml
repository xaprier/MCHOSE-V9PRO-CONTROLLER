import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import V9Control

ScrollView {
    id: root

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        Card {
            title: qsTr("Output")
            subtitle: qsTr("PipeWire sink of the headset. Volume above 100% is software amplification.")
            Layout.fillWidth: true

            LevelSlider {
                label: qsTr("Headphones")
                icon: "../icons/volume.svg"
                mutedIcon: "../icons/volume-off.svg"
                available: Mixer.outputAvailable
                value: Mixer.outputVolume
                to: 150
                muted: Mixer.outputMuted
                Layout.fillWidth: true
                onMoved: value => Mixer.outputVolume = value
                onMuteToggled: Mixer.outputMuted = !Mixer.outputMuted
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: Mixer.outputIsDefault ? qsTr("The headset is the default output.") : qsTr("Another device is the default output.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: qsTr("Make default")
                    kind: "primary"
                    visible: Mixer.outputAvailable && !Mixer.outputIsDefault
                    onClicked: Mixer.makeDefaultOutput()
                }
            }
        }

        Card {
            title: qsTr("Microphone")
            subtitle: qsTr("PipeWire source of the headset. The mute button on the headset itself is handled by its firmware.")
            Layout.fillWidth: true

            LevelSlider {
                label: qsTr("Microphone")
                icon: "../icons/mic.svg"
                mutedIcon: "../icons/mic-off.svg"
                available: Mixer.micAvailable
                value: Mixer.micVolume
                to: 150
                muted: Mixer.micMuted
                Layout.fillWidth: true
                onMoved: value => Mixer.micVolume = value
                onMuteToggled: Mixer.micMuted = !Mixer.micMuted
            }
        }

        Card {
            title: qsTr("Noise suppression")
            subtitle: qsTr("Creates a cleaned-up microphone (\"V9 PRO Microphone (noise suppressed)\") and makes it the default input while enabled. Replaces Hub's AI noise reduction.")
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                ToggleSwitch {
                    text: NoiseSuppression.active ? qsTr("On") : qsTr("Off")
                    checked: NoiseSuppression.enabled
                    enabled: NoiseSuppression.available && Mixer.micAvailable
                    onToggled: NoiseSuppression.enabled = checked
                }
                Item {
                    Layout.fillWidth: true
                }
                SegmentedControl {
                    value: NoiseSuppression.engine
                    options: [
                        { text: qsTr("WebRTC"), value: NoiseSuppression.WebRtc },
                        { text: qsTr("RNNoise (AI)"), value: NoiseSuppression.RNNoise, enabled: NoiseSuppression.rnnoiseAvailable }
                    ]
                    onActivated: value => NoiseSuppression.engine = value
                }
            }

            LevelSlider {
                visible: NoiseSuppression.engine === NoiseSuppression.RNNoise
                label: qsTr("Voice detection threshold")
                value: NoiseSuppression.vadThreshold
                to: 99
                mutable: false
                Layout.fillWidth: true
                onMoved: value => NoiseSuppression.vadThreshold = value
            }

            Text {
                visible: !NoiseSuppression.rnnoiseAvailable
                text: qsTr("RNNoise needs the LADSPA plugin from noise-suppression-for-voice (Arch: noise-suppression-for-voice).")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                visible: NoiseSuppression.error.length > 0
                text: NoiseSuppression.error
                color: Theme.danger
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Card {
            title: qsTr("Microphone monitor")
            subtitle: qsTr("Hear your own voice in the headset while you talk.")
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                ToggleSwitch {
                    text: Monitor.active ? qsTr("On") : qsTr("Off")
                    checked: Monitor.enabled
                    enabled: Monitor.available && Mixer.micAvailable && Mixer.outputAvailable
                    onToggled: Monitor.enabled = checked
                }
            }

            LevelSlider {
                label: qsTr("Monitor level")
                icon: "../icons/headset.svg"
                mutable: false
                available: Monitor.enabled
                value: Monitor.level
                Layout.fillWidth: true
                onMoved: value => Monitor.level = value
            }

            Text {
                text: NoiseSuppression.active
                      ? qsTr("Loops the noise-suppressed microphone into the headset, so you hear what others hear.")
                      : qsTr("Loops the microphone into the headset through PipeWire (a few milliseconds of delay).")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                visible: Monitor.error.length > 0
                text: Monitor.error
                color: Theme.danger
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
