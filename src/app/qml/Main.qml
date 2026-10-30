import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import V9Control

ApplicationWindow {
    id: window

    width: 1100
    height: 700
    // Wide enough for the Home page's two columns (content >= 720 px next to the sidebar).
    minimumWidth: 1000
    minimumHeight: 640
    title: qsTr("V9 Pro Control")
    color: Theme.background

    property int currentPage: 0
    // Set from C++ when a tray icon exists: closing then hides the window and the app keeps running.
    property bool hideOnClose: false

    onClosing: close => {
        if (window.hideOnClose) {
            close.accepted = false
            window.hide()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            color: Theme.sidebar
            Layout.preferredWidth: 220
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: 4

                RowLayout {
                    spacing: 10
                    Layout.bottomMargin: Theme.spacing
                    Layout.leftMargin: 6

                    Rectangle {
                        radius: Theme.radius
                        color: Theme.accentSoft
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38

                        Image {
                            anchors.centerIn: parent
                            source: "icons/headset.svg"
                            sourceSize.width: 22
                            sourceSize.height: 22
                            Accessible.ignored: true
                        }
                    }
                    ColumnLayout {
                        spacing: 0

                        Text {
                            text: qsTr("V9 Pro")
                            color: Theme.text
                            font.pixelSize: Theme.fontHeading
                            font.weight: Font.Bold
                        }
                        Text {
                            text: qsTr("Control")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }

                NavButton {
                    text: qsTr("Home")
                    icon.source: Qt.resolvedUrl("icons/headset.svg")
                    checked: window.currentPage === 0
                    onClicked: window.currentPage = 0
                    Layout.fillWidth: true
                }
                NavButton {
                    text: qsTr("Equalizer")
                    icon.source: Qt.resolvedUrl("icons/equalizer.svg")
                    checked: window.currentPage === 1
                    onClicked: window.currentPage = 1
                    Layout.fillWidth: true
                }
                NavButton {
                    text: qsTr("Patchbay")
                    icon.source: Qt.resolvedUrl("icons/patch.svg")
                    checked: window.currentPage === 2
                    onClicked: window.currentPage = 2
                    Layout.fillWidth: true
                }
                NavButton {
                    text: qsTr("Soundboard")
                    icon.source: Qt.resolvedUrl("icons/soundboard.svg")
                    checked: window.currentPage === 3
                    onClicked: window.currentPage = 3
                    Layout.fillWidth: true
                }
                NavButton {
                    text: qsTr("About")
                    icon.source: Qt.resolvedUrl("icons/info.svg")
                    checked: window.currentPage === 4
                    onClicked: window.currentPage = 4
                    Layout.fillWidth: true
                }

                Item {
                    Layout.fillHeight: true
                }

                StatusPill {
                    text: Headset.dongleConnected ? (Headset.headsetOnline ? qsTr("Connected · 2.4G") : qsTr("Headset offline")) : qsTr("Dongle not found")
                    tone: Headset.dongleConnected ? (Headset.headsetOnline ? Theme.success : Theme.warning) : Theme.danger
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        StackLayout {
            id: pages

            currentIndex: window.currentPage
            // The page coming in fades up and settles from a slight zoom.
            onCurrentIndexChanged: {
                const page = pages.children[pages.currentIndex]
                if (!page)
                    return
                pageIn.stop()
                pageFade.target = page
                pageZoom.target = page
                pageIn.start()
            }
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24

            HomePage {
                onOpenEqualizer: window.currentPage = 1
            }
            EqualizerPage {}
            PatchbayPage {}
            SoundboardPage {}
            AboutPage {}
        }
    }

    ParallelAnimation {
        id: pageIn

        NumberAnimation {
            id: pageFade
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.animNormal
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            id: pageZoom
            property: "scale"
            from: 0.985
            to: 1
            duration: Theme.animNormal
            easing.type: Easing.OutCubic
        }
    }
}
