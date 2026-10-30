import QtQuick
import QtQuick.Shapes
import V9Control

Item {
    id: root

    property int percent: -1
    property bool charging: false
    property string caption

    readonly property real shownPercent: Math.max(0, percent)
    readonly property color tone: Theme.batteryColor(percent)
    property real animatedPercent: shownPercent
    // 0 -> 1 whenever the ring becomes visible, so it fills up each time the page opens.
    property real intro: 1
    readonly property real displayPercent: root.animatedPercent * root.intro

    implicitWidth: 180
    implicitHeight: 180

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Battery %1%").arg(root.percent)

    onVisibleChanged: if (visible) introAnimation.restart()
    Component.onCompleted: if (visible) introAnimation.restart()

    NumberAnimation {
        id: introAnimation
        target: root
        property: "intro"
        from: 0
        to: 1
        duration: Theme.animSlow * 1.5
        easing.type: Easing.OutCubic
    }

    Behavior on animatedPercent {
        NumberAnimation {
            duration: 700
            easing.type: Easing.OutCubic
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.track
            strokeWidth: 12
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.width / 2 - 10
                radiusY: root.height / 2 - 10
                startAngle: 135
                sweepAngle: 270
            }
        }

        ShapePath {
            strokeColor: root.tone
            strokeWidth: 12
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.width / 2 - 10
                radiusY: root.height / 2 - 10
                startAngle: 135
                sweepAngle: 270 * root.displayPercent / 100
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.percent < 0 ? "—" : qsTr("%1%").arg(Math.round(root.displayPercent))
            color: Theme.text
            font.pixelSize: Theme.fontDisplay
            font.weight: Font.Bold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.caption
            color: root.charging ? Theme.success : Theme.textMuted
            font.pixelSize: Theme.fontSmall
            font.weight: Font.DemiBold
        }
    }
}
