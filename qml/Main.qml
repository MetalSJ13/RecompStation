import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 850
    minimumHeight: 550
    visible: true
    title: "RecompStation"
    color: theme.windowBackground

    property string currentPage: "Library"
    property string selectedPlatform: "All Games"
    property string librarySearchText: ""
    property var selectedGame: null
    property int libraryRevision: 0
    property int launcherRevision: 0
    property string pendingUninstallProjectId: ""
    property int selectedMetadataPlatformId: 0
    property int selectedLocalGameId: 0
    property string metadataAssociationMessage: ""
    property string artworkMessage: ""
    property string scanMessage: "Scan your platform folders to discover games."
    property var pendingReleaseRequests: ({})
    property var releaseStates: ({})
    property var pendingDownloadRequests: ({})
    property var pendingInstallRequests: ({})
    property var downloadStates: ({})
    property var launchErrors: ({})
    property var runningLaunchers: ({})

    Theme {
        id: theme
    }

    function installedLauncher(projectId) {
        launcherRevision
        const entries = launcherDownloadManager.installedLaunchers()
        return entries.find(entry => entry.projectId === projectId) || null
    }

    function launchSupportedGame(projectId) {
        let errors = Object.assign({}, launchErrors)
        delete errors[projectId]
        launchErrors = errors
        gameLaunchManager.launchProject(projectId)
    }

    function startAssetDownload(project, asset) {
        const key = project.id + "|" + asset.name
        let states = Object.assign({}, downloadStates)
        states[key] = { loading: true, completed: false, error: "", bytesReceived: 0, bytesTotal: asset.size, filePath: "" }
        downloadStates = states

        const requestId = launcherDownloadManager.downloadAsset(
            project.id, asset.name, asset.downloadUrl, asset.size)
        let pending = Object.assign({}, pendingDownloadRequests)
        pending[requestId] = key
        pendingDownloadRequests = pending
    }

    function startAssetInstall(projectId, assetName) {
        const key = projectId + "|" + assetName
        const project = projectCatalog.project(projectId)
        const rules = project.architectures
            ? project.architectures[projectCatalog.currentArchitecture] : null
        if (!rules) {
            let states = Object.assign({}, downloadStates)
            states[key] = Object.assign({}, states[key] || {}, {
                installing: false,
                error: "No installation rules exist for this project and device architecture."
            })
            downloadStates = states
            return
        }

        let states = Object.assign({}, downloadStates)
        states[key] = Object.assign({}, states[key] || {}, {
            installing: true,
            installed: false,
            error: ""
        })
        downloadStates = states

        const requestId = launcherDownloadManager.installDownloadedAsset(
            projectId, assetName, rules.packageFormat, rules.executablePath)
        let pending = Object.assign({}, pendingInstallRequests)
        pending[requestId] = key
        pendingInstallRequests = pending
    }

    function finishInstallRequest(requestId) {
        let pending = Object.assign({}, pendingInstallRequests)
        delete pending[requestId]
        pendingInstallRequests = pending
    }

    function cancelAssetDownload(projectId, assetName) {
        const key = projectId + "|" + assetName
        for (const requestId in pendingDownloadRequests) {
            if (pendingDownloadRequests[requestId] === key) {
                launcherDownloadManager.cancelDownload(Number(requestId))
                return
            }
        }
    }

    function updateDownloadState(requestId, values) {
        const key = pendingDownloadRequests[requestId]
        if (!key)
            return
        let states = Object.assign({}, downloadStates)
        states[key] = Object.assign({}, states[key] || {}, values)
        downloadStates = states
    }

    function finishDownloadRequest(requestId) {
        let pending = Object.assign({}, pendingDownloadRequests)
        delete pending[requestId]
        pendingDownloadRequests = pending
    }

    function checkProjectRelease(project) {
        let nextStates = Object.assign({}, releaseStates)
        nextStates[project.id] = { loading: true, error: "", release: null, assets: [] }
        releaseStates = nextStates
        const requestId = githubManager.fetchLatestRelease(
            project.repositoryOwner, project.repositoryName)
        let pending = Object.assign({}, pendingReleaseRequests)
        pending[requestId] = project.id
        pendingReleaseRequests = pending
    }

    palette.window: theme.windowBackground
    palette.windowText: theme.textPrimary
    palette.button: theme.controlBackground
    palette.buttonText: theme.textPrimary
    palette.highlight: theme.accent
    palette.highlightedText: theme.textPrimary

    Connections {
        target: artworkManager
        function onArtworkSaved(gameId, localPath) {
            window.libraryRevision++
            window.artworkMessage = "Artwork saved to the local cache."
        }
        function onArtworkFailed(gameId, message) {
            window.artworkMessage = message
        }
    }

    Connections {
        target: githubManager
        function onReleaseDataLoaded(requestId, release) {
            const projectId = window.pendingReleaseRequests[requestId]
            if (!projectId)
                return
            let pending = Object.assign({}, window.pendingReleaseRequests)
            delete pending[requestId]
            window.pendingReleaseRequests = pending
            let states = Object.assign({}, window.releaseStates)
            states[projectId] = {
                loading: false,
                error: "",
                release: release,
                assets: projectCatalog.compatibleAssets(projectId, release.assets)
            }
            window.releaseStates = states
        }
        function onRequestFailed(requestId, message) {
            const projectId = window.pendingReleaseRequests[requestId]
            if (!projectId)
                return
            let pending = Object.assign({}, window.pendingReleaseRequests)
            delete pending[requestId]
            window.pendingReleaseRequests = pending
            let states = Object.assign({}, window.releaseStates)
            states[projectId] = {
                loading: false,
                error: message,
                release: null,
                assets: []
            }
            window.releaseStates = states
        }
    }

    Connections {
        target: launcherDownloadManager
        function onDownloadProgress(requestId, bytesReceived, bytesTotal) {
            window.updateDownloadState(requestId, {
                loading: true,
                bytesReceived: bytesReceived,
                bytesTotal: bytesTotal
            })
        }
        function onDownloadCompleted(requestId, projectId, assetName, filePath) {
            window.updateDownloadState(requestId, {
                loading: false,
                completed: true,
                installing: true,
                installed: false,
                error: "",
                filePath: filePath,
                bytesReceived: window.downloadStates[projectId + "|" + assetName]
                    ? window.downloadStates[projectId + "|" + assetName].bytesTotal : 0
            })
            window.finishDownloadRequest(requestId)
            window.startAssetInstall(projectId, assetName)
        }
        function onInstallationCompleted(requestId, projectId, executablePath) {
            const key = window.pendingInstallRequests[requestId]
            if (!key)
                return
            let states = Object.assign({}, window.downloadStates)
            states[key] = Object.assign({}, states[key] || {}, {
                installing: false,
                installed: true,
                error: "",
                filePath: executablePath
            })
            window.downloadStates = states
            window.launcherRevision++
            window.finishInstallRequest(requestId)
        }
        function onInstallationFailed(requestId, message) {
            const key = window.pendingInstallRequests[requestId]
            if (!key)
                return
            let states = Object.assign({}, window.downloadStates)
            states[key] = Object.assign({}, states[key] || {}, {
                installing: false,
                installed: false,
                error: message
            })
            window.downloadStates = states
            window.finishInstallRequest(requestId)
        }
        function onDownloadFailed(requestId, message) {
            window.updateDownloadState(requestId, {
                loading: false,
                completed: false,
                error: message
            })
            window.finishDownloadRequest(requestId)
        }
    }

    Connections {
        target: gameLaunchManager
        function onLaunchStarted(projectId, processId) {
            let running = Object.assign({}, window.runningLaunchers)
            running[projectId] = true
            window.runningLaunchers = running
        }
        function onLaunchFailed(projectId, message) {
            let running = Object.assign({}, window.runningLaunchers)
            delete running[projectId]
            window.runningLaunchers = running
            let errors = Object.assign({}, window.launchErrors)
            errors[projectId] = message
            window.launchErrors = errors
        }
        function onProcessFinished(projectId, exitCode, crashed) {
            let running = Object.assign({}, window.runningLaunchers)
            delete running[projectId]
            window.runningLaunchers = running
            if (crashed) {
                let errors = Object.assign({}, window.launchErrors)
                errors[projectId] = "The game launcher exited unexpectedly."
                window.launchErrors = errors
            }
        }
    }

    Connections {
        target: libraryScanner
        function onScanCompleted(gamesFound) {
            window.libraryRevision++
            window.scanMessage = libraryScanner.lastError.length > 0
                ? libraryScanner.lastError
                : gamesFound + " new game(s) found. Checking metadata..."
            if (libraryScanner.lastError.length === 0)
                metadataMatcher.scrapeLibrary()
        }
    }

    Connections {
        target: metadataMatcher
        function onStatusChanged() {
            window.scanMessage = metadataMatcher.status
            window.libraryRevision++
        }
        function onScrapeFinished(matched, unresolved) {
            window.libraryRevision++
            window.scanMessage = "Metadata complete: " + matched
                + " matched, " + unresolved + " left unresolved."
        }
    }

    Dialog {
        id: uninstallDialog
        title: "Uninstall launcher?"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 340
            wrapMode: Text.Wrap
            text: "This removes only the RecompStation-managed launcher files for "
                  + window.pendingUninstallProjectId
                  + ". Your ROMs, saves, and other files are not targeted."
            color: "#dddddf"
        }
        onAccepted: {
            if (launcherDownloadManager.uninstallLauncher(window.pendingUninstallProjectId)) {
                let states = Object.assign({}, window.downloadStates)
                for (const key of Object.keys(states)) {
                    if (key.startsWith(window.pendingUninstallProjectId + "|")) {
                        states[key] = Object.assign({}, states[key], {
                            completed: false,
                            installing: false,
                            installed: false,
                            error: "",
                            filePath: ""
                        })
                    }
                }
                window.downloadStates = states
                window.launcherRevision++
            }
        }
    }

    Popup {
        id: gameDetailsPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(window.width - 64, 900)
        height: Math.min(window.height - 64, 620)
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 22
        property var supportedProjects: window.selectedGame
            ? projectCatalog.projectsForGame(
                  window.selectedGame.metadataTitle || window.selectedGame.title,
                  window.selectedGame.platform)
            : []
        property var primaryProject: supportedProjects.length > 0 ? supportedProjects[0] : null
        property var installedProject: primaryProject
            ? window.installedLauncher(primaryProject.id) : null
        property bool installPromptVisible: false
        property var releaseState: primaryProject
            ? (window.releaseStates[primaryProject.id]
                || ({ loading: false, error: "", release: null, assets: [] }))
            : ({ loading: false, error: "", release: null, assets: [] })
        property var releaseInfo: releaseState && releaseState.release
            ? releaseState.release : null
        // Keep nullable API objects out of text bindings. Release lookups are
        // asynchronous, so these strings remain safe while the popup opens.
        property string releaseTag: releaseState && releaseState.release
            && releaseState.release.tagName
            ? String(releaseState.release.tagName) : "Not available"
        property var compatibleAssets: releaseState && releaseState.assets
            ? releaseState.assets : []
        property var selectedAsset: compatibleAssets.length > 0
            ? compatibleAssets[0] : null
        property string selectedAssetName: selectedAsset && selectedAsset.name
            ? String(selectedAsset.name) : "Not available"
        property var downloadState: primaryProject && selectedAsset
            ? (window.downloadStates[primaryProject.id + "|" + selectedAsset.name]
                || ({ loading: false, installing: false, error: "",
                      bytesReceived: 0, bytesTotal: selectedAsset.size }))
            : ({ loading: false, installing: false, error: "",
                 bytesReceived: 0, bytesTotal: 0 })

        onOpened: {
            installPromptVisible = false
            if (primaryProject && !releaseState.loading
                    && !releaseState.release && releaseState.error.length === 0)
                window.checkProjectRelease(primaryProject)
        }

        background: Rectangle {
            radius: theme.radiusLarge
            color: theme.elevatedSurface
            border.color: theme.divider
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: window.selectedGame ? window.selectedGame.title : ""
                    color: theme.textPrimary
                    font.pixelSize: 25
                    font.bold: true
                    elide: Text.ElideRight
                }
                Button {
                    text: "Close"
                    onClicked: gameDetailsPopup.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 22

                Rectangle {
                    Layout.preferredWidth: 250
                    Layout.fillHeight: true
                    radius: theme.radiusCard
                    color: theme.sidebarBackground
                    clip: true
                    Image {
                        anchors.fill: parent
                        anchors.margins: 8
                        source: window.selectedGame ? window.selectedGame.artworkPath : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !window.selectedGame || window.selectedGame.artworkPath.length === 0
                        text: "Artwork unavailable"
                        color: theme.textMuted
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    Label {
                        text: window.selectedGame ? window.selectedGame.platform : ""
                        color: theme.accentHover
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: window.selectedGame && window.selectedGame.metadataOverview.length > 0
                            ? window.selectedGame.metadataOverview
                            : "Game information has not been retrieved yet. RecompStation will populate details when matching metadata is available."
                        color: theme.textSecondary
                        wrapMode: Text.Wrap
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
                                  .filter(value => value.length > 0).join("  •  ")
                            : ""
                        color: theme.textMuted
                        font.pixelSize: 11
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.divider
                    }
                    Label {
                        text: "RECOMPILATION SUPPORT"
                        color: theme.textMuted
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: gameDetailsPopup.primaryProject
                            ? gameDetailsPopup.primaryProject.name
                            : "No supported recompilation project is currently listed for this game."
                        color: gameDetailsPopup.primaryProject ? theme.success : theme.textSecondary
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: gameDetailsPopup.installedProject !== null
                        text: gameDetailsPopup.installedProject !== null
                            ? "Installed launcher: " + gameDetailsPopup.installedProject.executablePath
                            : ""
                        color: theme.textMuted
                        font.pixelSize: 10
                        wrapMode: Text.WrapAnywhere
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: gameDetailsPopup.primaryProject !== null
                            && window.launchErrors[gameDetailsPopup.primaryProject.id] !== undefined
                        text: gameDetailsPopup.primaryProject
                            ? (window.launchErrors[gameDetailsPopup.primaryProject.id] || "") : ""
                        color: theme.error
                        wrapMode: Text.Wrap
                    }
                    Item { Layout.fillHeight: true }
                    Label {
                        Layout.fillWidth: true
                        visible: gameDetailsPopup.primaryProject !== null
                            && gameDetailsPopup.releaseState.loading
                        text: "Checking for the latest compatible Linux release…"
                        color: theme.accentHover
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: gameDetailsPopup.primaryProject !== null
                            && gameDetailsPopup.releaseState.error.length > 0
                        text: gameDetailsPopup.releaseState.error
                        color: theme.error
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: gameDetailsPopup.primaryProject !== null
                            && !!gameDetailsPopup.releaseInfo
                            && gameDetailsPopup.compatibleAssets.length === 0
                        text: "No compatible Linux launcher package is available for "
                            + projectCatalog.currentArchitecture + "."
                        color: theme.textMuted
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: gameDetailsPopup.primaryProject !== null
                            && !!gameDetailsPopup.releaseInfo
                            && gameDetailsPopup.selectedAsset !== null
                            && gameDetailsPopup.installedProject === null
                        text: "Available version: " + gameDetailsPopup.releaseTag
                            + "  •  Linux (" + projectCatalog.currentArchitecture + ")"
                            + "\
Package: " + gameDetailsPopup.selectedAssetName
                        color: theme.textSecondary
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!gameDetailsPopup.primaryProject
                            && !!gameDetailsPopup.downloadState.error
                            && gameDetailsPopup.downloadState.error.length > 0
                        text: gameDetailsPopup.downloadState.error || ""
                        color: theme.error
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!gameDetailsPopup.primaryProject
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
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            visible: gameDetailsPopup.installedProject !== null
                            highlighted: true
                            text: gameDetailsPopup.primaryProject
                                && window.runningLaunchers[gameDetailsPopup.primaryProject.id]
                                ? "Running…" : "Play Game"
                            enabled: gameDetailsPopup.primaryProject !== null
                                && gameDetailsPopup.installedProject !== null
                                && !window.runningLaunchers[gameDetailsPopup.primaryProject.id]
                            onClicked: window.launchSupportedGame(gameDetailsPopup.primaryProject.id)
                        }
                        Button {
                            visible: gameDetailsPopup.installedProject !== null
                            text: "Uninstall Launcher"
                            onClicked: {
                                window.pendingUninstallProjectId = gameDetailsPopup.primaryProject.id
                                uninstallDialog.open()
                            }
                        }
                        Button {
                            visible: gameDetailsPopup.primaryProject !== null
                                && gameDetailsPopup.installedProject === null
                                && !gameDetailsPopup.installPromptVisible
                                && !gameDetailsPopup.releaseState.loading
                                && gameDetailsPopup.releaseState.assets.length > 0
                            highlighted: true
                            text: "Install Compatible Launcher?"
                            onClicked: gameDetailsPopup.installPromptVisible = true
                        }
                        Label {
                            visible: !!gameDetailsPopup.installPromptVisible
                                && !!gameDetailsPopup.primaryProject
                                && gameDetailsPopup.installedProject === null
                            text: gameDetailsPopup.primaryProject
                                ? "Install " + gameDetailsPopup.primaryProject.name + "?" : ""
                            color: theme.textPrimary
                        }
                        Button {
                            visible: gameDetailsPopup.installPromptVisible
                                && gameDetailsPopup.installedProject === null
                            highlighted: true
                            text: "Yes"
                            onClicked: {
                                gameDetailsPopup.installPromptVisible = false
                                const asset = gameDetailsPopup.selectedAsset
                                if (asset)
                                    window.startAssetDownload(gameDetailsPopup.primaryProject, asset)
                            }
                        }
                        Button {
                            visible: gameDetailsPopup.installPromptVisible
                                && gameDetailsPopup.installedProject === null
                            text: "No"
                            onClicked: gameDetailsPopup.installPromptVisible = false
                        }
                        Button {
                            visible: gameDetailsPopup.primaryProject !== null
                                && gameDetailsPopup.releaseState.error.length > 0
                            text: "Retry"
                            onClicked: window.checkProjectRelease(gameDetailsPopup.primaryProject)
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 225
            Layout.fillHeight: true
            color: theme.sidebarBackground

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        width: 38
                        height: 38
                        radius: 10
                        color: theme.accent
                        Label {
                            anchors.centerIn: parent
                            text: "R"
                            font.pixelSize: 23
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: "RecompStation"
                            font.pixelSize: 17
                            font.bold: true
                            color: "white"
                        }
                        Label {
                            text: "Version 0.1.0"
                            font.pixelSize: 11
                            color: "#9999aa"
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                Label {
                    text: "EXPLORE"
                    color: theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }

                Button {
                    Layout.fillWidth: true
                    text: "Game Library"
                    highlighted: currentPage === "Library"
                    onClicked: currentPage = "Library"
                }

                Item { Layout.fillHeight: true }

                Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }

                Button {
                    Layout.fillWidth: true
                    text: "Settings"
                    highlighted: currentPage === "Settings"
                    onClicked: currentPage = "Settings"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: theme.windowBackground

            StackLayout {
                anchors.fill: parent
                anchors.margins: 30
                currentIndex: {
                    switch (currentPage) {
                    case "Library": return 0
                    case "Metadata Search": return 1
                    case "Platforms": return 2
                    case "Launchers": return 3
                    case "Settings": return 4
                    default: return 0
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Label {
                                    text: selectedPlatform === "All Games" ? "Your Collection" : selectedPlatform
                                    font.pixelSize: 32
                                    font.bold: true
                                    color: theme.textPrimary
                                }
                                RowLayout {
                                    spacing: 8
                                    Label {
                                        text: gameGrid.count + (gameGrid.count === 1 ? " GAME" : " GAMES")
                                        color: theme.accentHover
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    Rectangle {
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: theme.textMuted
                                    }
                                    Label {
                                        text: "Your recompiled game library"
                                        color: theme.textMuted
                                        font.pixelSize: 13
                                    }
                                }
                            }

                            TextField {
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: 42
                                placeholderText: "Search collection"
                                selectByMouse: true
                                onTextChanged: window.librarySearchText = text
                            }
                            ComboBox {
                                Layout.preferredWidth: 170
                                Layout.preferredHeight: 42
                                model: ["All Games"].concat(platformManager.platforms())
                                currentIndex: Math.max(0, model.indexOf(window.selectedPlatform))
                                onActivated: window.selectedPlatform = currentText
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 54
                            radius: theme.radiusCard
                            color: theme.surface
                            border.color: theme.divider
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 10
                                spacing: 10

                                Label {
                                    Layout.fillWidth: true
                                    text: "LIBRARY MANAGEMENT"
                                    color: theme.textMuted
                                    font.pixelSize: 10
                                    font.bold: true
                                    font.letterSpacing: 1
                                }
                                Button {
                                    text: "Refresh Metadata"
                                    onClicked: {
                                        scanMessage = "Refreshing metadata and missing artwork..."
                                        metadataMatcher.scrapeLibrary(true)
                                    }
                                }
                                Button {
                                    text: "Scan Library"
                                    highlighted: true
                                    onClicked: {
                                        scanMessage = "Scanning game folders..."
                                        libraryScanner.scanAll()
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: scanMessage
                            color: "#a98cff"
                            wrapMode: Text.Wrap
                        }

                        GridView {
                            id: gameGrid
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            cellWidth: Math.max(184, Math.min(232,
                                Math.floor(width / Math.max(1, Math.floor(width / 216)))))
                            cellHeight: Math.round(cellWidth * 1.34)
                            keyNavigationEnabled: true
                            model: {
                                window.libraryRevision
                                const games = databaseManager.games(
                                    selectedPlatform === "All Games" ? "" : selectedPlatform)
                                const query = window.librarySearchText.trim().toLowerCase()
                                return query.length === 0
                                    ? games
                                    : games.filter(game => game.title.toLowerCase().includes(query))
                            }
                            delegate: Item {
                                id: gameTile
                                required property string title
                                required property string platform
                                required property string path
                                required property string artworkPath
                                required property string metadataTitle
                                required property string metadataOverview
                                required property string metadataReleaseDate
                                required property string metadataRating
                                required property int id
                                required property int index
                                property int gameId: id
                                function openDetails() {
                                    window.selectedGame = {
                                        id: gameTile.gameId,
                                        title: gameTile.title,
                                        platform: gameTile.platform,
                                        path: gameTile.path,
                                        artworkPath: gameTile.artworkPath,
                                        metadataTitle: gameTile.metadataTitle,
                                        metadataOverview: gameTile.metadataOverview,
                                        metadataReleaseDate: gameTile.metadataReleaseDate,
                                        metadataRating: gameTile.metadataRating
                                    }
                                    gameDetailsPopup.open()
                                }
                                width: gameGrid.cellWidth - 14
                                height: gameGrid.cellHeight - 14
                                focus: GridView.isCurrentItem
                                scale: tileMouse.containsMouse || gameTile.GridView.isCurrentItem ? 1.025 : 1.0
                                Behavior on scale {
                                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radiusCard
                                    color: theme.surface
                                    border.width: tileMouse.containsMouse || gameTile.GridView.isCurrentItem ? 2 : 1
                                    border.color: tileMouse.containsMouse || gameTile.GridView.isCurrentItem
                                        ? theme.accent : theme.divider
                                    Behavior on border.color {
                                        ColorAnimation { duration: 140 }
                                    }
                                    Behavior on border.width {
                                        NumberAnimation { duration: 140 }
                                    }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 7

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumHeight: 165
                                            radius: theme.radiusSmall
                                            color: theme.sidebarBackground
                                            clip: true

                                            Image {
                                                anchors.fill: parent
                                                source: gameTile.artworkPath
                                                fillMode: Image.PreserveAspectFit
                                                asynchronous: true
                                                cache: true
                                                visible: gameTile.artworkPath.length > 0
                                            }
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 5
                                                visible: gameTile.artworkPath.length === 0
                                                Label {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: "▧"
                                                    color: theme.textMuted
                                                    font.pixelSize: 34
                                                }
                                                Label {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: "Artwork unavailable"
                                                    color: theme.textMuted
                                                    font.pixelSize: 10
                                                }
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: gameTile.title
                                            color: theme.textPrimary
                                            font.pixelSize: 14
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                Layout.fillWidth: true
                                                text: gameTile.platform
                                                color: theme.textSecondary
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                text: gameTile.metadataTitle.length > 0 ? "METADATA" : ""
                                                color: theme.success
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: tileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        gameGrid.currentIndex = gameTile.index
                                        gameTile.openDetails()
                                    }
                                }

                                Keys.onReturnPressed: gameTile.openDetails()
                                Keys.onEnterPressed: gameTile.openDetails()
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: gameGrid.count === 0
                            radius: theme.radiusLarge
                            color: theme.surface
                            border.color: theme.divider
                            ColumnLayout {
                                anchors.centerIn: parent
                                width: Math.min(parent.width - 48, 440)
                                spacing: 12
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: window.librarySearchText.trim().length > 0 ? "NO MATCHES" : "WELCOME TO YOUR COLLECTION"
                                    color: theme.accentHover
                                    font.pixelSize: 11
                                    font.bold: true
                                    font.letterSpacing: 1.4
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: window.librarySearchText.trim().length > 0
                                        ? "No games match your search."
                                        : "Your library is ready for its first game."
                                    color: theme.textPrimary
                                    font.pixelSize: 22
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: window.librarySearchText.trim().length === 0
                                    text: "Add game files to one of your platform folders, then scan your library to discover them."
                                    color: theme.textSecondary
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                }
                                Button {
                                    Layout.alignment: Qt.AlignHCenter
                                    visible: window.librarySearchText.trim().length === 0
                                    text: "Scan Library"
                                    highlighted: true
                                    onClicked: {
                                        scanMessage = "Scanning game folders..."
                                        libraryScanner.scanAll()
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        Label {
                            text: "Metadata Search"
                            font.pixelSize: 30
                            font.bold: true
                            color: "white"
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Search TheGamesDB for a game and review possible matches."
                            color: "#9999aa"
                            wrapMode: Text.Wrap
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 10
                            color: theme.surface
                            visible: !metadataManager.configured
                            implicitHeight: keyNotice.implicitHeight + 28
                            Label {
                                id: keyNotice
                                anchors.fill: parent
                                anchors.margins: 14
                                text: "TheGamesDB is not configured. Set RECOMPSTATION_TGDB_API_KEY in the environment before launching RecompStation."
                                color: "#e5bd79"
                                wrapMode: Text.Wrap
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Platform"
                                color: "#b7b7c5"
                            }
                            ComboBox {
                                id: metadataPlatformSelector
                                Layout.preferredWidth: 230
                                textRole: "name"
                                valueRole: "id"
                                model: metadataManager.platformOptions
                                enabled: !metadataManager.platformsLoading
                                onActivated: window.selectedMetadataPlatformId = currentValue
                            }
                            Label {
                                visible: metadataManager.platformsLoading
                                text: "Loading platform catalog..."
                                color: "#a98cff"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Local ROM"
                                color: "#b7b7c5"
                            }
                            ComboBox {
                                id: localGameSelector
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "id"
                                model: {
                                    window.libraryRevision
                                    databaseManager.games()
                                }
                                onActivated: window.selectedLocalGameId = currentValue
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Choose the local ROM first, then associate one of the metadata results below."
                            color: "#9999aa"
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: window.metadataAssociationMessage.length > 0
                            text: window.metadataAssociationMessage
                            color: window.metadataAssociationMessage.startsWith("Metadata associated")
                                   ? "#91d6a5" : "#ff8b8b"
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: metadataSearchField
                                Layout.fillWidth: true
                                placeholderText: "Enter a game title..."
                                enabled: metadataManager.configured && !metadataManager.searching
                                onAccepted: {
                                    if (text.trim().length > 0)
                                        metadataManager.searchGames(text, window.selectedMetadataPlatformId)
                                }
                            }
                            Button {
                                text: metadataManager.searching ? "Searching..." : "Search"
                                highlighted: true
                                enabled: metadataManager.configured
                                         && !metadataManager.searching
                                         && metadataSearchField.text.trim().length > 0
                                onClicked: metadataManager.searchGames(metadataSearchField.text, window.selectedMetadataPlatformId)
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: window.artworkMessage.length > 0
                            text: window.artworkMessage
                            color: window.artworkMessage.startsWith("Artwork saved") ? "#91d6a5" : "#a98cff"
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: metadataManager.errorString.length > 0
                            text: metadataManager.errorString
                            color: "#ff8b8b"
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: metadataManager.searching
                            text: "Searching TheGamesDB..."
                            color: "#a98cff"
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: !metadataManager.searching
                                     && metadataManager.errorString.length === 0
                                     && metadataManager.results.length > 0
                            text: metadataManager.results.length + " result(s) found."
                            color: "#9999aa"
                        }

                        ListView {
                            id: metadataResultsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 10
                            model: metadataManager.results
                            delegate: Rectangle {
                                required property string provider
                                required property string title
                                required property string overview
                                required property string releaseDate
                                required property string rating
                                required property int providerGameId
                                required property int platformId
                                required property string artworkUrl
                                width: metadataResultsList.width
                                height: resultContent.implicitHeight + 28
                                radius: 10
                                color: theme.surface
                                ColumnLayout {
                                    id: resultContent
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 5
                                    Label {
                                        Layout.fillWidth: true
                                        text: title
                                        color: "white"
                                        font.pixelSize: 17
                                        font.bold: true
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: (releaseDate.length > 0 ? releaseDate : "Release date unknown")
                                              + "  •  " + (rating.length > 0 ? rating : "Rating unavailable")
                                              + "  •  TheGamesDB ID: " + providerGameId
                                        color: "#a98cff"
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: overview.length > 0 ? overview : "No description available."
                                        color: "#b7b7c5"
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 4
                                        elide: Text.ElideRight
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Button {
                                            text: artworkUrl.length > 0
                                                  ? "Associate & Download Cover"
                                                  : "Associate Metadata"
                                            highlighted: true
                                            enabled: window.selectedLocalGameId > 0
                                            onClicked: {
                                                const saved = databaseManager.associateMetadata(
                                                    window.selectedLocalGameId,
                                                    provider,
                                                    providerGameId,
                                                    title,
                                                    overview,
                                                    releaseDate,
                                                    platformId,
                                                    rating,
                                                    artworkUrl
                                                )
                                                if (saved) {
                                                    window.libraryRevision++
                                                    window.metadataAssociationMessage = "Metadata associated with the selected ROM."
                                                    if (artworkUrl.length > 0) {
                                                        window.artworkMessage = "Downloading cover artwork..."
                                                        artworkManager.downloadArtwork(window.selectedLocalGameId, artworkUrl)
                                                    } else {
                                                        window.artworkMessage = "No front-cover artwork is available for this result."
                                                    }
                                                } else {
                                                    window.metadataAssociationMessage = databaseManager.lastError
                                                }
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            visible: artworkUrl.length === 0
                                            text: "No cover image available"
                                            color: "#9999aa"
                                            font.pixelSize: 11
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            visible: !metadataManager.searching
                                     && metadataManager.errorString.length === 0
                                     && metadataResultsList.count === 0
                                     && metadataManager.configured
                            text: "Enter a game title above to search for metadata."
                            color: "#9999aa"
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20
                        Label { text: "Platforms"; font.pixelSize: 30; font.bold: true; color: "white" }
                        Label { text: "Browse your supported console platforms."; color: "#9999aa" }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 15
                            columnSpacing: 15
                            Repeater {
                                model: platformManager.platforms()
                                delegate: Rectangle {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 115
                                    radius: 12
                                    color: theme.surface
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 18
                                        spacing: 5
                                        Label { text: modelData; color: "white"; font.pixelSize: 20; font.bold: true }
                                        Label { text: "Game directory"; color: "#9999aa"; font.pixelSize: 12 }
                                        Item { Layout.fillHeight: true }
                                        Label { text: "Open platform"; color: "#a98cff"; font.pixelSize: 12 }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            selectedPlatform = modelData
                                            currentPage = "Library"
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Game folders: " + platformManager.dataDirectory + "/games/"
                            color: "#777788"
                            font.pixelSize: 11
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 16
                        Label { text: "Launcher Discovery"; color: "white"; font.pixelSize: 30; font.bold: true }
                        Label {
                            Layout.fillWidth: true
                            text: "Browse supported projects, check compatible Linux releases, and install compatible launchers automatically."
                            color: "#9999aa"
                            wrapMode: Text.Wrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 12
                            model: projectCatalog.projects()
                            delegate: Rectangle {
                                required property var modelData
                                property var releaseState: window.releaseStates[modelData.id]
                                    || ({ loading: false, error: "", release: null, assets: [] })
                                property var installedState: window.installedLauncher(modelData.id)
                                width: ListView.view.width
                                height: projectCard.implicitHeight + 28
                                radius: 10
                                color: theme.surface
                                ColumnLayout {
                                    id: projectCard
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 7
                                    Label {
                                        text: modelData.name
                                        color: "white"
                                        font.pixelSize: 18
                                        font.bold: true
                                    }
                                    Label {
                                        text: modelData.platform + "  •  " + modelData.repositoryOwner + "/" + modelData.repositoryName
                                        color: "#9999aa"
                                        font.pixelSize: 12
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.romRequirements
                                        color: "#b7b7c5"
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Button {
                                            text: releaseState.loading ? "Checking GitHub..." : "Check for Linux Release"
                                            highlighted: true
                                            enabled: !releaseState.loading
                                            onClicked: window.checkProjectRelease(modelData)
                                        }
                                        Button {
                                            visible: installedState !== null
                                            text: "Uninstall"
                                            onClicked: {
                                                window.pendingUninstallProjectId = modelData.id
                                                uninstallDialog.open()
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            visible: releaseState.release !== null
                                            text: releaseState.release
                                                  ? "Latest: " + releaseState.release.tagName
                                                  : ""
                                            color: "#a98cff"
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: releaseState.loading === true
                                        text: "Contacting GitHub and checking release assets..."
                                        color: "#a98cff"
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: releaseState.error.length > 0
                                        text: releaseState.error || ""
                                        color: "#ff8b8b"
                                        wrapMode: Text.Wrap
                                    }
                                    Repeater {
                                        model: (releaseState.assets || []).map(asset => ({
                                            name: asset.name,
                                            downloadUrl: asset.downloadUrl,
                                            size: asset.size,
                                            projectId: modelData.id,
                                            project: modelData
                                        }))
                                        delegate: Rectangle {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            implicitHeight: assetRow.implicitHeight + 16
                                            radius: 6
                                            color: theme.sidebarBackground
                                            RowLayout {
                                                id: assetRow
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: modelData.name
                                                    color: "#91d6a5"
                                                    font.pixelSize: 12
                                                    wrapMode: Text.Wrap
                                                }
                                                Label {
                                                    text: (modelData.size / 1048576).toFixed(1) + " MB"
                                                    color: "#9999aa"
                                                    font.pixelSize: 11
                                                }
                                                Button {
                                                    property string downloadKey: modelData.projectId + "|" + modelData.name
                                                    property var downloadState: window.downloadStates[downloadKey]
                                                        || ({ loading: false, completed: false, installing: false, installed: false, error: "", bytesReceived: 0, bytesTotal: modelData.size, filePath: "" })
                                                    property var installedInfo: window.installedLauncher(modelData.projectId)
                                                    text: installedInfo !== null || downloadState.installed ? "Installed"
                                                        : downloadState.installing ? "Installing…"
                                                        : downloadState.completed && downloadState.error.length > 0 ? "Retry Install"
                                                        : downloadState.completed ? "Downloaded" 
                                                        : downloadState.loading ? "Cancel" : "Download"
                                                    enabled: installedInfo === null && !downloadState.installed && !downloadState.installing
                                                    onClicked: {
                                                        if (downloadState.loading)
                                                            window.cancelAssetDownload(modelData.projectId, modelData.name)
                                                        else if (downloadState.completed)
                                                            window.startAssetInstall(modelData.projectId, modelData.name)
                                                        else
                                                            window.startAssetDownload(modelData.project, modelData)
                                                    }
                                                }
                                                Label {
                                                    property string downloadKey: modelData.projectId + "|" + modelData.name
                                                    property var downloadState: window.downloadStates[downloadKey]
                                                        || ({ loading: false, completed: false, installing: false, installed: false, error: "", bytesReceived: 0, bytesTotal: modelData.size, filePath: "" })
                                                    property var installedInfo: window.installedLauncher(modelData.projectId)
                                                    visible: downloadState.loading || downloadState.completed || downloadState.installing || downloadState.error.length > 0 || installedInfo !== null
                                                    Layout.maximumWidth: 190
                                                    text: installedInfo !== null ? "Installed: " + installedInfo.executablePath
                                                        : downloadState.error.length > 0 ? downloadState.error
                                                        : downloadState.installing ? "Extracting and installing…"
                                                        : downloadState.completed ? "Installed: " + downloadState.filePath
                                                        : downloadState.bytesTotal > 0
                                                            ? Math.floor(downloadState.bytesReceived / downloadState.bytesTotal * 100) + "%"
                                                            : (downloadState.bytesReceived / 1048576).toFixed(1) + " MB"
                                                    color: downloadState.error.length > 0 ? "#ff8b8b" : "#a98cff"
                                                    font.pixelSize: 10
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: releaseState.release !== null && (releaseState.assets || []).length === 0
                                        text: "No compatible Linux package was found for this device (" + projectCatalog.currentArchitecture + ")."
                                        color: "#e5bd79"
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: !!releaseState.release
                                        text: releaseState.release
                                              ? "Release published: " + releaseState.release.publishedAt
                                              : ""
                                        color: "#9999aa"
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20
                        Label { text: "Settings"; color: "white"; font.pixelSize: 30; font.bold: true }
                        Label { text: "Application configuration"; color: "#9999aa" }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 100
                            radius: 12
                            color: theme.surface
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                Label { text: "Application data directory"; color: "white"; font.bold: true }
                                Label {
                                    text: platformManager.dataDirectory
                                    color: "#9999aa"
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
