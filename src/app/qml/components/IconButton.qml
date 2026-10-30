import QtQuick
import QtQuick.Controls.Basic
import V9Control

Button {
    id: root

    property color tone: Theme.text
    property bool active: false

    implicitWidth: 38
    implicitHeight: 38
    padding: 8
    display: AbstractButton.IconOnly
    icon.width: 20
    icon.height: 20
    icon.color: root.active ? Theme.accent : root.tone
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radius
        color: root.active ? Theme.accentSoft : (root.down ? Theme.border : (root.hovered ? Theme.surfaceRaised : "transparent"))
        border.color: root.visualFocus ? Theme.accent : "transparent"
    }

    ToolTip.visible: hovered && text.length > 0
    ToolTip.delay: 600
    ToolTip.text: text
}
