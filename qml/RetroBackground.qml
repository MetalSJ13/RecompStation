import QtQuick

// The "Still" background: the theme gradient, or one of four retro patterns
// drawn by shaders/retro.frag (CRT glow, dithered sunset, pixel starfield,
// circuit board). The pattern is drawn once into a layer and kept, so it is
// redrawn only when the size, the theme colors or the pattern change, never
// while the Library scrolls. With Qt's software renderer, which has no
// shaders, the gradient is shown.
Item {
    id: root

    // "gradient", "crt", "sunset", "stars" or "circuit".
    property string pattern: "gradient"
    property color accent: "#55a8f5"
    property color topColor: "#172b40"
    property color bottomColor: "#0b111b"

    readonly property real pixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
    readonly property bool shaderSupported: GraphicsInfo.api !== GraphicsInfo.Software
    readonly property int patternIndex: ["gradient", "crt", "sunset", "stars", "circuit"].indexOf(pattern)
    readonly property bool drawsPattern: shaderSupported && patternIndex > 0

    Rectangle {
        anchors.fill: parent
        visible: !root.drawsPattern
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.topColor }
            GradientStop { position: 1.0; color: root.bottomColor }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.drawsPattern
        layer.enabled: visible
        layer.smooth: false

        ShaderEffect {
            anchors.fill: parent
            property size resolution: Qt.size(Math.max(1, width * root.pixelRatio),
                                              Math.max(1, height * root.pixelRatio))
            property real pattern: root.patternIndex
            property real pixel: root.pixelRatio
            property color accent: root.accent
            property color topColor: root.topColor
            property color bottomColor: root.bottomColor
            fragmentShader: "qrc:/shaders/retro.frag.qsb"
        }
    }
}
