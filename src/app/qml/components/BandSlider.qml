import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import V9Control

// Vertical gain slider for one equalizer band, ±20 dB.
ColumnLayout {
    id: root

    property string label
    property real value: 0
    signal moved(real value)

    spacing: 6

    Text {
        text: (root.value > 0 ? "+" : "") + root.value.toFixed(1)
        color: root.value === 0 ? Theme.textMuted : Theme.text
        font.pixelSize: Theme.fontSmall
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }

    Slider {
        id: slider
        orientation: Qt.Vertical
        from: -20
        to: 20
        stepSize: 0.5
        value: root.value
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 28
        Accessible.name: qsTr("%1 Hz band").arg(root.label)
        onMoved: root.moved(value)

        background: Item {
            x: slider.leftPadding + slider.availableWidth / 2 - 3
            y: slider.topPadding
            implicitWidth: 6
            implicitHeight: 160
            width: 6
            height: slider.availableHeight

            Rectangle {
                anchors.fill: parent
                radius: 3
                color: Theme.track
            }
            // Fill from the 0 dB line towards the handle.
            Rectangle {
                readonly property real zeroY: parent.height / 2
                readonly property real handleY: slider.visualPosition * parent.height
                y: Math.min(zeroY, handleY)
                width: parent.width
                height: Math.abs(handleY - zeroY)
                radius: 3
                color: Theme.accent
            }
            Rectangle {
                y: parent.height / 2 - 0.5
                x: -5
                width: parent.width + 10
                height: 1
                color: Theme.textFaint
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.availableWidth / 2 - width / 2
            y: slider.topPadding + slider.visualPosition * (slider.availableHeight - height)
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: slider.pressed ? Theme.accent : Theme.text
            border.color: slider.visualFocus ? Theme.accent : Theme.surface
            border.width: 3
        }
    }

    Text {
        text: root.label
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
}
