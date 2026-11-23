import QtQuick
import QtQuick.Effects

// The user's own Library wallpaper.
//  - Fill: covers the area and crops the overflow, like a desktop wallpaper.
//  - Fit: shows the whole image at its own shape over a blurred, enlarged
//    copy of itself, so nothing is cropped and there are no black bars.
// The image is decoded at the screen's size (times its pixel ratio), so a
// 6000-pixel photo does not sit in video memory at full size, and resizing
// the window does not decode it again.
Item {
    id: wallpaper

    property url source
    property string fitMode: "fill"
    property int dimPercent: 15
    property color dimColor: "black"
    readonly property bool ready: sharpImage.status === Image.Ready
    readonly property real pixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1

    clip: true

    // Fit mode's backdrop: a small, blurred, cropped copy. Decoding it small
    // keeps the blur cheap.
    Image {
        id: blurSource
        anchors.fill: parent
        visible: false
        source: wallpaper.fitMode === "fit" ? wallpaper.source : ""
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: 480
        sourceSize.height: Math.max(1, Math.round(480 * wallpaper.height / Math.max(1, wallpaper.width)))
        asynchronous: true
        cache: true
    }
    MultiEffect {
        anchors.fill: parent
        visible: wallpaper.fitMode === "fit" && blurSource.status === Image.Ready
        source: blurSource
        blurEnabled: true
        blur: 1.0
        blurMax: 48
        autoPaddingEnabled: false
        brightness: -0.12
    }

    Image {
        id: sharpImage
        anchors.fill: parent
        source: wallpaper.source
        fillMode: wallpaper.fitMode === "fit" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        // Both dimensions set: with PreserveAspectCrop Qt decodes just large
        // enough to cover this size; with PreserveAspectFit, to fit inside it.
        sourceSize.width: Math.max(1, Math.round(Screen.width * wallpaper.pixelRatio))
        sourceSize.height: Math.max(1, Math.round(Screen.height * wallpaper.pixelRatio))
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
    }

    Rectangle {
        anchors.fill: parent
        color: wallpaper.dimColor
        opacity: Math.max(0, Math.min(80, wallpaper.dimPercent)) / 100
    }
}
