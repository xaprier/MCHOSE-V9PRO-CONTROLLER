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
    property int from: 0
    property int to: 100
    property int step: 1
    property string suffix: "%"
    property bool available: true

    signal moved(int value)
    signal muteToggled()

    // 0 -> 1 whenever the slider becomes visible (its page opens): the fill grows from the start.
    property real intro: 1

    spacing: Theme.spacing
    enabled: available

    onVisibleChanged: if (visible) introAnimation.restart()
    Component.onCompleted: if (visible) introAnimation.restart()

    NumberAnimation {
        id: introAnimation
        target: root
        property: "intro"
        from: 0
        to: 1
        duration: Theme.animSlow
        easing.type: Easing.OutCubic
    }
    opacity: available ? 1.0 : 0.45

    IconButton {
        visible: root.icon.toString().length > 0
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
                text: root.muted ? qsTr("Muted")
                                 : (slider.pressed ? Math.round(slider.value) : Math.round(root.from + (root.value - root.from) * root.intro)) + root.suffix
                color: root.muted ? Theme.danger : Theme.text
                font.pixelSize: Theme.fontSmall
                font.weight: Font.DemiBold
            }
        }

        Slider {
            id: slider
            from: root.from
            to: root.to
            stepSize: root.step
            value: root.value
            Layout.fillWidth: true
            Accessible.name: root.label
            onMoved: root.moved(Math.round(value))

            // Follows the value with a short glide when it changes from outside (profiles, the
            // tray, the headset); while dragging it tracks the pointer directly.
            property real glidePosition: visualPosition
            readonly property real shownPosition: slider.pressed ? visualPosition : glidePosition * root.intro
            Behavior on glidePosition {
                enabled: !slider.pressed
                NumberAnimation {
                    duration: Theme.animNormal
                    easing.type: Easing.OutCubic
                }
            }

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
                    width: slider.shownPosition * parent.width
                    height: parent.height
                    radius: 3
                    color: root.muted ? Theme.textFaint : Theme.accent
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.shownPosition * (slider.availableWidth - width)
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
