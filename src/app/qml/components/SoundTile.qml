import QtQuick
import QtQuick.Controls.Basic
import V9Control

// One soundboard pad: click to play, click again to stop.
AbstractButton {
    id: root

    property bool playing: false

    implicitWidth: 160
    implicitHeight: 64
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: root.playing ? qsTr("Stop %1").arg(root.text) : qsTr("Play %1").arg(root.text)

    contentItem: Row {
        spacing: 10
        leftPadding: 12
        rightPadding: 12

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            radius: 14
            color: root.playing ? Theme.accent : Theme.track

            Text {
                anchors.centerIn: parent
                text: root.playing ? "■" : "▶"
                color: Theme.text
                font.pixelSize: root.playing ? 11 : 12
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: root.width - 28 - 10 - 24
            // File names come from disk; never interpret them as markup.
            textFormat: Text.PlainText
            text: root.text
            color: root.enabled ? Theme.text : Theme.textFaint
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.Wrap
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.playing ? Theme.accentSoft : (root.hovered && root.enabled ? Theme.surfaceRaised : Theme.surface)
        border.color: root.playing ? Theme.accent : Theme.border
    }
}
