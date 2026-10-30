import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import V9Control

// What the app is and who made it, plus the device details (firmware, connection).
ScrollView {
    id: root

    readonly property url repositoryUrl: "https://github.com/xaprier/MCHOSE-V9PRO-CONTROLLER"
    readonly property url developerUrl: "https://github.com/xaprier"

    contentWidth: availableWidth
    clip: true

    component InfoRow: RowLayout {
        property string label
        property string value
        property bool mono: false

        spacing: Theme.spacing
        Layout.fillWidth: true

        Text {
            text: parent.label
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            Layout.preferredWidth: 160
        }
        Text {
            text: parent.value.length > 0 ? parent.value : "—"
            color: Theme.text
            font: parent.mono ? Theme.monoFont : Theme.bodyFont
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }
    }

    // Round picture: the image is masked by a circle instead of being clipped to a square.
    component Avatar: Item {
        property url source

        implicitWidth: 72
        implicitHeight: 72

        Image {
            id: picture
            anchors.fill: parent
            source: parent.source
            sourceSize.width: 144
            sourceSize.height: 144
            fillMode: Image.PreserveAspectCrop
            visible: false
            Accessible.ignored: true
        }
        Rectangle {
            id: circle
            anchors.fill: parent
            radius: width / 2
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: picture
            maskEnabled: true
            maskSource: circle
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.color: Theme.border
            border.width: 2
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        Card {
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                Image {
                    source: "qrc:/icons/v9pro-control.svg"
                    sourceSize.width: 72
                    sourceSize.height: 72
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 72
                    Layout.alignment: Qt.AlignTop
                    Accessible.ignored: true
                }
                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("V9 Pro Control")
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.weight: Font.Bold
                    }
                    Text {
                        text: qsTr("Version %1").arg(Qt.application.version)
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }
                    Text {
                        text: qsTr("Native Linux control app for the MCHOSE V9 PRO wireless headset: battery, sound modes, a HUB-compatible equalizer, noise suppression, mic monitor, soundboard and a PipeWire patchbay. Built because MCHOSE HUB doesn't run on Linux.")
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spacingSmall
                    }
                    Text {
                        text: qsTr("Unofficial and not affiliated with MCHOSE. Talks to the headset over hidraw and to PipeWire; no network access.")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        text: qsTr("Source code on GitHub")
                        kind: "primary"
                        Layout.topMargin: Theme.spacingSmall
                        onClicked: Qt.openUrlExternally(root.repositoryUrl)
                    }
                }
            }
        }

        Card {
            title: qsTr("Developer")
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                Avatar {
                    source: "qrc:/images/developer.jpg"
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 72
                }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Text {
                        text: "xaprier"
                        color: Theme.text
                        font.pixelSize: Theme.fontHeading
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "github.com/xaprier"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }
                }
                ActionButton {
                    text: qsTr("GitHub profile")
                    onClicked: Qt.openUrlExternally(root.developerUrl)
                }
            }
        }

        Card {
            title: qsTr("Firmware")
            subtitle: qsTr("Versions as the dongle reports them; raw replies are shown for reference.")
            Layout.fillWidth: true

            InfoRow {
                label: qsTr("Dongle")
                value: Headset.dongleFirmware
            }
            InfoRow {
                label: qsTr("Dongle (raw)")
                value: Headset.dongleFirmwareRaw
                mono: true
            }
            InfoRow {
                label: qsTr("Headset")
                value: Headset.headsetFirmware
            }
            InfoRow {
                label: qsTr("Headset (raw)")
                value: Headset.headsetFirmwareRaw
                mono: true
            }
        }

        Card {
            title: qsTr("Connection")
            Layout.fillWidth: true

            InfoRow {
                label: qsTr("USB")
                value: "291d:385d"
                mono: true
            }
            InfoRow {
                label: qsTr("HID node")
                value: Headset.devicePath
                mono: true
            }
            InfoRow {
                label: qsTr("Headset")
                value: Headset.headsetOnline ? qsTr("Online") : qsTr("Off or out of range")
            }
            InfoRow {
                visible: Headset.lastError.length > 0
                label: qsTr("Device error")
                value: Headset.lastError
            }
            InfoRow {
                visible: Mixer.lastError.length > 0
                label: qsTr("Audio error")
                value: Mixer.lastError
            }
        }
    }
}
