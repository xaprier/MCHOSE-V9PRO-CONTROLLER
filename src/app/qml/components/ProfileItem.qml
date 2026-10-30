import QtQuick
import QtQuick.Controls.Basic
import V9Control

// One selectable row of the equalizer profile list.
ItemDelegate {
    id: root

    property bool modified: false

    implicitHeight: 40
    leftPadding: 14
    rightPadding: 14

    contentItem: Text {
        text: root.modified ? qsTr("%1 · modified").arg(root.text) : root.text
        color: root.checked ? Theme.text : Theme.textMuted
        font.pixelSize: Theme.fontBody
        font.weight: root.checked ? Font.DemiBold : Font.Normal
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.radius
        color: root.checked ? Theme.accentSoft : (root.hovered ? Theme.surfaceRaised : "transparent")
        border.color: root.checked ? Theme.accent : "transparent"
    }
}
