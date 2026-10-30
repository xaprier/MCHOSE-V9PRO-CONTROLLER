import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import V9Control

// Everything used day to day on one page: headset state on top, then the output chain (volume,
// equalizer, enhancements) next to the input chain (volume, noise suppression, monitor).
ScrollView {
    id: root

    signal openEqualizer()

    readonly property bool online: Headset.dongleConnected && Headset.headsetOnline
    readonly property string chargeCaption: {
        switch (Headset.chargeState) {
        case Headset.Charging:
            return qsTr("Charging")
        case Headset.Full:
            return qsTr("Fully charged")
        case Headset.Asleep:
            return qsTr("Asleep")
        case Headset.Discharging:
            return qsTr("On battery")
        }
        return Headset.dongleConnected ? qsTr("Headset off") : qsTr("No dongle")
    }

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacing

        RowLayout {
            spacing: Theme.spacing
            Layout.fillWidth: true

            Card {
                title: qsTr("Battery")
                Layout.preferredWidth: 240
                Layout.fillHeight: true

                // Centred inside a full-width holder: the ring's own Layout.alignment is not
                // honoured when the card's body column is narrower than the card.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: ring.implicitHeight

                    BatteryRing {
                        id: ring
                        anchors.horizontalCenter: parent.horizontalCenter
                        percent: root.online || Headset.chargeState === Headset.Asleep ? Headset.batteryPercent : -1
                        charging: Headset.chargeState === Headset.Charging
                        caption: root.chargeCaption
                    }
                }
                Text {
                    visible: Headset.lastBatteryUpdate.getTime() > 0
                    text: qsTr("Updated %1").arg(Qt.formatTime(Headset.lastBatteryUpdate, "HH:mm:ss"))
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }

            Card {
                title: qsTr("MCHOSE V9 PRO")
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flow {
                    spacing: Theme.spacingSmall
                    Layout.fillWidth: true

                    StatusPill {
                        text: Headset.dongleConnected ? qsTr("Dongle connected") : qsTr("Dongle not found")
                        tone: Headset.dongleConnected ? Theme.success : Theme.danger
                    }
                    StatusPill {
                        visible: Headset.dongleConnected
                        text: Headset.headsetOnline ? qsTr("Headset online") : qsTr("Headset offline")
                        tone: Headset.headsetOnline ? Theme.success : Theme.warning
                    }
                }

                SectionHeader {
                    title: qsTr("Sound mode")
                    description: qsTr("Processed inside the headset; the mode button on the headset switches it too.")
                }

                RowLayout {
                    spacing: Theme.spacingSmall
                    enabled: root.online
                    opacity: enabled ? 1.0 : 0.5
                    Layout.fillWidth: true

                    ButtonGroup {
                        id: modeGroup
                    }

                    ModeTile {
                        text: qsTr("Game 1")
                        description: qsTr("Footsteps and positional cues")
                        icon.source: Qt.resolvedUrl("../icons/target.svg")
                        checked: Headset.soundMode === Headset.GameOne
                        ButtonGroup.group: modeGroup
                        Layout.fillWidth: true
                        onClicked: Headset.soundMode = Headset.GameOne
                    }
                    ModeTile {
                        text: qsTr("Game 2")
                        description: qsTr("Immersive, wider stage")
                        icon.source: Qt.resolvedUrl("../icons/gamepad.svg")
                        checked: Headset.soundMode === Headset.GameTwo
                        ButtonGroup.group: modeGroup
                        Layout.fillWidth: true
                        onClicked: Headset.soundMode = Headset.GameTwo
                    }
                    ModeTile {
                        text: qsTr("Music")
                        description: qsTr("Balanced listening")
                        icon.source: Qt.resolvedUrl("../icons/music.svg")
                        checked: Headset.soundMode === Headset.Music
                        ButtonGroup.group: modeGroup
                        Layout.fillWidth: true
                        onClicked: Headset.soundMode = Headset.Music
                    }
                }
            }
        }

        GridLayout {
            columns: root.availableWidth >= 720 ? 2 : 1
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing
            Layout.fillWidth: true

            Card {
                title: qsTr("Output")
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop

                LevelSlider {
                    label: qsTr("Headphones")
                    icon: "../icons/volume.svg"
                    mutedIcon: "../icons/volume-off.svg"
                    available: Mixer.outputAvailable
                    value: Mixer.outputVolume
                    to: 150
                    muted: Mixer.outputMuted
                    Layout.fillWidth: true
                    onMoved: value => Mixer.outputVolume = value
                    onMuteToggled: Mixer.outputMuted = !Mixer.outputMuted
                }
                RowLayout {
                    visible: Mixer.outputAvailable && !Mixer.outputIsDefault
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Another device is the default output.")
                        color: Theme.warning
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        text: qsTr("Make default")
                        kind: "primary"
                        onClicked: Mixer.makeDefaultOutput()
                    }
                }

                SectionHeader {
                    title: qsTr("Equalizer")
                }
                RowLayout {
                    spacing: Theme.spacing
                    Layout.fillWidth: true

                    ToggleSwitch {
                        text: Equalizer.enabled
                              ? (Equalizer.currentProfile.length > 0 ? Equalizer.currentProfile : qsTr("Unsaved curve"))
                              : qsTr("Off")
                        checked: Equalizer.enabled
                        enabled: Equalizer.available
                        Layout.fillWidth: true
                        onToggled: Equalizer.enabled = checked
                    }
                    ActionButton {
                        text: qsTr("Edit…")
                        onClicked: root.openEqualizer()
                    }
                }

                SectionHeader {
                    title: qsTr("Enhancements")
                    description: qsTr("Linux counterparts of Hub's dynamic bass, audio brightening and vocal clarity. They work with the equalizer on or off.")
                }
                GridLayout {
                    columns: 2
                    columnSpacing: Theme.spacing
                    rowSpacing: Theme.spacingSmall
                    Layout.fillWidth: true

                    ToggleSwitch {
                        text: qsTr("Dynamic bass")
                        checked: Equalizer.bassEnabled
                        enabled: Equalizer.available
                        Layout.preferredWidth: 150
                        onToggled: Equalizer.bassEnabled = checked
                    }
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
                    Item {
                        visible: Equalizer.bassEnabled
                        Layout.preferredWidth: 1
                    }
                    LevelSlider {
                        visible: Equalizer.bassEnabled
                        label: qsTr("Cut-off")
                        mutable: false
                        from: 50
                        to: 400
                        step: 10
                        suffix: qsTr(" Hz")
                        value: Equalizer.bassCutoff
                        Layout.fillWidth: true
                        onMoved: value => Equalizer.bassCutoff = value
                    }

                    ToggleSwitch {
                        text: qsTr("Brightening")
                        checked: Equalizer.brillianceEnabled
                        enabled: Equalizer.available
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
                        enabled: Equalizer.available
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
                }
            }

            Card {
                title: qsTr("Microphone")
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop

                LevelSlider {
                    label: qsTr("Microphone")
                    icon: "../icons/mic.svg"
                    mutedIcon: "../icons/mic-off.svg"
                    available: Mixer.micAvailable
                    value: Mixer.micVolume
                    to: 150
                    muted: Mixer.micMuted
                    Layout.fillWidth: true
                    onMoved: value => Mixer.micVolume = value
                    onMuteToggled: Mixer.micMuted = !Mixer.micMuted
                }
                Text {
                    text: qsTr("The mute button on the headset works inside the headset and is not visible here.")
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                SectionHeader {
                    title: qsTr("Noise suppression")
                    description: qsTr("Adds a cleaned-up microphone and makes it the default input while on.")
                }
                RowLayout {
                    spacing: Theme.spacing
                    Layout.fillWidth: true

                    ToggleSwitch {
                        text: NoiseSuppression.active ? qsTr("On") : qsTr("Off")
                        checked: NoiseSuppression.enabled
                        enabled: NoiseSuppression.available && Mixer.micAvailable
                        onToggled: NoiseSuppression.enabled = checked
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    SegmentedControl {
                        value: NoiseSuppression.engine
                        options: [
                            { text: qsTr("WebRTC"), value: NoiseSuppression.WebRtc },
                            { text: qsTr("RNNoise (AI)"), value: NoiseSuppression.RNNoise, enabled: NoiseSuppression.rnnoiseAvailable }
                        ]
                        onActivated: value => NoiseSuppression.engine = value
                    }
                }
                LevelSlider {
                    visible: NoiseSuppression.engine === NoiseSuppression.RNNoise
                    label: qsTr("Voice detection threshold")
                    value: NoiseSuppression.vadThreshold
                    to: 99
                    mutable: false
                    Layout.fillWidth: true
                    onMoved: value => NoiseSuppression.vadThreshold = value
                }
                Text {
                    visible: !NoiseSuppression.rnnoiseAvailable
                    text: qsTr("RNNoise needs the LADSPA plugin from noise-suppression-for-voice.")
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    visible: NoiseSuppression.error.length > 0
                    text: NoiseSuppression.error
                    color: Theme.danger
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                SectionHeader {
                    title: qsTr("Microphone monitor")
                    description: NoiseSuppression.active
                                 ? qsTr("Hear your own voice, noise-suppressed, in the headset.")
                                 : qsTr("Hear your own voice in the headset while you talk.")
                }
                ToggleSwitch {
                    text: Monitor.active ? qsTr("On") : qsTr("Off")
                    checked: Monitor.enabled
                    enabled: Monitor.available && Mixer.micAvailable && Mixer.outputAvailable
                    onToggled: Monitor.enabled = checked
                }
                LevelSlider {
                    label: qsTr("Monitor level")
                    icon: "../icons/headset.svg"
                    mutable: false
                    available: Monitor.enabled
                    value: Monitor.level
                    Layout.fillWidth: true
                    onMoved: value => Monitor.level = value
                }
                Text {
                    visible: Monitor.error.length > 0
                    text: Monitor.error
                    color: Theme.danger
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
