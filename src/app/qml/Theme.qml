pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#0d0f13"
    readonly property color sidebar: "#111419"
    readonly property color surface: "#161a21"
    readonly property color surfaceRaised: "#1d222b"
    readonly property color border: "#262c37"
    readonly property color track: "#2a303b"

    readonly property color text: "#e8ecf2"
    readonly property color textMuted: "#8a93a3"
    readonly property color textFaint: "#5c6575"

    readonly property color accent: "#ff6a3d"
    readonly property color accentSoft: "#33ff6a3d"
    readonly property color success: "#3ddc97"
    readonly property color warning: "#f5b841"
    readonly property color danger: "#ff4d5e"
    readonly property color info: "#4da3ff"

    readonly property int radiusLarge: 18
    readonly property int radius: 12
    readonly property int radiusSmall: 8

    readonly property int spacing: 16
    readonly property int spacingSmall: 8
    readonly property int padding: 20

    readonly property int fontTitle: 22
    readonly property int fontHeading: 15
    readonly property int fontBody: 13
    readonly property int fontSmall: 11
    readonly property int fontDisplay: 34

    // Motion: hover/press feedback, value changes, page and graph entrances.
    readonly property int animFast: 120
    readonly property int animNormal: 240
    readonly property int animSlow: 600

    readonly property font bodyFont: Qt.font({ pixelSize: fontBody })
    readonly property font monoFont: Qt.font({ family: "monospace", pixelSize: fontBody })

    function batteryColor(percent) {
        if (percent < 0)
            return textFaint
        if (percent <= 15)
            return danger
        if (percent <= 35)
            return warning
        return success
    }
}
