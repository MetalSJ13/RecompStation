import QtQuick

// The Moving background, drawn live on the GPU. scene picks the shader:
//   "waves" (shaders/waves.frag), "stars" (stars.frag), "lights"
//   (lights.frag) or "flow" (flow.frag). All four take the same color,
//   motion and quality settings.
//  - quality: share of the full resolution the shader renders at (1.0, 0.75
//    or 0.5); lower values are scaled up smoothly and cost far less on
//    integrated or older GPUs.
//  - fps: 60 follows the display; lower values advance the animation on a
//    timer, so the scene is only redrawn that often.
//  - running: false freezes the current frame (Reduced motion, a game
//    running, the window inactive).
// With Qt's software renderer, which has no shaders, the theme gradient is
// shown instead.
Item {
    id: root

    property string scene: "waves"
    property color waveColor: "#3d8bff"
    property color topColor: "#172b40"
    property color bottomColor: "#0b111b"
    property real speed: 1.0
    property real quality: 1.0
    property int fps: 60
    property bool running: true
    // Starting a few seconds in gives a pleasing first (and paused) frame.
    property real time: 6.0

    readonly property real pixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
    readonly property bool shaderSupported: GraphicsInfo.api !== GraphicsInfo.Software
    readonly property string shaderName: ["waves", "stars", "lights", "flow"].indexOf(scene) >= 0 ? scene : "waves"

    Rectangle {
        anchors.fill: parent
        visible: !root.shaderSupported
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.topColor }
            GradientStop { position: 1.0; color: root.bottomColor }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.shaderSupported
        // Below full quality, render into a smaller texture and scale it up.
        layer.enabled: root.quality < 0.99
        layer.smooth: true
        layer.textureSize: Qt.size(Math.max(1, Math.round(width * root.pixelRatio * root.quality)),
                                   Math.max(1, Math.round(height * root.pixelRatio * root.quality)))

        ShaderEffect {
            anchors.fill: parent
            property real time: root.time
            property size resolution: Qt.size(Math.max(1, width * root.pixelRatio * root.quality),
                                              Math.max(1, height * root.pixelRatio * root.quality))
            property color waveColor: root.waveColor
            property color topColor: root.topColor
            property color bottomColor: root.bottomColor
            // Rendered pixels per logical pixel (the stars are sized in
            // logical pixels so they look the same on every screen).
            property real pixel: root.pixelRatio * root.quality
            fragmentShader: "qrc:/shaders/" + root.shaderName + ".frag.qsb"
        }
    }

    FrameAnimation {
        running: root.running && root.visible && root.shaderSupported && root.fps >= 60
        onTriggered: root.time += Math.min(frameTime, 0.1) * root.speed
    }
    Timer {
        interval: Math.round(1000 / Math.max(1, root.fps))
        repeat: true
        running: root.running && root.visible && root.shaderSupported && root.fps < 60
        onTriggered: root.time += interval / 1000 * root.speed
    }
}
