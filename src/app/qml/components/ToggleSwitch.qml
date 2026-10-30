import QtQuick
import QtQuick.Controls.Basic
import V9Control

Switch {
    id: root

    hoverEnabled: true
    padding: 0
    spacing: 10

    indicator: Rectangle {
        implicitWidth: 42
        implicitHeight: 24
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.track
        border.color: root.visualFocus ? Theme.info : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }

        Rectangle {
            x: root.checked ? parent.width - width - 3 : 3
            y: 3
            width: 18
            height: 18
            radius: 9
            color: Theme.text

            Behavior on x {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    contentItem: Text {
        leftPadding: root.text.length > 0 ? root.indicator.width + root.spacing : root.indicator.width
        text: root.text
        color: root.enabled ? Theme.text : Theme.textFaint
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
    }
}
