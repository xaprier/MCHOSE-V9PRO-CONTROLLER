import QtQuick
import QtQuick.Layouts
import V9Control

Rectangle {
    id: root

    property string title
    property string text
    property color tone: Theme.warning

    implicitHeight: column.implicitHeight + 24
    radius: Theme.radius
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.1)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.45)

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title

    Rectangle {
        width: 4
        radius: 2
        color: root.tone
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 8
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 14
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 4

        Text {
            text: root.title
            visible: text.length > 0
            color: root.tone
            font.pixelSize: Theme.fontBody
            font.weight: Font.Bold
            Layout.fillWidth: true
        }
        Text {
            text: root.text
            color: Theme.text
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            lineHeight: 1.2
            Layout.fillWidth: true
        }
    }
}
