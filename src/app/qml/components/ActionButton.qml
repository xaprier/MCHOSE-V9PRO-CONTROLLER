import QtQuick
import QtQuick.Controls.Basic
import V9Control

Button {
    id: root

    // "primary", "secondary" or "danger"
    property string kind: "secondary"
    readonly property color tone: kind === "danger" ? Theme.danger : (kind === "primary" ? Theme.accent : Theme.textMuted)

    hoverEnabled: true
    implicitHeight: 34
    leftPadding: 16
    rightPadding: 16

    contentItem: Text {
        text: root.text
        color: !root.enabled ? Theme.textFaint : (root.kind === "secondary" ? Theme.text : root.tone)
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: !root.enabled ? Theme.surface
             : root.down ? Qt.rgba(root.tone.r, root.tone.g, root.tone.b, 0.32)
             : root.hovered ? Qt.rgba(root.tone.r, root.tone.g, root.tone.b, 0.2)
             : (root.kind === "secondary" ? Theme.surfaceRaised : Qt.rgba(root.tone.r, root.tone.g, root.tone.b, 0.12))
        border.color: root.visualFocus ? Theme.info : (root.kind === "secondary" ? Theme.border : Qt.rgba(root.tone.r, root.tone.g, root.tone.b, root.enabled ? 0.7 : 0.2))

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }
    }
}
