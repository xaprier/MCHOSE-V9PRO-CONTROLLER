import QtQuick
import QtQuick.Layouts
import V9Control

Rectangle {
    id: root

    property string title
    property string subtitle
    default property alias content: body.data

    implicitWidth: body.implicitWidth + 2 * Theme.padding
    implicitHeight: header.implicitHeight + body.implicitHeight + 2 * Theme.padding + (header.visible ? Theme.spacing : 0)
    radius: Theme.radiusLarge
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.padding
        spacing: Theme.spacing

        ColumnLayout {
            id: header
            visible: root.title.length > 0
            spacing: 2
            Layout.fillWidth: true

            Text {
                text: root.title
                color: Theme.text
                font.pixelSize: Theme.fontHeading
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Text {
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            id: body
            spacing: Theme.spacingSmall
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
