import QtQuick
import QtQuick.Controls

// Artwork-forward Library card: the cover art is the card, with the focus
// ring drawn outside it and a two-line caption (title, then recompilation
// status or platform). Shared by the platform-filtered Grid and the sectioned
// All Platforms Grid so both draw identical cards.
//
// projectCatalog is a context property; installed launcher ids are passed in
// so each card does not read the launcher directories itself.
Item {
    id: root

    property string title: ""
    property string platform: ""
    property string artworkPath: ""
    property string metadataTitle: ""
    property real artworkHeight: Math.round(width * 7 / 5)
    property bool focused: false
    property bool hovered: false
    property bool reducedMotion: false
    property QtObject themeTokens: null
    // Object keyed by project id: { "<projectId>": true }.
    property var installedProjectIds: ({})

    readonly property real fontScale: themeTokens ? themeTokens.fontScale : 1
    // Prefer the matched metadata title ("The Legend of Zelda: The Wind Waker")
    // over the ROM file name ("Legend of Zelda, The - The Wind Waker (USA)").
    readonly property string displayTitle:
        metadataTitle.length > 0 ? metadataTitle : title

    // The game's default launcher when several support it (Main.qml).
    readonly property var supportedProject: {
        root.installedProjectIds
        return window.defaultLauncherProject(projectCatalog.projectsForGame(
            root.metadataTitle.length > 0 ? root.metadataTitle : root.title,
            root.platform))
    }
    readonly property bool launcherInstalled:
        root.supportedProject !== null
        && root.installedProjectIds !== null
        && root.installedProjectIds[root.supportedProject.id] === true

    GameArtwork {
        id: artwork
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.artworkHeight
        source: root.artworkPath
        platform: root.platform
        highlighted: root.focused || root.hovered
        focused: root.focused
        hovered: root.hovered
        reducedMotion: root.reducedMotion
        themeTokens: root.themeTokens
        frameMode: "platform"
        frameAlignment: "bottom"
        frameInset: 0
        showViewportBackground: false
        showFocusRing: true
    }

    Column {
        anchors.top: artwork.bottom
        anchors.topMargin: 9
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 3

        Label {
            width: parent.width
            text: root.displayTitle
            color: root.themeTokens ? root.themeTokens.textPrimary : "white"
            font.pixelSize: 16 * root.fontScale
            font.bold: true
            elide: Text.ElideRight
        }
        Label {
            width: parent.width
            // Keep the card status short; Game Details shows the full
            // project name and install state.
            text: root.launcherInstalled
                ? "Recomp installed"
                : root.supportedProject !== null
                    ? "Recompilation available"
                    : root.platform
            color: !root.themeTokens ? "white"
                : root.launcherInstalled ? root.themeTokens.success
                : root.supportedProject !== null ? root.themeTokens.accentHover
                : root.themeTokens.textSecondary
            font.pixelSize: 12 * root.fontScale
            font.bold: root.supportedProject !== null
            elide: Text.ElideRight
        }
    }
}
