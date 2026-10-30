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

    implicitWidth: 180
    implicitHeight: 180

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Battery %1%").arg(root.percent)

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
                sweepAngle: 270 * root.animatedPercent / 100
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.percent < 0 ? "—" : qsTr("%1%").arg(Math.round(root.animatedPercent))
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
