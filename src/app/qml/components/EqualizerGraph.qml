pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import V9Control

// Frequency response of the equalizer with a draggable handle per band.
Item {
    id: root

    property var curve: []
    property var gains: []
    property var labels: []
    property bool active: true
    signal bandMoved(int band, real gain)

    // Vertical scale grows when enhancements push the curve past ±20 dB, but stays put while a
    // handle is dragged so the handle keeps tracking the pointer.
    property real range: 20
    property int activeDrags: 0
    readonly property real curveExtent: root.curve.reduce((m, p) => Math.max(m, Math.abs(p.y)), 0)
    readonly property var bandFrequencies: [32, 60, 100, 250, 500, 1000, 2000, 4000, 8000, 14000]
    readonly property color lineColor: active ? Theme.accent : Theme.textMuted

    function xForPosition(position) {
        return position * width
    }
    function xForFrequency(frequency) {
        return xForPosition((Math.log10(frequency) - Math.log10(20)) / (Math.log10(20000) - Math.log10(20)))
    }
    function yForGain(gain) {
        return height / 2 - Math.max(-range, Math.min(range, gain)) / range * (height / 2 - 12)
    }

    implicitHeight: 240

    Binding on range {
        when: root.activeDrags === 0
        value: Math.max(20, Math.ceil(root.curveExtent / 10) * 10)
        restoreMode: Binding.RestoreNone
    }

    Accessible.role: Accessible.Chart
    Accessible.name: qsTr("Equalizer frequency response")

    Repeater {
        model: [-root.range, -root.range / 2, 0, root.range / 2, root.range]

        delegate: Item {
            id: gridLine

            required property real modelData

            y: root.yForGain(gridLine.modelData)
            width: root.width

            Rectangle {
                width: parent.width
                height: 1
                color: gridLine.modelData === 0 ? Theme.border : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.5)
            }
            Text {
                y: -height - 2
                text: (gridLine.modelData > 0 ? "+" : "") + gridLine.modelData + " dB"
                color: Theme.textFaint
                font.pixelSize: 10
            }
        }
    }

    Repeater {
        model: [100, 1000, 10000]

        delegate: Rectangle {
            id: frequencyLine

            required property int modelData

            x: root.xForFrequency(frequencyLine.modelData)
            width: 1
            height: root.height
            color: Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.5)
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 2.5
            fillColor: Qt.rgba(root.lineColor.r, root.lineColor.g, root.lineColor.b, 0.10)
            joinStyle: ShapePath.RoundJoin

            // The polyline is its own subpath, so the 0 dB end points must be part of it for the
            // fill to close along the baseline.
            PathPolyline {
                path: [Qt.point(0, root.yForGain(0))]
                    .concat(root.curve.map(p => Qt.point(root.xForPosition(p.x), root.yForGain(p.y))))
                    .concat([Qt.point(root.width, root.yForGain(0))])
            }
        }
    }

    Repeater {
        model: root.bandFrequencies.length

        delegate: Rectangle {
            id: handle

            required property int index
            readonly property real gain: root.gains.length > handle.index ? root.gains[handle.index] : 0

            x: root.xForFrequency(root.bandFrequencies[handle.index]) - width / 2
            y: root.yForGain(handle.gain) - height / 2
            width: 14
            height: 14
            radius: 7
            color: drag.active || hover.hovered ? root.lineColor : Theme.surface
            border.color: root.lineColor
            border.width: 2

            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("%1 Hz").arg(root.labels[handle.index] ?? "")

            HoverHandler {
                id: hover
                cursorShape: Qt.SizeVerCursor
            }
            DragHandler {
                id: drag
                xAxis.enabled: false
                target: null
                property real startGain: 0
                onActiveChanged: {
                    root.activeDrags += active ? 1 : -1
                    if (active)
                        drag.startGain = handle.gain
                }
                onTranslationChanged: {
                    const perPixel = root.range / (root.height / 2 - 12)
                    root.bandMoved(handle.index, Math.round((drag.startGain - drag.translation.y * perPixel) * 2) / 2)
                }
            }
        }
    }

    Repeater {
        model: root.labels

        delegate: Text {
            id: bandLabel

            required property int index
            required property string modelData

            x: root.xForFrequency(root.bandFrequencies[bandLabel.index]) - width / 2
            y: root.height - height
            text: bandLabel.modelData
            color: Theme.textFaint
            font.pixelSize: 10
        }
    }
}
