import QtQuick
import QtQuick.Controls.Basic
import V9Control

TextField {
    id: root

    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accent
    font.pixelSize: Theme.fontBody

    background: Rectangle {
        radius: Theme.radius
        color: Theme.background
        border.color: root.activeFocus ? Theme.accent : Theme.border
    }
}
