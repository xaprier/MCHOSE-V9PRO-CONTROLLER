import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import V9Control

// Yes/no question in the app's dark theme. Emits accepted() / rejected() like Dialog.
Dialog {
    id: root

    property string text
    property string confirmText: qsTr("OK")
    property bool destructive: false

    anchors.centerIn: Overlay.overlay
    modal: true
    padding: Theme.padding
    width: Math.min(440, parent ? parent.width - 40 : 440)

    Overlay.modal: Rectangle {
        color: "#99000000"
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.surface
        border.color: Theme.border
    }

    header: Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontHeading
        font.weight: Font.DemiBold
        leftPadding: Theme.padding
        rightPadding: Theme.padding
        topPadding: Theme.padding
        wrapMode: Text.WordWrap
    }

    contentItem: Text {
        text: root.text
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
        wrapMode: Text.WordWrap
    }

    footer: RowLayout {
        spacing: Theme.spacingSmall

        Item {
            Layout.fillWidth: true
        }
        ActionButton {
            text: qsTr("Cancel")
            Layout.bottomMargin: Theme.padding
            onClicked: root.reject()
        }
        ActionButton {
            text: root.confirmText
            kind: root.destructive ? "danger" : "primary"
            Layout.rightMargin: Theme.padding
            Layout.bottomMargin: Theme.padding
            onClicked: root.accept()
        }
    }
}
