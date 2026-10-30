import QtQuick
import QtQuick.Controls.Basic
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
                    text: Mixer.outputIsDefault ? qsTr("The headset is the default output and input.") : qsTr("Another device is the default output.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Button {
                    id: defaultButton
                    text: qsTr("Make default")
                    visible: Mixer.outputAvailable && !Mixer.outputIsDefault
                    onClicked: Mixer.makeDefaultOutput()

                    contentItem: Text {
                        text: defaultButton.text
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitWidth: 120
                        implicitHeight: 34
                        radius: Theme.radius
                        color: defaultButton.down ? Theme.accent : Theme.accentSoft
                        border.color: Theme.accent
                    }
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
            title: qsTr("Sidetone")
            subtitle: qsTr("Hear your own voice in the headset. Uses the dongle's secondary ALSA playback control, which is believed to be the hardware mic monitor (unverified).")
            Layout.fillWidth: true

            LevelSlider {
                label: qsTr("Sidetone level")
                icon: "../icons/headset.svg"
                mutable: false
                available: Mixer.sidetoneAvailable
                value: Math.max(0, Mixer.sidetoneLevel)
                Layout.fillWidth: true
                onMoved: value => Mixer.sidetoneLevel = value
            }
        }
    }
}
