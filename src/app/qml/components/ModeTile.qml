import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import V9Control

AbstractButton {
    id: root

    property string description

    checkable: true
    hoverEnabled: true
    implicitWidth: 180
    implicitHeight: 118
    padding: 16
    icon.width: 26
    icon.height: 26

    Accessible.role: Accessible.RadioButton
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radius
        color: root.checked ? Theme.accentSoft : (root.hovered ? Theme.surfaceRaised : Theme.surface)
        border.color: root.checked ? Theme.accent : (root.visualFocus ? Theme.info : Theme.border)
        border.width: root.checked ? 2 : 1

        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 6

        Image {
            source: root.icon.source
            sourceSize.width: root.icon.width
            sourceSize.height: root.icon.height
            Layout.preferredWidth: root.icon.width
            Layout.preferredHeight: root.icon.height
            opacity: root.checked ? 1.0 : 0.7
            Accessible.ignored: true
        }
        Item {
            Layout.fillHeight: true
        }
        Text {
            text: root.text
            color: root.checked ? Theme.accent : Theme.text
            font.pixelSize: Theme.fontHeading
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }
        Text {
            text: root.description
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
