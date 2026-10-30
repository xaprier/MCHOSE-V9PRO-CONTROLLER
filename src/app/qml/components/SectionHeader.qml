import QtQuick
import QtQuick.Layouts
import V9Control

// Divider plus a small heading that splits a card into sections.
ColumnLayout {
    id: root

    property string title
    property string description

    spacing: 2
    Layout.fillWidth: true
    Layout.topMargin: Theme.spacingSmall

    Rectangle {
        color: Theme.border
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        Layout.bottomMargin: Theme.spacingSmall
    }
    Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        Layout.fillWidth: true
    }
    Text {
        visible: root.description.length > 0
        text: root.description
        color: Theme.textFaint
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
