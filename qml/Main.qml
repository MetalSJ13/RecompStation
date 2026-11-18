import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs
import QtCore

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 850
    minimumHeight: 550
    visible: true
    title: "RecompStation"

    // Explicitly terminate the Qt event loop when the desktop window's close
    // button is used. This ensures scripts/run.sh returns to the shell prompt.
    onClosing: function(close) {
        console.info("[Application] Main window close requested; quitting.")
        close.accepted = true
        Qt.quit()
    }

    property string activeInputDevice: controllerManager.connected ? "controller" : "keyboard"
    property bool systemMenuVisible: false
    property bool systemMenuClosing: false
    property bool quitConfirmationVisible: false

    Settings {
        id: libraryVisualSettings
        category: "LibraryVisuals"
        property string wavePalette: "ocean"
    }

    function inputHintItems() {
        if (activeInputDevice === "controller" && controllerManager.connected) {
            let selectButton = "A"
            let backButton = "B"
            let switchButtons = "LB / RB"
            let selectColor = "#48C78E"
            let backColor = "#E58B78"
            if (controllerManager.controllerType === "playstation") {
                selectButton = "✕"
                backButton = "○"
                switchButtons = "L1 / R1"
                selectColor = "#55A9F4"
                backColor = "#F06B8B"
            } else if (controllerManager.controllerType === "nintendo") {
                selectButton = "B"
                backButton = "A"
                switchButtons = "L / R"
                selectColor = "#E58B78"
                backColor = "#48A8E8"
            } else if (controllerManager.controllerType !== "xbox") {
                selectButton = "S"
                backButton = "E"
                switchButtons = "L / R"
            }
            return [
                { icon: "qrc:/icons/dpad.svg", key: "", label: "Navigate", color: "#B8C7D9", round: false },
                { icon: "", key: selectButton, label: "Select", color: selectColor, round: true },
                { icon: "", key: backButton, label: "Back", color: backColor, round: true },
                { icon: "", key: switchButtons, label: "Switch", color: "#A9B7CA", round: false }
            ]
        }
        if (activeInputDevice === "mouse") {
            return [
                { icon: "qrc:/icons/mouse.svg", key: "", label: "Select", color: "#B8C7D9", round: false },
                { icon: "", key: "↕", label: "Navigate", color: "#A9B7CA", round: false },
                { icon: "", key: "Esc", label: "Back", color: "#A9B7CA", round: false }
            ]
        }
        return [
            { icon: "", key: "← ↑ ↓ →", label: "Navigate", color: "#A9B7CA", round: false },
            { icon: "", key: "Enter", label: "Select", color: "#A9B7CA", round: false },
            { icon: "", key: "Esc", label: "Back", color: "#A9B7CA", round: false },
            { icon: "", key: "F11", label: "Fullscreen", color: "#A9B7CA", round: false },
            { icon: "", key: "Ctrl+Q", label: "Quit", color: "#A9B7CA", round: false }
        ]
    }

    function collectFocusableItems(item, output, visited) {
        if (!item || item.visible === false || item.enabled === false)
            return
        if (visited.indexOf(item) >= 0)
            return
        visited.push(item)
        if (item.activeFocusOnTab && item.width > 0 && item.height > 0)
            output.push(item)
        const children = item.children || []
        for (let i = 0; i < children.length; ++i)
            collectFocusableItems(children[i], output, visited)
        if (item.contentItem && item.contentItem !== item)
            collectFocusableItems(item.contentItem, output, visited)
        if (item.popup && item.popup.visible && item.popup.contentItem)
            collectFocusableItems(item.popup.contentItem, output, visited)
    }

    function moveDirectionalFocus(container, direction) {
        const candidates = []
        collectFocusableItems(container, candidates, [])
        const current = window.activeFocusItem
        if (!current || candidates.indexOf(current) < 0) {
            if (candidates.length > 0)
                candidates[0].forceActiveFocus()
            return candidates.length > 0
        }

        const origin = current.mapToItem(container, current.width / 2, current.height / 2)
        let best = null
        let bestScore = Number.POSITIVE_INFINITY
        for (let i = 0; i < candidates.length; ++i) {
            const candidate = candidates[i]
            if (candidate === current)
                continue
            const point = candidate.mapToItem(container,
                candidate.width / 2, candidate.height / 2)
            const dx = point.x - origin.x
            const dy = point.y - origin.y
            const primary = direction === "right" ? dx
                : direction === "left" ? -dx
                : direction === "down" ? dy : -dy
            if (primary <= 4)
                continue
            const cross = direction === "left" || direction === "right"
                ? Math.abs(dy) : Math.abs(dx)
            const score = primary + cross * 2.2
            if (score < bestScore) {
                bestScore = score
                best = candidate
            }
        }

        if (!best)
            return false
        best.forceActiveFocus()
        return true
    }

    function isDescendantOf(item, ancestor) {
        let current = item
        while (current) {
            if (current === ancestor)
                return true
            current = current.parent
        }
        return false
    }

    function settingsNavigationButtonForSection(section) {
        switch (section) {
        case "Display": return displayNavigationButton
        case "Library": return libraryNavigationButton
        case "Metadata": return metadataNavigationButton
        case "Controller": return controllerNavigationButton
        case "Application": return applicationNavigationButton
        default: return appearanceNavigationButton
        }
    }

    function controllerNavigate(direction) {
        if (systemMenuVisible) {
            window.moveDirectionalFocus(systemMenuPanel, direction)
            return
        }

        if (currentPage === "Settings" && window.activeFocusItem
                && window.activeFocusItem.popup
                && window.activeFocusItem.popup.visible) {
            const combo = window.activeFocusItem
            if (direction === "up")
                combo.currentIndex = Math.max(0, combo.currentIndex - 1)
            else if (direction === "down")
                combo.currentIndex = Math.min(combo.count - 1, combo.currentIndex + 1)
            return
        }

        if (gameDetailsPopup.visible) {
            if (window.moveDirectionalFocus(detailsContentFrame, direction))
                return
            if (direction === "up" || direction === "down") {
                const bar = gameInfoScroll.ScrollBar.vertical
                bar.position = Math.max(0, Math.min(1,
                    bar.position + (direction === "down" ? 0.12 : -0.12)))
            }
            return
        }

        if (currentPage === "Library" && gameGrid.count > 0) {
            let next = gameGrid.currentIndex < 0 ? 0 : gameGrid.currentIndex
            if (settingsManager.libraryView === "list") {
                if (direction === "up") next--
                else if (direction === "down") next++
                next = Math.max(0, Math.min(gameGrid.count - 1, next))
                gameGrid.currentIndex = next
                gameList.positionViewAtIndex(next, ListView.Contain)
                gameList.forceActiveFocus()
                return
            }
            const columns = Math.max(1, Math.floor(gameGrid.width / gameGrid.cellWidth))
            if (direction === "left") next--
            else if (direction === "right") next++
            else if (direction === "up") next -= columns
            else if (direction === "down") next += columns
            next = Math.max(0, Math.min(gameGrid.count - 1, next))
            gameGrid.currentIndex = next
            gameGrid.positionViewAtIndex(next, GridView.Contain)
            gameGrid.forceActiveFocus()
        } else if (currentPage === "Settings") {
            const sections = ["Appearance", "Display", "Library", "Metadata", "Controller", "Application"]
            const focused = window.activeFocusItem
            const inNavigation = window.isDescendantOf(focused, settingsNavigationPanel)
            const inContent = window.isDescendantOf(focused, settingsContentCard)

            // The category rail and the active settings page are separate
            // focus zones. Up/down changes categories only while the rail
            // owns focus; left returns from page controls to the rail.
            if (inNavigation) {
                let index = sections.indexOf(settingsPage.section)
                if (direction === "up" || direction === "down") {
                    index = Math.max(0, Math.min(sections.length - 1,
                        index + (direction === "up" ? -1 : 1)))
                    settingsPage.section = sections[index]
                    Qt.callLater(function() {
                        window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus()
                    })
                } else if (direction === "right") {
                    const candidates = []
                    window.collectFocusableItems(settingsContentCard, candidates, [])
                    if (candidates.length > 0)
                        candidates[0].forceActiveFocus()
                }
                return
            }

            if (inContent) {
                if (direction === "left") {
                    window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus()
                    return
                }
                window.moveDirectionalFocus(settingsContentCard, direction)
                return
            }

            window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus()
        }
    }

    function controllerSelect(inputDevice) {
        activeInputDevice = inputDevice || "controller"
        if (systemMenuVisible) {
            const focused = window.activeFocusItem
            if (focused && typeof focused.click === "function")
                focused.click()
            return
        }
        if (gameDetailsPopup.visible) {
            const focused = window.activeFocusItem
            if (focused && typeof focused.click === "function")
                focused.click()
            return
        }
        if (currentPage === "Library" && gameGrid.count > 0) {
            const index = gameGrid.currentIndex < 0 ? 0 : gameGrid.currentIndex
            if (settingsManager.libraryView === "list") {
                const tile = gameList.itemAtIndex(index)
                if (tile && tile.openDetails)
                    tile.openDetails()
            } else {
                const tile = gameGrid.itemAtIndex(index)
                if (tile && tile.openDetails)
                    tile.openDetails()
            }
        } else if (currentPage === "Settings") {
            const focused = window.activeFocusItem
            if (focused && focused.popup) {
                if (focused.popup.visible) {
                    focused.popup.close()
                    if (focused === displayModeSelector) {
                        if (focused.currentValue === "fullscreen"
                                && window.visibility !== Window.FullScreen) {
                            window.preFullscreenMode = window.visibility === Window.Maximized
                                ? "maximized" : "windowed"
                            settingsManager.setRestoreDisplayMode(window.preFullscreenMode)
                        }
                        settingsManager.setDisplayMode(focused.currentValue)
                        window.applyDisplayMode(focused.currentValue)
                    } else if (focused === fontSizeSelector) {
                        settingsManager.fontSize = focused.currentValue
                    }
                } else {
                    focused.popup.open()
                }
            } else if (focused && typeof focused.click === "function") {
                focused.click()
            } else {
                moveDirectionalFocus(settingsPage, "down")
            }
        }
    }

    function closeSystemMenu() {
        if (!systemMenuVisible)
            return
        systemMenuClosing = true
        systemMenuVisible = false
    }

    function controllerBack() {
        activeInputDevice = "controller"
        if (quitConfirmationVisible) {
            quitConfirmationVisible = false
            return
        }
        if (currentPage === "Settings" && window.activeFocusItem
                && window.activeFocusItem.popup && window.activeFocusItem.popup.visible) {
            window.activeFocusItem.popup.close()
            return
        }
        if (systemMenuVisible) {
            closeSystemMenu()
            return
        }
        if (gameDetailsPopup.visible)
            gameDetailsPopup.close()
        else if (currentPage !== "Library")
            navigateToPage("Library")
    }
    color: theme.windowBackground
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: theme.backgroundTop }
            GradientStop { position: 1.0; color: theme.backgroundBottom }
        }
    }

    Component.onCompleted: window.applyDisplayMode(settingsManager.displayMode)

    onCurrentPageChanged: {
        if (currentPage === "Settings")
            Qt.callLater(function() { appearanceNavigationButton.forceActiveFocus() })
    }

    onWidthChanged: console.info("[Settings Geometry] window width:", width)
    onHeightChanged: console.info("[Settings Geometry] window height:", height)

    property string preFullscreenMode: settingsManager.restoreDisplayMode

    function applyDisplayMode(mode) {
        // Apply the preferred geometry before changing the window state.
        if (window.visibility === Window.Windowed)
            window.applyPreferredPresentationSize()

        if (mode === "fullscreen") {
            window.showFullScreen()
        } else if (mode === "maximized") {
            window.showMaximized()
        } else {
            window.showNormal()
            window.applyPreferredPresentationSize()
        }
    }

    function toggleFullscreen() {
        if (window.visibility === Window.FullScreen) {
            const restoreMode = window.preFullscreenMode
            settingsManager.setRestoreDisplayMode(restoreMode)
            settingsManager.setDisplayMode(restoreMode)
            window.applyDisplayMode(restoreMode)
        } else {
            window.preFullscreenMode = window.visibility === Window.Maximized
                ? "maximized" : "windowed"
            settingsManager.setRestoreDisplayMode(preFullscreenMode)
            settingsManager.setDisplayMode("fullscreen")
            window.applyDisplayMode("fullscreen")
        }
    }

    onVisibilityChanged: function(visibility) {
        if (visibility === Window.FullScreen) {
            // Preserve the last non-fullscreen preference when fullscreen is
            // entered by the compositor or another window-manager action.
            if (settingsManager.displayMode !== "fullscreen") {
                preFullscreenMode = settingsManager.displayMode === "maximized"
                    ? "maximized" : "windowed"
                settingsManager.setRestoreDisplayMode(preFullscreenMode)
                settingsManager.setDisplayMode("fullscreen")
            }
        } else if (visibility === Window.Maximized) {
            preFullscreenMode = "maximized"
            if (settingsManager.displayMode !== "maximized")
                settingsManager.setDisplayMode("maximized")
        } else if (visibility === Window.Windowed) {
            preFullscreenMode = "windowed"
            if (settingsManager.displayMode !== "windowed")
                settingsManager.setDisplayMode("windowed")
        }
    }

    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: window.toggleFullscreen()
    }

    // Ctrl+Q opens the in-app quit confirmation, so keyboard users can exit
    // without relying on the terminal or the window manager close button.
    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: {
            window.systemMenuClosing = false
            window.quitConfirmationVisible = true
            window.systemMenuVisible = true
            Qt.callLater(function() { cancelQuitButton.forceActiveFocus() })
        }
    }

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
    property string pendingNavigationPage: ""
    property bool navigationTransitionRunning: false
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
        target: controllerManager

        function onConnectedChanged() {
            if (controllerManager.connected) {
                window.activeInputDevice = "controller"
                console.info("[Input] Controller available:", controllerManager.controllerName)
            } else {
                window.activeInputDevice = "keyboard"
                console.info("[Input] No controller connected; using keyboard hints")
            }
        }
        function onNavigationRequested(direction) {
            window.activeInputDevice = "controller"
            window.controllerNavigate(direction)
        }
        function onSelectRequested() {
            window.controllerSelect()
        }
        function onBackRequested() {
            window.controllerBack()
        }
        function onMenuRequested() {
            window.activeInputDevice = "controller"
            if (window.systemMenuVisible)
                window.closeSystemMenu()
            else
                window.systemMenuVisible = true
            window.systemMenuClosing = false
            window.quitConfirmationVisible = false
            if (window.systemMenuVisible)
                Qt.callLater(function() { resumeMenuButton.forceActiveFocus() })
        }
        function onPreviousSectionRequested() {
            window.activeInputDevice = "controller"
            if (!gameDetailsPopup.visible)
                window.navigateToPage("Library")
        }
        function onNextSectionRequested() {
            window.activeInputDevice = "controller"
            if (!gameDetailsPopup.visible)
                window.navigateToPage("Settings")
        }
    }

    // Window-scoped shortcuts keep library navigation working even when
    // the pointer is over a card or no QML item currently owns keyboard focus.
    // Application-scoped shortcuts are intentional here: the library is
    // composed of nested visual items and card MouseAreas, none of which
    // should be required to own keyboard focus for navigation to work.
    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
            && settingsManager.libraryView !== "list"
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("left")
        }
    }
    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
            && settingsManager.libraryView !== "list"
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("right")
        }
    }
    Shortcut {
        sequence: "Up"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("up")
        }
    }
    Shortcut {
        sequence: "Down"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("down")
        }
    }
    Shortcut {
        sequence: "Return"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
        onActivated: window.controllerSelect("keyboard")
    }
    Shortcut {
        sequence: "Enter"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
        onActivated: window.controllerSelect("keyboard")
    }

    Connections {
        target: settingsManager
        function onThemeNameChanged() {
            pendingThemeName = settingsManager.themeName
            console.info("[Settings QML] Applied theme changed:", settingsManager.themeName)
        }
        function onDisplayModeChanged() {
            displayModeSelector.currentIndex =
                displayModeSelector.indexOfValue(settingsManager.displayMode)
        }
    }

    function navigateToPage(pageName) {
        if (pageName === currentPage || navigationTransitionRunning)
            return

        pendingNavigationPage = pageName
        pageTransitionSnapshot.opacity = 1
        navigationTransitionRunning = true
        pageTransitionSnapshot.scheduleUpdate()
        pageNavigationAnimation.start()
    }

    SequentialAnimation {
        id: pageNavigationAnimation
        PauseAnimation { duration: 40 }
        ScriptAction {
            script: window.currentPage = window.pendingNavigationPage
        }
        NumberAnimation {
            target: pageTransitionSnapshot
            property: "opacity"
            from: 1
            to: 0
            duration: 420
            easing.type: Easing.OutCubic
        }
        ScriptAction {
            script: {
                pageTransitionSnapshot.opacity = 1
                window.navigationTransitionRunning = false
            }
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
                ? libraryScanner.lastError + "\nChecking metadata for discovered games..."
                : gamesFound + " new game(s) found. Checking metadata..."
            // A problem with one platform folder must not block metadata for
            // games successfully discovered on other platforms.
            metadataMatcher.scrapeLibrary()
        }
    }

    Connections {
        target: screenshotManager
        function onRefreshFinished(downloaded, withoutScreenshots, failed) {
            window.libraryRevision++
            window.scanMessage = "Screenshots refreshed: " + downloaded
                + " downloaded, " + withoutScreenshots + " without screenshots, "
                + failed + " failed."
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
        property int screenshotIndex: 0
        property int previousScreenshotIndex: -1
        property bool screenshotLayerA: true
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
            if (primaryProject && !releaseState.loading
                    && !releaseState.release && releaseState.error.length === 0)
                window.checkProjectRelease(primaryProject)
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
                text: "Back to Library"
                icon.source: "qrc:/icons/back.svg"
                icon.width: 18
                icon.height: 18
                icon.color: theme.textPrimary
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
                    else if (status === Image.Ready)
                        console.info("[Details Screenshots] Image ready:", source, sourceSize.width, "x", sourceSize.height)
                }
                opacity: visible && gameDetailsPopup.screenshotLayerA
                    && status === Image.Ready ? 1.0 : 0
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
                    else if (status === Image.Ready)
                        console.info("[Details Screenshots] Image ready:", source, sourceSize.width, "x", sourceSize.height)
                }
                opacity: visible && !gameDetailsPopup.screenshotLayerA
                    && status === Image.Ready ? 1.0 : 0
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
                opacity: status === Image.Ready ? 0.62 : 0
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
                border.color: theme.textPrimary
                border.width: 2
                clip: true

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: Math.max(0, parent.radius - 2)
                    color: "transparent"
                    border.color: theme.accent
                    border.width: 1
                }

                Image {
                    anchors.fill: parent
                    anchors.margins: 5
                    source: window.selectedGame ? window.selectedGame.artworkPath : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: !window.selectedGame || window.selectedGame.artworkPath.length === 0
                    spacing: 8
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/icons/artwork-placeholder.svg"
                        sourceSize.width: 42
                        sourceSize.height: 42
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        opacity: 0.65
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
                                font.pixelSize: 19 * theme.fontScale
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
                                        implicitHeight: 48
                                        Layout.minimumWidth: 150
                                        Layout.preferredWidth: 150
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
                                        implicitHeight: 48
                                        Layout.minimumWidth: 150
                                        Layout.preferredWidth: 150
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
                                            text: gameDetailsPopup.primaryProject
                                                ? (gameDetailsPopup.primaryProject.developmentStatus === "beta"
                                                    ? "Install this BETA launcher?"
                                                    : gameDetailsPopup.primaryProject.developmentStatus === "alpha"
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
                                                    window.startAssetDownload(gameDetailsPopup.primaryProject, asset)
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


            }
            }
        }
    }

    Item {
        id: mainNavigationRow
        anchors.fill: parent

        Keys.onPressed: function(event) {
            if (!event.isAutoRepeat)
                window.activeInputDevice = "keyboard"
        }

        HoverHandler {
            acceptedDevices: PointerDevice.Mouse
            onPointChanged: window.activeInputDevice = "mouse"
        }

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
                    GradientStop { position: 0.52; color: Qt.lighter(theme.windowBackground, 1.12) }
                    GradientStop { position: 1.0; color: theme.backgroundBottom }
                }
            }

            // A single full-window image avoids the mismatched crop visible
            // around the edges when separate fit/crop layers are combined.
            Image {
                anchors.fill: parent
                visible: currentPage === "Library"
                    && settingsManager.libraryBackgroundType === "custom"
                    && settingsManager.customLibraryBackground.length > 0
                source: settingsManager.customLibraryBackground
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                mipmap: true
            }

            // Keep the wallpaper vivid while gently separating it from cards and text.
            Rectangle {
                anchors.fill: parent
                visible: currentPage === "Library"
                    && settingsManager.libraryBackgroundType === "custom"
                    && settingsManager.customLibraryBackground.length > 0
                color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g,
                               theme.windowBackground.b, 0.14)
            }

            Rectangle {
                anchors.fill: parent
                visible: currentPage === "Library"
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.055) }
                    GradientStop { position: 0.42; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.035) }
                }
            }

            // The wave artwork is rasterized once by Qt and then animated as
            // a textured scene-graph item. No JavaScript painting or frame timer.
            Item {
                id: libraryWaveLayer
                anchors.fill: parent
                visible: currentPage === "Library"
                    && settingsManager.libraryBackgroundType === "animated"
                clip: true

                Image {
                    id: libraryWaveImage
                    width: parent.width * 1.22
                    height: parent.height * 1.05
                    x: (parent.width - width) / 2 + parent.width * 0.07
                    y: (parent.height - height) / 2
                    source: libraryVisualSettings.wavePalette === "violet"
                        ? "qrc:/qt/qml/RecompStation/assets/library-waves-violet.svg"
                        : libraryVisualSettings.wavePalette === "emerald"
                            ? "qrc:/qt/qml/RecompStation/assets/library-waves-emerald.svg"
                            : libraryVisualSettings.wavePalette === "ember"
                                ? "qrc:/qt/qml/RecompStation/assets/library-waves-ember.svg"
                                : "qrc:/qt/qml/RecompStation/assets/library-waves.svg"
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    smooth: true
                    mipmap: true
                    opacity: 0.78

                    SequentialAnimation {
                        running: libraryWaveLayer.visible && !settingsManager.reducedMotion
                            && window.active && window.visibility !== Window.Minimized
                        loops: Animation.Infinite
                        NumberAnimation { target: libraryWaveImage; property: "x"; to: (libraryWaveLayer.width - libraryWaveImage.width) / 2 - libraryWaveLayer.width * 0.07; duration: 22000; easing.type: Easing.InOutSine }
                        NumberAnimation { target: libraryWaveImage; property: "x"; to: (libraryWaveLayer.width - libraryWaveImage.width) / 2 + libraryWaveLayer.width * 0.07; duration: 22000; easing.type: Easing.InOutSine }
                    }
                }

                Image {
                    id: libraryWaveImageSecondary
                    width: parent.width * 1.22
                    height: parent.height * 1.05
                    x: (parent.width - width) / 2 - parent.width * 0.07
                    y: (parent.height - height) / 2
                    source: libraryVisualSettings.wavePalette === "violet"
                        ? "qrc:/qt/qml/RecompStation/assets/library-waves-violet.svg"
                        : libraryVisualSettings.wavePalette === "emerald"
                            ? "qrc:/qt/qml/RecompStation/assets/library-waves-emerald.svg"
                            : libraryVisualSettings.wavePalette === "ember"
                                ? "qrc:/qt/qml/RecompStation/assets/library-waves-ember.svg"
                                : "qrc:/qt/qml/RecompStation/assets/library-waves.svg"
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    smooth: true
                    mipmap: true
                    opacity: 0.24
                    rotation: 180

                    SequentialAnimation {
                        running: libraryWaveLayer.visible && !settingsManager.reducedMotion
                            && window.active && window.visibility !== Window.Minimized
                        loops: Animation.Infinite
                        NumberAnimation { target: libraryWaveImageSecondary; property: "x"; to: (libraryWaveLayer.width - libraryWaveImageSecondary.width) / 2 + libraryWaveLayer.width * 0.07; duration: 28000; easing.type: Easing.InOutSine }
                        NumberAnimation { target: libraryWaveImageSecondary; property: "x"; to: (libraryWaveLayer.width - libraryWaveImageSecondary.width) / 2 - libraryWaveLayer.width * 0.07; duration: 28000; easing.type: Easing.InOutSine }
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

                            RowLayout {
                                spacing: 8
                                ComboBox {
                                    Layout.preferredWidth: 190
                                    Layout.preferredHeight: 42
                                    model: ["All Platforms"].concat(platformManager.platforms())
                                    currentIndex: Math.max(0, model.indexOf(window.selectedPlatform))
                                    onActivated: window.selectedPlatform = currentText
                                }
                                ComboBox {
                                    Layout.preferredWidth: 132
                                    Layout.preferredHeight: 42
                                    textRole: "label"
                                    valueRole: "value"
                                    model: [{ label: "Grid View", value: "grid" }, { label: "List View", value: "list" }]
                                    currentIndex: Math.max(0, indexOfValue(settingsManager.libraryView))
                                    onActivated: settingsManager.libraryView = currentValue
                                }
                            }
                        }

                        GridView {
                            id: gameGrid
                            visible: settingsManager.libraryView === "grid"
                            Layout.fillWidth: true
                            Layout.fillHeight: gameGrid.count > 0 && settingsManager.libraryView === "grid"
                            Layout.preferredHeight: gameGrid.count > 0 && settingsManager.libraryView === "grid" ? 760 : 0
                            clip: true
                            readonly property int preferredColumns: settingsManager.libraryDensity === "compact" ? 10
                                : settingsManager.libraryDensity === "spacious" ? 6 : 8
                            cellWidth: Math.max(180, Math.min(480,
                                Math.floor(width / Math.max(1, preferredColumns))))
                            cellHeight: Math.round(cellWidth * 1.35)
                            // Window-level shortcuts own directional movement so
                            // keyboard input cannot be processed twice by the view.
                            keyNavigationEnabled: false
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
                                required property var screenshots
                                required property int id
                                required property int index
                                property int gameId: id
                                readonly property bool highlighted:
                                    tileMouse.containsMouse || gameTile.GridView.isCurrentItem
                                function openDetails() {
                                    window.selectedGame = {
                                        id: gameTile.gameId,
                                        title: gameTile.title,
                                        platform: gameTile.platform,
                                        path: gameTile.path,
                                        artworkPath: gameTile.artworkPath,
                                        screenshots: gameTile.screenshots,
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
                                    anchors.margins: -4
                                    radius: theme.radiusCard + 4
                                    color: theme.accent
                                    opacity: gameTile.GridView.isCurrentItem ? 0.16
                                        : tileMouse.containsMouse ? 0.07 : 0
                                    Behavior on opacity {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radiusCard
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0.0
                                            color: gameTile.highlighted ? theme.elevatedSurface
                                                : Qt.rgba(theme.surface.r, theme.surface.g,
                                                          theme.surface.b, 0.46)
                                        }
                                        GradientStop {
                                            position: 1.0
                                            color: gameTile.highlighted ? theme.surface
                                                : Qt.rgba(theme.surface.r, theme.surface.g,
                                                          theme.surface.b, 0.38)
                                        }
                                    }
                                    Behavior on opacity {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
                                    }
                                    border.width: gameTile.highlighted ? 2 : 1
                                    border.color: gameTile.highlighted ? theme.accent
                                        : Qt.rgba(theme.divider.r, theme.divider.g,
                                                  theme.divider.b, 0.24)
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
                                            border.width: 1
                                            border.color: gameTile.highlighted
                                                ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.78)
                                                : Qt.rgba(theme.divider.r, theme.divider.g,
                                                          theme.divider.b, 0.34)
                                            Behavior on border.color {
                                                enabled: !settingsManager.reducedMotion
                                                ColorAnimation { duration: 170; easing.type: Easing.OutCubic }
                                            }
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
                                                Image {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    source: "qrc:/icons/artwork-placeholder.svg"
                                                    sourceSize.width: 38
                                                    sourceSize.height: 38
                                                    width: 38
                                                    height: 38
                                                    opacity: 0.65
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
                                            Rectangle {
                                                visible: gameTile.metadataTitle.length > 0
                                                radius: 5
                                                color: Qt.rgba(theme.success.r, theme.success.g,
                                                               theme.success.b, 0.12)
                                                border.width: 1
                                                border.color: Qt.rgba(theme.success.r, theme.success.g,
                                                                      theme.success.b, 0.24)
                                                implicitWidth: metadataTagLabel.implicitWidth + 14
                                                implicitHeight: 22

                                                Label {
                                                    id: metadataTagLabel
                                                    anchors.centerIn: parent
                                                    text: "METADATA"
                                                    color: theme.success
                                                    font.pixelSize: 9 * theme.fontScale
                                                    font.bold: true
                                                    font.letterSpacing: 0.5
                                                }
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: tileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        gameGrid.currentIndex = gameTile.index
                                        window.activeInputDevice = "mouse"
                                    }
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

                        ListView {
                            id: gameList
                            Layout.fillWidth: true
                            Layout.fillHeight: settingsManager.libraryView === "list" && gameGrid.count > 0
                            Layout.preferredHeight: settingsManager.libraryView === "list" && gameGrid.count > 0 ? 760 : 0
                            visible: settingsManager.libraryView === "list"
                            clip: true
                            spacing: 9
                            model: gameGrid.model
                            currentIndex: -1
                            Component.onCompleted: {
                                currentIndex = gameGrid.currentIndex >= 0
                                    ? gameGrid.currentIndex : (count > 0 ? 0 : -1)
                            }
                            onCurrentIndexChanged: {
                                if (currentIndex !== gameGrid.currentIndex)
                                    gameGrid.currentIndex = currentIndex
                            }
                            Connections {
                                target: gameGrid
                                function onCurrentIndexChanged() {
                                    if (gameList.currentIndex !== gameGrid.currentIndex)
                                        gameList.currentIndex = gameGrid.currentIndex
                                }
                            }
                            keyNavigationEnabled: false
                            delegate: Item {
                                id: listTile
                                required property string title
                                required property string platform
                                required property string artworkPath
                                required property string metadataTitle
                                required property string metadataOverview
                                required property string metadataReleaseDate
                                required property string metadataRating
                                required property var screenshots
                                required property string path
                                required property int id
                                required property int index
                                width: gameList.width
                                height: 112
                                focus: ListView.isCurrentItem
                                readonly property bool highlighted:
                                    ListView.isCurrentItem || listTileMouse.containsMouse
                                function openDetails() {
                                    window.selectedGame = { id: listTile.id, title: listTile.title,
                                        platform: listTile.platform, path: listTile.path,
                                        artworkPath: listTile.artworkPath, screenshots: listTile.screenshots,
                                        metadataTitle: listTile.metadataTitle,
                                        metadataOverview: listTile.metadataOverview,
                                        metadataReleaseDate: listTile.metadataReleaseDate,
                                        metadataRating: listTile.metadataRating }
                                    gameDetailsPopup.open()
                                }
                                Keys.onReturnPressed: openDetails()
                                Keys.onEnterPressed: openDetails()
                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radiusCard
                                    color: listTile.highlighted ? theme.elevatedSurface : theme.surface
                                    border.width: listTile.highlighted ? 2 : 1
                                    border.color: listTile.highlighted ? theme.accent
                                        : Qt.rgba(theme.divider.r, theme.divider.g,
                                                  theme.divider.b, 0.42)
                                    Behavior on border.color {
                                        enabled: !settingsManager.reducedMotion
                                        ColorAnimation { duration: 170; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on border.width {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
                                    }
                                }
                                Image {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 76
                                    height: 88
                                    source: listTile.artworkPath
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                }
                                ColumnLayout {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 104
                                    anchors.right: parent.right
                                    anchors.rightMargin: 18
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5
                                    Label { Layout.fillWidth: true; text: listTile.title; color: theme.textPrimary; font.pixelSize: 15 * theme.fontScale; font.bold: true; elide: Text.ElideRight }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: listTile.platform; color: theme.textSecondary; font.pixelSize: 11 * theme.fontScale }
                                        Rectangle { width: 5; height: 5; radius: 3; color: theme.textMuted }
                                        Label { text: listTile.metadataTitle.length > 0 ? "Metadata available" : "Metadata pending"; color: theme.textMuted; font.pixelSize: 11 * theme.fontScale }
                                    }
                                }
                                MouseArea {
                                    id: listTileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        gameList.currentIndex = listTile.index
                                        window.activeInputDevice = "mouse"
                                    }
                                    onClicked: {
                                        gameGrid.currentIndex = listTile.index
                                        listTile.openDetails()
                                    }
                                }
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

                        Rectangle {
                            id: settingsNavigationPanel
                            Layout.minimumWidth: 248
                            Layout.maximumWidth: 248
                            Layout.preferredWidth: 248
                            Layout.fillHeight: true
                            radius: theme.radiusLarge
                            color: theme.sidebarBackground
                            border.color: theme.divider

                            ColumnLayout {
                                id: settingsNavigationColumn
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 14

                                Label {
                                    text: "PREFERENCES"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1.2
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Configure your RecompStation experience."
                                    color: theme.textSecondary
                                    font.pixelSize: 12 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: theme.divider
                                }

                                Button {
                                    activeFocusOnTab: true
                                    focusPolicy: Qt.StrongFocus
                                    id: appearanceNavigationButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 52
                                    padding: 10
                                    highlighted: settingsPage.section === "Appearance"
                                    onClicked: settingsPage.section = "Appearance"
                                    background: Rectangle {
                                        radius: theme.radiusCard
                                        color: appearanceNavigationButton.highlighted
                                            ? theme.elevatedSurface
                                            : appearanceNavigationButton.hovered
                                                ? Qt.lighter(theme.sidebarBackground, 1.08)
                                                : "transparent"
                                        border.width: appearanceNavigationButton.highlighted ? 1 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: RowLayout {
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.fillHeight: true
                                            radius: 2
                                            color: appearanceNavigationButton.highlighted
                                                ? theme.accent : "transparent"
                                        }
                                        Image {
                                            source: "qrc:/icons/appearance.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            Layout.preferredWidth: 20
                                            Layout.preferredHeight: 20
                                            opacity: appearanceNavigationButton.highlighted ? 1.0 : 0.78
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Appearance"
                                            color: appearanceNavigationButton.highlighted
                                                ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: appearanceNavigationButton.highlighted
                                        }
                                    }
                                }
                                Button {
                                    activeFocusOnTab: true
                                    focusPolicy: Qt.StrongFocus
                                    id: displayNavigationButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 52
                                    padding: 10
                                    highlighted: settingsPage.section === "Display"
                                    onClicked: settingsPage.section = "Display"
                                    background: Rectangle {
                                        radius: theme.radiusCard
                                        color: displayNavigationButton.highlighted
                                            ? theme.elevatedSurface
                                            : displayNavigationButton.hovered
                                                ? Qt.lighter(theme.sidebarBackground, 1.08)
                                                : "transparent"
                                        border.width: displayNavigationButton.highlighted ? 1 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: RowLayout {
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.fillHeight: true
                                            radius: 2
                                            color: displayNavigationButton.highlighted
                                                ? theme.accent : "transparent"
                                        }
                                        Image {
                                            source: "qrc:/icons/display.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            Layout.preferredWidth: 20
                                            Layout.preferredHeight: 20
                                            opacity: displayNavigationButton.highlighted ? 1.0 : 0.78
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Display"
                                            color: displayNavigationButton.highlighted
                                                ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: displayNavigationButton.highlighted
                                        }
                                    }
                                }
                                Button {
                                    activeFocusOnTab: true
                                    focusPolicy: Qt.StrongFocus
                                    id: libraryNavigationButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 52
                                    padding: 10
                                    highlighted: settingsPage.section === "Library"
                                    onClicked: settingsPage.section = "Library"
                                    background: Rectangle {
                                        radius: theme.radiusCard
                                        color: libraryNavigationButton.highlighted
                                            ? theme.elevatedSurface
                                            : libraryNavigationButton.hovered
                                                ? Qt.lighter(theme.sidebarBackground, 1.08)
                                                : "transparent"
                                        border.width: libraryNavigationButton.highlighted ? 1 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: RowLayout {
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.fillHeight: true
                                            radius: 2
                                            color: libraryNavigationButton.highlighted
                                                ? theme.accent : "transparent"
                                        }
                                        Image {
                                            source: "qrc:/icons/library.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            Layout.preferredWidth: 20
                                            Layout.preferredHeight: 20
                                            opacity: libraryNavigationButton.highlighted ? 1.0 : 0.78
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Library"
                                            color: libraryNavigationButton.highlighted
                                                ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: libraryNavigationButton.highlighted
                                        }
                                    }
                                }
                                Button {
                                    activeFocusOnTab: true
                                    focusPolicy: Qt.StrongFocus
                                    id: metadataNavigationButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 52
                                    padding: 10
                                    highlighted: settingsPage.section === "Metadata"
                                    onClicked: settingsPage.section = "Metadata"
                                    background: Rectangle {
                                        radius: theme.radiusCard
                                        color: metadataNavigationButton.highlighted
                                            ? theme.elevatedSurface
                                            : metadataNavigationButton.hovered
                                                ? Qt.lighter(theme.sidebarBackground, 1.08)
                                                : "transparent"
                                        border.width: metadataNavigationButton.highlighted ? 1 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: RowLayout {
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.fillHeight: true
                                            radius: 2
                                            color: metadataNavigationButton.highlighted
                                                ? theme.accent : "transparent"
                                        }
                                        Image {
                                            source: "qrc:/icons/metadata.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            Layout.preferredWidth: 20
                                            Layout.preferredHeight: 20
                                            opacity: metadataNavigationButton.highlighted ? 1.0 : 0.78
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Metadata"
                                            color: metadataNavigationButton.highlighted
                                                ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: metadataNavigationButton.highlighted
                                        }
                                    }
                                }
                                Button {
                                    activeFocusOnTab: true
                                    focusPolicy: Qt.StrongFocus
                                    id: controllerNavigationButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 52
                                    padding: 10
                                    highlighted: settingsPage.section === "Controller"
                                    onClicked: settingsPage.section = "Controller"
                                    background: Rectangle {
                                        radius: theme.radiusCard
                                        color: controllerNavigationButton.highlighted
                                            ? theme.elevatedSurface
                                            : controllerNavigationButton.hovered
                                                ? Qt.lighter(theme.sidebarBackground, 1.08)
                                                : "transparent"
                                        border.width: controllerNavigationButton.highlighted ? 1 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: RowLayout {
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.fillHeight: true
                                            radius: 2
                                            color: controllerNavigationButton.highlighted
                                                ? theme.accent : "transparent"
                                        }
                                        Image {
                                            source: "qrc:/icons/gamepad.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            Layout.preferredWidth: 20
                                            Layout.preferredHeight: 20
                                            opacity: controllerNavigationButton.highlighted ? 1.0 : 0.78
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Controller"
                                            color: controllerNavigationButton.highlighted
                                                ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: controllerNavigationButton.highlighted
                                        }
                                    }
                                }

                                Button {
                                    activeFocusOnTab: true
                                    focusPolicy: Qt.StrongFocus
                                    id: applicationNavigationButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 52
                                    padding: 10
                                    highlighted: settingsPage.section === "Application"
                                    onClicked: settingsPage.section = "Application"
                                    background: Rectangle {
                                        radius: theme.radiusCard
                                        color: applicationNavigationButton.highlighted
                                            ? theme.elevatedSurface
                                            : applicationNavigationButton.hovered
                                                ? Qt.lighter(theme.sidebarBackground, 1.08)
                                                : "transparent"
                                        border.width: applicationNavigationButton.highlighted ? 1 : 0
                                        border.color: theme.accent
                                    }
                                    contentItem: RowLayout {
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 3
                                            Layout.fillHeight: true
                                            radius: 2
                                            color: applicationNavigationButton.highlighted
                                                ? theme.accent : "transparent"
                                        }
                                        Image {
                                            source: "qrc:/icons/application.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            Layout.preferredWidth: 20
                                            Layout.preferredHeight: 20
                                            opacity: applicationNavigationButton.highlighted ? 1.0 : 0.78
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Application"
                                            color: applicationNavigationButton.highlighted
                                                ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 14 * theme.fontScale
                                            font.bold: applicationNavigationButton.highlighted
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }
                                Label {
                                    Layout.fillWidth: true
                                    text: "More preferences will appear here as features are added."
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
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

                            ScrollView {
                                id: appearanceContent
                                anchors.fill: parent
                                anchors.margins: 30
                                clip: true
                                visible: settingsPage.section === "Appearance"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 16
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
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 76
                                        padding: 0
                                        onClicked: pendingThemeName = modelData.id

                                        background: Rectangle {
                                            radius: theme.radiusCard
                                            color: themePresetButton.hovered
                                                ? theme.elevatedSurface : theme.sidebarBackground
                                            border.width: themePresetButton.activeFocus ? 3
                                                : pendingThemeName === modelData.id ? 2 : 1
                                            border.color: themePresetButton.activeFocus
                                                || pendingThemeName === modelData.id
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
                                        id: applyThemeButton
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        Layout.preferredWidth: 180
                                        Layout.preferredHeight: 44
                                        text: "Apply Theme"
                                        enabled: pendingThemeName !== settingsManager.themeName
                                        highlighted: enabled
                                        background: Rectangle {
                                            radius: theme.radiusCard
                                            color: !applyThemeButton.enabled
                                                ? theme.sidebarBackground
                                                : applyThemeButton.activeFocus
                                                    ? theme.elevatedSurface
                                                    : applyThemeButton.hovered
                                                        ? Qt.lighter(theme.accent, 1.08)
                                                        : theme.accent
                                            border.width: applyThemeButton.activeFocus ? 3 : 1
                                            border.color: applyThemeButton.activeFocus
                                                ? theme.textPrimary : theme.accent
                                        }
                                        contentItem: Label {
                                            text: applyThemeButton.text
                                            color: applyThemeButton.enabled
                                                ? theme.windowBackground : theme.textMuted
                                            font.pixelSize: 13 * theme.fontScale
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
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
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                            border.color: parent.activeFocus ? theme.accent : theme.divider
                                            border.width: parent.activeFocus ? 2 : 1
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        id: fontSizeSelector
                                        Layout.preferredWidth: 170
                                        Layout.preferredHeight: 44
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
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"
                                            border.color: parent.activeFocus ? theme.accent : "transparent"
                                            border.width: parent.activeFocus ? 2 : 0
                                            anchors.margins: -6
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
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
                            }

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 30
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
                                        text: "WINDOW MODE"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    ComboBox {
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                            border.color: parent.activeFocus ? theme.accent : theme.divider
                                            border.width: parent.activeFocus ? 2 : 1
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        id: displayModeSelector
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
                                        textRole: "label"
                                        valueRole: "value"
                                        model: [
                                            { label: "Windowed", value: "windowed" },
                                            { label: "Maximized", value: "maximized" },
                                            { label: "Fullscreen", value: "fullscreen" }
                                        ]
                                        Component.onCompleted: {
                                            const savedIndex = indexOfValue(settingsManager.displayMode)
                                            currentIndex = savedIndex >= 0 ? savedIndex : 0
                                        }
                                        onActivated: {
                                            if (currentValue === "fullscreen"
                                                    && window.visibility !== Window.FullScreen) {
                                                window.preFullscreenMode =
                                                    window.visibility === Window.Maximized
                                                    ? "maximized" : "windowed"
                                                settingsManager.setRestoreDisplayMode(
                                                    window.preFullscreenMode)
                                            }
                                            settingsManager.setDisplayMode(currentValue)
                                            window.applyDisplayMode(currentValue)
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Fullscreen hides the desktop window frame without changing your monitor resolution. Press F11 to toggle fullscreen and return to your previous window mode."
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        text: "PREFERRED WINDOW SIZE"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    ComboBox {
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                            border.color: parent.activeFocus ? theme.accent : theme.divider
                                            border.width: parent.activeFocus ? 2 : 1
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        id: presentationSizeSelector
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
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
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                    : (parent.highlighted ? theme.accent : theme.surface)
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.preferredWidth: 190
                                            Layout.preferredHeight: 44
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
                                anchors.margins: 30
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
                                    Label {
                                        text: "LIBRARY PRESENTATION"
                                        color: theme.textMuted; font.pixelSize: 10 * theme.fontScale
                                        font.bold: true; font.letterSpacing: 1
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label { text: "Layout"; color: theme.textPrimary; font.pixelSize: 13 * theme.fontScale; font.bold: true }
                                            Label { text: "Choose how your collection is presented."; color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap }
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.preferredWidth: 190
                                            textRole: "label"; valueRole: "value"
                                            model: [{ label: "Grid View", value: "grid" }, { label: "List View", value: "list" }]
                                            currentIndex: Math.max(0, indexOfValue(settingsManager.libraryView))
                                            onActivated: settingsManager.libraryView = currentValue
                                        }
                                    }
                                    RowLayout {
                                        visible: settingsManager.libraryView === "grid"
                                        Layout.fillWidth: true
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label { text: "Grid density"; color: theme.textPrimary; font.pixelSize: 13 * theme.fontScale; font.bold: true }
                                            Label { text: "Curated sizes adapt to your available screen width."; color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap }
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.preferredWidth: 190
                                            textRole: "label"; valueRole: "value"
                                            model: [{ label: "Compact", value: "compact" }, { label: "Comfortable", value: "comfortable" }, { label: "Spacious", value: "spacious" }]
                                            currentIndex: Math.max(0, indexOfValue(settingsManager.libraryDensity))
                                            onActivated: settingsManager.libraryDensity = currentValue
                                        }
                                    }
                                    Label {
                                        text: "LIBRARY BACKGROUND"
                                        color: theme.textMuted; font.pixelSize: 10 * theme.fontScale
                                        font.bold: true; font.letterSpacing: 1
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label { text: "Background treatment"; color: theme.textPrimary; font.pixelSize: 13 * theme.fontScale; font.bold: true }
                                            Label { text: "Keep the collection backdrop coordinated with your active theme."; color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap }
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.preferredWidth: 210
                                            textRole: "label"; valueRole: "value"
                                            model: [
                                                { label: "Static theme", value: "static" },
                                                { label: "Animated ambience", value: "animated" },
                                                { label: "Custom image", value: "custom" }
                                            ]
                                            currentIndex: Math.max(0, indexOfValue(settingsManager.libraryBackgroundType))
                                            onActivated: settingsManager.libraryBackgroundType = currentValue
                                        }
                                    }
                                    RowLayout {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                        Layout.fillWidth: true
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label { text: "Wave color"; color: theme.textPrimary; font.pixelSize: 13 * theme.fontScale; font.bold: true }
                                            Label { text: "Choose the ambient color independently of your application theme."; color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap }
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.preferredWidth: 210
                                            textRole: "label"; valueRole: "value"
                                            model: [
                                                { label: "Ocean Blue", value: "ocean" },
                                                { label: "Violet", value: "violet" },
                                                { label: "Emerald", value: "emerald" },
                                                { label: "Ember", value: "ember" }
                                            ]
                                            currentIndex: Math.max(0, indexOfValue(libraryVisualSettings.wavePalette))
                                            onActivated: libraryVisualSettings.wavePalette = currentValue
                                        }
                                    }
                                    RowLayout {
                                        visible: settingsManager.libraryBackgroundType === "custom"
                                        Layout.fillWidth: true
                                        spacing: 10
                                        Button {
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                    : (parent.highlighted ? theme.accent : theme.surface)
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            text: settingsManager.customLibraryBackground.length > 0
                                                ? "Choose another image" : "Choose image"
                                            onClicked: libraryBackgroundFileDialog.open()
                                        }
                                        Button {
                                            background: Rectangle {
                                                radius: 8
                                                color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                    : (parent.highlighted ? theme.accent : theme.surface)
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                            visible: settingsManager.customLibraryBackground.length > 0
                                            text: "Clear"
                                            onClicked: settingsManager.setCustomLibraryBackground("")
                                        }
                                    }
                                    Label {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                        Layout.fillWidth: true
                                        text: settingsManager.reducedMotion
                                            ? "Ambient wave motion is paused while Reduced Motion is enabled."
                                            : "Layered, flowing light ribbons create a console-hub atmosphere. Animation pauses when the app is inactive or minimized."
                                        color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
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
                                                background: Rectangle {
                                                    radius: 8
                                                    color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"
                                                    border.color: parent.activeFocus ? theme.accent : "transparent"
                                                    border.width: parent.activeFocus ? 2 : 0
                                                    anchors.margins: -6
                                                }
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                checked: settingsManager.scanOnStartup
                                                onToggled: settingsManager.setScanOnStartup(checked)
                                            }
                                        }
                                    }
                                    Button {
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                : (parent.highlighted ? theme.accent : theme.surface)
                                            border.color: parent.activeFocus ? theme.accent : theme.divider
                                            border.width: parent.activeFocus ? 2 : 1
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        Layout.preferredWidth: 210
                                        Layout.preferredHeight: 44
                                        text: "Scan Library Now"; highlighted: true
                                        enabled: platformManager.libraryConfigured
                                        onClicked: { window.scanMessage = "Scanning configured game folders..."; libraryScanner.scanAll() }
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                    Label {
                                        text: "METADATA"
                                        color: theme.textMuted; font.pixelSize: 10 * theme.fontScale; font.bold: true; font.letterSpacing: 1
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Retry lookups for unmatched games using your enabled metadata providers. Existing matches are preserved, and previously checked games are not repeatedly queried during automatic scans."
                                        color: theme.textSecondary; font.pixelSize: 12 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                    Button {
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                : (parent.highlighted ? theme.accent : theme.surface)
                                            border.color: parent.activeFocus ? theme.accent : theme.divider
                                            border.width: parent.activeFocus ? 2 : 1
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        Layout.preferredWidth: 210
                                        Layout.preferredHeight: 44
                                        text: metadataMatcher.busy ? "Refreshing Metadata…" : "Refresh Metadata"
                                        highlighted: true
                                        enabled: metadataManager.configured && !metadataMatcher.busy
                                        onClicked: {
                                            window.scanMessage = "Refreshing metadata for unresolved games..."
                                            metadataMatcher.scrapeLibrary(true)
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                    Label {
                                        text: "GAMEPLAY SCREENSHOTS"
                                        color: theme.textMuted; font.pixelSize: 10 * theme.fontScale; font.bold: true; font.letterSpacing: 1
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Download gameplay screenshots for games that already have saved TheGamesDB metadata. This does not repeat metadata matching or change cover artwork."
                                        color: theme.textSecondary; font.pixelSize: 12 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                    Button {
                                        background: Rectangle {
                                            radius: 8
                                            color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                : (parent.highlighted ? theme.accent : theme.surface)
                                            border.color: parent.activeFocus ? theme.accent : theme.divider
                                            border.width: parent.activeFocus ? 2 : 1
                                        }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        Layout.preferredWidth: 210
                                        Layout.preferredHeight: 44
                                        text: screenshotManager.busy ? "Refreshing Screenshots…" : "Refresh Screenshots"
                                        highlighted: true
                                        enabled: metadataManager.configured && !screenshotManager.busy
                                        onClicked: screenshotManager.refreshLibraryScreenshots()
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: screenshotManager.status
                                        visible: screenshotManager.status.length > 0
                                        color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Place game files inside their matching platform folders. RecompStation monitors these folders and updates your library when games are added or removed."
                                        color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                }
                            }

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 30
                                clip: true
                                visible: settingsPage.section === "Application"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 16
                                Label {
                                    text: "Application"
                                    color: theme.textPrimary
                                    font.pixelSize: 27 * theme.fontScale
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Application details and local storage."
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                    wrapMode: Text.Wrap
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

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 30
                                clip: true
                                visible: settingsPage.section === "Metadata"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 18

                                    Label {
                                        text: "Metadata"
                                        color: theme.textPrimary
                                        font.pixelSize: 27 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Choose which services RecompStation can use to identify games and retrieve metadata."
                                        color: theme.textSecondary
                                        font.pixelSize: 13 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }

                                    Label {
                                        text: "AVAILABLE PROVIDERS"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: theGamesDbProviderColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: settingsManager.theGamesDbEnabled ? theme.accent : theme.divider
                                        ColumnLayout {
                                            id: theGamesDbProviderColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 8
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {

                                                    background: Rectangle {

                                                        radius: 8

                                                        color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"

                                                        border.color: parent.activeFocus ? theme.accent : "transparent"

                                                        border.width: parent.activeFocus ? 2 : 0

                                                        anchors.margins: -6

                                                    }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                                    text: "TheGamesDB"
                                                    checked: settingsManager.theGamesDbEnabled
                                                    onToggled: settingsManager.setTheGamesDbEnabled(checked)
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label {
                                                    text: !settingsManager.theGamesDbEnabled ? "Disabled"
                                                        : metadataManager.configured ? "Configured" : "API key required"
                                                    color: !settingsManager.theGamesDbEnabled ? theme.textMuted
                                                        : metadataManager.configured ? "#48C78E" : theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                    font.bold: true
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Game search, platform information, descriptions, ratings, and cover artwork. Uses the locally configured TheGamesDB API key."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Label {
                                        text: "PLANNED PROVIDERS"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: plannedProvidersColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: theme.divider
                                        ColumnLayout {
                                            id: plannedProvidersColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 12
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {

                                                    background: Rectangle {

                                                        radius: 8

                                                        color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"

                                                        border.color: parent.activeFocus ? theme.accent : "transparent"

                                                        border.width: parent.activeFocus ? 2 : 0

                                                        anchors.margins: -6

                                                    }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                                    text: "ScreenScraper"
                                                    enabled: false
                                                    checked: false
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label { text: "Not integrated"; color: theme.textMuted; font.pixelSize: 10 * theme.fontScale }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "ROM-focused identification using file hashes, with extensive game information and media."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                            Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {

                                                    background: Rectangle {

                                                        radius: 8

                                                        color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"

                                                        border.color: parent.activeFocus ? theme.accent : "transparent"

                                                        border.width: parent.activeFocus ? 2 : 0

                                                        anchors.margins: -6

                                                    }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                                    text: "Hasheous"
                                                    enabled: false
                                                    checked: false
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label { text: "Under evaluation"; color: theme.textMuted; font.pixelSize: 10 * theme.fontScale }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Potential hash-based identification and metadata enrichment provider."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                            Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {

                                                    background: Rectangle {

                                                        radius: 8

                                                        color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"

                                                        border.color: parent.activeFocus ? theme.accent : "transparent"

                                                        border.width: parent.activeFocus ? 2 : 0

                                                        anchors.margins: -6

                                                    }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                                    text: "IGDB"
                                                    enabled: false
                                                    checked: false
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label { text: "Under evaluation"; color: theme.textMuted; font.pixelSize: 10 * theme.fontScale }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Potential source for supplementary game descriptions, genres, and release information."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Only integrated providers can be enabled. Additional providers, provider priority, fallback behavior, and separate artwork sources will become configurable as integrations are added."
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 30
                                clip: true
                                visible: settingsPage.section === "Controller"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 18

                                    Label {
                                        text: "Controller"
                                        color: theme.textPrimary
                                        font.pixelSize: 27 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "View connected controller information and configure controller support."
                                        color: theme.textSecondary
                                        font.pixelSize: 13 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }

                                    Label {
                                        text: "CONNECTED DEVICE"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: controllerInfoColumn.implicitHeight + 30
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: controllerManager.connected ? theme.accent : theme.divider

                                        RowLayout {
                                            id: controllerInfoColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 16
                                            spacing: 14

                                            Image {
                                                source: "qrc:/qt/qml/RecompStation/assets/images/recompstation-controller.png"
                                                sourceSize: Qt.size(384, 256)
                                                fillMode: Image.PreserveAspectFit
                                                Layout.preferredWidth: 52
                                                Layout.preferredHeight: 36
                                                smooth: true
                                                mipmap: true
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 5
                                                Label {
                                                    text: controllerManager.connected
                                                        ? controllerManager.controllerName : "No controller connected"
                                                    color: theme.textPrimary
                                                    font.pixelSize: 14 * theme.fontScale
                                                    font.bold: true
                                                }
                                                Label {
                                                    text: controllerManager.connected
                                                        ? controllerManager.controllerType.toUpperCase() + " controller"
                                                        : controllerManager.available
                                                            ? "Connect a supported gamepad to use controller navigation."
                                                            : "Controller support is unavailable in this build."
                                                    color: theme.textSecondary
                                                    font.pixelSize: 11 * theme.fontScale
                                                    wrapMode: Text.Wrap
                                                    Layout.fillWidth: true
                                                }
                                            }
                                            Rectangle {
                                                radius: 10
                                                color: controllerManager.connected
                                                    ? Qt.rgba(0.25, 0.75, 0.48, 0.14)
                                                    : Qt.rgba(0.55, 0.60, 0.68, 0.12)
                                                border.color: controllerManager.connected ? "#48C78E" : theme.divider
                                                implicitWidth: controllerStatusLabel.implicitWidth + 20
                                                implicitHeight: 28
                                                Label {
                                                    id: controllerStatusLabel
                                                    anchors.centerIn: parent
                                                    text: controllerManager.connected ? "Connected" : "Disconnected"
                                                    color: controllerManager.connected ? "#48C78E" : theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                    font.bold: true
                                                }
                                            }
                                        }
                                    }

                                    Label {
                                        text: "CONTROLLER OPTIONS"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: controllerOptionsColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: theme.divider
                                        ColumnLayout {
                                            id: controllerOptionsColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 10
                                            RowLayout {
                                                Layout.fillWidth: true
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 3
                                                    Label {
                                                        text: "Controller navigation"
                                                        color: theme.textPrimary
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.bold: true
                                                    }
                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: "Allow a connected gamepad to navigate RecompStation menus."
                                                        color: theme.textSecondary
                                                        font.pixelSize: 11 * theme.fontScale
                                                        wrapMode: Text.Wrap
                                                    }
                                                }
                                                Switch {
                                                    background: Rectangle {
                                                        radius: 8
                                                        color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"
                                                        border.color: parent.activeFocus ? theme.accent : "transparent"
                                                        border.width: parent.activeFocus ? 2 : 0
                                                        anchors.margins: -6
                                                    }
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                                    checked: controllerManager.inputEnabled
                                                    onToggled: controllerManager.inputEnabled = checked
                                                }
                                            }
                                            Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                            Label {
                                                Layout.fillWidth: true
                                                text: controllerManager.connected
                                                    ? "Controller input is active. You can connect or disconnect a gamepad while RecompStation is running."
                                                    : "Keyboard and mouse remain available. Controller hot-plug detection is active."
                                                color: theme.textMuted
                                                font.pixelSize: 10 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Label {
                                        text: "ACTIVE PROFILE"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: profileColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: controllerManager.connected ? theme.accent : theme.divider
                                        ColumnLayout {
                                            id: profileColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 6
                                            RowLayout {
                                                Layout.fillWidth: true
                                                Label {
                                                    text: controllerManager.connected
                                                        ? controllerManager.controllerName : "Default controller profile"
                                                    color: theme.textPrimary
                                                    font.pixelSize: 13 * theme.fontScale
                                                    font.bold: true
                                                    Layout.fillWidth: true
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    text: "DEFAULT"
                                                    color: theme.accent
                                                    font.pixelSize: 9 * theme.fontScale
                                                    font.bold: true
                                                    font.letterSpacing: 1
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: controllerManager.connected
                                                    ? "Using the built-in layout for this connected device. Device-specific profiles and saved remaps are the next controller milestone."
                                                    : "Connect a controller to see its active device. Independent profiles will be saved for each controller once remapping is introduced."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Label {
                                        text: "DEFAULT BUTTON LAYOUT"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Current navigation bindings. These are read-only for now; remapping will be added with per-controller profiles."
                                        color: theme.textSecondary
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        columnSpacing: 10
                                        rowSpacing: 10

                                        Repeater {
                                            model: [
                                                { button: "D-pad / Left stick", action: "Navigate menus" },
                                                { button: "A", action: "Select / Confirm" },
                                                { button: "B / Guide", action: "Back" },
                                                { button: "Start", action: "Open menu" },
                                                { button: "Left shoulder", action: "Previous section" },
                                                { button: "Right shoulder", action: "Next section" }
                                            ]
                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                implicitHeight: bindingColumn.implicitHeight + 22
                                                radius: theme.radiusSmall
                                                color: theme.sidebarBackground
                                                border.color: theme.divider
                                                ColumnLayout {
                                                    id: bindingColumn
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.top: parent.top
                                                    anchors.margins: 11
                                                    spacing: 4
                                                    Label {
                                                        text: modelData.button
                                                        color: theme.textPrimary
                                                        font.pixelSize: 11 * theme.fontScale
                                                        font.bold: true
                                                        wrapMode: Text.Wrap
                                                        Layout.fillWidth: true
                                                    }
                                                    Label {
                                                        text: modelData.action
                                                        color: theme.textSecondary
                                                        font.pixelSize: 10 * theme.fontScale
                                                        wrapMode: Text.Wrap
                                                        Layout.fillWidth: true
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
            }

            FileDialog {
                id: libraryBackgroundFileDialog
                title: "Choose a Library Background"
                nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
                onAccepted: settingsManager.setCustomLibraryBackground(selectedFile.toString())
            }

            ShaderEffectSource {
                id: pageTransitionSnapshot
                anchors.fill: mainPageStack
                sourceItem: mainPageStack
                live: false
                hideSource: false
                recursive: true
                // A slightly smaller snapshot texture reduces GPU work during
                // navigation, while smooth filtering keeps the transition crisp.
                textureSize: Qt.size(Math.max(1, Math.round(width * 0.75)),
                                      Math.max(1, Math.round(height * 0.75)))
                smooth: true
                visible: window.navigationTransitionRunning
                z: 10
            }
        }
    
        Rectangle {
            id: topNavigationBar
            anchors.top: parent.top
            Keys.onPressed: function(event) {
                if (!event.isAutoRepeat)
                    window.activeInputDevice = "keyboard"
            }
            HoverHandler {
                acceptedDevices: PointerDevice.Mouse
                onPointChanged: window.activeInputDevice = "mouse"
            }
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
                        source: "qrc:/qt/qml/RecompStation/assets/images/recompstation-controller.png"
                        sourceSize: Qt.size(384, 256)
                        fillMode: Image.PreserveAspectFit
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 28
                        smooth: true
                        mipmap: true
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
                    spacing: 22

                    Button {
                        id: headerLibraryNavigationButton
                        Layout.preferredHeight: 42
                        Layout.preferredWidth: libraryNavigationContent.implicitWidth + 18
                        padding: 4
                        background: Item {
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                width: parent.width * 0.62
                                height: 2
                                radius: 1
                                color: theme.accent
                                opacity: currentPage === "Library" ? 1 : 0
                                Behavior on opacity {
                                    enabled: !settingsManager.reducedMotion
                                    NumberAnimation { duration: 160 }
                                }
                            }
                        }
                        contentItem: RowLayout {
                            id: libraryNavigationContent
                            spacing: 8
                            Image {
                                source: "qrc:/icons/library.svg"
                                sourceSize: Qt.size(19, 19)
                                Layout.preferredWidth: 19
                                Layout.preferredHeight: 19
                                opacity: headerLibraryNavigationButton.hovered
                                    || currentPage === "Library" ? 1 : 0.76
                            }
                            Label {
                                text: "Library"
                                color: headerLibraryNavigationButton.hovered
                                    || currentPage === "Library" ? theme.textPrimary
                                    : theme.textSecondary
                                font.pixelSize: 12 * theme.fontScale
                                font.bold: currentPage === "Library"
                            }
                        }
                        enabled: !window.navigationTransitionRunning
                        onClicked: {
                            console.info("[UI] Navigation: Game Library")
                            window.navigateToPage("Library")
                        }
                    }

                    Button {
                        id: settingsNavigationButton
                        Layout.preferredHeight: 42
                        Layout.preferredWidth: settingsNavigationContent.implicitWidth + 18
                        padding: 4
                        background: Item {
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                width: parent.width * 0.62
                                height: 2
                                radius: 1
                                color: theme.accent
                                opacity: currentPage === "Settings" ? 1 : 0
                                Behavior on opacity {
                                    enabled: !settingsManager.reducedMotion
                                    NumberAnimation { duration: 160 }
                                }
                            }
                        }
                        contentItem: RowLayout {
                            id: settingsNavigationContent
                            spacing: 8
                            Image {
                                source: "qrc:/icons/settings.svg"
                                sourceSize: Qt.size(19, 19)
                                Layout.preferredWidth: 19
                                Layout.preferredHeight: 19
                                opacity: settingsNavigationButton.hovered
                                    || currentPage === "Settings" ? 1 : 0.76
                            }
                            Label {
                                text: "Settings"
                                color: settingsNavigationButton.hovered
                                    || currentPage === "Settings" ? theme.textPrimary
                                    : theme.textSecondary
                                font.pixelSize: 12 * theme.fontScale
                                font.bold: currentPage === "Settings"
                            }
                        }
                        enabled: !window.navigationTransitionRunning
                        onClicked: {
                            console.info("[UI] Navigation: Settings")
                            window.navigateToPage("Settings")
                        }
                    }
                }
            }
        }

        Item {
            id: systemMenuOverlay
            anchors.fill: parent
            z: 100
            visible: window.systemMenuVisible || window.systemMenuClosing
            opacity: window.systemMenuVisible ? 1 : 0
            focus: window.systemMenuVisible
            enabled: window.systemMenuVisible
            Behavior on opacity {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
            }
            onOpacityChanged: {
                if (!window.systemMenuVisible && opacity <= 0.001)
                    window.systemMenuClosing = false
            }

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g,
                               theme.windowBackground.b, 0.84)
            }

            Rectangle {
                id: systemMenuPanel
                anchors.centerIn: parent
                scale: window.systemMenuVisible ? 1 : 0.94
                Behavior on scale {
                    enabled: !settingsManager.reducedMotion
                    NumberAnimation { duration: 220; easing.type: Easing.OutBack }
                }
                width: Math.min(parent.width - 48, 480)
                height: systemMenuContent.implicitHeight + 60
                radius: theme.radiusLarge
                color: theme.sidebarBackground
                border.color: theme.divider
                border.width: 1

                ColumnLayout {
                    id: systemMenuContent
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: window.quitConfirmationVisible ? "Quit RecompStation?" : "RecompStation"
                        color: theme.textPrimary
                        font.pixelSize: 25 * theme.fontScale
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        Layout.fillWidth: true
                        text: window.quitConfirmationVisible
                              ? "Are you sure you want to close RecompStation?"
                              : "What would you like to do?"
                        color: theme.textSecondary
                        font.pixelSize: 13 * theme.fontScale
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }

                    component MenuChoice: Button {
                        id: choice
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        activeFocusOnTab: true
                        focusPolicy: Qt.StrongFocus
                        leftPadding: 18
                        rightPadding: 18
                        background: Rectangle {
                            radius: theme.radiusCard
                            color: choice.activeFocus ? theme.controlHover : theme.surface
                            border.width: choice.activeFocus ? 2 : 1
                            border.color: choice.activeFocus ? theme.accent : theme.divider
                            Rectangle {
                                width: 4
                                radius: 2
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.margins: 8
                                color: choice.activeFocus ? theme.accent : "transparent"
                            }
                        }
                        contentItem: RowLayout {
                            spacing: 12
                            Label {
                                Layout.fillWidth: true
                                text: choice.text
                                color: choice.activeFocus ? theme.textPrimary : theme.textSecondary
                                font.pixelSize: 15 * theme.fontScale
                                font.bold: choice.activeFocus
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: choice.activeFocus ? "●" : ""
                                color: theme.accent
                                font.pixelSize: 13
                            }
                        }
                    }

                    MenuChoice {
                        id: resumeMenuButton
                        text: "Resume"
                        visible: !window.quitConfirmationVisible
                        onClicked: {
                            window.quitConfirmationVisible = false
                            window.closeSystemMenu()
                        }
                    }
                    MenuChoice {
                        id: settingsMenuButton
                        text: "Settings"
                        visible: !window.quitConfirmationVisible
                        onClicked: {
                            window.closeSystemMenu()
                            window.navigateToPage("Settings")
                        }
                    }
                    MenuChoice {
                        id: quitMenuButton
                        text: "Quit RecompStation"
                        visible: !window.quitConfirmationVisible
                        onClicked: {
                            window.quitConfirmationVisible = true
                            Qt.callLater(function() { confirmQuitButton.forceActiveFocus() })
                        }
                    }
                    MenuChoice {
                        id: confirmQuitButton
                        text: "Yes, Quit"
                        visible: window.quitConfirmationVisible
                        onClicked: Qt.quit()
                    }
                    MenuChoice {
                        id: cancelQuitButton
                        text: "Cancel"
                        visible: window.quitConfirmationVisible
                        onClicked: {
                            window.quitConfirmationVisible = false
                            Qt.callLater(function() { resumeMenuButton.forceActiveFocus() })
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.divider
                    }
                    Label {
                        Layout.fillWidth: true
                        text: controllerManager.connected
                              ? "A  Select     B  Back"
                              : "Enter  Select     Esc  Back"
                        color: theme.textMuted
                        font.pixelSize: 10 * theme.fontScale
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                    window.controllerBack()
                    event.accepted = true
                }
            }

            onVisibleChanged: {
                if (visible)
                    Qt.callLater(function() { resumeMenuButton.forceActiveFocus() })
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            width: Math.min(parent.width - 48, inputHintRow.implicitWidth + 40)
            height: 46
            radius: 23
            color: Qt.rgba(theme.sidebarBackground.r, theme.sidebarBackground.g,
                           theme.sidebarBackground.b, 0.90)
            border.color: theme.divider
            z: 30
            visible: !gameDetailsPopup.visible && currentPage !== ""

            Row {
                id: inputHintRow
                anchors.centerIn: parent
                spacing: 16

                Repeater {
                    model: window.inputHintItems()
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
                            width: Math.max(22, hintKeyLabel.implicitWidth + 12)
                            height: 23
                            radius: modelData.round ? 12 : 5
                            color: modelData.round
                                   ? Qt.rgba(0.22, 0.68, 0.50, 0.16)
                                   : Qt.rgba(0.58, 0.68, 0.82, 0.12)
                            border.color: modelData.color
                            anchors.verticalCenter: parent.verticalCenter

                            Label {
                                id: hintKeyLabel
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
    }
}
