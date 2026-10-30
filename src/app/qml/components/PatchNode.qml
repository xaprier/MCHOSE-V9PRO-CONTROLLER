pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import V9Control

// One PipeWire node in the patchbay: inputs on the left edge, outputs on the right. Positions are
// in the canvas' coordinates; the page moves and wires them.
Rectangle {
    id: root

    required property var node
    // Port id being wired from and its direction, or -1; compatible ports light up.
    property int wiringPort: -1
    property bool wiringFromOutput: false
    property int wiringNode: -1

    readonly property int nodeId: node.id
    readonly property var inputs: node.inputs
    readonly property var outputs: node.outputs
    readonly property int rows: Math.max(inputs.length, outputs.length)
    readonly property real headerHeight: 40
    readonly property real rowHeight: 22
    readonly property color tint: node.kind === "internal" ? Theme.accent
                                : node.kind === "playback" ? Theme.info
                                : node.kind === "capture" ? Theme.success : Theme.textMuted

    signal wireStarted(int portId, bool output, point scenePoint)
    signal wireMoved(point scenePoint)
    signal wireEnded(point scenePoint)
    signal moved(real x, real y)
    // Middle click on a port: drop every cable on it.
    signal portCleared(int portId)

    // Y of a port's dot relative to the box, or -1 when the port is not on this node.
    function portY(portId, output) {
        const list = output ? outputs : inputs
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === portId)
                return headerHeight + i * rowHeight + rowHeight / 2
        }
        return -1
    }
    // Port under a point given in this item's coordinates: {id, output} or null.
    function portAt(px, py) {
        if (py < headerHeight || px < -10 || px > width + 10)
            return null
        const row = Math.floor((py - headerHeight) / rowHeight)
        const list = px < width / 2 ? inputs : outputs
        if (row < 0 || row >= list.length)
            return null
        return { id: list[row].id, output: px >= width / 2 }
    }

    width: 210
    height: headerHeight + rows * rowHeight + 8
    radius: Theme.radiusSmall
    color: Theme.surfaceRaised
    border.color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.55)

    Accessible.role: Accessible.Grouping
    Accessible.name: root.node.label

    Rectangle {
        width: parent.width
        height: root.headerHeight
        radius: Theme.radiusSmall
        color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.18)

        Column {
            anchors.verticalCenter: parent.verticalCenter
            x: 10
            width: parent.width - 20
            spacing: 1

            Text {
                width: parent.width
                textFormat: Text.PlainText
                text: root.node.label
                color: Theme.text
                font.pixelSize: Theme.fontSmall
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            // Which tab or song a browser stream is playing.
            Text {
                visible: root.node.media.length > 0
                width: parent.width
                textFormat: Text.PlainText
                text: root.node.media
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall - 1
                elide: Text.ElideRight
            }
        }
        // media.name is a web page title: never let it be read as rich text.
        ToolTip {
            visible: headerHover.hovered && root.node.media.length > 0
            delay: 500
            contentItem: Text {
                textFormat: Text.PlainText
                text: root.node.media
                color: Theme.text
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.surfaceRaised
                border.color: Theme.border
            }
        }
        DragHandler {
            target: root
            cursorShape: Qt.ClosedHandCursor
            onActiveChanged: if (!active) root.moved(root.x, root.y)
        }
        HoverHandler {
            id: headerHover
            cursorShape: Qt.OpenHandCursor
        }
    }

    Repeater {
        model: [
            { list: root.inputs, output: false },
            { list: root.outputs, output: true }
        ]

        delegate: Item {
            id: side

            required property var modelData

            anchors.fill: parent

            Repeater {
                model: side.modelData.list

                delegate: Item {
                    id: port

                    required property int index
                    required property var modelData
                    readonly property bool output: side.modelData.output
                    readonly property bool compatible: root.wiringPort >= 0 && root.wiringNode !== root.nodeId
                                                       && root.wiringFromOutput !== port.output
                    readonly property bool active: root.wiringPort === port.modelData.id || portMouse.containsMouse

                    x: port.output ? root.width / 2 : 0
                    y: root.headerHeight + port.index * root.rowHeight
                    width: root.width / 2
                    height: root.rowHeight

                    Rectangle {
                        x: port.output ? parent.width - width / 2 : -width / 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: port.active || port.compatible ? 12 : 9
                        height: width
                        radius: width / 2
                        color: port.active ? Theme.text : (port.compatible ? root.tint : Theme.surface)
                        border.color: root.tint
                        border.width: 2
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: port.output ? parent.width - width - 12 : 12
                        width: parent.width - 16
                        horizontalAlignment: port.output ? Text.AlignRight : Text.AlignLeft
                        textFormat: Text.PlainText
                        text: port.modelData.name
                        color: port.active ? Theme.text : Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: portMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        preventStealing: true
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                        cursorShape: Qt.CrossCursor
                        onPressed: mouse => {
                            if (mouse.button === Qt.MiddleButton)
                                root.portCleared(port.modelData.id)
                            else
                                root.wireStarted(port.modelData.id, port.output, port.mapToItem(null, mouse.x, mouse.y))
                        }
                        onPositionChanged: mouse => {
                            if (pressedButtons & Qt.LeftButton)
                                root.wireMoved(port.mapToItem(null, mouse.x, mouse.y))
                        }
                        onReleased: mouse => {
                            if (mouse.button === Qt.LeftButton)
                                root.wireEnded(port.mapToItem(null, mouse.x, mouse.y))
                        }
                    }
                }
            }
        }
    }
}
