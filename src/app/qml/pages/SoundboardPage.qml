pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import V9Control

// Sound pads from a folder, played into every program that records the microphone.
ScrollView {
    id: root

    contentWidth: availableWidth
    clip: true

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose the soundboard folder")
        onAccepted: Soundboard.setFolderUrl(selectedFolder)
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        Card {
            title: qsTr("Soundboard")
            subtitle: qsTr("Sounds go straight into every program that records your microphone (Discord, OBS, …), without noise suppression. Your microphone keeps working as usual.")
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                ToggleSwitch {
                    text: qsTr("Soundboard")
                    checked: Soundboard.enabled
                    enabled: Soundboard.available
                    onToggled: Soundboard.enabled = checked
                }
                StatusPill {
                    text: !Soundboard.enabled ? qsTr("Off") : (Soundboard.active ? qsTr("Active") : qsTr("Starting…"))
                    tone: !Soundboard.enabled ? Theme.textMuted : (Soundboard.active ? Theme.success : Theme.warning)
                }
                Item {
                    Layout.fillWidth: true
                }
                ToggleSwitch {
                    text: qsTr("Hear it myself")
                    checked: Soundboard.listen
                    enabled: Soundboard.enabled
                    onToggled: Soundboard.listen = checked
                }
                ActionButton {
                    text: qsTr("Stop all")
                    enabled: Soundboard.playing.length > 0
                    onClicked: Soundboard.stopAll()
                }
            }

            LevelSlider {
                label: qsTr("Volume")
                icon: "../icons/volume.svg"
                mutable: false
                value: Soundboard.volume
                Layout.fillWidth: true
                onMoved: value => Soundboard.volume = value
            }
            Text {
                text: qsTr("Volume applies to sounds started after the change.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                Layout.fillWidth: true
            }

            SectionHeader {
                title: qsTr("Folder")
                description: qsTr("WAV, MP3, OGG, Opus and FLAC files in this folder become pads. The list updates when files change.")
            }
            RowLayout {
                spacing: Theme.spacingSmall
                Layout.fillWidth: true

                Text {
                    textFormat: Text.PlainText
                    text: Soundboard.folder
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: qsTr("Change…")
                    onClicked: folderDialog.open()
                }
                ActionButton {
                    text: qsTr("Refresh")
                    onClicked: Soundboard.refresh()
                }
            }

            Text {
                visible: Soundboard.error.length > 0
                textFormat: Text.PlainText
                text: Soundboard.error
                color: Theme.danger
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Text {
            visible: Soundboard.sounds.length === 0
            text: qsTr("No sounds yet. Put audio files in the folder above, or choose another folder.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Flow {
            spacing: Theme.spacingSmall
            Layout.fillWidth: true

            Repeater {
                model: Soundboard.sounds

                delegate: SoundTile {
                    id: tile

                    required property string modelData

                    // Name without the extension.
                    text: tile.modelData.replace(/\.[^.]+$/, "")
                    playing: Soundboard.playing.indexOf(tile.modelData) >= 0
                    enabled: Soundboard.active
                    onClicked: Soundboard.toggle(tile.modelData)
                }
            }
        }
    }
}
