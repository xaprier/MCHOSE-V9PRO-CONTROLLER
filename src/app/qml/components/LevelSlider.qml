import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import V9Control

RowLayout {
    id: root

    property string label
    property url icon
    property url mutedIcon
    property bool muted: false
    property bool mutable: true
    property int value: 0
    property int to: 100
    property bool available: true

    signal moved(int value)
    signal muteToggled()

    spacing: Theme.spacing
    enabled: available
    opacity: available ? 1.0 : 0.45

    IconButton {
        text: root.muted ? qsTr("Unmute %1").arg(root.label) : qsTr("Mute %1").arg(root.label)
        icon.source: root.muted && root.mutedIcon.toString().length > 0 ? root.mutedIcon : root.icon
        tone: root.muted ? Theme.danger : Theme.text
        enabled: root.mutable
        onClicked: root.muteToggled()
    }

    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: root.label
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Text {
                text: root.muted ? qsTr("Muted") : qsTr("%1%").arg(slider.pressed ? Math.round(slider.value) : root.value)
                color: root.muted ? Theme.danger : Theme.text
                font.pixelSize: Theme.fontSmall
                font.weight: Font.DemiBold
            }
        }

        Slider {
            id: slider
            from: 0
            to: root.to
            stepSize: 1
            value: root.value
            Layout.fillWidth: true
            Accessible.name: root.label
            onMoved: root.moved(Math.round(value))

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 6
                width: slider.availableWidth
                height: implicitHeight
                radius: 3
                color: Theme.track

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: 3
                    color: root.muted ? Theme.textFaint : Theme.accent
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 18
                implicitHeight: 18
                radius: 9
                color: slider.pressed ? Theme.accent : Theme.text
                border.color: slider.visualFocus ? Theme.accent : Theme.surface
                border.width: 3
            }
        }
    }
}
