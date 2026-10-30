pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import V9Control

// MCHOSE HUB style band editor: bands evenly spaced, a smooth line through the band points and the
// value under each point. Press anywhere in the plot to grab the nearest band and drag it.
Item {
    id: root

    property var gains: []
    property var labels: []
    property bool active: true
    signal bandMoved(int band, real gain)

    readonly property real range: 20
    readonly property int bandCount: root.labels.length
    readonly property color lineColor: active ? Theme.accent : Theme.textMuted

    readonly property real plotLeft: 24
    readonly property real plotRight: width - 56
    readonly property real plotTop: 34
    readonly property real plotBottom: height - 30
    readonly property real step: bandCount > 1 ? (plotRight - plotLeft) / (bandCount - 1) : 0

    function xForBand(band) {
        return plotLeft + band * step
    }
    function yForGain(gain) {
        const g = Math.max(-range, Math.min(range, gain))
        return plotTop + (range - g) / (2 * range) * (plotBottom - plotTop)
    }
    function gainForY(y) {
        const g = range - (y - plotTop) / (plotBottom - plotTop) * 2 * range
        return Math.round(Math.max(-range, Math.min(range, g)) * 2) / 2
    }
    function gainText(gain) {
        return gain > 0 ? "+" + gain : String(gain)
    }

    // Monotone cubic (Fritsch–Carlson) through the band points: smooth, and it never overshoots a
    // point, so the line does not suggest boosts that are not there.
    readonly property var linePoints: {
        const n = root.bandCount
        if (n < 2 || root.gains.length < n || root.width <= 0)
            return []
        const ys = []
        for (let i = 0; i < n; ++i)
            ys.push(root.yForGain(root.gains[i]))
        const d = []
        for (let i = 0; i < n - 1; ++i)
            d.push((ys[i + 1] - ys[i]) / root.step)
        const m = [d[0]]
        for (let i = 1; i < n - 1; ++i)
            m.push(d[i - 1] * d[i] <= 0 ? 0 : (d[i - 1] + d[i]) / 2)
        m.push(d[n - 2])
        for (let i = 0; i < n - 1; ++i) {
            if (d[i] === 0) {
                m[i] = 0
                m[i + 1] = 0
                continue
            }
            const a = m[i] / d[i]
            const b = m[i + 1] / d[i]
            const s = a * a + b * b
            if (s > 9) {
                const t = 3 / Math.sqrt(s)
                m[i] = t * a * d[i]
                m[i + 1] = t * b * d[i]
            }
        }
        const points = []
        const samples = 12
        for (let i = 0; i < n - 1; ++i) {
            for (let k = 0; k < samples; ++k) {
                const t = k / samples
                const t2 = t * t
                const t3 = t2 * t
                const y = (2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * root.step * m[i]
                        + (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * root.step * m[i + 1]
                points.push(Qt.point(root.xForBand(i) + t * root.step, y))
            }
        }
        points.push(Qt.point(root.xForBand(n - 1), ys[n - 1]))
        return points
    }

    implicitHeight: 320

    Accessible.role: Accessible.Chart
    Accessible.name: qsTr("Equalizer bands")

    // Bass / Mid / High header with dividers between 250–500 Hz and 2k–4k Hz.
    Repeater {
        model: [
            { text: qsTr("Bass"), from: -0.5, to: 3.5 },
            { text: qsTr("Mid"), from: 3.5, to: 6.5 },
            { text: qsTr("High"), from: 6.5, to: 9.5 }
        ]

        delegate: Item {
            id: region

            required property var modelData

            x: Math.max(0, root.xForBand(region.modelData.from))
            width: Math.min(root.plotRight + root.step / 2, root.xForBand(region.modelData.to)) - x
            height: 22

            Rectangle {
                width: 1
                height: parent.height
                color: Theme.border
            }
            Rectangle {
                x: parent.width - 1
                width: 1
                height: parent.height
                color: Theme.border
            }
            Text {
                anchors.centerIn: parent
                text: region.modelData.text
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    Repeater {
        model: [20, 15, 10, 5, 0, -5, -10, -15, -20]

        delegate: Item {
            id: gridLine

            required property int modelData

            x: root.plotLeft - 12
            y: root.yForGain(gridLine.modelData)
            width: root.plotRight - root.plotLeft + 24

            Rectangle {
                width: parent.width
                height: 1
                color: gridLine.modelData === 0 ? Theme.border : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.55)
            }
            Text {
                visible: gridLine.modelData % 10 === 0
                x: parent.width + 8
                anchors.verticalCenter: parent.top
                text: root.gainText(gridLine.modelData) + " dB"
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillColor: Qt.rgba(root.lineColor.r, root.lineColor.g, root.lineColor.b, 0.08)

            // The polyline is its own subpath, so the 0 dB end points must be part of it for the
            // fill to close along the baseline.
            PathPolyline {
                path: root.linePoints.length === 0 ? []
                    : [Qt.point(root.plotLeft, root.yForGain(0))].concat(root.linePoints, [Qt.point(root.plotRight, root.yForGain(0))])
            }
        }
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 2.5
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap

            PathPolyline {
                path: root.linePoints
            }
        }
    }

    MouseArea {
        id: plotArea

        property int band: -1

        x: root.plotLeft - root.step / 2
        y: root.plotTop - 12
        width: root.plotRight - root.plotLeft + root.step
        height: root.plotBottom - root.plotTop + 24
        cursorShape: plotArea.band >= 0 ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        preventStealing: true

        onPressed: mouse => {
            plotArea.band = Math.max(0, Math.min(root.bandCount - 1, Math.round((mouse.x + plotArea.x - root.plotLeft) / root.step)))
            root.bandMoved(plotArea.band, root.gainForY(mouse.y + plotArea.y))
            handles.itemAt(plotArea.band)?.forceActiveFocus()
        }
        onPositionChanged: mouse => {
            if (plotArea.band >= 0)
                root.bandMoved(plotArea.band, root.gainForY(mouse.y + plotArea.y))
        }
        onReleased: plotArea.band = -1
        onCanceled: plotArea.band = -1
    }

    Repeater {
        id: handles

        model: root.bandCount

        delegate: Item {
            id: handle

            required property int index
            readonly property real gain: root.gains.length > handle.index ? root.gains[handle.index] : 0
            readonly property bool highlighted: plotArea.band === handle.index || handle.activeFocus

            x: root.xForBand(handle.index)
            y: root.yForGain(handle.gain)
            activeFocusOnTab: true

            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("%1 Hz").arg(root.labels[handle.index] ?? "")
            Accessible.description: qsTr("%1 dB").arg(handle.gain)

            Keys.onUpPressed: root.bandMoved(handle.index, handle.gain + 0.5)
            Keys.onDownPressed: root.bandMoved(handle.index, handle.gain - 0.5)
            Keys.onPressed: event => {
                if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                    root.bandMoved(handle.index, handle.gain + (event.key === Qt.Key_PageUp ? 3 : -3))
                    event.accepted = true
                } else if (event.key === Qt.Key_0 || event.key === Qt.Key_Home) {
                    root.bandMoved(handle.index, 0)
                    event.accepted = true
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: handle.highlighted ? 18 : 15
                height: width
                radius: width / 2
                color: handle.highlighted ? Theme.text : root.lineColor
                border.color: root.lineColor
                border.width: handle.highlighted ? 4 : 0
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 12
                text: root.gainText(handle.gain)
                color: handle.highlighted ? Theme.text : Theme.textMuted
                font.pixelSize: Theme.fontSmall
                font.weight: handle.highlighted ? Font.DemiBold : Font.Normal
            }
        }
    }

    Repeater {
        model: root.labels

        delegate: Text {
            id: bandLabel

            required property int index
            required property string modelData

            x: root.xForBand(bandLabel.index) - width / 2
            y: root.height - height
            text: root.step >= 52 ? qsTr("%1Hz").arg(bandLabel.modelData) : bandLabel.modelData
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
        }
    }
}
