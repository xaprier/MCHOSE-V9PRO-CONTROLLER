pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import V9Control

// Features beyond what MCHOSE HUB offers for the V9 PRO. Everything here is opt-in and some of
// it writes to the headset, so each section carries its own warning.
ScrollView {
    id: root

    property bool unlocked: false

    contentWidth: availableWidth
    clip: true

    ListModel {
        id: commandLog
    }

    Connections {
        target: Headset

        function onExperimentalResult(command, answered, detail) {
            commandLog.insert(0, {
                time: Qt.formatTime(new Date(), "HH:mm:ss"),
                command: command,
                answered: answered,
                detail: detail
            })
        }
    }

    FileDialog {
        id: saveLogDialog
        title: qsTr("Save HID capture")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        onAccepted: HidLog.saveTo(selectedFile)
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        WarningBanner {
            title: qsTr("Lab — use at your own risk")
            text: qsTr("These tools go beyond what MCHOSE HUB offers for the V9 PRO. Some commands are untested on this model. Nothing here runs on its own; every write needs an explicit action.")
            Layout.fillWidth: true
        }

        ToggleSwitch {
            text: qsTr("I understand, show the lab tools")
            checked: root.unlocked
            onToggled: root.unlocked = checked
        }

        Card {
            visible: root.unlocked
            title: qsTr("HID monitor")
            subtitle: qsTr("Shows every report the dongle sends. To test the mic button: start capturing, press the button a few times, and look for highlighted lines (e.g. TELEPHONY). If nothing appears, the button produces no USB traffic.")
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacingSmall
                Layout.fillWidth: true

                ActionButton {
                    text: HidLog.capturing ? qsTr("Stop") : qsTr("Start capture")
                    kind: HidLog.capturing ? "danger" : "primary"
                    enabled: Headset.dongleConnected
                    onClicked: HidLog.capturing = !HidLog.capturing
                }
                ActionButton {
                    text: qsTr("Clear")
                    onClicked: HidLog.clear()
                }
                ActionButton {
                    text: qsTr("Save…")
                    enabled: HidLog.count > 0
                    onClicked: saveLogDialog.open()
                }
                Item {
                    Layout.fillWidth: true
                }
                ToggleSwitch {
                    text: qsTr("Hide polling")
                    checked: HidLog.hidePolling
                    onToggled: HidLog.hidePolling = checked
                }
            }

            Rectangle {
                radius: Theme.radius
                color: Theme.background
                border.color: Theme.border
                Layout.fillWidth: true
                Layout.preferredHeight: 220

                ListView {
                    id: hidList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: HidLog
                    reuseItems: true

                    delegate: RowLayout {
                        id: hidRow

                        required property string time
                        required property string direction
                        required property string hex
                        required property string description
                        required property bool interesting

                        width: ListView.view.width
                        spacing: 12

                        Text {
                            text: hidRow.time
                            color: Theme.textFaint
                            font: Theme.monoFont
                        }
                        Text {
                            text: hidRow.direction
                            color: hidRow.direction === "TX" ? Theme.info : Theme.success
                            font: Theme.monoFont
                        }
                        Text {
                            text: hidRow.hex
                            color: hidRow.interesting ? Theme.warning : Theme.text
                            font: Theme.monoFont
                            Layout.preferredWidth: 360
                        }
                        Text {
                            text: hidRow.description
                            color: hidRow.interesting ? Theme.warning : Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: hidList.count === 0
                        text: HidLog.capturing ? qsTr("Listening… press the mic button") : qsTr("Not capturing")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontBody
                    }
                }
            }
        }

        Card {
            visible: root.unlocked
            title: qsTr("Experimental headset settings")
            subtitle: qsTr("Commands MCHOSE HUB uses for the V9 TURBO family (same chip). Headset firmware 1015 does not answer either read, so they look unsupported on the V9 PRO; kept for other firmware versions.")
            Layout.fillWidth: true

            GridLayout {
                columns: 4
                columnSpacing: Theme.spacing
                rowSpacing: Theme.spacingSmall
                Layout.fillWidth: true

                Text {
                    text: qsTr("Voice prompt volume")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    Layout.preferredWidth: 170
                }
                Text {
                    text: Headset.promptVolume < 0 ? qsTr("unknown") : Headset.promptVolume
                    color: Theme.textMuted
                    font: Theme.monoFont
                    Layout.preferredWidth: 70
                }
                ActionButton {
                    text: qsTr("Read")
                    enabled: Headset.dongleConnected
                    onClicked: Headset.readPromptVolume()
                }
                RowLayout {
                    Layout.fillWidth: true

                    LevelSlider {
                        id: promptVolume
                        label: qsTr("New value")
                        mutable: false
                        from: 1
                        to: 16
                        suffix: ""
                        value: Headset.promptVolume > 0 ? Headset.promptVolume : 8
                        Layout.fillWidth: true
                        onMoved: value => promptVolume.value = value
                    }
                    ActionButton {
                        text: qsTr("Write")
                        kind: "primary"
                        enabled: Headset.dongleConnected && Headset.promptVolume >= 0
                        onClicked: Headset.writePromptVolume(promptVolume.value)
                    }
                }

                Text {
                    text: qsTr("Auto power-off")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                Text {
                    text: Headset.autoShutdown < 0 ? qsTr("unknown") : Headset.autoShutdown
                    color: Theme.textMuted
                    font: Theme.monoFont
                }
                ActionButton {
                    text: qsTr("Read")
                    enabled: Headset.dongleConnected
                    onClicked: Headset.readAutoShutdown()
                }
                RowLayout {
                    Layout.fillWidth: true

                    SegmentedControl {
                        id: shutdownChoice
                        value: 1
                        // Values as MCHOSE HUB sends them on V9 TURBO (raw code → label).
                        options: [
                            { text: qsTr("15 min"), value: 1 },
                            { text: qsTr("30 min"), value: 4 },
                            { text: qsTr("45 min"), value: 0 }
                        ]
                        onActivated: value => shutdownChoice.value = value
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        text: qsTr("Write")
                        kind: "primary"
                        enabled: Headset.dongleConnected && Headset.autoShutdown >= 0
                        onClicked: Headset.writeAutoShutdown(shutdownChoice.value)
                    }
                }
            }

            Text {
                text: qsTr("Write stays disabled until a read succeeds, so an unsupported command is never written blindly.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Rectangle {
                visible: commandLog.count > 0
                radius: Theme.radius
                color: Theme.background
                border.color: Theme.border
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(130, commandLog.count * 22 + 16)

                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: commandLog

                    delegate: Text {
                        id: logLine

                        required property string time
                        required property string command
                        required property bool answered
                        required property string detail

                        width: ListView.view.width
                        text: logLine.time + "  " + logLine.command + " — " + logLine.detail
                        color: logLine.answered ? Theme.success : Theme.warning
                        font: Theme.monoFont
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                color: Theme.border
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: Theme.spacingSmall
            }

            RowLayout {
                spacing: Theme.spacingSmall
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Factory reset")
                        color: Theme.danger
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("Same command family as above, which firmware 1015 ignores. May clear pairing and settings if it is supported. Type RESET to enable.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                StyledTextField {
                    id: resetConfirm
                    placeholderText: "RESET"
                    Layout.preferredWidth: 110
                }
                ActionButton {
                    text: qsTr("Reset headset")
                    kind: "danger"
                    enabled: Headset.dongleConnected && resetConfirm.text === "RESET"
                    onClicked: {
                        Headset.factoryReset(resetConfirm.text)
                        resetConfirm.clear()
                    }
                }
            }
        }
    }
}
