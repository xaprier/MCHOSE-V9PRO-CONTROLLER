pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import V9Control

// Context menu in the app's dark theme. Add MenuItems as usual.
Menu {
    id: root

    padding: 6

    background: Rectangle {
        implicitWidth: 180
        radius: Theme.radius
        color: Theme.surfaceRaised
        border.color: Theme.border
    }

    delegate: MenuItem {
        id: item

        implicitHeight: 32
        leftPadding: 12
        rightPadding: 12

        contentItem: Text {
            text: item.text
            color: item.enabled ? Theme.text : Theme.textFaint
            font.pixelSize: Theme.fontBody
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: item.highlighted ? Theme.accentSoft : "transparent"
        }
    }
}
