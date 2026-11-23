import QtQuick
import QtQuick.Effects

// Screenshot panel background for wide panels (Shelves spotlight, List
// preview header). A screenshot is shown whole at its own aspect ratio over a
// blurred, dimmed copy of itself that fills the rest of the panel, so wide
// panels do not crop it to a thin band. Screenshots cycle as a slideshow and
// cross-fade; show(index) jumps to one and restarts the timer. Without
// screenshots the fallback image (the cover) is shown blurred only.
Item {
    id: root

    property var sources: []
    property string fallbackSource: ""
    // "right" or "center": where the sharp screenshot sits.
    property string alignment: "center"
    // false shows only the blurred fill (for panels that show the cover instead).
    property bool showSharpImage: true
    // Strength of the blurred fill behind or instead of the sharp image.
    property real fillOpacity: 0.55
    property bool cycling: true
    property int interval: 9000
    property bool reducedMotion: false
    property int currentIndex: 0

    readonly property bool hasScreenshots: !!sources && sources.length > 0
    readonly property string currentSource: hasScreenshots
        ? sources[Math.max(0, Math.min(sources.length - 1, currentIndex))]
        : fallbackSource

    function show(index) {
        if (!hasScreenshots)
            return
        currentIndex = Math.max(0, Math.min(sources.length - 1, index))
        cycleTimer.restart()
    }

    onSourcesChanged: {
        currentIndex = 0
        cycleTimer.restart()
    }

    Timer {
        id: cycleTimer
        interval: root.interval
        repeat: true
        running: root.cycling && root.visible && !root.reducedMotion
            && root.hasScreenshots && root.sources.length > 1
        onTriggered: root.currentIndex = (root.currentIndex + 1) % root.sources.length
    }

    // Two layers cross-fade: the hidden one loads the next image.
    property bool showFirst: true
    property string firstSource: ""
    property string secondSource: ""
    property bool firstIsScreenshot: false
    property bool secondIsScreenshot: false
    Component.onCompleted: {
        firstSource = currentSource
        firstIsScreenshot = hasScreenshots
    }
    onCurrentSourceChanged: {
        if (showFirst) {
            secondSource = currentSource
            secondIsScreenshot = hasScreenshots
        } else {
            firstSource = currentSource
            firstIsScreenshot = hasScreenshots
        }
        showFirst = !showFirst
    }

    Repeater {
        model: 2
        delegate: Item {
            id: layer
            required property int index
            readonly property string layerSource: index === 0 ? root.firstSource : root.secondSource
            readonly property bool isScreenshot: index === 0 ? root.firstIsScreenshot : root.secondIsScreenshot
            anchors.fill: parent
            opacity: (index === 0) === root.showFirst ? 1 : 0
            Behavior on opacity {
                enabled: !root.reducedMotion
                NumberAnimation { duration: 700; easing.type: Easing.InOutQuad }
            }

            Image {
                id: fillImage
                anchors.fill: parent
                source: layer.layerSource
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                visible: false
            }
            MultiEffect {
                anchors.fill: parent
                source: fillImage
                blurEnabled: true
                blur: 1.0
                blurMax: 64
                autoPaddingEnabled: false
                opacity: fillImage.status === Image.Ready ? (layer.isScreenshot ? root.fillOpacity : 0.5) : 0
            }
            Image {
                anchors.fill: parent
                visible: layer.isScreenshot && root.showSharpImage
                source: layer.layerSource
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: root.alignment === "right" ? Image.AlignRight : Image.AlignHCenter
                asynchronous: true
                cache: true
                smooth: true
                mipmap: true
            }
        }
    }
}
