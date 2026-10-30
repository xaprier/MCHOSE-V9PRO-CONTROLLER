import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import V9Control

AbstractButton {
    id: root

    checkable: true
    autoExclusive: true
    hoverEnabled: true
    implicitHeight: 42
    leftPadding: 14
    rightPadding: 14
    icon.width: 18
    icon.height: 18

    Accessible.role: Accessible.PageTab
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radius
        color: root.checked ? Theme.accentSoft : (root.hovered ? Theme.surface : "transparent")
        border.color: root.visualFocus ? Theme.accent : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }

        Rectangle {
            width: 3
            height: root.checked ? 18 : 0
            opacity: root.checked ? 1 : 0
            radius: 1.5
            color: Theme.accent
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Behavior on height {
                NumberAnimation {
                    duration: Theme.animNormal
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.animFast
                }
            }
        }
    }

    contentItem: RowLayout {
        spacing: 12

        Image {
            source: root.icon.source
            sourceSize.width: root.icon.width
            sourceSize.height: root.icon.height
            Layout.preferredWidth: root.icon.width
            Layout.preferredHeight: root.icon.height
            opacity: root.checked || root.hovered ? 1.0 : 0.6
            Accessible.ignored: true

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.animFast
                }
            }
        }
        Text {
            text: root.text
            color: root.checked ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontBody
            font.weight: root.checked ? Font.DemiBold : Font.Normal
            Layout.fillWidth: true
        }
    }
}
