pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import V9Control

// Row of mutually exclusive options. `options` is a list of { text, value, enabled? }.
Rectangle {
    id: root

    property var options: []
    property var value
    signal activated(var value)

    implicitWidth: row.implicitWidth + 6
    implicitHeight: 34
    radius: Theme.radius
    color: Theme.background
    border.color: Theme.border

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: root.options

            delegate: AbstractButton {
                id: option

                required property var modelData
                readonly property bool selected: root.value === option.modelData.value

                text: option.modelData.text
                enabled: option.modelData.enabled === undefined || option.modelData.enabled
                hoverEnabled: true
                implicitHeight: 28
                leftPadding: 14
                rightPadding: 14
                Accessible.role: Accessible.RadioButton
                Accessible.checked: selected
                onClicked: root.activated(option.modelData.value)

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: option.selected ? Theme.accentSoft : (option.hovered ? Theme.surfaceRaised : Theme.clear(Theme.surfaceRaised))
                    border.color: option.selected ? Theme.accent : (option.visualFocus ? Theme.info : "transparent")
                }
                contentItem: Text {
                    text: option.text
                    color: !option.enabled ? Theme.textFaint : (option.selected ? Theme.accent : Theme.textMuted)
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
