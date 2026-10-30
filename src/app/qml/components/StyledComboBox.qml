pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import V9Control

ComboBox {
    id: root

    property string placeholder

    implicitHeight: 34
    implicitWidth: 200
    hoverEnabled: true

    contentItem: Text {
        leftPadding: 12
        rightPadding: root.indicator.width + 8
        text: root.currentIndex < 0 ? root.placeholder : root.displayText
        color: root.currentIndex < 0 ? Theme.textFaint : Theme.text
        font.pixelSize: Theme.fontBody
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: root.width - width - 12
        y: (root.height - height) / 2
        text: "▾"
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.hovered ? Theme.surfaceRaised : Theme.background
        border.color: root.visualFocus || root.popup.visible ? Theme.accent : Theme.border
    }

    delegate: ItemDelegate {
        id: item

        required property int index
        required property var modelData

        width: root.width
        hoverEnabled: true
        highlighted: root.highlightedIndex === item.index

        contentItem: Text {
            text: item.modelData
            color: item.index === root.currentIndex ? Theme.accent : Theme.text
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: item.highlighted ? Theme.surfaceRaised : Theme.clear(Theme.surfaceRaised)
        }
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
        }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
        }
    }
}
