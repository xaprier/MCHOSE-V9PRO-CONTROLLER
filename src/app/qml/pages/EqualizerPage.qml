pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import V9Control

// Profile list on the left, band editor for the selected profile on the right, as in MCHOSE HUB.
ScrollView {
    id: root

    property bool showCustom: Equalizer.customProfiles.indexOf(Equalizer.currentProfile) >= 0
    readonly property bool customSelected: Equalizer.customProfiles.indexOf(Equalizer.currentProfile) >= 0

    contentWidth: availableWidth
    clip: true

    FileDialog {
        id: importDialog
        title: qsTr("Import equalizer profile")
        nameFilters: [qsTr("Profiles (*.json)")]
        onAccepted: {
            if (Equalizer.importProfile(selectedFile))
                root.showCustom = true
        }
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export equalizer profile")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("Profiles (*.json)")]
        onAccepted: Equalizer.exportProfile(selectedFile)
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        RowLayout {
            spacing: Theme.spacing
            Layout.fillWidth: true

            ToggleSwitch {
                text: qsTr("Equalizer")
                checked: Equalizer.enabled
                enabled: Equalizer.available
                onToggled: Equalizer.enabled = checked
            }
            StatusPill {
                text: !Equalizer.enabled ? qsTr("Off") : (Equalizer.active ? qsTr("Active") : qsTr("Starting…"))
                tone: !Equalizer.enabled ? Theme.textMuted : (Equalizer.active ? Theme.success : Theme.warning)
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                text: qsTr("Profiles use MCHOSE HUB's file format.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
            }
        }

        Text {
            visible: Equalizer.error.length > 0
            text: Equalizer.error
            color: Theme.danger
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Theme.spacing
            Layout.fillWidth: true

            Card {
                Layout.preferredWidth: 230
                Layout.fillHeight: true

                SegmentedControl {
                    value: root.showCustom
                    options: [
                        { text: qsTr("Presets"), value: false },
                        { text: qsTr("Custom"), value: true }
                    ]
                    Layout.fillWidth: true
                    onActivated: value => root.showCustom = value
                }

                ListView {
                    id: profileList

                    model: root.showCustom ? Equalizer.customProfiles : Equalizer.builtInProfiles
                    spacing: 4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 200
                    Layout.topMargin: Theme.spacingSmall

                    delegate: ProfileItem {
                        id: profileItem

                        required property string modelData

                        width: ListView.view.width
                        text: profileItem.modelData
                        checked: profileItem.modelData === Equalizer.currentProfile
                        modified: profileItem.checked && Equalizer.modified
                        onClicked: Equalizer.applyProfile(profileItem.modelData)
                    }
                }

                Text {
                    visible: root.showCustom && Equalizer.customProfiles.length === 0
                    text: qsTr("No custom profiles yet. Create one from the current curve or import a Hub profile.")
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                GridLayout {
                    visible: root.showCustom
                    columns: 2
                    columnSpacing: Theme.spacingSmall
                    rowSpacing: Theme.spacingSmall
                    Layout.fillWidth: true

                    ActionButton {
                        text: qsTr("New")
                        kind: "primary"
                        Layout.fillWidth: true
                        onClicked: Equalizer.createProfile()
                    }
                    ActionButton {
                        text: qsTr("Import")
                        Layout.fillWidth: true
                        onClicked: importDialog.open()
                    }
                    ActionButton {
                        text: qsTr("Export")
                        enabled: root.customSelected
                        Layout.fillWidth: true
                        onClicked: exportDialog.open()
                    }
                    ActionButton {
                        text: qsTr("Delete")
                        kind: "danger"
                        enabled: root.customSelected
                        Layout.fillWidth: true
                        onClicked: Equalizer.deleteProfile(Equalizer.currentProfile)
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    spacing: Theme.spacingSmall
                    Layout.fillWidth: true

                    StyledTextField {
                        id: nameField

                        visible: root.customSelected
                        text: Equalizer.currentProfile
                        font.pixelSize: Theme.fontHeading
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Profile name")
                        onEditingFinished: {
                            if (!Equalizer.renameProfile(Equalizer.currentProfile, text))
                                text = Equalizer.currentProfile
                        }
                    }
                    Text {
                        visible: !root.customSelected
                        text: Equalizer.currentProfile.length > 0
                              ? (Equalizer.modified ? qsTr("%1 (modified)").arg(Equalizer.currentProfile) : Equalizer.currentProfile)
                              : qsTr("Unsaved curve")
                        color: Theme.text
                        font.pixelSize: Theme.fontHeading
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        visible: !root.customSelected && (Equalizer.modified || Equalizer.currentProfile.length === 0)
                        text: qsTr("Save as custom")
                        kind: "primary"
                        onClicked: {
                            if (Equalizer.createProfile().length > 0)
                                root.showCustom = true
                        }
                    }
                    ActionButton {
                        text: root.customSelected || Equalizer.currentProfile.length === 0 ? qsTr("Flatten") : qsTr("Revert")
                        enabled: root.customSelected || Equalizer.modified || Equalizer.currentProfile.length === 0
                        onClicked: Equalizer.reset()
                    }
                }

                Text {
                    text: root.customSelected
                          ? qsTr("Drag the points to shape the sound. Changes are saved to this profile.")
                          : qsTr("Drag the points to shape the sound. Save as a custom profile to keep changes.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                EqualizerGraph {
                    gains: Equalizer.gains
                    labels: Equalizer.bandLabels
                    active: Equalizer.enabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: 320
                    Layout.topMargin: Theme.spacingSmall
                    onBandMoved: (band, gain) => Equalizer.setBand(band, gain)
                }

                RowLayout {
                    spacing: Theme.spacing
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spacingSmall

                    ToggleSwitch {
                        text: qsTr("Auto preamp")
                        checked: Equalizer.autoPreamp
                        onToggled: Equalizer.autoPreamp = checked
                    }
                    LevelSlider {
                        label: qsTr("Preamp")
                        mutable: false
                        from: -24
                        to: 6
                        suffix: qsTr(" dB")
                        value: Math.round(Equalizer.preamp)
                        available: !Equalizer.autoPreamp
                        Layout.fillWidth: true
                        onMoved: value => Equalizer.preamp = value
                    }
                }
                Text {
                    text: qsTr("Auto preamp lowers the level by the highest boost so loud passages do not distort.")
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
