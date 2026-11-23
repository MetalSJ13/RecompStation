import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

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
            detailsBackButton.forceActiveFocus()
        }
    }

    onOpened: {
        resettingProjectSelection = true
        selectedProjectId = supportedProjects.length > 0 ? supportedProjects[0].id : ""
        resettingProjectSelection = false
        detailsBackButton.forceActiveFocus()
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

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            width: Math.min(parent.width - 48, detailsHintLabel.implicitWidth + 34)
            height: 38
            radius: 19
            color: Qt.rgba(theme.sidebarBackground.r, theme.sidebarBackground.g,
                           theme.sidebarBackground.b, 0.88)
            border.color: theme.divider
            z: 20
            Label {
                id: detailsHintLabel
                anchors.centerIn: parent
                text: window.inputHintItems().map(function(item) {
                    return item.key.length > 0
                        ? item.key + "  " + item.label : item.label
                }).join("     ")
                color: theme.textSecondary
                font.pixelSize: 10 * theme.fontScale
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

        RowLayout {
            anchors.fill: parent
            anchors.margins: Math.max(32, Math.min(88, detailsContentFrame.width * 0.045))
            spacing: 48

        Rectangle {
            Layout.preferredWidth: 214
            Layout.preferredHeight: 306
            Layout.fillHeight: false
            Layout.alignment: Qt.AlignTop
            radius: theme.radiusCard
            color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.12)
            clip: true

            Image {
                id: gameCoverImage
                anchors.fill: parent
                anchors.margins: 0
                source: window.selectedGame ? window.selectedGame.artworkPath : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
            }
            Image {
                anchors.fill: parent
                anchors.margins: 0
                source: "qrc:/icons/game-artwork-unavailable.png"
                fillMode: Image.PreserveAspectFit
                visible: !window.selectedGame
                    || !window.selectedGame.artworkPath
                    || window.selectedGame.artworkPath.length === 0
                    || gameCoverImage.status === Image.Error
                asynchronous: true
                smooth: true
            }
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 54
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: theme.artworkFade }
                }
            }
            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                text: window.selectedGame ? window.selectedGame.platform : ""
                color: theme.textPrimary
                font.pixelSize: 12 * theme.fontScale
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        Layout.fillWidth: true
                        text: window.selectedGame ? window.selectedGame.title : ""
                        color: theme.textPrimary
                        font.pixelSize: 38 * theme.fontScale
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: window.selectedGame
                            && window.selectedGame.metadataTitle.length > 0
                            ? "GAME INFORMATION"
                            : "LOCAL LIBRARY ENTRY"
                        color: theme.accentHover
                        font.pixelSize: 10 * theme.fontScale
                        font.bold: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: theme.divider
            }

            ScrollView {
                id: gameInfoScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: gameInfoScroll.availableWidth
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 7
                        Label {
                            text: "ABOUT THIS GAME"
                            color: theme.textMuted
                            font.pixelSize: 10 * theme.fontScale
                            font.bold: true
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            visible: !!window.selectedGame
                                && (!window.selectedGame.metadataTitle
                                    || window.selectedGame.metadataTitle.length === 0)
                            radius: theme.radiusCard
                            color: theme.surface
                            border.color: theme.divider
                            border.width: 1
                            implicitHeight: metadataEmptyContent.implicitHeight + 32

                            ColumnLayout {
                                id: metadataEmptyContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 16
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Image {
                                        source: "qrc:/icons/metadata.svg"
                                        sourceSize.width: 24
                                        sourceSize.height: 24
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                        opacity: 0.8
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "GAME INFORMATION NOT FOUND"
                                        color: theme.textPrimary
                                        font.pixelSize: 13 * theme.fontScale
                                        font.bold: true
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "We couldn't find matching game information. Try a different metadata provider in Settings → Metadata, then retry the lookup. Your local game entry is safe, and compatible launchers can still be installed and played."
                                    color: theme.textSecondary
                                    font.pixelSize: 15 * theme.fontScale
                                    wrapMode: Text.Wrap
                                    lineHeight: 1.18
                                    lineHeightMode: Text.ProportionalHeight
                                }
                                RowLayout {
                                    Layout.fillWidth: true
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
                            Layout.fillWidth: true
                            visible: !!window.selectedGame
                                && !!window.selectedGame.metadataTitle
                                && window.selectedGame.metadataTitle.length > 0
                            text: window.selectedGame ? window.selectedGame.metadataOverview : ""
                            color: theme.textSecondary
                            font.pixelSize: 17 * theme.fontScale
                            wrapMode: Text.Wrap
                            lineHeight: 1.18
                            lineHeightMode: Text.ProportionalHeight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: !!window.selectedGame
                                && (window.selectedGame.metadataReleaseDate.length > 0
                                    || window.selectedGame.metadataRating.length > 0)
                            text: window.selectedGame
                                ? [window.selectedGame.metadataReleaseDate,
                                   window.selectedGame.metadataRating.length > 0
                                       ? "Rating " + window.selectedGame.metadataRating : ""]
                                      .filter(value => value.length > 0).join("    •    ")
                                : ""
                            color: theme.textMuted
                            font.pixelSize: 11 * theme.fontScale
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: theme.radiusCard
                        color: theme.surface
                        border.color: theme.divider
                        border.width: 1
                        implicitHeight: supportContent.implicitHeight + 28

                        ColumnLayout {
                            id: supportContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 8

                            Label {
                                text: "RECOMPILATION SUPPORT"
                                color: theme.textMuted
                                font.pixelSize: 10 * theme.fontScale
                                font.bold: true
                            }
                            ComboBox {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.supportedProjects.length > 1
                                textRole: "name"
                                model: gameDetailsPopup.supportedProjects
                                currentIndex: {
                                    for (let i = 0; i < gameDetailsPopup.supportedProjects.length; ++i) {
                                        if (gameDetailsPopup.supportedProjects[i].id
                                                === gameDetailsPopup.selectedProjectId)
                                            return i
                                    }
                                    return 0
                                }
                                onActivated: function(index) {
                                    const project = gameDetailsPopup.supportedProjects[index]
                                    if (project)
                                        gameDetailsPopup.selectedProjectId = project.id
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: gameDetailsPopup.selectedProject
                                    ? gameDetailsPopup.selectedProject.name
                                    : "No supported recompilation project is currently listed for this game."
                                color: gameDetailsPopup.selectedProject ? theme.success : theme.textSecondary
                                font.pixelSize: 14 * theme.fontScale
                                font.bold: true
                                wrapMode: Text.Wrap
                            }
                            Rectangle {
                                visible: gameDetailsPopup.selectedProject !== null
                                    && gameDetailsPopup.selectedProject.developmentStatus !== "playable"
                                radius: 5
                                color: gameDetailsPopup.selectedProject
                                    && gameDetailsPopup.selectedProject.developmentStatus === "beta"
                                    ? theme.warningSurface : theme.neutralStatusSurface
                                implicitWidth: projectStatusLabel.implicitWidth + 16
                                implicitHeight: projectStatusLabel.implicitHeight + 8

                                Label {
                                    id: projectStatusLabel
                                    anchors.centerIn: parent
                                    text: gameDetailsPopup.selectedProject
                                        ? String(gameDetailsPopup.selectedProject.developmentStatus)
                                            .replace("_", " ").toUpperCase() : ""
                                    color: theme.warningText
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                }
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
                                color: theme.textSecondary
                                font.pixelSize: 11 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.installedProject !== null
                                text: gameDetailsPopup.installedProject !== null
                                    ? "Installed at  " + gameDetailsPopup.installedProject.executablePath
                                    : ""
                                color: theme.textMuted
                                font.pixelSize: 10 * theme.fontScale
                                wrapMode: Text.WrapAnywhere
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                text: gameDetailsPopup.selectedProject
                                    ? gameDetailsPopup.selectedProject.romRequirements : ""
                                color: theme.textSecondary
                                font.pixelSize: 11 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                    && window.launchErrors[gameDetailsPopup.selectedProject.id] !== undefined
                                text: gameDetailsPopup.selectedProject
                                    ? (window.launchErrors[gameDetailsPopup.selectedProject.id] || "") : ""
                                color: theme.error
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                    && gameDetailsPopup.releaseState.loading
                                text: "Checking for the latest compatible Linux release…"
                                color: theme.accentHover
                                wrapMode: Text.Wrap
                            }
                            Button {
                                visible: gameDetailsPopup.selectedProject !== null
                                enabled: gameDetailsPopup.selectedProject !== null
                                    && !gameDetailsPopup.releaseState.loading
                                    && !window.pendingReleaseRequestsForProject(
                                        gameDetailsPopup.selectedProject.id)
                                text: gameDetailsPopup.releaseState.loading
                                    ? "Checking…"
                                    : gameDetailsPopup.installedProject !== null
                                        ? "Check for Updates" : "Find Compatible Launcher"
                                onClicked: window.checkProjectRelease(
                                    gameDetailsPopup.selectedProject)
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                    && gameDetailsPopup.releaseState.error.length > 0
                                text: gameDetailsPopup.releaseState.error
                                color: theme.error
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                    && !!gameDetailsPopup.releaseInfo
                                    && gameDetailsPopup.compatibleAssets.length === 0
                                text: "No compatible Linux launcher package is available for "
                                    + projectCatalog.currentArchitecture + "."
                                color: theme.textMuted
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                    && !!gameDetailsPopup.releaseInfo
                                    && gameDetailsPopup.selectedAsset !== null
                                    && gameDetailsPopup.installedProject === null
                                text: "Compatible launcher available"
                                color: theme.success
                                font.pixelSize: 12 * theme.fontScale
                                font.bold: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: gameDetailsPopup.selectedProject !== null
                                    && !!gameDetailsPopup.releaseInfo
                                    && gameDetailsPopup.selectedAsset !== null
                                    && gameDetailsPopup.installedProject === null
                                text: "Version " + gameDetailsPopup.releaseTag
                                    + "    •    Linux / " + projectCatalog.currentArchitecture
                                    + "\n" + gameDetailsPopup.selectedAssetName
                                color: theme.textSecondary
                                font.pixelSize: 11 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !!gameDetailsPopup.selectedProject
                                    && !!gameDetailsPopup.downloadState.error
                                    && gameDetailsPopup.downloadState.error.length > 0
                                text: gameDetailsPopup.downloadState.error || ""
                                color: theme.error
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !!gameDetailsPopup.selectedProject
                                    && (!!gameDetailsPopup.downloadState.loading
                                        || !!gameDetailsPopup.downloadState.installing)
                                text: gameDetailsPopup.downloadState.installing
                                    ? "Extracting and installing launcher…"
                                    : "Downloading launcher… "
                                        + (gameDetailsPopup.downloadState.bytesTotal > 0
                                            ? Math.floor((gameDetailsPopup.downloadState.bytesReceived || 0)
                                                / gameDetailsPopup.downloadState.bytesTotal * 100) + "%"
                                            : "")
                                color: theme.accentHover
                            }
                            ProgressBar {
                                Layout.fillWidth: true
                                visible: !!gameDetailsPopup.downloadState.loading
                                from: 0
                                to: 1
                                value: gameDetailsPopup.downloadState.bytesTotal > 0
                                    ? Math.max(0, Math.min(1,
                                        (gameDetailsPopup.downloadState.bytesReceived || 0)
                                        / gameDetailsPopup.downloadState.bytesTotal))
                                    : 0
                            }
                                                        RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Button {
                                    visible: gameDetailsPopup.installedProject !== null
                                    highlighted: true
                                    text: gameDetailsPopup.selectedProject
                                        && window.runningLaunchers[gameDetailsPopup.selectedProject.id]
                                        ? "Running…" : "Play Game"
                                    icon.source: "qrc:/icons/play.svg"
                                    icon.width: 18
                                    icon.height: 18
                                    icon.color: theme.textPrimary
                                    implicitHeight: 48
                                    Layout.minimumWidth: 150
                                    Layout.preferredWidth: 150
                                    enabled: gameDetailsPopup.selectedProject !== null
                                        && gameDetailsPopup.installedProject !== null
                                        && !window.runningLaunchers[gameDetailsPopup.selectedProject.id]
                                    onClicked: window.launchSupportedGame(gameDetailsPopup.selectedProject.id)
                                }
                                Button {
                                    visible: gameDetailsPopup.installedProject !== null
                                    text: "Uninstall"
                                    icon.source: "qrc:/icons/uninstall.svg"
                                    icon.width: 18
                                    icon.height: 18
                                    icon.color: theme.textPrimary
                                    implicitHeight: 48
                                    Layout.minimumWidth: 150
                                    Layout.preferredWidth: 150
                                    onClicked: {
                                        window.pendingUninstallProjectId = gameDetailsPopup.selectedProject.id
                                        uninstallDialog.open()
                                    }
                                }
                                Button {
                                    visible: gameDetailsPopup.selectedProject !== null
                                        && gameDetailsPopup.installedProject === null
                                        && !gameDetailsPopup.installPromptVisible
                                        && !gameDetailsPopup.releaseState.loading
                                        && gameDetailsPopup.releaseState.assets.length > 0
                                    highlighted: true
                                    text: "Install Launcher"
                                    icon.source: "qrc:/icons/install.svg"
                                    icon.width: 18
                                    icon.height: 18
                                    icon.color: theme.textPrimary
                                    implicitHeight: 48
                                    Layout.minimumWidth: 150
                                    Layout.preferredWidth: 150
                                    onClicked: gameDetailsPopup.installPromptVisible = true
                                }
                                RowLayout {
                                    visible: gameDetailsPopup.installPromptVisible
                                        && gameDetailsPopup.installedProject === null
                                    spacing: 8
                                    Label {
                                        Layout.fillWidth: true
                                        text: gameDetailsPopup.selectedProject
                                            ? (gameDetailsPopup.selectedProject.developmentStatus === "beta"
                                                ? "Install this BETA launcher?"
                                                : gameDetailsPopup.selectedProject.developmentStatus === "alpha"
                                                    ? "Install this ALPHA launcher?"
                                                    : "Install this launcher?")
                                            : "Install launcher?"
                                        color: theme.textPrimary
                                        wrapMode: Text.Wrap
                                    }
                                    Button {
                                        highlighted: true
                                        text: "Yes"
                                        icon.source: "qrc:/icons/check.svg"
                                        icon.width: 16
                                        icon.height: 16
                                        icon.color: theme.textPrimary
                                        implicitHeight: 48
                                        Layout.minimumWidth: 88
                                        onClicked: {
                                            gameDetailsPopup.installPromptVisible = false
                                            const asset = gameDetailsPopup.selectedAsset
                                            if (asset)
                                                window.startAssetDownload(gameDetailsPopup.selectedProject, asset)
                                        }
                                    }
                                    Button {
                                        text: "No"
                                        icon.source: "qrc:/icons/close.svg"
                                        icon.width: 16
                                        icon.height: 16
                                        icon.color: theme.textPrimary
                                        implicitHeight: 48
                                        Layout.minimumWidth: 88
                                        onClicked: gameDetailsPopup.installPromptVisible = false
                                    }
                                }
                                Button {
                                    visible: gameDetailsPopup.selectedProject !== null
                                        && gameDetailsPopup.releaseState.error.length > 0
                                    text: "Retry"
                                    onClicked: window.checkProjectRelease(gameDetailsPopup.selectedProject)
                                }
                            }
}
                    }
                }
            }


        }
        }
    }
}
