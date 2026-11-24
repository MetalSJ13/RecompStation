import QtQuick
import QtQuick.Effects
import "ArtworkLayout.js" as ArtworkLayout

// Shared cover-art presentation for Library views.
//
// The component fills the artwork viewport it is given and centers a cover
// frame inside it. Two frame modes are available:
//
//   "measured" (default) - the frame follows the loaded image's own aspect
//                          ratio, bounded by the viewport. This is the
//                          behavior the Grid has used to date.
//   "platform"           - the frame follows the platform's typical box-art
//                          shape (see ArtworkLayout.js), unless the image's
//                          measured ratio clearly differs, such as a PAL
//                          portrait N64 box. In this mode the image fills the
//                          frame edge to edge (small differences are cropped).
//                          Used by the artwork-forward Grid.
//
// Colors come from the application Theme object passed in themeTokens, so the
// component has no hidden dependency on ids declared in Main.qml.
Item {
    id: root

    // Inputs
    property string source: ""
    property string platform: ""
    property bool highlighted: false
    property bool reducedMotion: false
    property QtObject themeTokens: null
    property string frameMode: "measured"
    // Gutter between the cover frame and the viewport edge.
    property real frameInset: 4
    // Viewport background, blurred landscape backdrop and theme overlay.
    property bool showViewportBackground: true
    // "center" or "bottom": where the frame sits inside the viewport. Bottom
    // alignment lets mixed-shape covers share a baseline above their captions.
    property string frameAlignment: "center"
    // Focus ring drawn outside the frame (artwork-forward cards).
    property bool showFocusRing: false
    property bool focused: false
    property bool hovered: false
    property real focusRingGap: 4
    property real focusRingWidth: 3

    // Outputs
    readonly property bool ready: cover.status === Image.Ready
    readonly property real imageAspectRatio:
        ready && cover.sourceSize.width > 0 && cover.sourceSize.height > 0
            ? cover.sourceSize.width / cover.sourceSize.height
            : 0
    readonly property bool landscapeImage:
        ready && cover.sourceSize.width > cover.sourceSize.height

    readonly property real availableWidth: Math.max(1, width - frameInset * 2)
    readonly property real availableHeight: Math.max(1, height - frameInset * 2)

    readonly property real frameAspectRatio:
        ArtworkLayout.frameAspectRatio(frameMode, platform, imageAspectRatio,
                                       availableWidth / availableHeight)
    readonly property var frameSize:
        ArtworkLayout.fitFrame(availableWidth, availableHeight, frameAspectRatio)

    // The blurred fill helps landscape covers that are letterboxed, but
    // duplicates portrait cover art. N64 artwork is displayed cleanly.
    readonly property bool useBlurredBackdrop:
        showViewportBackground && platform !== "N64" && landscapeImage

    Rectangle {
        anchors.fill: parent
        visible: root.showViewportBackground
        radius: root.themeTokens ? root.themeTokens.radiusSmall : 6
        color: root.themeTokens ? root.themeTokens.sidebarBackground : "transparent"
        clip: true

        Image {
            id: backdropImage
            anchors.fill: parent
            source: root.source
            fillMode: Image.PreserveAspectCrop
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
            asynchronous: true
            cache: true
            visible: root.useBlurredBackdrop
            opacity: 0.42
        }
        MultiEffect {
            anchors.fill: parent
            source: backdropImage
            blurEnabled: true
            blur: 0.8
            autoPaddingEnabled: false
            visible: root.useBlurredBackdrop
        }
        Rectangle {
            anchors.fill: parent
            color: root.themeTokens ? root.themeTokens.artworkOverlay : "transparent"
            visible: root.source.length > 0
        }
    }

    Rectangle {
        id: frame
        width: root.frameSize.width
        height: root.frameSize.height
        x: Math.round((root.width - width) / 2)
        y: root.frameAlignment === "bottom"
            ? Math.round(root.height - root.frameInset - height)
            : Math.round((root.height - height) / 2)
        radius: root.themeTokens ? root.themeTokens.radiusSmall : 6
        color: root.themeTokens ? root.themeTokens.sidebarBackground : "transparent"
        border.width: 1
        border.color: !root.themeTokens ? "transparent"
            : root.highlighted
                ? Qt.rgba(root.themeTokens.accent.r, root.themeTokens.accent.g,
                          root.themeTokens.accent.b, 0.78)
                : Qt.rgba(root.themeTokens.divider.r, root.themeTokens.divider.g,
                          root.themeTokens.divider.b, 0.34)
        clip: true
        Behavior on border.color {
            enabled: !root.reducedMotion
            ColorAnimation { duration: 170; easing.type: Easing.OutCubic }
        }

        Image {
            id: cover
            anchors.fill: parent
            source: root.source
            fillMode: root.frameMode === "platform"
                ? Image.PreserveAspectCrop : Image.PreserveAspectFit
            asynchronous: true
            cache: true
            smooth: true
            mipmap: true
            visible: status === Image.Ready
        }
        // Placeholder cover at the frame's (the system's box) shape: the
        // theme's dark surface with the RecompStation mark, faintly. The
        // title and system are already shown around the cover.
        Item {
            id: placeholder
            anchors.fill: parent
            visible: cover.status !== Image.Ready

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#2a2f48" }
                    GradientStop { position: 0.6; color: "#1a1d2c" }
                    GradientStop { position: 1.0; color: "#141724" }
                }
            }
            Image {
                anchors.centerIn: parent
                width: Math.round(Math.min(placeholder.width, placeholder.height) * 0.34)
                height: width
                source: "qrc:/qt/qml/RecompStation/assets/logo/recompstation-mark.svg"
                sourceSize: Qt.size(128, 128)
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: 0.22
            }
        }
    }

    Rectangle {
        id: focusRing
        visible: root.showFocusRing
        x: frame.x - root.focusRingGap - root.focusRingWidth
        y: frame.y - root.focusRingGap - root.focusRingWidth
        width: frame.width + (root.focusRingGap + root.focusRingWidth) * 2
        height: frame.height + (root.focusRingGap + root.focusRingWidth) * 2
        radius: frame.radius + root.focusRingGap + root.focusRingWidth
        color: "transparent"
        border.width: root.focusRingWidth
        border.color: root.themeTokens ? root.themeTokens.accent : "white"
        opacity: root.focused ? 1 : root.hovered ? 0.4 : 0
        Behavior on opacity {
            enabled: !root.reducedMotion
            NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
        }
    }
}
