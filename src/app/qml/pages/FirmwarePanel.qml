pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import V9Control

// Voice prompt silencer: stock image → choose clips → build → flash (no-op first).
Card {
    id: root

    signal openRequested()
    signal saveRequested()

    readonly property bool flashing: Firmware.flashing

    title: qsTr("Voice prompt silencer (firmware)")
    subtitle: qsTr("The \"microphone on/off\" announcement and the short audio cut around it come from the headset firmware. This rebuilds the stock firmware with chosen prompts replaced by silence and flashes it over the 2.4G dongle. The ~1.5 s audio duck stays; only the voice goes.")

    WarningBanner {
        tone: Theme.danger
        title: qsTr("Firmware flashing")
        text: qsTr("Keep the stock image (it is your recovery). Flash the no-op image first: it is byte-identical to stock and proves the update path works on your unit. Keep the headset on, charged and next to the dongle, and do not unplug anything while flashing. An interrupted transfer is discarded by the headset, which keeps its current firmware.")
        Layout.fillWidth: true
    }

    Text {
        text: qsTr("1 · Stock firmware")
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        Layout.topMargin: Theme.spacingSmall
    }
    RowLayout {
        spacing: Theme.spacingSmall
        Layout.fillWidth: true

        ActionButton {
            text: qsTr("Download English")
            enabled: !Firmware.busy && !root.flashing
            onClicked: Firmware.downloadStock(true)
        }
        ActionButton {
            text: qsTr("Download Chinese")
            enabled: !Firmware.busy && !root.flashing
            onClicked: Firmware.downloadStock(false)
        }
        ActionButton {
            text: qsTr("Open file…")
            enabled: !root.flashing
            onClicked: root.openRequested()
        }
        Text {
            text: Firmware.imageLoaded ? Firmware.imageName + " — " + Firmware.imageSummary : qsTr("No image loaded")
            color: Firmware.imageLoaded ? Theme.textMuted : Theme.textFaint
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
    Text {
        text: qsTr("Downloads come from MCHOSE's update server (cdn.mchose.com.cn) only when you click, and are kept in %1.").arg(Firmware.firmwareDirectory)
        color: Theme.textFaint
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }

    Text {
        visible: Firmware.imageLoaded
        text: qsTr("2 · Prompts to silence")
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        Layout.topMargin: Theme.spacingSmall
    }
    Flow {
        visible: Firmware.imageLoaded
        spacing: 6
        Layout.fillWidth: true

        Repeater {
            model: Firmware.clips

            delegate: AbstractButton {
                id: chip

                required property var modelData

                checkable: true
                checked: chip.modelData.selected
                enabled: !root.flashing
                hoverEnabled: true
                implicitHeight: 30
                leftPadding: 10
                rightPadding: 10
                text: "#" + chip.modelData.index + (chip.modelData.role.length > 0 ? " " + chip.modelData.role : "")
                Accessible.role: Accessible.CheckBox
                onToggled: Firmware.setClipSelected(chip.modelData.index, chip.checked)

                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Clip %1, %2 s%3").arg(chip.modelData.index).arg(chip.modelData.seconds.toFixed(1)).arg(chip.modelData.silent ? qsTr(", already silent") : "")

                background: Rectangle {
                    radius: height / 2
                    color: chip.checked ? Theme.accentSoft : (chip.hovered ? Theme.surfaceRaised : Theme.background)
                    border.color: chip.checked ? Theme.accent : (chip.modelData.role.length > 0 ? Theme.info : Theme.border)
                }
                contentItem: Text {
                    text: chip.text
                    color: chip.checked ? Theme.accent : (chip.modelData.silent ? Theme.textFaint : Theme.text)
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    Text {
        visible: Firmware.imageLoaded
        text: qsTr("On English V9 PRO firmware clip 8 is \"microphone on\" and clip 9 \"microphone off\" (confirmed by Be-Quiet-MCHOSE). Other clips were not mapped; find them by silencing a group, flashing and listening.")
        color: Theme.textFaint
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Text {
        visible: Firmware.imageLoaded
        text: qsTr("3 · Build and flash")
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        Layout.topMargin: Theme.spacingSmall
    }
    RowLayout {
        visible: Firmware.imageLoaded
        spacing: Theme.spacingSmall
        Layout.fillWidth: true

        ActionButton {
            text: qsTr("Build patched image")
            kind: "primary"
            enabled: !root.flashing
            onClicked: Firmware.buildPatched()
        }
        ActionButton {
            text: qsTr("Save patched…")
            enabled: Firmware.patchedReady && !root.flashing
            onClicked: root.saveRequested()
        }
        Item {
            Layout.fillWidth: true
        }
        StatusPill {
            text: Firmware.patchedReady ? qsTr("Patched image ready") : qsTr("Not built")
            tone: Firmware.patchedReady ? Theme.success : Theme.textMuted
        }
    }
    RowLayout {
        visible: Firmware.imageLoaded
        spacing: Theme.spacingSmall
        Layout.fillWidth: true

        StyledTextField {
            id: flashConfirm
            placeholderText: qsTr("Type FLASH")
            enabled: !root.flashing
            Layout.preferredWidth: 120
        }
        ActionButton {
            text: qsTr("Flash no-op (test)")
            kind: "primary"
            enabled: flashConfirm.text === "FLASH" && !root.flashing && Headset.headsetOnline
            onClicked: if (Firmware.flash("noop", flashConfirm.text)) flashConfirm.clear()
        }
        ActionButton {
            text: qsTr("Flash patched")
            kind: "danger"
            enabled: flashConfirm.text === "FLASH" && Firmware.patchedReady && !root.flashing && Headset.headsetOnline
            onClicked: if (Firmware.flash("patched", flashConfirm.text)) flashConfirm.clear()
        }
        ActionButton {
            text: qsTr("Restore stock")
            enabled: flashConfirm.text === "FLASH" && !root.flashing && Headset.headsetOnline
            onClicked: if (Firmware.flash("stock", flashConfirm.text)) flashConfirm.clear()
        }
        Item {
            Layout.fillWidth: true
        }
        ActionButton {
            visible: root.flashing
            text: qsTr("Abort")
            kind: "danger"
            onClicked: Firmware.abortFlash()
        }
    }

    ProgressBar {
        id: progress
        visible: root.flashing || Firmware.flashProgress > 0
        value: Firmware.flashProgress
        Layout.fillWidth: true

        background: Rectangle {
            implicitHeight: 6
            radius: 3
            color: Theme.track
        }
        contentItem: Item {
            implicitHeight: 6

            Rectangle {
                width: progress.visualPosition * parent.width
                height: parent.height
                radius: 3
                color: Firmware.flashState === FirmwareFlasher.Failed ? Theme.danger
                     : Firmware.flashState === FirmwareFlasher.Succeeded ? Theme.success : Theme.accent
            }
        }
    }

    Rectangle {
        visible: Firmware.log.length > 0
        radius: Theme.radius
        color: Theme.background
        border.color: Theme.border
        Layout.fillWidth: true
        Layout.preferredHeight: 150

        ListView {
            id: logView
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            model: Firmware.log
            onCountChanged: positionViewAtEnd()

            delegate: Text {
                id: logText

                required property string modelData

                width: ListView.view.width
                text: logText.modelData
                color: Theme.textMuted
                font: Theme.monoFont
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}
