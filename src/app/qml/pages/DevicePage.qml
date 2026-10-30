import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import V9Control

ScrollView {
    id: root

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

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        Card {
            title: qsTr("Firmware")
            subtitle: qsTr("Version decoding follows MCHOSE HUB and is not yet confirmed; raw replies are shown for reference.")
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

        Card {
            title: qsTr("About")
            Layout.fillWidth: true

            InfoRow {
                label: qsTr("Application")
                value: qsTr("V9 Pro Control %1").arg(Qt.application.version)
            }
            Text {
                text: qsTr("Unofficial. Talks to the headset directly over hidraw and to PipeWire; no network access and no MCHOSE HUB required.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
