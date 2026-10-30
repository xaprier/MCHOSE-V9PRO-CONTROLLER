import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes
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
            scale: root.down ? 0.9 : 1

            Behavior on color {
                ColorAnimation {
                    duration: Theme.animFast
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.animFast
                    easing.type: Easing.OutCubic
                }
            }

            // Drawn rather than ▶/■ glyphs, whose font metrics put them off centre.
            Rectangle {
                visible: root.playing
                anchors.centerIn: parent
                width: 10
                height: 10
                radius: 2
                color: Theme.text
            }
            Shape {
                visible: !root.playing
                // A triangle's visual centre is its centroid, a third of the way in from the flat
                // side, so the box is nudged right by width / 6.
                x: (parent.width - width) / 2 + width / 6
                y: (parent.height - height) / 2
                width: 10
                height: 12
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    fillColor: Theme.text
                    strokeColor: "transparent"
                    startX: 0
                    startY: 0
                    PathLine { x: 10; y: 6 }
                    PathLine { x: 0; y: 12 }
                    PathLine { x: 0; y: 0 }
                }
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

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }
    }
}
