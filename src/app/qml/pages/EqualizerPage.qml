pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import V9Control

ScrollView {
    id: root

    contentWidth: availableWidth
    clip: true

    FileDialog {
        id: importDialog
        title: qsTr("Import equalizer profile")
        nameFilters: [qsTr("Profiles (*.json)")]
        onAccepted: Equalizer.importProfile(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export equalizer profile")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("Profiles (*.json)")]
        onAccepted: Equalizer.exportProfile(selectedFile, Equalizer.currentProfile.length > 0 ? Equalizer.currentProfile : profileName.text)
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        Card {
            title: qsTr("Equalizer")
            subtitle: qsTr("Runs in PipeWire as \"V9 PRO Equalizer\" and becomes the default output while enabled. Profiles use MCHOSE HUB's JSON format.")
            Layout.fillWidth: true

            RowLayout {
                spacing: Theme.spacing
                Layout.fillWidth: true

                ToggleSwitch {
                    text: Equalizer.enabled ? qsTr("Enabled") : qsTr("Disabled")
                    checked: Equalizer.enabled
                    enabled: Equalizer.available
                    onToggled: Equalizer.enabled = checked
                }
                StatusPill {
                    text: Equalizer.active ? qsTr("Active") : (Equalizer.enabled ? qsTr("Starting…") : qsTr("Bypassed"))
                    tone: Equalizer.active ? Theme.success : (Equalizer.enabled ? Theme.warning : Theme.textMuted)
                }
                Item {
                    Layout.fillWidth: true
                }
                StyledComboBox {
                    id: profileBox
                    model: Equalizer.profiles
                    currentIndex: Equalizer.profiles.indexOf(Equalizer.currentProfile)
                    placeholder: qsTr("Custom")
                    Layout.preferredWidth: 190
                    onActivated: index => Equalizer.applyProfile(Equalizer.profiles[index])
                }
                ActionButton {
                    text: qsTr("Reset")
                    onClicked: Equalizer.reset()
                }
            }

            RowLayout {
                spacing: Theme.spacingSmall
                Layout.fillWidth: true

                StyledTextField {
                    id: profileName
                    placeholderText: qsTr("Profile name")
                    Layout.preferredWidth: 190
                }
                ActionButton {
                    text: qsTr("Save")
                    kind: "primary"
                    enabled: profileName.text.trim().length > 0 && !Equalizer.isBuiltInProfile(profileName.text.trim())
                    onClicked: {
                        if (Equalizer.saveProfile(profileName.text))
                            profileName.clear()
                    }
                }
                ActionButton {
                    text: qsTr("Delete")
                    kind: "danger"
                    enabled: Equalizer.currentProfile.length > 0 && !Equalizer.isBuiltInProfile(Equalizer.currentProfile)
                    onClicked: Equalizer.deleteProfile(Equalizer.currentProfile)
                }
                Item {
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: qsTr("Import…")
                    onClicked: importDialog.open()
                }
                ActionButton {
                    text: qsTr("Export…")
                    onClicked: exportDialog.open()
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
        }

        Card {
            title: qsTr("Response")
            subtitle: qsTr("Drag the points or use the sliders. Preamp %1 dB.").arg(Equalizer.preamp.toFixed(1))
            Layout.fillWidth: true

            EqualizerGraph {
                curve: Equalizer.curve
                gains: Equalizer.gains
                labels: Equalizer.bandLabels
                active: Equalizer.enabled
                Layout.fillWidth: true
                Layout.preferredHeight: 230
                onBandMoved: (band, gain) => Equalizer.setBand(band, gain)
            }

            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                Layout.topMargin: Theme.spacing

                Repeater {
                    model: Equalizer.bandLabels

                    delegate: BandSlider {
                        id: band

                        required property int index
                        required property string modelData

                        label: band.modelData
                        value: Equalizer.gains[band.index]
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onMoved: value => Equalizer.setBand(band.index, value)
                    }
                }
            }
        }

        Card {
            title: qsTr("Enhancements")
            subtitle: qsTr("Linux counterparts of Hub's dynamic bass, audio brightening and vocal clarity, applied in the same equalizer chain.")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: Theme.spacing * 2
                rowSpacing: Theme.spacing
                Layout.fillWidth: true

                ToggleSwitch {
                    text: qsTr("Dynamic bass")
                    checked: Equalizer.bassEnabled
                    onToggled: Equalizer.bassEnabled = checked
                    Layout.preferredWidth: 170
                }
                ColumnLayout {
                    Layout.fillWidth: true

                    LevelSlider {
                        label: qsTr("Intensity")
                        mutable: false
                        to: 10
                        suffix: "/10"
                        value: Equalizer.bassLevel
                        available: Equalizer.bassEnabled
                        Layout.fillWidth: true
                        onMoved: value => Equalizer.bassLevel = value
                    }
                    LevelSlider {
                        label: qsTr("Cut-off")
                        mutable: false
                        from: 50
                        to: 400
                        step: 10
                        suffix: qsTr(" Hz")
                        value: Equalizer.bassCutoff
                        available: Equalizer.bassEnabled
                        Layout.fillWidth: true
                        onMoved: value => Equalizer.bassCutoff = value
                    }
                }

                ToggleSwitch {
                    text: qsTr("Brightening")
                    checked: Equalizer.brillianceEnabled
                    onToggled: Equalizer.brillianceEnabled = checked
                }
                LevelSlider {
                    label: qsTr("Intensity")
                    mutable: false
                    to: 10
                    suffix: "/10"
                    value: Equalizer.brillianceLevel
                    available: Equalizer.brillianceEnabled
                    Layout.fillWidth: true
                    onMoved: value => Equalizer.brillianceLevel = value
                }

                ToggleSwitch {
                    text: qsTr("Vocal clarity")
                    checked: Equalizer.clarityEnabled
                    onToggled: Equalizer.clarityEnabled = checked
                }
                LevelSlider {
                    label: qsTr("Intensity")
                    mutable: false
                    to: 10
                    suffix: "/10"
                    value: Equalizer.clarityLevel
                    available: Equalizer.clarityEnabled
                    Layout.fillWidth: true
                    onMoved: value => Equalizer.clarityLevel = value
                }

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
                text: qsTr("Auto preamp lowers the level by the curve's highest boost so loud passages do not clip.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
