import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "LibraryGridLayout.js" as LibraryGridLayout

// Game Details: the full-window page opened from every Library view (A / Y /
// Enter / click). Moved out of Main.qml unchanged; it still reads application
// state (window, theme, gameGrid, launcher helpers) from the Main.qml scope
// it is created in.
Popup {
    id: gameDetailsPopup

    // Inner items used by Main.qml for controller navigation and scrolling.
    property alias contentFrame: detailsContentFrame
    property alias infoScroll: gameInfoScroll

    parent: Overlay.overlay
    x: 0
    y: 0
    width: window.width
    height: window.height
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: settingsManager.reducedMotion ? 0 : 560
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: detailsContentFrame
                property: "y"
                from: window.height
                to: 0
                duration: settingsManager.reducedMotion ? 0 : 680
                easing.type: Easing.OutCubic
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: settingsManager.reducedMotion ? 0 : 320
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: detailsContentFrame
                property: "y"
                from: 0
                to: window.height
                duration: settingsManager.reducedMotion ? 0 : 520
                easing.type: Easing.InCubic
            }
        }
    }

    property var supportedProjects: window.selectedGame
        ? projectCatalog.projectsForGame(
              window.selectedGame.metadataTitle || window.selectedGame.title,
              window.selectedGame.platform)
        : []
    property string selectedProjectId: ""
    property bool resettingProjectSelection: false
    property var selectedProject: {
        for (let i = 0; i < supportedProjects.length; ++i) {
            if (supportedProjects[i].id === selectedProjectId)
                return supportedProjects[i]
        }
        return supportedProjects.length > 0 ? supportedProjects[0] : null
    }
    property var installedProject: selectedProject
        ? window.installedLauncher(selectedProject.id) : null
    // The line under Play: whether Play will just work (LaunchPreparer).
    property var launchState: {
        window.launchStatusRevision
        settingsManager.launchSettingsRevision
        if (!selectedProject || installedProject === null || !visible)
            return ({})
        return gameLaunchManager.launchReadiness(selectedProject.id,
            window.selectedGame ? window.selectedGame.path : "")
    }
    readonly property string launchTone: launchState.tone || ""

    function folderOf(path) {
        const slash = path.lastIndexOf("/")
        return slash > 0 ? path.substring(0, slash) : path
    }
    function shortPath(path) {
        const home = platformManager.homeDirectory()
        return path && path.indexOf(home + "/") === 0 ? "~" + path.substring(home.length) : (path || "")
    }
    function copyText(text) {
        clipboardHelper.text = text
        clipboardHelper.selectAll()
        clipboardHelper.copy()
    }

    TextEdit {
        id: clipboardHelper
        visible: false
    }
    property bool installPromptVisible: false
    property bool switchAnimating: false
    property real gameBackgroundOpacity: 1.0
    property bool backgroundFadePending: false
    property int switchDirection: 1
    property int switchTargetIndex: -1
    property var switchTargetGame: null

    function switchToAdjacentGame(direction) {
        if (!visible || switchAnimating || !window.selectedGame)
            return

        const games = gameGrid.model || []
        let currentIndex = -1
        for (let i = 0; i < games.length; ++i) {
            if (Number(games[i].id) === Number(window.selectedGame.id)) {
                currentIndex = i
                break
            }
        }

        if (currentIndex < 0)
            return

        const nextIndex = currentIndex + (direction < 0 ? -1 : 1)
        if (nextIndex < 0 || nextIndex >= games.length)
            return

        const game = games[nextIndex]
        switchTargetIndex = nextIndex
        switchTargetGame = {
            id: game.id,
            title: game.title,
            platform: game.platform,
            path: game.path,
            artworkPath: game.artworkPath,
            screenshots: game.screenshots,
            metadataTitle: game.metadataTitle,
            metadataOverview: game.metadataOverview,
            metadataReleaseDate: game.metadataReleaseDate,
            metadataRating: game.metadataRating
        }
        switchDirection = direction < 0 ? -1 : 1
        switchAnimating = true
        beginAdjacentGameSwitch()
    }

    property int screenshotIndex: 0
    property int previousScreenshotIndex: -1
    property bool screenshotLayerA: true
    property var releaseState: selectedProject
        ? (window.releaseStates[selectedProject.id]
            || ({ loading: false, error: "", release: null, assets: [] }))
        : ({ loading: false, error: "", release: null, assets: [] })
    property var releaseInfo: releaseState && releaseState.release
        ? releaseState.release : null
    property string releaseTag: releaseState && releaseState.release
        && releaseState.release.tagName
        ? String(releaseState.release.tagName) : "Not available"
    property var compatibleAssets: releaseState && releaseState.assets
        ? releaseState.assets : []
    property var selectedAsset: compatibleAssets.length > 0
        ? compatibleAssets[0] : null
    property string selectedAssetName: selectedAsset && selectedAsset.name
        ? String(selectedAsset.name) : "Not available"
    property var downloadState: selectedProject && selectedAsset
        ? (window.downloadStates[selectedProject.id + "|" + selectedAsset.name]
            || ({ loading: false, installing: false, error: "",
                  bytesReceived: 0, bytesTotal: selectedAsset.size }))
        : ({ loading: false, installing: false, error: "",
             bytesReceived: 0, bytesTotal: 0 })

    // Show a chosen screenshot as the background (cross-fade, like the
    // slideshow) and restart the slideshow timer from it.
    function selectScreenshot(index) {
        const shots = window.selectedGame && window.selectedGame.screenshots
            ? window.selectedGame.screenshots : []
        if (index < 0 || index >= shots.length || index === screenshotIndex)
            return
        previousScreenshotIndex = screenshotIndex
        screenshotIndex = index
        screenshotLayerA = !screenshotLayerA
        detailsScreenshotTimer.restart()
    }

    function revealSwitchedBackground() {
        if (!backgroundFadePending)
            return
        backgroundFadePending = false
        detailsBackgroundFade.restart()
    }

    NumberAnimation {
        id: detailsBackgroundFade
        target: gameDetailsPopup
        property: "gameBackgroundOpacity"
        to: 1.0
        duration: settingsManager.reducedMotion ? 0 : 900
        easing.type: Easing.OutCubic
    }

    function beginAdjacentGameSwitch() {
        detailsContentFrame.grabToImage(function(result) {
            gameBackgroundOpacity = 0.0
            backgroundFadePending = true
            detailsOutgoingSnapshot.source = result.url
            detailsOutgoingSnapshot.x = 0
            detailsOutgoingSnapshot.visible = true

            window.selectedGame = gameDetailsPopup.switchTargetGame
            gameGrid.currentIndex = gameDetailsPopup.switchTargetIndex
            if (settingsManager.libraryView === "list")
                gameList.positionViewAtIndex(gameGrid.currentIndex, ListView.Contain)
            else if (settingsManager.libraryView === "carousel")
                consoleView.positionViewAtIndex(gameGrid.currentIndex, ListView.Contain)
            else if (settingsManager.libraryView === "shelves")
                shelvesView.positionAtIndex(gameGrid.currentIndex)
            else if (window.sectionedLibraryGrid)
                sectionedGrid.positionAtIndex(gameGrid.currentIndex)
            else
                gameGrid.positionViewAtIndex(gameGrid.currentIndex, GridView.Contain)

            gameDetailsPopup.screenshotIndex = 0
            gameDetailsPopup.previousScreenshotIndex = -1
            gameDetailsPopup.screenshotLayerA = true
            gameDetailsPopup.installPromptVisible = false
            gameDetailsPopup.resettingProjectSelection = true
            gameDetailsPopup.selectedProjectId =
                gameDetailsPopup.supportedProjects.length > 0
                    ? gameDetailsPopup.supportedProjects[0].id : ""
            gameDetailsPopup.resettingProjectSelection = false

            detailsContentFrame.x =
                gameDetailsPopup.switchDirection * window.width
            detailsContentFrame.opacity = 0
            detailsContentFrame.enabled = false
            detailsGameSwitchAnimation.start()

        })
    }

    ParallelAnimation {
        id: detailsGameSwitchAnimation

        NumberAnimation {
            target: detailsOutgoingSnapshot
            property: "x"
            to: -gameDetailsPopup.switchDirection * window.width
            duration: settingsManager.reducedMotion ? 0 : 420
            easing.type: Easing.InOutCubic
        }

        NumberAnimation {
            target: detailsContentFrame
            property: "x"
            to: 0
            duration: settingsManager.reducedMotion ? 0 : 420
            easing.type: Easing.InOutCubic
        }

        NumberAnimation {
            target: detailsContentFrame
            property: "opacity"
            from: 0
            to: 1
            duration: settingsManager.reducedMotion ? 0 : 360
            easing.type: Easing.OutCubic
        }

        onFinished: {
            detailsOutgoingSnapshot.visible = false
            detailsOutgoingSnapshot.source = ""
            detailsOutgoingSnapshot.x = 0
            detailsContentFrame.opacity = 1
            detailsContentFrame.enabled = true
            gameDetailsPopup.switchAnimating = false
            detailsContentFrame.focusPrimaryAction()
        }
    }

    onOpened: {
        resettingProjectSelection = true
        selectedProjectId = supportedProjects.length > 0 ? supportedProjects[0].id : ""
        resettingProjectSelection = false
        Qt.callLater(detailsContentFrame.focusPrimaryAction)
        screenshotIndex = 0
        previousScreenshotIndex = -1
        screenshotLayerA = true
        installPromptVisible = false
        const screenshotCount = window.selectedGame && window.selectedGame.screenshots
            ? window.selectedGame.screenshots.length : 0
        console.info("[Details Screenshots] Opened:", window.selectedGame
            ? window.selectedGame.title : "no selection",
            "count:", screenshotCount,
            "reduced motion:", settingsManager.reducedMotion,
            "window active:", window.active)
        if (selectedProject)
            window.loadCachedProjectRelease(selectedProject)
    }

    onSelectedProjectIdChanged: {
        installPromptVisible = false
        if (opened && !resettingProjectSelection && selectedProject)
            window.loadCachedProjectRelease(selectedProject)
    }

    background: Rectangle {
        // Keep the library page fully covered while background screenshots
        // crossfade or wait for the next image to finish loading.
        color: Qt.rgba(theme.windowBackground.r,
                       theme.windowBackground.g,
                       theme.windowBackground.b, 1.0)
    }

    contentItem: Item {
        id: detailsContentFrame
        width: window.width
        height: window.height
        clip: true

        Image {
            id: detailsOutgoingSnapshot
            z: 100
            width: window.width
            height: window.height
            visible: false
            asynchronous: false
            fillMode: Image.Stretch
            cache: false
        }

        Keys.onPressed: function(event) {
            if (!event.isAutoRepeat)
                window.activeInputDevice = "keyboard"
        }

        HoverHandler {
            acceptedDevices: PointerDevice.Mouse
            onPointChanged: window.activeInputDevice = "mouse"
        }

        // Input hints, styled like the Library hint bar. A reads "Play" or
        // "Install" when that button has focus.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            width: Math.min(parent.width - 48, detailsHintRow.implicitWidth + 40)
            height: 46
            radius: 23
            color: Qt.rgba(theme.sidebarBackground.r, theme.sidebarBackground.g,
                           theme.sidebarBackground.b, 0.90)
            border.color: theme.divider
            z: 20

            Row {
                id: detailsHintRow
                anchors.centerIn: parent
                spacing: 16

                Repeater {
                    model: window.inputHintItems().map(function(item) {
                        if (item.label !== "Select")
                            return item
                        const label = detailsPlayButton.activeFocus ? "Play"
                            : detailsInstallButton.activeFocus ? "Install" : item.label
                        return Object.assign({}, item, { label: label })
                    })
                    delegate: Row {
                        required property var modelData
                        spacing: 5
                        anchors.verticalCenter: parent ? parent.verticalCenter : undefined

                        Image {
                            visible: modelData.icon !== ""
                            source: modelData.icon
                            width: 17
                            height: 17
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            visible: modelData.key !== ""
                            width: Math.max(22, detailsHintKeyLabel.implicitWidth + 12)
                            height: 23
                            radius: modelData.round ? 12 : 5
                            color: modelData.round
                                   ? Qt.rgba(0.22, 0.68, 0.50, 0.16)
                                   : Qt.rgba(0.58, 0.68, 0.82, 0.12)
                            border.color: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                            Label {
                                id: detailsHintKeyLabel
                                anchors.centerIn: parent
                                text: modelData.key
                                color: modelData.color
                                font.pixelSize: 10 * theme.fontScale
                                font.weight: Font.Bold
                            }
                        }
                        Label {
                            text: modelData.label
                            color: theme.textSecondary
                            font.pixelSize: 11 * theme.fontScale
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        Button {
            id: detailsBackButton
            z: 10
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 28
            anchors.topMargin: 24
            width: detailsBackContent.implicitWidth
            height: 28
            padding: 0
            focusPolicy: Qt.StrongFocus
            background: Item { }
            contentItem: RowLayout {
                id: detailsBackContent
                spacing: 8

                Image {
                    source: "qrc:/icons/back.svg"
                    sourceSize: Qt.size(18, 18)
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    text: "Back to Library"
                    color: "#FFFFFF"
                    font.pixelSize: 12 * theme.fontScale
                    font.bold: true
                    font.underline: detailsBackButton.hovered
                    verticalAlignment: Text.AlignVCenter
                }
            }
            onClicked: gameDetailsPopup.close()
        }

        Timer {
            id: detailsScreenshotTimer
            interval: 9000
            repeat: true
            running: !!(gameDetailsPopup.visible && window.active
                && !settingsManager.reducedMotion && window.selectedGame
                && window.selectedGame.screenshots
                && window.selectedGame.screenshots.length > 1)
            onRunningChanged: console.info("[Details Screenshots] Rotation timer running:", running)
            onTriggered: {
                const count = window.selectedGame.screenshots.length
                gameDetailsPopup.previousScreenshotIndex = gameDetailsPopup.screenshotIndex
                gameDetailsPopup.screenshotIndex = (gameDetailsPopup.screenshotIndex + 1) % count
                gameDetailsPopup.screenshotLayerA = !gameDetailsPopup.screenshotLayerA
                console.info("[Details Screenshots] Advanced to image:",
                    gameDetailsPopup.screenshotIndex, "of", count)
            }
        }

        Image {
            anchors.fill: parent
            visible: !!(window.selectedGame && window.selectedGame.screenshots
                && window.selectedGame.screenshots.length > 0)
            source: visible ? (gameDetailsPopup.screenshotLayerA
                ? window.selectedGame.screenshots[gameDetailsPopup.screenshotIndex]
                : (gameDetailsPopup.previousScreenshotIndex >= 0
                    ? window.selectedGame.screenshots[gameDetailsPopup.previousScreenshotIndex]
                    : window.selectedGame.screenshots[gameDetailsPopup.screenshotIndex])) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            onStatusChanged: {
                if (status === Image.Error)
                    console.warn("[Details Screenshots] Image failed:", source)
                else if (status === Image.Ready) {
                    console.info("[Details Screenshots] Image ready:", source, sourceSize.width, "x", sourceSize.height)
                    if (gameDetailsPopup.screenshotLayerA)
                        gameDetailsPopup.revealSwitchedBackground()
                }
            }
            opacity: visible && gameDetailsPopup.screenshotLayerA
                && status === Image.Ready ? gameDetailsPopup.gameBackgroundOpacity : 0
            Behavior on opacity {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 1500; easing.type: Easing.InOutCubic }
            }
        }
        Image {
            anchors.fill: parent
            visible: !!(window.selectedGame && window.selectedGame.screenshots
                && window.selectedGame.screenshots.length > 0)
            source: visible ? (gameDetailsPopup.screenshotLayerA
                ? (gameDetailsPopup.previousScreenshotIndex >= 0
                    ? window.selectedGame.screenshots[gameDetailsPopup.previousScreenshotIndex]
                    : window.selectedGame.screenshots[gameDetailsPopup.screenshotIndex])
                : window.selectedGame.screenshots[gameDetailsPopup.screenshotIndex]) : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            onStatusChanged: {
                if (status === Image.Error)
                    console.warn("[Details Screenshots] Image failed:", source)
                else if (status === Image.Ready) {
                    console.info("[Details Screenshots] Image ready:", source, sourceSize.width, "x", sourceSize.height)
                    if (!gameDetailsPopup.screenshotLayerA)
                        gameDetailsPopup.revealSwitchedBackground()
                }
            }
            opacity: visible && !gameDetailsPopup.screenshotLayerA
                && status === Image.Ready ? gameDetailsPopup.gameBackgroundOpacity : 0
            Behavior on opacity {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 1500; easing.type: Easing.InOutCubic }
            }
        }
        Image {
            id: detailsBackdropImage
            anchors.fill: parent
            visible: !(window.selectedGame && window.selectedGame.screenshots
                && window.selectedGame.screenshots.length > 0)
            source: window.selectedGame ? window.selectedGame.artworkPath : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            opacity: status === Image.Ready
                ? 0.62 * gameDetailsPopup.gameBackgroundOpacity : 0
            onStatusChanged: {
                if (status === Image.Ready && visible)
                    gameDetailsPopup.revealSwitchedBackground()
            }
            Behavior on opacity {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 900; easing.type: Easing.OutCubic }
            }
        }

        MultiEffect {
            anchors.fill: detailsBackdropImage
            source: detailsBackdropImage
            visible: detailsBackdropImage.visible
            blurEnabled: true
            blur: 0.86
            autoPaddingEnabled: false
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: Qt.rgba(theme.windowBackground.r,
                                   theme.windowBackground.g,
                                   theme.windowBackground.b, 0.48)
                }
                GradientStop {
                    position: 0.55
                    color: Qt.rgba(theme.windowBackground.r,
                                   theme.windowBackground.g,
                                   theme.windowBackground.b, 0.62)
                }
                GradientStop {
                    position: 1.0
                    color: Qt.rgba(theme.backgroundBottom.r,
                                   theme.backgroundBottom.g,
                                   theme.backgroundBottom.b, 0.78)
                }
            }
        }

        // ------------------------------------------------------------------
        // Layout (redesign pass 2): cover at its platform shape beside the
        // title and the primary launcher action, then About / Screenshots
        // and a Recompilation card. All actions call the same window helpers
        // and popup state as before.
        // ------------------------------------------------------------------
        readonly property real pageMargin: Math.max(32, Math.min(88, width * 0.045))
        // Spare height when everything fits; the hero moves down by part of
        // it so the page is centered (slightly high) instead of top-heavy.
        readonly property real verticalSlack: Math.max(0,
            (height - 76 - 70) - (detailsHero.height + 32 + gameInfoScroll.contentHeight))
        readonly property var gamePosition: {
            const games = gameGrid.model || []
            if (!window.selectedGame)
                return { index: -1, count: games.length }
            for (let i = 0; i < games.length; ++i) {
                if (Number(games[i].id) === Number(window.selectedGame.id))
                    return { index: i, count: games.length }
            }
            return { index: -1, count: games.length }
        }

        // LB / RB position, top right.
        Row {
            z: 10
            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.top: parent.top
            anchors.topMargin: 26
            spacing: 10
            visible: detailsContentFrame.gamePosition.index >= 0
            Label {
                visible: window.activeInputDevice === "controller" && controllerManager.connected
                text: window.platformCycleButtonLabels()[0]
                color: theme.textMuted
                font.pixelSize: 10 * theme.fontScale
                font.bold: true
                leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                background: Rectangle { radius: 4; color: "transparent"; border.color: theme.divider }
            }
            Label {
                text: (detailsContentFrame.gamePosition.index + 1) + " of " + detailsContentFrame.gamePosition.count
                color: theme.textSecondary
                font.pixelSize: 12 * theme.fontScale
            }
            Label {
                visible: window.activeInputDevice === "controller" && controllerManager.connected
                text: window.platformCycleButtonLabels()[1]
                color: theme.textMuted
                font.pixelSize: 10 * theme.fontScale
                font.bold: true
                leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                background: Rectangle { radius: 4; color: "transparent"; border.color: theme.divider }
            }
        }

        // Hero: cover, chips, title, metadata line, primary action.
        Item {
            id: detailsHero
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: detailsContentFrame.pageMargin
            anchors.rightMargin: detailsContentFrame.pageMargin
            anchors.topMargin: 76 + Math.round(detailsContentFrame.verticalSlack * 0.42)
            readonly property real coverHeight: Math.round(Math.max(220, Math.min(360, detailsContentFrame.height * 0.36)))
            height: coverHeight

            GameArtwork {
                id: detailsCover
                anchors.left: parent.left
                anchors.top: parent.top
                height: detailsHero.coverHeight
                width: Math.round(height * LibraryGridLayout.frameRatioForPlatform(
                    window.selectedGame ? window.selectedGame.platform : ""))
                source: window.selectedGame ? window.selectedGame.artworkPath : ""
                platform: window.selectedGame ? window.selectedGame.platform : ""
                themeTokens: theme
                reducedMotion: settingsManager.reducedMotion
                frameMode: "platform"
                frameInset: 0
                showViewportBackground: false
            }

            ColumnLayout {
                anchors.left: detailsCover.right
                anchors.leftMargin: 36
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                spacing: 10

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        height: 26
                        width: detailsPlatformChipLabel.implicitWidth + 24
                        radius: 13
                        color: Qt.rgba(theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.14)
                        Label {
                            id: detailsPlatformChipLabel
                            anchors.centerIn: parent
                            text: window.selectedGame
                                ? window.platformDisplayName(window.selectedGame.platform).toUpperCase() : ""
                            color: theme.textPrimary
                            font.pixelSize: 11 * theme.fontScale
                            font.bold: true
                            font.letterSpacing: 0.6
                        }
                    }
                    Rectangle {
                        visible: gameDetailsPopup.selectedProject !== null
                        height: 26
                        width: detailsProjectChipLabel.implicitWidth + 24
                        radius: 13
                        color: gameDetailsPopup.installedProject !== null
                            ? Qt.rgba(theme.success.r, theme.success.g, theme.success.b, 0.16)
                            : Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                        Label {
                            id: detailsProjectChipLabel
                            anchors.centerIn: parent
                            text: !gameDetailsPopup.selectedProject ? ""
                                : gameDetailsPopup.selectedProject.name
                                    + (gameDetailsPopup.installedProject !== null ? " · Installed"
                                        : (gameDetailsPopup.downloadState.loading || gameDetailsPopup.downloadState.installing)
                                            ? " · Installing" : " · Available")
                            color: gameDetailsPopup.installedProject !== null ? theme.success : theme.accentHover
                            font.pixelSize: 11 * theme.fontScale
                            font.bold: true
                        }
                    }
                    Rectangle {
                        visible: gameDetailsPopup.selectedProject !== null
                            && gameDetailsPopup.selectedProject.developmentStatus !== undefined
                            && gameDetailsPopup.selectedProject.developmentStatus !== "playable"
                        height: 26
                        width: detailsDevStatusLabel.implicitWidth + 24
                        radius: 13
                        color: gameDetailsPopup.selectedProject
                            && gameDetailsPopup.selectedProject.developmentStatus === "beta"
                            ? theme.warningSurface : theme.neutralStatusSurface
                        Label {
                            id: detailsDevStatusLabel
                            anchors.centerIn: parent
                            text: gameDetailsPopup.selectedProject
                                ? String(gameDetailsPopup.selectedProject.developmentStatus).replace("_", " ").toUpperCase() : ""
                            color: theme.warningText
                            font.pixelSize: 11 * theme.fontScale
                            font.bold: true
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: window.selectedGame
                        ? (window.selectedGame.metadataTitle || window.selectedGame.title) : ""
                    color: theme.textPrimary
                    font.pixelSize: 46 * theme.fontScale
                    font.bold: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    readonly property string year: window.selectedGame && window.selectedGame.metadataReleaseDate
                        ? String(window.selectedGame.metadataReleaseDate).substring(0, 4) : ""
                    readonly property string rating: window.selectedGame
                        ? window.formatGameRating(window.selectedGame.metadataRating) : ""
                    text: [year, rating].filter(part => part.length > 0).join("  ·  ")
                        || (window.selectedGame && !window.selectedGame.metadataTitle
                            ? "Local library entry" : "")
                    visible: text.length > 0
                    color: theme.textSecondary
                    font.pixelSize: 15 * theme.fontScale
                }

                // Primary action, following the launcher lifecycle.
                RowLayout {
                    id: detailsActions
                    Layout.topMargin: 8
                    spacing: 10
                    readonly property bool hasProject: gameDetailsPopup.selectedProject !== null
                    readonly property bool installed: gameDetailsPopup.installedProject !== null
                    readonly property bool busy: !!gameDetailsPopup.downloadState.loading
                        || !!gameDetailsPopup.downloadState.installing
                    readonly property bool running: hasProject
                        && !!window.runningLaunchers[gameDetailsPopup.selectedProject.id]
                    readonly property bool canInstall: hasProject && !installed && !busy
                        && !gameDetailsPopup.releaseState.loading
                        && gameDetailsPopup.releaseState.assets.length > 0
                    readonly property bool canFind: hasProject && !installed && !busy
                        && !gameDetailsPopup.releaseState.loading
                        && !gameDetailsPopup.releaseInfo
                        && gameDetailsPopup.releaseState.error.length === 0
                    property bool moreOpen: false

                    // Installed: Play.
                    Button {
                        id: detailsPlayButton
                        visible: detailsActions.installed
                        enabled: !detailsActions.running
                        text: detailsActions.running ? "Running…" : "Play"
                        icon.source: "qrc:/icons/play.svg"
                        icon.width: 18
                        icon.height: 18
                        implicitHeight: 52
                        Layout.preferredWidth: 150
                        background: Rectangle {
                            radius: height / 2
                            color: detailsPlayButton.enabled ? theme.accent : theme.controlBackground
                            border.width: detailsPlayButton.activeFocus ? 3 : 0
                            border.color: theme.textPrimary
                        }
                        onClicked: window.launchSupportedGame(gameDetailsPopup.selectedProject.id, window.selectedGame)
                    }
                    // Not installed, release found: Install (asks to confirm below).
                    Button {
                        id: detailsInstallButton
                        visible: detailsActions.canInstall && !gameDetailsPopup.installPromptVisible
                        text: "Install " + (gameDetailsPopup.selectedProject ? gameDetailsPopup.selectedProject.name : "")
                        icon.source: "qrc:/icons/install.svg"
                        icon.width: 18
                        icon.height: 18
                        implicitHeight: 52
                        leftPadding: 24
                        rightPadding: 24
                        background: Rectangle {
                            radius: height / 2
                            color: theme.accent
                            border.width: detailsInstallButton.activeFocus ? 3 : 0
                            border.color: theme.textPrimary
                        }
                        onClicked: gameDetailsPopup.installPromptVisible = true
                    }
                    // Install confirmation (unchanged behavior).
                    Label {
                        visible: gameDetailsPopup.installPromptVisible && !detailsActions.installed
                        text: gameDetailsPopup.selectedProject
                            ? (gameDetailsPopup.selectedProject.developmentStatus === "beta"
                                ? "Install this BETA launcher?"
                                : gameDetailsPopup.selectedProject.developmentStatus === "alpha"
                                    ? "Install this ALPHA launcher?"
                                    : "Install this launcher?")
                            : "Install launcher?"
                        color: theme.textPrimary
                        font.pixelSize: 15 * theme.fontScale
                        font.bold: true
                    }
                    Button {
                        id: detailsInstallYesButton
                        visible: gameDetailsPopup.installPromptVisible && !detailsActions.installed
                        highlighted: true
                        text: "Yes"
                        icon.source: "qrc:/icons/check.svg"
                        icon.width: 16
                        icon.height: 16
                        implicitHeight: 46
                        Layout.minimumWidth: 96
                        onClicked: {
                            gameDetailsPopup.installPromptVisible = false
                            const asset = gameDetailsPopup.selectedAsset
                            if (asset)
                                window.startAssetDownload(gameDetailsPopup.selectedProject, asset)
                        }
                    }
                    Button {
                        visible: gameDetailsPopup.installPromptVisible && !detailsActions.installed
                        text: "No"
                        icon.source: "qrc:/icons/close.svg"
                        icon.width: 16
                        icon.height: 16
                        implicitHeight: 46
                        Layout.minimumWidth: 96
                        onClicked: gameDetailsPopup.installPromptVisible = false
                    }
                    // Not installed, release not checked yet: look it up.
                    Button {
                        id: detailsFindButton
                        visible: detailsActions.canFind
                        enabled: !window.pendingReleaseRequestsForProject(
                            gameDetailsPopup.selectedProject ? gameDetailsPopup.selectedProject.id : "")
                        text: "Find compatible launcher"
                        implicitHeight: 52
                        leftPadding: 24
                        rightPadding: 24
                        background: Rectangle {
                            radius: height / 2
                            color: theme.accent
                            border.width: detailsFindButton.activeFocus ? 3 : 0
                            border.color: theme.textPrimary
                        }
                        onClicked: window.checkProjectRelease(gameDetailsPopup.selectedProject)
                    }
                    // Checking for a release while not installed.
                    Button {
                        visible: detailsActions.hasProject && !detailsActions.installed && !detailsActions.busy
                            && gameDetailsPopup.releaseState.loading
                        enabled: false
                        text: "Checking for releases…"
                        implicitHeight: 52
                        leftPadding: 24
                        rightPadding: 24
                    }
                    // Downloading / installing progress.
                    Rectangle {
                        visible: detailsActions.busy
                        Layout.preferredWidth: 320
                        Layout.preferredHeight: 52
                        radius: 26
                        color: theme.controlBackground
                        clip: true
                        Accessible.role: Accessible.ProgressBar
                        Accessible.name: detailsProgressLabel.text
                        readonly property real progress: gameDetailsPopup.downloadState.installing ? 1
                            : gameDetailsPopup.downloadState.bytesTotal > 0
                                ? Math.max(0, Math.min(1, (gameDetailsPopup.downloadState.bytesReceived || 0)
                                    / gameDetailsPopup.downloadState.bytesTotal)) : 0
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * parent.progress
                            color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.55)
                        }
                        Label {
                            id: detailsProgressLabel
                            anchors.centerIn: parent
                            text: gameDetailsPopup.downloadState.installing
                                ? "Installing…"
                                : "Downloading…  " + Math.floor(parent.progress * 100) + "%"
                            color: theme.textPrimary
                            font.pixelSize: 14 * theme.fontScale
                            font.bold: true
                        }
                    }
                    // Release check failed: retry.
                    Button {
                        visible: detailsActions.hasProject && gameDetailsPopup.releaseState.error.length > 0
                            && !detailsActions.installed
                        text: "Retry"
                        implicitHeight: 52
                        Layout.minimumWidth: 110
                        onClicked: window.checkProjectRelease(gameDetailsPopup.selectedProject)
                    }
                    // Installed: secondary actions.
                    Button {
                        visible: detailsActions.installed
                        enabled: !gameDetailsPopup.releaseState.loading
                            && !(gameDetailsPopup.selectedProject
                                 && window.pendingReleaseRequestsForProject(gameDetailsPopup.selectedProject.id))
                        text: gameDetailsPopup.releaseState.loading ? "Checking…" : "Check for updates"
                        implicitHeight: 46
                        leftPadding: 18
                        rightPadding: 18
                        onClicked: window.checkProjectRelease(gameDetailsPopup.selectedProject)
                    }
                    Button {
                        id: detailsMoreButton
                        visible: detailsActions.installed
                        text: "⋯"
                        Accessible.name: "More actions"
                        implicitHeight: 46
                        implicitWidth: 46
                        onClicked: detailsActions.moreOpen = !detailsActions.moreOpen
                    }
                    // The ⋯ menu opens inline so it stays reachable with the
                    // D-pad; Uninstall still asks for confirmation.
                    Button {
                        id: detailsUninstallButton
                        visible: detailsActions.installed && detailsActions.moreOpen
                        text: "Uninstall…"
                        icon.source: "qrc:/icons/uninstall.svg"
                        icon.width: 16
                        icon.height: 16
                        implicitHeight: 46
                        leftPadding: 18
                        rightPadding: 18
                        onClicked: {
                            detailsActions.moreOpen = false
                            window.pendingUninstallProjectId = gameDetailsPopup.selectedProject.id
                            uninstallDialog.open()
                        }
                    }
                    Connections {
                        target: gameDetailsPopup
                        function onSelectedProjectChanged() { detailsActions.moreOpen = false }
                        function onClosed() { detailsActions.moreOpen = false }
                    }
                }

                // Will Play just work? One line, and what to do if not.
                RowLayout {
                    id: detailsLaunchStatus
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 12
                    visible: gameDetailsPopup.installedProject !== null
                        && (gameDetailsPopup.launchState.state || "").length > 0
                        && !(gameDetailsPopup.selectedProject
                             && window.launchErrors[gameDetailsPopup.selectedProject.id])
                    readonly property string stateName: gameDetailsPopup.launchState.state || ""
                    readonly property color toneColor: gameDetailsPopup.launchTone === "ok" ? theme.success
                        : gameDetailsPopup.launchTone === "warn" ? theme.warning : theme.accent

                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 5
                        width: 18; height: 18; radius: 9
                        color: Qt.rgba(detailsLaunchStatus.toneColor.r, detailsLaunchStatus.toneColor.g,
                                       detailsLaunchStatus.toneColor.b, 0.22)
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10; height: 10; radius: 5
                            color: detailsLaunchStatus.toneColor
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: gameDetailsPopup.launchState.title || ""
                            color: theme.textPrimary
                            font.pixelSize: 14 * theme.fontScale
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: gameDetailsPopup.launchState.detail || ""
                            color: theme.textMuted
                            font.pixelSize: 12.5 * theme.fontScale
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            Layout.topMargin: 6
                            spacing: 8
                            visible: detailsShowFileButton.visible || detailsAddCopyButton.visible
                            Button {
                                id: detailsShowFileButton
                                visible: detailsLaunchStatus.stateName === "manual" || detailsLaunchStatus.stateName === "disabled"
                                text: "Show file"
                                implicitHeight: 38
                                onClicked: Qt.openUrlExternally("file://" + gameDetailsPopup.folderOf(gameDetailsPopup.launchState.gameFile || ""))
                            }
                            Button {
                                visible: detailsShowFileButton.visible
                                text: "Copy path"
                                implicitHeight: 38
                                onClicked: gameDetailsPopup.copyText(gameDetailsPopup.launchState.gameFile || "")
                            }
                            Button {
                                id: detailsAddCopyButton
                                visible: detailsLaunchStatus.stateName === "wrong_version"
                                    || detailsLaunchStatus.stateName === "wrong_game"
                                    || detailsLaunchStatus.stateName === "wrong_file"
                                    || detailsLaunchStatus.stateName === "no_game"
                                text: "Add another copy"
                                implicitHeight: 38
                                onClicked: window.openAddGames([])
                            }
                        }
                    }
                }

                // One line of context or the latest error.
                Label {
                    Layout.fillWidth: true
                    readonly property string launchError: gameDetailsPopup.selectedProject
                        ? (window.launchErrors[gameDetailsPopup.selectedProject.id] || "") : ""
                    readonly property string downloadError: gameDetailsPopup.downloadState.error || ""
                    readonly property string releaseError: gameDetailsPopup.releaseState.error || ""
                    readonly property string errorText: launchError || downloadError || releaseError
                    text: errorText.length > 0 ? errorText
                        : !gameDetailsPopup.selectedProject ? ""
                        : gameDetailsPopup.installedProject !== null
                            ? (gameDetailsPopup.releaseInfo && !detailsLaunchStatus.visible
                                ? "Latest release " + gameDetailsPopup.releaseTag + "  ·  Linux / " + projectCatalog.currentArchitecture
                                : "")
                        : gameDetailsPopup.releaseInfo && gameDetailsPopup.compatibleAssets.length === 0
                            ? "No compatible Linux launcher package is available for " + projectCatalog.currentArchitecture + "."
                        : gameDetailsPopup.releaseInfo
                            ? "Version " + gameDetailsPopup.releaseTag + "  ·  Linux / " + projectCatalog.currentArchitecture
                        : ""
                    visible: text.length > 0
                    color: errorText.length > 0 ? theme.error : theme.textMuted
                    font.pixelSize: 12 * theme.fontScale
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
        }

        // About, screenshots and the recompilation card.
        ScrollView {
            id: gameInfoScroll
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: detailsHero.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: detailsContentFrame.pageMargin
            anchors.rightMargin: detailsContentFrame.pageMargin
            anchors.topMargin: 32
            anchors.bottomMargin: 70
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            GridLayout {
                width: gameInfoScroll.availableWidth
                columns: width > 1100 ? 2 : 1
                columnSpacing: 32
                rowSpacing: 24

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.horizontalStretchFactor: 5
                    Layout.alignment: Qt.AlignTop
                    spacing: 10

                    Label {
                        text: "ABOUT"
                        color: theme.textMuted
                        font.pixelSize: 11 * theme.fontScale
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!window.selectedGame && !!window.selectedGame.metadataTitle
                            && window.selectedGame.metadataTitle.length > 0
                        text: window.selectedGame ? window.selectedGame.metadataOverview : ""
                        color: theme.textSecondary
                        font.pixelSize: 15 * theme.fontScale
                        wrapMode: Text.Wrap
                        lineHeight: 1.22
                        lineHeightMode: Text.ProportionalHeight
                    }
                    // No metadata: same guidance and actions as before.
                    Rectangle {
                        Layout.fillWidth: true
                        visible: !!window.selectedGame
                            && (!window.selectedGame.metadataTitle
                                || window.selectedGame.metadataTitle.length === 0)
                        radius: theme.radiusCard
                        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.85)
                        border.color: theme.divider
                        implicitHeight: metadataEmptyContent.implicitHeight + 32
                        ColumnLayout {
                            id: metadataEmptyContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 10
                            Label {
                                Layout.fillWidth: true
                                text: "GAME INFORMATION NOT FOUND"
                                color: theme.textPrimary
                                font.pixelSize: 13 * theme.fontScale
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "We couldn't find matching game information. Try a different metadata provider in Settings → Metadata, then retry the lookup. Your local game entry is safe, and compatible launchers can still be installed and played."
                                color: theme.textSecondary
                                font.pixelSize: 14 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            RowLayout {
                                spacing: 10
                                Button {
                                    text: metadataMatcher.busy ? "Checking Metadata…" : "Retry Metadata"
                                    enabled: (metadataManager.configured
                                        || (settingsManager.launchBoxEnabled
                                            && launchBoxCatalogManager.catalogAvailable))
                                        && !metadataMatcher.busy
                                    onClicked: {
                                        console.info("[UI] Retry metadata requested for game ID:", window.selectedGame.id)
                                        metadataMatcher.scrapeGame(window.selectedGame.id)
                                    }
                                }
                                Button {
                                    text: "View Metadata Settings"
                                    onClicked: {
                                        gameDetailsPopup.close()
                                        currentPage = "Settings"
                                        settingsPage.section = "Metadata"
                                        Qt.callLater(function() {
                                            metadataNavigationButton.forceActiveFocus()
                                        })
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        Layout.topMargin: 12
                        text: "SCREENSHOTS"
                        color: theme.textMuted
                        font.pixelSize: 11 * theme.fontScale
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Flow {
                        id: detailsScreenshotStrip
                        Layout.fillWidth: true
                        spacing: 10
                        readonly property var shots: window.selectedGame && window.selectedGame.screenshots
                            ? window.selectedGame.screenshots.slice(0, 3) : []
                        readonly property real thumbWidth: Math.floor(Math.min(380, (width - 20) / 3))
                        Repeater {
                            model: detailsScreenshotStrip.shots
                            // D-pad / click: choosing a screenshot makes it the background.
                            delegate: Button {
                                id: detailsShotButton
                                required property string modelData
                                required property int index
                                activeFocusOnTab: true
                                focusPolicy: Qt.StrongFocus
                                width: detailsScreenshotStrip.thumbWidth
                                height: Math.round(width * 9 / 16)
                                padding: 0
                                Accessible.name: "Show screenshot " + (index + 1) + " as background"
                                contentItem: Item {}
                                background: Rectangle {
                                    radius: theme.radiusSmall
                                    color: theme.surface
                                    clip: true
                                    // Accent ring when focused; white outline on the
                                    // screenshot currently shown as the background.
                                    border.width: detailsShotButton.activeFocus ? 3
                                        : detailsShotButton.index === gameDetailsPopup.screenshotIndex ? 2 : 0
                                    border.color: detailsShotButton.activeFocus ? theme.accent : theme.textPrimary
                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: parent.border.width
                                        source: detailsShotButton.modelData
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: true
                                    }
                                }
                                onClicked: gameDetailsPopup.selectScreenshot(index)
                            }
                        }
                        Label {
                            visible: detailsScreenshotStrip.shots.length === 0
                            text: "No screenshots yet. Run Refresh All Metadata in Settings to fetch them."
                            color: theme.textMuted
                            font.pixelSize: 13 * theme.fontScale
                        }
                    }
                }

                // Recompilation card.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.horizontalStretchFactor: 4
                    Layout.alignment: Qt.AlignTop
                    radius: theme.radiusLarge
                    color: Qt.rgba(theme.sidebarBackground.r, theme.sidebarBackground.g, theme.sidebarBackground.b, 0.88)
                    border.color: Qt.rgba(theme.divider.r, theme.divider.g, theme.divider.b, 0.7)
                    implicitHeight: supportContent.implicitHeight + 40

                    ColumnLayout {
                        id: supportContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 20
                        spacing: 10

                        Label {
                            text: "RECOMPILATION"
                            color: theme.textMuted
                            font.pixelSize: 11 * theme.fontScale
                            font.bold: true
                            font.letterSpacing: 0.8
                        }
                        // Games with several supported projects: pick one.
                        Flow {
                            Layout.fillWidth: true
                            visible: gameDetailsPopup.supportedProjects.length > 1
                            spacing: 8
                            Repeater {
                                model: gameDetailsPopup.supportedProjects
                                delegate: Button {
                                    id: projectChip
                                    required property var modelData
                                    readonly property bool selected: gameDetailsPopup.selectedProject !== null
                                        && gameDetailsPopup.selectedProject.id === modelData.id
                                    text: modelData.name
                                    implicitHeight: 34
                                    leftPadding: 14
                                    rightPadding: 14
                                    background: Rectangle {
                                        radius: height / 2
                                        color: projectChip.selected ? theme.textPrimary : theme.controlBackground
                                        border.width: projectChip.activeFocus ? 2 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: Label {
                                        text: projectChip.text
                                        color: projectChip.selected ? theme.windowBackground : theme.textPrimary
                                        font.pixelSize: 12 * theme.fontScale
                                        font.bold: projectChip.selected
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: gameDetailsPopup.selectedProjectId = modelData.id
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: gameDetailsPopup.selectedProject
                                ? gameDetailsPopup.selectedProject.name
                                : "No supported recompilation project is currently listed for this game."
                            color: gameDetailsPopup.selectedProject ? theme.textPrimary : theme.textSecondary
                            font.pixelSize: (gameDetailsPopup.selectedProject ? 19 : 14) * theme.fontScale
                            font.bold: gameDetailsPopup.selectedProject !== null
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: gameDetailsPopup.selectedProject !== null
                            text: gameDetailsPopup.selectedProject
                                ? gameDetailsPopup.selectedProject.repositoryOwner + "/"
                                    + gameDetailsPopup.selectedProject.repositoryName : ""
                            color: theme.textMuted
                            font.pixelSize: 12 * theme.fontScale
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: gameDetailsPopup.selectedProject !== null
                                && (gameDetailsPopup.selectedProject.developmentStatus === "beta"
                                    || gameDetailsPopup.selectedProject.developmentStatus === "alpha")
                            text: gameDetailsPopup.selectedProject
                                && gameDetailsPopup.selectedProject.developmentStatus === "beta"
                                ? "This project is in beta. Features may be incomplete and you may encounter bugs."
                                : "This project is in alpha. Expect incomplete features and potential instability."
                            color: theme.warningText
                            font.pixelSize: 12 * theme.fontScale
                            wrapMode: Text.Wrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            visible: gameDetailsPopup.selectedProject !== null
                            columns: 2
                            columnSpacing: 18
                            rowSpacing: 8

                            Label { text: "Status"; color: theme.textMuted; font.pixelSize: 13 * theme.fontScale }
                            Label {
                                Layout.fillWidth: true
                                text: gameDetailsPopup.installedProject !== null ? "Installed"
                                    : (gameDetailsPopup.downloadState.loading || gameDetailsPopup.downloadState.installing) ? "Installing"
                                    : gameDetailsPopup.releaseState.loading ? "Checking for releases…"
                                    : "Not installed"
                                color: gameDetailsPopup.installedProject !== null ? theme.success : theme.accentHover
                                font.pixelSize: 13 * theme.fontScale
                                font.bold: true
                                wrapMode: Text.Wrap
                            }
                            Label { text: "Release"; color: theme.textMuted; font.pixelSize: 13 * theme.fontScale }
                            Label {
                                Layout.fillWidth: true
                                text: gameDetailsPopup.releaseInfo ? gameDetailsPopup.releaseTag : "Not checked yet"
                                color: theme.textSecondary
                                font.pixelSize: 13 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            Label { text: "Package"; color: theme.textMuted; font.pixelSize: 13 * theme.fontScale }
                            Label {
                                Layout.fillWidth: true
                                text: gameDetailsPopup.selectedAsset
                                    ? gameDetailsPopup.selectedAssetName
                                    : "Linux / " + projectCatalog.currentArchitecture
                                color: theme.textSecondary
                                font.pixelSize: 13 * theme.fontScale
                                wrapMode: Text.WrapAnywhere
                            }
                            Label {
                                visible: !!(gameDetailsPopup.selectedProject && gameDetailsPopup.selectedProject.romRequirements)
                                text: "ROM"
                                color: theme.textMuted
                                font.pixelSize: 13 * theme.fontScale
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !!(gameDetailsPopup.selectedProject && gameDetailsPopup.selectedProject.romRequirements)
                                text: gameDetailsPopup.selectedProject ? (gameDetailsPopup.selectedProject.romRequirements || "") : ""
                                color: theme.warning
                                font.pixelSize: 13 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            Label {
                                visible: gameDetailsPopup.installedProject !== null
                                text: "Installed at"
                                color: theme.textMuted
                                font.pixelSize: 13 * theme.fontScale
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.installedProject !== null
                                text: gameDetailsPopup.installedProject !== null
                                    ? gameDetailsPopup.installedProject.executablePath : ""
                                color: theme.textMuted
                                font.pixelSize: 11 * theme.fontScale
                                wrapMode: Text.WrapAnywhere
                            }
                        }

                        // How Play starts this game: which file, window mode, saves.
                        ColumnLayout {
                            id: detailsLaunchSection
                            Layout.fillWidth: true
                            Layout.topMargin: 10
                            spacing: 8
                            visible: gameDetailsPopup.installedProject !== null
                                && (gameDetailsPopup.launchState.state || "").length > 0
                            readonly property string method: gameDetailsPopup.launchState.method || ""

                            Label {
                                text: "LAUNCH"
                                color: theme.textMuted
                                font.pixelSize: 11 * theme.fontScale
                                font.bold: true
                                font.letterSpacing: 0.8
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 3
                                columnSpacing: 14
                                rowSpacing: 8

                                Label { text: "Game file"; color: theme.textMuted; font.pixelSize: 13 * theme.fontScale }
                                Label {
                                    Layout.fillWidth: true
                                    text: {
                                        const path = window.selectedGame ? (window.selectedGame.path || "") : ""
                                        return path.length > 0 ? path.substring(path.lastIndexOf("/") + 1) : "None"
                                    }
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                    elide: Text.ElideMiddle
                                }
                                Item { implicitWidth: 1; implicitHeight: 1 }

                                Label {
                                    visible: detailsLaunchSection.method === "n64recomp"
                                    text: "Window"
                                    color: theme.textMuted
                                    font.pixelSize: 13 * theme.fontScale
                                }
                                ValueCycler {
                                    id: detailsWindowModeCycler
                                    visible: detailsLaunchSection.method === "n64recomp"
                                    Layout.columnSpan: 2
                                    preferredWidth: 270
                                    options: [
                                        { label: settingsManager.startGamesFullscreen ? "Default (fullscreen)" : "Default", value: "default" },
                                        { label: "Fullscreen", value: "fullscreen" },
                                        { label: "Windowed", value: "windowed" }
                                    ]
                                    value: {
                                        settingsManager.launchSettingsRevision
                                        return gameDetailsPopup.selectedProject
                                            ? settingsManager.launchWindowMode(gameDetailsPopup.selectedProject.id) : "default"
                                    }
                                    onPicked: (value) => settingsManager.setLaunchWindowMode(gameDetailsPopup.selectedProject.id, value)
                                }

                                Label {
                                    visible: (gameDetailsPopup.launchState.savesFolder || "").length > 0
                                    text: "Saves"
                                    color: theme.textMuted
                                    font.pixelSize: 13 * theme.fontScale
                                }
                                Label {
                                    visible: (gameDetailsPopup.launchState.savesFolder || "").length > 0
                                    Layout.fillWidth: true
                                    text: gameDetailsPopup.shortPath(gameDetailsPopup.launchState.savesFolder)
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                    elide: Text.ElideMiddle
                                }
                                Button {
                                    visible: (gameDetailsPopup.launchState.savesFolder || "").length > 0
                                    text: "Open folder"
                                    implicitHeight: 34
                                    onClicked: {
                                        const saves = gameDetailsPopup.launchState.savesFolder
                                        // Before the first start the saves folder may not exist yet.
                                        Qt.openUrlExternally("file://" + (platformManager.pathExists(saves)
                                            ? saves : gameDetailsPopup.launchState.dataFolder))
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: detailsLaunchSection.method === "n64recomp"
                                text: "A window choice here is applied each time this game starts (from its second start, once the launcher has created its settings). \"Default\" follows Settings › Library."
                                color: theme.textMuted
                                font.pixelSize: 11.5 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        }

        // Focus the most useful action when the page opens or switches games,
        // so A plays or installs straight away; Back otherwise.
        function focusPrimaryAction() {
            const candidates = [detailsPlayButton, detailsInstallButton, detailsFindButton]
            for (let i = 0; i < candidates.length; ++i) {
                if (candidates[i].visible && candidates[i].enabled) {
                    candidates[i].forceActiveFocus()
                    return
                }
            }
            detailsBackButton.forceActiveFocus()
        }
    }
}

