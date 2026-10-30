pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import V9Control

// qjackctl-style graph of PipeWire audio: drag from a port to another to connect. A cable goes with
// a middle click, Delete after selecting it, or its right-click menu; a middle click on a port drops
// all of its cables. Empty space pans, the wheel zooms. Reset brings the default routing back.
Item {
    id: root

    property real zoom: 1.0
    property int selectedLink: -1
    // Wire being drawn: start port, its direction and node, and the loose end in canvas coordinates.
    property int wirePort: -1
    property bool wireFromOutput: false
    property int wireNode: -1
    property point wireStart
    property point wireEnd
    // Bumped whenever node delegates are created or destroyed, so cables re-resolve their ends.
    property int nodeRevision: 0
    // Keep everything in view until the user pans, zooms or moves a box; "Fit" turns it back on.
    property bool autoFit: true

    readonly property var nodeIndex: {
        const index = {}
        for (let i = 0; i < Patchbay.nodes.length; ++i)
            index[Patchbay.nodes[i].id] = i
        return index
    }

    function nodeItem(nodeId) {
        root.nodeRevision // dependency
        const i = root.nodeIndex[nodeId]
        return i === undefined ? null : nodeRepeater.itemAt(i)
    }
    function toCanvas(scenePoint) {
        return canvas.mapFromItem(null, scenePoint.x, scenePoint.y)
    }
    function portAtCanvas(p) {
        for (let i = 0; i < nodeRepeater.count; ++i) {
            const item = nodeRepeater.itemAt(i)
            if (!item)
                continue
            const hit = item.portAt(p.x - item.x, p.y - item.y)
            if (hit)
                return hit
        }
        return null
    }
    function cableEnds(link) {
        const outItem = root.nodeItem(link.outNode)
        const inItem = root.nodeItem(link.inNode)
        if (!outItem || !inItem)
            return null
        const oy = outItem.portY(link.outPort, true)
        const iy = inItem.portY(link.inPort, false)
        if (oy < 0 || iy < 0)
            return null
        return { a: Qt.point(outItem.x + outItem.width, outItem.y + oy), b: Qt.point(inItem.x, inItem.y + iy) }
    }
    function bezierPoint(a, b, t) {
        const dx = Math.max(60, Math.abs(b.x - a.x) / 2)
        const c1 = Qt.point(a.x + dx, a.y)
        const c2 = Qt.point(b.x - dx, b.y)
        const u = 1 - t
        return Qt.point(u * u * u * a.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * b.x,
                        u * u * u * a.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * b.y)
    }
    // Link id of the cable nearest to a canvas point (within a few pixels), or -1.
    function linkAt(p) {
        let best = -1
        let bestDistance = 7 / root.zoom
        for (const link of Patchbay.links) {
            const ends = root.cableEnds(link)
            if (!ends)
                continue
            for (let k = 0; k <= 24; ++k) {
                const q = root.bezierPoint(ends.a, ends.b, k / 24)
                const d = Math.hypot(q.x - p.x, q.y - p.y)
                if (d < bestDistance) {
                    bestDistance = d
                    best = link.id
                }
            }
        }
        return best
    }
    function requestDisconnect(links) {
        root.selectedLink = -1
        for (const link of links)
            Patchbay.disconnectLink(link.id)
    }
    function linksOf(linkId) {
        return Patchbay.links.filter(l => l.id === linkId)
    }
    function setZoom(value, anchor) {
        root.autoFit = false
        const z = Math.max(0.4, Math.min(2.0, value))
        const before = canvas.mapFromItem(viewport, anchor.x, anchor.y)
        root.zoom = z
        canvas.x = anchor.x - before.x * z
        canvas.y = anchor.y - before.y * z
    }
    function fit() {
        if (nodeRepeater.count === 0 || viewport.width <= 0 || viewport.height <= 0)
            return
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity
        for (let i = 0; i < nodeRepeater.count; ++i) {
            const item = nodeRepeater.itemAt(i)
            if (!item)
                continue
            minX = Math.min(minX, item.x)
            minY = Math.min(minY, item.y)
            maxX = Math.max(maxX, item.x + item.width)
            maxY = Math.max(maxY, item.y + item.height)
        }
        const margin = 30
        root.zoom = Math.max(0.4, Math.min(1.2, Math.min((viewport.width - 2 * margin) / (maxX - minX),
                                                         (viewport.height - 2 * margin) / (maxY - minY))))
        canvas.x = margin - minX * root.zoom + Math.max(0, (viewport.width - 2 * margin - (maxX - minX) * root.zoom) / 2)
        canvas.y = margin - minY * root.zoom
    }

    focus: true
    Keys.onDeletePressed: root.requestDisconnect(root.linksOf(root.selectedLink))
    Keys.onEscapePressed: root.selectedLink = -1

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        RowLayout {
            spacing: Theme.spacing
            Layout.fillWidth: true

            ToggleSwitch {
                text: qsTr("Restore links")
                checked: Patchbay.restoreLinks
                enabled: Patchbay.available
                onToggled: Patchbay.restoreLinks = checked
            }
            Text {
                text: qsTr("Re-creates links you made here when a program comes back.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ActionButton {
                text: "−"
                Accessible.name: qsTr("Zoom out")
                onClicked: root.setZoom(root.zoom / 1.2, Qt.point(viewport.width / 2, viewport.height / 2))
            }
            Text {
                text: qsTr("%1%").arg(Math.round(root.zoom * 100))
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: 40
            }
            ActionButton {
                text: "+"
                Accessible.name: qsTr("Zoom in")
                onClicked: root.setZoom(root.zoom * 1.2, Qt.point(viewport.width / 2, viewport.height / 2))
            }
            ActionButton {
                text: qsTr("Reset")
                enabled: Patchbay.available
                onClicked: resetDialog.open()
            }
            ActionButton {
                text: qsTr("Fit")
                onClicked: {
                    root.autoFit = true
                    root.fit()
                }
            }
        }

        Flow {
            visible: Patchbay.restoreLinks && Patchbay.rememberedLinks.length > 0
            spacing: Theme.spacingSmall
            Layout.fillWidth: true

            Text {
                text: qsTr("Remembered:")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                height: 26
                verticalAlignment: Text.AlignVCenter
            }
            Repeater {
                model: Patchbay.rememberedLinks

                delegate: Rectangle {
                    id: chip

                    required property int index
                    required property string modelData

                    width: chipRow.implicitWidth + 16
                    height: 26
                    radius: 13
                    color: Theme.surfaceRaised
                    border.color: Theme.border

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            textFormat: Text.PlainText
                            text: chip.modelData
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "✕"
                            color: forgetMouse.containsMouse ? Theme.danger : Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            anchors.verticalCenter: parent.verticalCenter

                            MouseArea {
                                id: forgetMouse
                                anchors.fill: parent
                                anchors.margins: -4
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Forget %1").arg(chip.modelData)
                                onClicked: Patchbay.forgetRule(chip.index)
                            }
                        }
                    }
                }
            }
        }

        Text {
            visible: Patchbay.error.length > 0
            textFormat: Text.PlainText
            text: Patchbay.error
            color: Theme.danger
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            id: viewport

            radius: Theme.radiusLarge
            color: Theme.surface
            border.color: Theme.border
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true

            onWidthChanged: if (root.autoFit) Qt.callLater(root.fit)
            onHeightChanged: if (root.autoFit) Qt.callLater(root.fit)

            // Pan, cable selection and the context menu live on the background; nodes sit above it.
            MouseArea {
                id: background

                property point pressPoint
                property point canvasStart
                property bool panned: false

                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                cursorShape: pressed && panned ? Qt.ClosedHandCursor : Qt.ArrowCursor

                onPressed: mouse => {
                    root.forceActiveFocus()
                    if (mouse.button === Qt.MiddleButton) {
                        root.requestDisconnect(root.linksOf(root.linkAt(canvas.mapFromItem(background, mouse.x, mouse.y))))
                        return
                    }
                    background.pressPoint = Qt.point(mouse.x, mouse.y)
                    background.canvasStart = Qt.point(canvas.x, canvas.y)
                    background.panned = false
                }
                onPositionChanged: mouse => {
                    if (!(background.pressedButtons & (Qt.LeftButton | Qt.RightButton)))
                        return
                    const dx = mouse.x - background.pressPoint.x
                    const dy = mouse.y - background.pressPoint.y
                    if (!background.panned && Math.hypot(dx, dy) < 4)
                        return
                    background.panned = true
                    root.autoFit = false
                    canvas.x = background.canvasStart.x + dx
                    canvas.y = background.canvasStart.y + dy
                }
                onReleased: mouse => {
                    if (background.panned || mouse.button === Qt.MiddleButton)
                        return
                    root.selectedLink = root.linkAt(canvas.mapFromItem(background, mouse.x, mouse.y))
                    if (mouse.button === Qt.RightButton && root.selectedLink >= 0)
                        linkMenu.popup()
                }
                onWheel: wheel => root.setZoom(root.zoom * (wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1), Qt.point(wheel.x, wheel.y))
            }

            Item {
                id: canvas

                scale: root.zoom
                transformOrigin: Item.TopLeft

                Repeater {
                    model: Patchbay.links

                    delegate: Shape {
                        id: cable

                        required property var modelData
                        readonly property var ends: root.cableEnds(cable.modelData)
                        readonly property bool selected: root.selectedLink === cable.modelData.id
                        readonly property var outItem: root.nodeItem(cable.modelData.outNode)
                        readonly property color tint: cable.outItem ? cable.outItem.tint : Theme.textMuted
                        // Re-evaluate while boxes are dragged.
                        readonly property real dependency: (cable.outItem ? cable.outItem.x + cable.outItem.y : 0)
                                                           + (root.nodeItem(cable.modelData.inNode)?.x ?? 0)
                                                           + (root.nodeItem(cable.modelData.inNode)?.y ?? 0)
                        readonly property point a: (cable.dependency, cable.ends ? cable.ends.a : Qt.point(0, 0))
                        readonly property point b: (cable.dependency, cable.ends ? cable.ends.b : Qt.point(0, 0))
                        readonly property real dx: Math.max(60, Math.abs(cable.b.x - cable.a.x) / 2)

                        visible: cable.ends !== null
                        preferredRendererType: Shape.CurveRenderer

                        ShapePath {
                            strokeColor: cable.selected ? Theme.text
                                       : Qt.rgba(cable.tint.r, cable.tint.g, cable.tint.b, cable.modelData.internal ? 0.45 : 0.9)
                            strokeWidth: cable.selected ? 3.5 : 2
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            startX: cable.a.x
                            startY: cable.a.y

                            PathCubic {
                                control1X: cable.a.x + cable.dx
                                control1Y: cable.a.y
                                control2X: cable.b.x - cable.dx
                                control2Y: cable.b.y
                                x: cable.b.x
                                y: cable.b.y
                            }
                        }
                    }
                }

                Shape {
                    id: wire

                    visible: root.wirePort >= 0
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: Theme.text
                        strokeWidth: 2
                        strokeStyle: ShapePath.DashLine
                        fillColor: "transparent"
                        startX: root.wireStart.x
                        startY: root.wireStart.y

                        PathCubic {
                            readonly property real dx: Math.max(60, Math.abs(root.wireEnd.x - root.wireStart.x) / 2) * (root.wireFromOutput ? 1 : -1)
                            control1X: root.wireStart.x + dx
                            control1Y: root.wireStart.y
                            control2X: root.wireEnd.x - dx
                            control2Y: root.wireEnd.y
                            x: root.wireEnd.x
                            y: root.wireEnd.y
                        }
                    }
                }

                Repeater {
                    id: nodeRepeater

                    model: Patchbay.nodes
                    onItemAdded: root.nodeRevision++
                    onItemRemoved: root.nodeRevision++

                    delegate: PatchNode {
                        id: patchNode

                        required property var modelData

                        node: patchNode.modelData
                        x: patchNode.modelData.x
                        y: patchNode.modelData.y
                        wiringPort: root.wirePort
                        wiringFromOutput: root.wireFromOutput
                        wiringNode: root.wireNode

                        onMoved: (x, y) => {
                            root.autoFit = false
                            Patchbay.setNodePosition(patchNode.nodeId, x, y)
                        }
                        onWireStarted: (portId, output, scenePoint) => {
                            root.selectedLink = -1
                            root.wirePort = portId
                            root.wireFromOutput = output
                            root.wireNode = patchNode.nodeId
                            const y = patchNode.y + patchNode.portY(portId, output)
                            root.wireStart = Qt.point(output ? patchNode.x + patchNode.width : patchNode.x, y)
                            root.wireEnd = root.toCanvas(scenePoint)
                        }
                        onPortCleared: portId => root.requestDisconnect(Patchbay.links.filter(l => l.outPort === portId || l.inPort === portId))
                        onWireMoved: scenePoint => root.wireEnd = root.toCanvas(scenePoint)
                        onWireEnded: scenePoint => {
                            const target = root.portAtCanvas(root.toCanvas(scenePoint))
                            if (target && target.output !== root.wireFromOutput)
                                Patchbay.connectPorts(root.wirePort, target.id)
                            root.wirePort = -1
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: nodeRepeater.count === 0
                text: Patchbay.available ? qsTr("Waiting for PipeWire…") : qsTr("pw-dump and pw-link are needed for the patchbay.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
            }
        }
    }

    StyledMenu {
        id: linkMenu

        MenuItem {
            text: qsTr("Disconnect")
            onTriggered: root.requestDisconnect(root.linksOf(root.selectedLink))
        }
    }

    ConfirmDialog {
        id: resetDialog

        title: qsTr("Reset the patchbay?")
        text: qsTr("Removes every cable drawn by hand, forgets remembered links and box positions, and sends all programs back to the default output and input. The equalizer, noise suppression and monitor chains are kept.")
        confirmText: qsTr("Reset")
        destructive: true

        onAccepted: {
            Patchbay.resetToDefaults()
            root.autoFit = true
        }
    }

    Connections {
        target: Patchbay
        function onGraphChanged() {
            if (root.autoFit)
                Qt.callLater(root.fit)
        }
    }
}
