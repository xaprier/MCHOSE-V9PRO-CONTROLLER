import QtQuick
import V9Control

Rectangle {
    id: root

    property string text
    property color tone: Theme.textMuted

    implicitWidth: row.implicitWidth + 20
    implicitHeight: 26
    radius: height / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 7

        Rectangle {
            width: 7
            height: 7
            radius: 3.5
            color: root.tone
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.text
            color: root.tone
            font.pixelSize: Theme.fontSmall
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
