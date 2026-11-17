import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 850
    minimumHeight: 550
    visible: true
    title: "RecompStation"
    color: theme.windowBackground
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: theme.backgroundTop }
            GradientStop { position: 1.0; color: theme.backgroundBottom }
        }
    }

    Component.onCompleted: window.applyPreferredPresentationSize()

    onWidthChanged: console.info("[Settings Geometry] window width:", width)
    onHeightChanged: console.info("[Settings Geometry] window height:", height)

    function applyPreferredPresentationSize() {
        if (window.visibility !== Window.Windowed)
            return

        const size = settingsManager.preferredPresentationSize
        if (size === "system")
            return

        const dimensions = size.split("x")
        if (dimensions.length !== 2)
            return

        const requestedWidth = Number(dimensions[0])
        const requestedHeight = Number(dimensions[1])
        if (!Number.isFinite(requestedWidth) || !Number.isFinite(requestedHeight))
            return

        // Stay within the current screen's logical dimensions. The compositor
        // remains responsible for placement and all monitor-wide mode changes.
        const safeWidth = Math.max(640, Math.floor(Screen.desktopAvailableWidth * 0.9))
        const safeHeight = Math.max(480, Math.floor(Screen.desktopAvailableHeight * 0.9))

        // Use one scale factor for both dimensions so every preset keeps its
        // intended aspect ratio when it must be reduced to fit the desktop.
        const scale = Math.min(
            1,
            safeWidth / requestedWidth,
            safeHeight / requestedHeight
        )
        window.width = Math.round(requestedWidth * scale)
        window.height = Math.round(requestedHeight * scale)
    }

    property string currentPage: "Library"
    property string pendingThemeName: settingsManager.themeName
    property string selectedPlatform: "All Platforms"
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

    Connections {
        target: settingsManager
        function onThemeNameChanged() {
            pendingThemeName = settingsManager.themeName
            console.info("[Settings QML] Applied theme changed:", settingsManager.themeName)
        }
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

    FolderDialog {
        id: libraryFolderDialog
        title: "Choose your RecompStation game library"
        onAccepted: {
            if (platformManager.setLibraryDirectory(selectedFolder)) {
                window.scanMessage = "Library folder saved. Scanning platform folders..."
                librarySetupDialog.close()
                libraryScanner.scanAll()
            } else {
                window.scanMessage = "Could not use that folder. Choose an existing, readable local directory."
            }
        }
    }

    Dialog {
        id: librarySetupDialog
        title: "Set up your game library"
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 520)
        closePolicy: platformManager.libraryConfigured ? Popup.CloseOnEscape : Popup.NoAutoClose
        visible: !platformManager.libraryConfigured
        standardButtons: Dialog.NoButton
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "First, choose the main folder where you keep your game files. RecompStation will create the supported platform folders inside it."
                color: theme.textSecondary; wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: "Your games will stay where they are; this only creates folders and sets the locations RecompStation scans."
                color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: "Choose Library Folder"
                highlighted: true
                onClicked: libraryFolderDialog.open()
            }
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
            color: theme.textPrimary
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
            color: "transparent"
        }

        contentItem: Item {
            id: detailsContentFrame
            width: window.width
            height: window.height
            clip: true

            Button {
                z: 10
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 28
                anchors.topMargin: 24
                text: "Back to Library"
                icon.source: "qrc:/icons/back.svg"
                icon.width: 18
                icon.height: 18
                icon.color: theme.textPrimary
                onClicked: gameDetailsPopup.close()
            }

            Image {
                id: detailsBackdropImage
                anchors.fill: parent
                source: window.selectedGame ? window.selectedGame.artworkPath : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                opacity: status === Image.Ready ? 0.62 : 0
                Behavior on opacity {
                    enabled: !settingsManager.reducedMotion
                    NumberAnimation { duration: 900; easing.type: Easing.OutCubic }
                }
            }

            MultiEffect {
                anchors.fill: detailsBackdropImage
                source: detailsBackdropImage
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
                anchors.margins: Math.max(32, Math.min(88, width * 0.045))
                spacing: 48

            Rectangle {
                Layout.preferredWidth: Math.min(420, Math.max(300, height * 0.48))
                Layout.fillHeight: true
                Layout.maximumHeight: 760
                Layout.alignment: Qt.AlignVCenter
                radius: theme.radiusLarge
                color: theme.sidebarBackground
                border.color: theme.divider
                border.width: 1
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 12
                    source: window.selectedGame ? window.selectedGame.artworkPath : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: !window.selectedGame || window.selectedGame.artworkPath.length === 0
                    spacing: 8
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "▧"
                        color: theme.textMuted
                        font.pixelSize: 38 * theme.fontScale
                    }
                    Label {
                        text: "Artwork unavailable"
                        color: theme.textMuted
                        font.pixelSize: 12 * theme.fontScale
                    }
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
                            Label {
                                Layout.fillWidth: true
                                visible: !!window.selectedGame
                                    && (!window.selectedGame.metadataTitle
                                        || window.selectedGame.metadataTitle.length === 0)
                                text: "METADATA NOT FOUND"
                                color: theme.textMuted
                                font.pixelSize: 10 * theme.fontScale
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: window.selectedGame && window.selectedGame.metadataOverview.length > 0
                                    ? window.selectedGame.metadataOverview
                                    : "No description is available yet. RecompStation will add game information when matching metadata is found."
                                color: theme.textSecondary
                                font.pixelSize: 13 * theme.fontScale
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
                                Label {
                                    Layout.fillWidth: true
                                    text: gameDetailsPopup.primaryProject
                                        ? gameDetailsPopup.primaryProject.name
                                        : "No supported recompilation project is currently listed for this game."
                                    color: gameDetailsPopup.primaryProject ? theme.success : theme.textSecondary
                                    font.pixelSize: 14 * theme.fontScale
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }
                                Rectangle {
                                    visible: gameDetailsPopup.primaryProject !== null
                                        && gameDetailsPopup.primaryProject.developmentStatus !== "playable"
                                    radius: 5
                                    color: gameDetailsPopup.primaryProject
                                        && gameDetailsPopup.primaryProject.developmentStatus === "beta"
                                        ? theme.warningSurface : theme.neutralStatusSurface
                                    implicitWidth: projectStatusLabel.implicitWidth + 16
                                    implicitHeight: projectStatusLabel.implicitHeight + 8

                                    Label {
                                        id: projectStatusLabel
                                        anchors.centerIn: parent
                                        text: gameDetailsPopup.primaryProject
                                            ? String(gameDetailsPopup.primaryProject.developmentStatus)
                                                .replace("_", " ").toUpperCase() : ""
                                        color: theme.warningText
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: gameDetailsPopup.primaryProject !== null
                                        && (gameDetailsPopup.primaryProject.developmentStatus === "beta"
                                            || gameDetailsPopup.primaryProject.developmentStatus === "alpha")
                                    text: gameDetailsPopup.primaryProject
                                        && gameDetailsPopup.primaryProject.developmentStatus === "beta"
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
                                    visible: gameDetailsPopup.primaryProject !== null
                                        && window.launchErrors[gameDetailsPopup.primaryProject.id] !== undefined
                                    text: gameDetailsPopup.primaryProject
                                        ? (window.launchErrors[gameDetailsPopup.primaryProject.id] || "") : ""
                                    color: theme.error
                                    wrapMode: Text.Wrap
                                }
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
                                    text: "Available " + gameDetailsPopup.releaseTag
                                        + "    •    Linux / " + projectCatalog.currentArchitecture
                                        + "\n" + gameDetailsPopup.selectedAssetName
                                    color: theme.textSecondary
                                    font.pixelSize: 11 * theme.fontScale
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
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        visible: !!window.selectedGame
                            && (!window.selectedGame.metadataTitle
                                || window.selectedGame.metadataTitle.length === 0
                                || !window.selectedGame.artworkPath
                                || window.selectedGame.artworkPath.length === 0)
                        enabled: metadataManager.configured && !metadataMatcher.busy
                        text: metadataMatcher.busy ? "Checking Metadata…" : "Retry Metadata"
                        onClicked: {
                            console.info("[UI] Retry metadata requested for game ID:", window.selectedGame.id)
                            metadataMatcher.scrapeGame(window.selectedGame.id)
                        }
                    }

                    Button {
                        visible: gameDetailsPopup.installedProject !== null
                        highlighted: true
                        text: gameDetailsPopup.primaryProject
                            && window.runningLaunchers[gameDetailsPopup.primaryProject.id]
                            ? "Running…" : "Play Game"
                        icon.source: "qrc:/icons/play.svg"
                        icon.width: 18
                        icon.height: 18
                        icon.color: theme.textPrimary
                        enabled: gameDetailsPopup.primaryProject !== null
                            && gameDetailsPopup.installedProject !== null
                            && !window.runningLaunchers[gameDetailsPopup.primaryProject.id]
                        onClicked: window.launchSupportedGame(gameDetailsPopup.primaryProject.id)
                    }
                    Button {
                        visible: gameDetailsPopup.installedProject !== null
                        text: "Uninstall"
                        icon.source: "qrc:/icons/uninstall.svg"
                        icon.width: 18
                        icon.height: 18
                        icon.color: theme.textPrimary
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
                        text: "Install Launcher"
                        icon.source: "qrc:/icons/install.svg"
                        icon.width: 18
                        icon.height: 18
                        icon.color: theme.textPrimary
                        onClicked: gameDetailsPopup.installPromptVisible = true
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!gameDetailsPopup.installPromptVisible
                            && !!gameDetailsPopup.primaryProject
                            && gameDetailsPopup.installedProject === null
                        text: gameDetailsPopup.primaryProject
                            ? (gameDetailsPopup.primaryProject.developmentStatus === "beta"
                                ? "This is a BETA project. It may contain bugs or incomplete features. Install "
                                    + gameDetailsPopup.primaryProject.name + "?"
                                : gameDetailsPopup.primaryProject.developmentStatus === "alpha"
                                    ? "This is an ALPHA project and may be unstable. Install "
                                        + gameDetailsPopup.primaryProject.name + "?"
                                    : "Install " + gameDetailsPopup.primaryProject.name + "?")
                            : ""
                        color: theme.textPrimary
                        wrapMode: Text.Wrap
                    }
                    Button {
                        visible: gameDetailsPopup.installPromptVisible
                            && gameDetailsPopup.installedProject === null
                        highlighted: true
                        text: "Yes"
                        icon.source: "qrc:/icons/check.svg"
                        icon.width: 16
                        icon.height: 16
                        icon.color: theme.textPrimary
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
                        icon.source: "qrc:/icons/close.svg"
                        icon.width: 16
                        icon.height: 16
                        icon.color: theme.textPrimary
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

    Item {
        id: mainNavigationRow
        anchors.fill: parent

        Rectangle {
            id: mainContentPanel
            anchors.fill: parent
            color: theme.windowBackground
            Component.onCompleted: console.info("[Settings Geometry] main content panel:", width, "x", height)
            onWidthChanged: console.info("[Settings Geometry] main content panel width:", width)

            // A stable, understated theme backdrop keeps the library visually
            // consistent while games are browsed. Selected artwork belongs to
            // the dedicated details experience, not the collection background.
            Rectangle {
                anchors.fill: parent
                visible: currentPage === "Library"
                gradient: Gradient {
                    GradientStop { position: 0.0; color: theme.backgroundTop }
                    GradientStop {
                        position: 0.52
                        color: Qt.lighter(theme.windowBackground, 1.12)
                    }
                    GradientStop { position: 1.0; color: theme.backgroundBottom }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: currentPage === "Library"
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0.0
                        color: Qt.rgba(theme.accent.r, theme.accent.g,
                                       theme.accent.b, 0.055)
                    }
                    GradientStop { position: 0.42; color: "transparent" }
                    GradientStop {
                        position: 1.0
                        color: Qt.rgba(theme.accent.r, theme.accent.g,
                                       theme.accent.b, 0.035)
                    }
                }
            }

            StackLayout {
                id: mainPageStack
                anchors.fill: parent
                Component.onCompleted: console.info("[Settings Geometry] main page stack:", width, "x", height)
                onWidthChanged: console.info("[Settings Geometry] main page stack width:", width)
                anchors.leftMargin: 30
                anchors.rightMargin: 30
                anchors.bottomMargin: 30
                anchors.topMargin: 82
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
                                    text: selectedPlatform === "All Platforms" ? "Your Collection" : selectedPlatform
                                    font.pixelSize: 32 * theme.fontScale
                                    font.bold: true
                                    color: theme.textPrimary
                                }
                                RowLayout {
                                    spacing: 8
                                    Label {
                                        text: gameGrid.count + (gameGrid.count === 1 ? " GAME" : " GAMES")
                                        color: theme.accentHover
                                        font.pixelSize: 11 * theme.fontScale
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
                                        font.pixelSize: 13 * theme.fontScale
                                    }
                                }
                            }

                            ComboBox {
                                Layout.preferredWidth: 190
                                Layout.preferredHeight: 42
                                model: ["All Platforms"].concat(platformManager.platforms())
                                currentIndex: Math.max(0, model.indexOf(window.selectedPlatform))
                                onActivated: window.selectedPlatform = currentText
                            }
                        }

                        GridView {
                            id: gameGrid
                            Layout.fillWidth: true
                            Layout.fillHeight: gameGrid.count > 0
                            Layout.preferredHeight: gameGrid.count > 0 ? 760 : 0
                            clip: true
                            cellWidth: Math.max(260, Math.min(320,
                                Math.floor(width / Math.max(1, Math.floor(width / 300)))))
                            cellHeight: Math.round(cellWidth * 1.35)
                            keyNavigationEnabled: true
                            model: {
                                window.libraryRevision
                                const games = databaseManager.games(
                                    selectedPlatform === "All Platforms" ? "" : selectedPlatform)
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
                                z: GridView.isCurrentItem ? 2 : 1
                                scale: GridView.isCurrentItem ? 1.035 : 1.0
                                Behavior on scale {
                                    enabled: !settingsManager.reducedMotion
                                    NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radiusCard
                                    color: theme.surface
                                    border.width: tileMouse.containsMouse || gameTile.GridView.isCurrentItem ? 2 : 1
                                    border.color: tileMouse.containsMouse || gameTile.GridView.isCurrentItem
                                        ? theme.accent : theme.divider
                                    Behavior on border.color {
                                        enabled: !settingsManager.reducedMotion
                                        ColorAnimation { duration: 140 }
                                    }
                                    Behavior on border.width {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 140 }
                                    }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 9

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumHeight: 210
                                            radius: theme.radiusSmall
                                            color: theme.sidebarBackground
                                            clip: true

                                            Image {
                                                id: artworkBackdrop
                                                anchors.fill: parent
                                                source: gameTile.artworkPath
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                                cache: true
                                                visible: gameTile.artworkPath.length > 0
                                                opacity: 0.42
                                            }
                                            MultiEffect {
                                                anchors.fill: parent
                                                source: artworkBackdrop
                                                blurEnabled: true
                                                blur: 0.8
                                                autoPaddingEnabled: false
                                                visible: gameTile.artworkPath.length > 0
                                            }
                                            Rectangle {
                                                anchors.fill: parent
                                                color: theme.artworkOverlay
                                                visible: gameTile.artworkPath.length > 0
                                            }
                                            Image {
                                                anchors.fill: parent
                                                anchors.margins: 5
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
                                                    font.pixelSize: 34 * theme.fontScale
                                                }
                                                Label {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: "Artwork unavailable"
                                                    color: theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                }
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: gameTile.title
                                            color: theme.textPrimary
                                            font.pixelSize: 16 * theme.fontScale
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                Layout.fillWidth: true
                                                text: gameTile.platform
                                                color: theme.textSecondary
                                                font.pixelSize: 12 * theme.fontScale
                                                elide: Text.ElideRight
                                            }
                                            Label {
                                                text: gameTile.metadataTitle.length > 0 ? "METADATA" : ""
                                                color: theme.success
                                                font.pixelSize: 9 * theme.fontScale
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
                                    font.pixelSize: 11 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1.4
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: window.librarySearchText.trim().length > 0
                                        ? "No games match your search."
                                        : "Your library is ready for its first game."
                                    color: theme.textPrimary
                                    font.pixelSize: 22 * theme.fontScale
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: window.librarySearchText.trim().length === 0
                                    text: "Add game files to one of your platform folders, then scan your library to discover them."
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
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
                            font.pixelSize: 30 * theme.fontScale
                            font.bold: true
                            color: theme.textPrimary
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Search TheGamesDB for a game and review possible matches."
                            color: theme.textMuted
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
                                color: theme.warning
                                wrapMode: Text.Wrap
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Platform"
                                color: theme.textSecondary
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
                                color: theme.accent
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Local ROM"
                                color: theme.textSecondary
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
                            color: theme.textMuted
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: window.metadataAssociationMessage.length > 0
                            text: window.metadataAssociationMessage
                            color: window.metadataAssociationMessage.startsWith("Metadata associated")
                                   ? theme.success : theme.error
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
                            color: window.artworkMessage.startsWith("Artwork saved") ? theme.success : theme.accent
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: metadataManager.errorString.length > 0
                            text: metadataManager.errorString
                            color: theme.error
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: metadataManager.searching
                            text: "Searching TheGamesDB..."
                            color: theme.accent
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: !metadataManager.searching
                                     && metadataManager.errorString.length === 0
                                     && metadataManager.results.length > 0
                            text: metadataManager.results.length + " result(s) found."
                            color: theme.textMuted
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
                                        color: theme.textPrimary
                                        font.pixelSize: 17 * theme.fontScale
                                        font.bold: true
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: (releaseDate.length > 0 ? releaseDate : "Release date unknown")
                                              + "  •  " + (rating.length > 0 ? rating : "Rating unavailable")
                                              + "  •  TheGamesDB ID: " + providerGameId
                                        color: theme.accent
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: overview.length > 0 ? overview : "No description available."
                                        color: theme.textSecondary
                                        font.pixelSize: 12 * theme.fontScale
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
                                            color: theme.textMuted
                                            font.pixelSize: 11 * theme.fontScale
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
                            color: theme.textMuted
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20
                        Label { text: "Platforms"; font.pixelSize: 30 * theme.fontScale; font.bold: true; color: theme.textPrimary }
                        Label { text: "Browse your supported console platforms."; color: theme.textMuted }
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
                                        Label { text: modelData; color: theme.textPrimary; font.pixelSize: 20 * theme.fontScale; font.bold: true }
                                        Label { text: "Game directory"; color: theme.textMuted; font.pixelSize: 12 * theme.fontScale }
                                        Item { Layout.fillHeight: true }
                                        Label { text: "Open platform"; color: theme.accent; font.pixelSize: 12 * theme.fontScale }
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
                            color: theme.textSubtle
                            font.pixelSize: 11 * theme.fontScale
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 16
                        Label { text: "Launcher Discovery"; color: theme.textPrimary; font.pixelSize: 30 * theme.fontScale; font.bold: true }
                        Label {
                            Layout.fillWidth: true
                            text: "Browse supported projects, check compatible Linux releases, and install compatible launchers automatically."
                            color: theme.textMuted
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
                                        color: theme.textPrimary
                                        font.pixelSize: 18 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        text: modelData.platform + "  •  " + modelData.repositoryOwner + "/" + modelData.repositoryName
                                        color: theme.textMuted
                                        font.pixelSize: 12 * theme.fontScale
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.romRequirements
                                        color: theme.textSecondary
                                        font.pixelSize: 11 * theme.fontScale
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
                                            color: theme.accent
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: releaseState.loading === true
                                        text: "Contacting GitHub and checking release assets..."
                                        color: theme.accent
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: releaseState.error.length > 0
                                        text: releaseState.error || ""
                                        color: theme.error
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
                                                    color: theme.success
                                                    font.pixelSize: 12 * theme.fontScale
                                                    wrapMode: Text.Wrap
                                                }
                                                Label {
                                                    text: (modelData.size / 1048576).toFixed(1) + " MB"
                                                    color: theme.textMuted
                                                    font.pixelSize: 11 * theme.fontScale
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
                                                    color: downloadState.error.length > 0 ? theme.error : theme.accent
                                                    font.pixelSize: 10 * theme.fontScale
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: releaseState.release !== null && (releaseState.assets || []).length === 0
                                        text: "No compatible Linux package was found for this device (" + projectCatalog.currentArchitecture + ")."
                                        color: theme.warning
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: !!releaseState.release
                                        text: releaseState.release
                                              ? "Release published: " + releaseState.release.publishedAt
                                              : ""
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }
                    }
                }

                Item {
                    id: settingsPage
                    property string section: "Appearance"
                    onSectionChanged: console.info("[Settings QML] Section changed:", section)

                    RowLayout {
                        id: settingsLayoutRow
                        anchors.fill: parent
                        spacing: 22

                        ColumnLayout {
                            id: settingsNavigationColumn
                            Layout.minimumWidth: 205
                            Layout.maximumWidth: 205
                            Layout.preferredWidth: 205
                            Layout.fillHeight: true
                            spacing: 10

                            Label {
                                text: "SETTINGS"
                                color: theme.textMuted
                                font.pixelSize: 11 * theme.fontScale
                                font.bold: true
                                font.letterSpacing: 1
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "Personalize your RecompStation experience."
                                color: theme.textSecondary
                                font.pixelSize: 12 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }

                            Button {
                                Layout.fillWidth: true
                                text: "✦   Appearance"
                                highlighted: settingsPage.section === "Appearance"
                                onClicked: {
                                    console.info("[Settings QML] Appearance navigation clicked")
                                    settingsPage.section = "Appearance"
                                }
                            }
                            Button {
                                Layout.fillWidth: true
                                text: "▣   Display"
                                highlighted: settingsPage.section === "Display"
                                onClicked: settingsPage.section = "Display"
                            }
                            Button {
                                Layout.fillWidth: true
                                text: "▤   Library"
                                highlighted: settingsPage.section === "Library"
                                onClicked: settingsPage.section = "Library"
                            }
                            Button {
                                Layout.fillWidth: true
                                text: "⌘   Application"
                                highlighted: settingsPage.section === "Application"
                                onClicked: settingsPage.section = "Application"
                            }

                            Item { Layout.fillHeight: true }
                            Label {
                                Layout.fillWidth: true
                                text: "Other settings will be added when their features are ready."
                                color: theme.textMuted
                                font.pixelSize: 11 * theme.fontScale
                                wrapMode: Text.Wrap
                            }
                        }

                        Rectangle {
                            id: settingsContentCard
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: theme.radiusLarge
                            color: theme.surface
                            border.color: theme.divider
                            clip: true

                            ColumnLayout {
                                id: appearanceContent
                                anchors.fill: parent
                                anchors.margins: 26
                                spacing: 16
                                visible: settingsPage.section === "Appearance"
                                onVisibleChanged: console.info(
                                    "[Settings QML] Appearance content visibility:", visible,
                                    "section:", settingsPage.section,
                                    "currentPage:", window.currentPage,
                                    "size:", width, "x", height)

                                Label {
                                    text: "Appearance"
                                    color: theme.textPrimary
                                    font.pixelSize: 27 * theme.fontScale
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Choose a coordinated color palette for the entire application."
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                Label {
                                    text: "COLOR PRESETS"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1
                                }

                                Repeater {
                                    model: [
                                        { id: "midnight-violet", title: "Midnight Violet", description: "A deep, atmospheric palette with violet highlights", accent: "#8174e8", accentSoft: "#978cf2", background: "#11131b", surface: "#222635", elevated: "#2a3042" },
                                        { id: "electric-blue", title: "Electric Blue", description: "Cool layered blues with bright electric accents", accent: "#55a8f5", accentSoft: "#7bbcff", background: "#101722", surface: "#202e40", elevated: "#293b51" },
                                        { id: "ember", title: "Ember", description: "Warm charcoal surfaces with copper and amber accents", accent: "#d58b59", accentSoft: "#e6a875", background: "#191411", surface: "#30231c", elevated: "#3b2b22" }
                                    ]
                                    delegate: Button {
                                        id: themePresetButton
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 76
                                        padding: 0
                                        onClicked: pendingThemeName = modelData.id

                                        background: Rectangle {
                                            radius: theme.radiusCard
                                            color: themePresetButton.hovered
                                                ? theme.elevatedSurface : theme.sidebarBackground
                                            border.width: pendingThemeName === modelData.id ? 2 : 1
                                            border.color: pendingThemeName === modelData.id
                                                ? theme.accent : theme.divider
                                        }

                                        contentItem: RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 13
                                            spacing: 14

                                            Rectangle {
                                                width: 48
                                                height: 48
                                                radius: 9
                                                color: modelData.background
                                                border.color: theme.divider

                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 6
                                                    spacing: 3
                                                    Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 3; color: modelData.surface }
                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        Layout.fillHeight: true
                                                        spacing: 3
                                                        Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 3; color: modelData.elevated }
                                                        Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; radius: 3; color: modelData.accent }
                                                    }
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 3
                                                Label {
                                                    Layout.fillWidth: true
                                                    Layout.minimumWidth: 0
                                                    text: modelData.title
                                                    color: theme.textPrimary
                                                    font.pixelSize: 14 * theme.fontScale
                                                    font.bold: true
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    Layout.minimumWidth: 0
                                                    text: modelData.description
                                                    color: theme.textMuted
                                                    font.pixelSize: 11 * theme.fontScale
                                                    wrapMode: Text.Wrap
                                                    maximumLineCount: 2
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            Label {
                                                Layout.preferredWidth: 94
                                                Layout.minimumWidth: 94
                                                Layout.maximumWidth: 94
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                                text: pendingThemeName === modelData.id
                                                    ? (settingsManager.themeName === modelData.id ? "✓  Applied" : "✓  Selected")
                                                    : "Select"
                                                color: pendingThemeName === modelData.id ? theme.accent : theme.textSecondary
                                                font.pixelSize: 12 * theme.fontScale
                                                font.bold: pendingThemeName === modelData.id
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        visible: pendingThemeName !== settingsManager.themeName
                                        text: "You have an unapplied theme selection."
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                    }
                                    Button {
                                        text: "Apply Theme"
                                        enabled: pendingThemeName !== settingsManager.themeName
                                        highlighted: enabled
                                        onClicked: settingsManager.themeName = pendingThemeName
                                    }
                                }
                                Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                Label {
                                    text: "ACCESSIBILITY"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 18
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 4
                                        Label {
                                            text: "Interface font size"
                                            color: theme.textPrimary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: true
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Adjust text throughout RecompStation."
                                            color: theme.textMuted
                                            font.pixelSize: 11 * theme.fontScale
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                    ComboBox {
                                        id: fontSizeSelector
                                        Layout.preferredWidth: 150
                                        textRole: "label"
                                        valueRole: "value"
                                        model: [
                                            { label: "Small", value: "small" },
                                            { label: "Medium", value: "medium" },
                                            { label: "Large", value: "large" }
                                        ]
                                        Component.onCompleted: {
                                            const savedIndex = indexOfValue(settingsManager.fontSize)
                                            currentIndex = savedIndex >= 0 ? savedIndex : 1
                                        }
                                        onActivated: settingsManager.fontSize = currentValue
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 18
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 4
                                        Label {
                                            text: "Reduced motion"
                                            color: theme.textPrimary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: true
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Disable nonessential interface animations."
                                            color: theme.textMuted
                                            font.pixelSize: 11 * theme.fontScale
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                    Switch {
                                        checked: settingsManager.reducedMotion
                                        onToggled: settingsManager.reducedMotion = checked
                                        Accessible.name: "Reduced motion"
                                    }
                                }
                                Item { Layout.fillHeight: true }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Select a palette, then apply it to save your choice."
                                    color: theme.textMuted
                                    font.pixelSize: 11 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                            }

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 26
                                clip: true
                                visible: settingsPage.section === "Display"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 16

                                    Label {
                                        text: "Display"
                                        color: theme.textPrimary
                                        font.pixelSize: 27 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Choose the preferred size of the RecompStation window. Your desktop compositor remains in control of the monitor resolution and window placement."
                                        color: theme.textSecondary
                                        font.pixelSize: 13 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                    Label {
                                        text: "CURRENT DISPLAY"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: displayInfo.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: theme.divider
                                        ColumnLayout {
                                            id: displayInfo
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 7
                                            Label {
                                                text: Screen.name.length > 0 ? Screen.name : "Current display"
                                                color: theme.textPrimary
                                                font.pixelSize: 13 * theme.fontScale
                                                font.bold: true
                                            }
                                            Label {
                                                text: "Desktop resolution: " + Screen.width + " × " + Screen.height
                                                color: theme.textSecondary
                                                font.pixelSize: 12 * theme.fontScale
                                            }
                                            Label {
                                                text: "Available screen area: " + Screen.desktopAvailableWidth + " × " + Screen.desktopAvailableHeight
                                                color: theme.textMuted
                                                font.pixelSize: 11 * theme.fontScale
                                            }
                                        }
                                    }
                                    Label {
                                        text: "PREFERRED WINDOW SIZE"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    ComboBox {
                                        id: presentationSizeSelector
                                        Layout.fillWidth: true
                                        textRole: "label"
                                        valueRole: "value"
                                        model: [
                                            { label: "Follow desktop / keep current size", value: "system" },
                                            { label: "1280 × 720  •  16:9", value: "1280x720" },
                                            { label: "1920 × 1080  •  16:9", value: "1920x1080" },
                                            { label: "1920 × 1440  •  4:3", value: "1920x1440" },
                                            { label: "3440 × 1440  •  21:9", value: "3440x1440" },
                                            { label: "3840 × 2160  •  16:9", value: "3840x2160" },
                                            { label: "5120 × 2160  •  21:9", value: "5120x2160" }
                                        ]
                                        Component.onCompleted: {
                                            const savedIndex = indexOfValue(settingsManager.preferredPresentationSize)
                                            currentIndex = savedIndex >= 0 ? savedIndex : 0
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            text: "Apply Window Size"
                                            highlighted: true
                                            enabled: presentationSizeSelector.currentValue
                                                !== settingsManager.preferredPresentationSize
                                                || presentationSizeSelector.currentValue !== "system"
                                            onClicked: {
                                                settingsManager.setPreferredPresentationSize(
                                                    presentationSizeSelector.currentValue)
                                                window.applyPreferredPresentationSize()
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Presets are scaled down proportionally, when needed, to fit within 90% of the current screen's logical size. Their aspect ratios are preserved. On Wayland, the compositor may also apply its own window-size constraints. Selecting Follow desktop leaves the current window size unchanged."
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Item { Layout.fillHeight: true }
                                }
                            }

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 26
                                clip: true
                                visible: settingsPage.section === "Library"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ColumnLayout {
                                    width: parent.width
                                    spacing: 16
                                    Label { text: "Library"; color: theme.textPrimary; font.pixelSize: 27 * theme.fontScale; font.bold: true }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Choose one home for your game collection. RecompStation organizes it into platform folders automatically."
                                        color: theme.textSecondary; font.pixelSize: 13 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                    Label { text: "SUPPORTED PLATFORM FOLDERS"; color: theme.textMuted; font.pixelSize: 10 * theme.fontScale; font.bold: true; font.letterSpacing: 1 }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Your library location is selected during first-time setup. RecompStation organizes supported games into platform subfolders beneath that location."
                                        color: theme.textSecondary; font.pixelSize: 12 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Repeater {
                                            model: platformManager.platforms()
                                            delegate: Rectangle {
                                                required property string modelData
                                                radius: 12; color: theme.surface; border.color: theme.divider
                                                width: platformChip.implicitWidth + 22; height: 30
                                                Label { id: platformChip; anchors.centerIn: parent; text: modelData; color: theme.textSecondary; font.pixelSize: 11 * theme.fontScale }
                                            }
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                    Label { text: "SCAN BEHAVIOR"; color: theme.textMuted; font.pixelSize: 10 * theme.fontScale; font.bold: true; font.letterSpacing: 1 }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: scanStartupRow.implicitHeight + 24
                                        radius: theme.radiusCard; color: theme.sidebarBackground; border.color: theme.divider
                                        RowLayout {
                                            id: scanStartupRow
                                            anchors.fill: parent; anchors.margins: 12; spacing: 12
                                            ColumnLayout {
                                                Layout.fillWidth: true; spacing: 4
                                                Label { text: "Scan library on startup"; color: theme.textPrimary; font.pixelSize: 13 * theme.fontScale; font.bold: true }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "Automatically check the configured platform folders when RecompStation launches."
                                                    color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
                                                }
                                            }
                                            Switch {
                                                checked: settingsManager.scanOnStartup
                                                onToggled: settingsManager.setScanOnStartup(checked)
                                            }
                                        }
                                    }
                                    Button {
                                        text: "Scan Library Now"; highlighted: true
                                        enabled: platformManager.libraryConfigured
                                        onClicked: { window.scanMessage = "Scanning configured game folders..."; libraryScanner.scanAll() }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Place game files inside their matching platform folders. Automatic live folder monitoring will be added separately."
                                        color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 26
                                spacing: 16
                                visible: settingsPage.section === "Application"

                                Label {
                                    text: "Application"
                                    color: theme.textPrimary
                                    font.pixelSize: 27 * theme.fontScale
                                    font.bold: true
                                }
                                Label {
                                    text: "Application details and local storage."
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                }
                                Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                Label {
                                    text: "APPLICATION DATA DIRECTORY"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: dataInfo.implicitHeight + 28
                                    radius: theme.radiusCard
                                    color: theme.sidebarBackground
                                    border.color: theme.divider
                                    ColumnLayout {
                                        id: dataInfo
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 7
                                        Label {
                                            text: "Storage location"
                                            color: theme.textPrimary
                                            font.pixelSize: 13 * theme.fontScale
                                            font.bold: true
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: platformManager.dataDirectory
                                            color: theme.textSecondary
                                            font.pixelSize: 12 * theme.fontScale
                                            wrapMode: Text.WrapAnywhere
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Contains your game database, artwork cache, downloads, and installed launchers."
                                            color: theme.textMuted
                                            font.pixelSize: 11 * theme.fontScale
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                                Item { Layout.fillHeight: true }
                                Label {
                                    text: "RecompStation  •  Version 0.1.0"
                                    color: theme.textMuted
                                    font.pixelSize: 11 * theme.fontScale
                                }
                            }
                        }
                    }
                }
            }
        }
    
        Rectangle {
            id: topNavigationBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 66
            color: Qt.rgba(theme.sidebarBackground.r,
                           theme.sidebarBackground.g,
                           theme.sidebarBackground.b, 0.72)
            border.color: theme.divider
            border.width: 1
            z: 20
            visible: !gameDetailsPopup.visible

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 30
                anchors.rightMargin: 30
                spacing: 24

                RowLayout {
                    spacing: 10
                    Image {
                        source: "qrc:/icons/gamepad.svg"
                        sourceSize.width: 28
                        sourceSize.height: 28
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        smooth: true
                    }
                    Label {
                        text: "RecompStation"
                        color: theme.textPrimary
                        font.pixelSize: 17 * theme.fontScale
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 8
                    Button {
                        text: "Library"
                        icon.source: "qrc:/icons/library.svg"
                        icon.width: 18
                        icon.height: 18
                        icon.color: theme.textPrimary
                        highlighted: currentPage === "Library"
                        onClicked: {
                            console.info("[UI] Navigation: Game Library")
                            currentPage = "Library"
                        }
                    }
                    Button {
                        text: "Settings"
                        icon.source: "qrc:/icons/settings.svg"
                        icon.width: 18
                        icon.height: 18
                        icon.color: theme.textPrimary
                        highlighted: currentPage === "Settings"
                        onClicked: {
                            console.info("[UI] Navigation: Settings")
                            currentPage = "Settings"
                        }
                    }
                }
            }
        }
}
}
