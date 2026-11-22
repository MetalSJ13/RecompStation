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
    // Set the initial native window state before the Wayland surface is mapped.
    // This avoids showing a windowed frame and switching to fullscreen afterward.
    // Restore the user's saved window state before the native surface is shown.
    // Fullscreen remains the first-run default, but windowed/maximized choices
    // survive a restart instead of being overwritten by a hard-coded startup mode.
    visibility: settingsManager.displayMode === "maximized" ? Window.Maximized
        : settingsManager.displayMode === "windowed" ? Window.Windowed
        : Window.FullScreen
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
    // Tracks the ComboBox whose popup is currently owned by controller input.
    property var controllerOpenComboBox: null

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
        const focusedItem = window.activeFocusItem
        let current = focusedItem
        // Controls such as ComboBox and TextField may give active focus to an
        // internal content item. Resolve that item back to its focusable control.
        while (current && candidates.indexOf(current) < 0)
            current = current.parent
        if (!current) {
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
            // Vertical navigation should always move to the nearest
            // visual row first, then choose the control closest to the
            // current horizontal position within that row. A strong primary
            // axis weight prevents long labels or wide controls from pulling
            // focus diagonally past the immediately adjacent row.
            const score = direction === "up" || direction === "down"
                ? primary * 1000 + cross
                : primary + cross * 2.2
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

    function focusedActionItem(item) {
        let current = item
        while (current) {
            if (typeof current.click === "function" || current.popup)
                return current
            current = current.parent
        }
        return null
    }

    function scrollSettingsContent(direction, amount) {
        const scrollView = settingsPage.section === "Appearance" ? appearanceContent
            : settingsPage.section === "Display" ? displayContent
            : settingsPage.section === "Library" ? libraryContent
            : settingsPage.section === "Application" ? applicationContent
            : settingsPage.section === "Metadata" ? metadataContent
            : settingsPage.section === "Controller" ? controllerContent : null
        if (!scrollView || !scrollView.visible || !scrollView.contentItem)
            return false
        const flickable = scrollView.contentItem
        const maxY = Math.max(0, flickable.contentHeight - flickable.height)
        if (maxY <= 0)
            return false
        const delta = amount || 84
        flickable.contentY = Math.max(0, Math.min(maxY,
            flickable.contentY + (direction === "down" ? delta : -delta)))
        return true
    }

    function navigationHintSummary() {
        return inputHintItems().map(function(item) {
            return (item.key ? item.key + " " : "") + item.label
        }).join("     ")
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

    function activeControllerComboBox() {
        const tracked = window.controllerOpenComboBox
        if (tracked && tracked.popup && tracked.popup.visible)
            return tracked

        const focused = window.focusedActionItem(window.activeFocusItem)
        if (focused && focused.popup && focused.popup.visible
                && typeof focused.incrementCurrentIndex === "function"
                && typeof focused.decrementCurrentIndex === "function")
            return focused
        return null
    }

    function finishControllerComboBoxSelection(combo, commitSelection) {
        if (!combo || !combo.popup)
            return false

        const selectedIndex = combo.highlightedIndex >= 0
            ? combo.highlightedIndex : combo.currentIndex
        combo.popup.close()
        window.controllerOpenComboBox = null

        if (commitSelection && selectedIndex >= 0 && selectedIndex < combo.count) {
            combo.currentIndex = selectedIndex
            // Emitting activated preserves each ComboBox's existing onActivated
            // behavior, including settings persistence and display-mode changes.
            combo.activated(selectedIndex)
        }

        Qt.callLater(function() { combo.forceActiveFocus() })
        return true
    }

    function keyboardDirectionalNavigationAllowed() {
        if (!window.visible)
            return false
        // Let an open ComboBox handle keyboard input natively. Application-level
        // directional shortcuts must not steal arrows or Enter from its popup.
        if (window.activeControllerComboBox())
            return false
        // Modal dialogs own directional keys while open. In particular, the
        // application-level Settings shortcuts must not steal arrows from the
        // Library Folder Picker's own ListView navigation.
        if (libraryFolderDialog.visible || newLibraryFolderDialog.visible
                || libraryBackgroundFileDialog.visible)
            return false
        const focused = window.activeFocusItem
        if (focused && focused.popup && focused.popup.visible)
            return false
        // Preserve normal caret movement while editing text fields.
        if (focused && !focused.readOnly
                && (focused instanceof TextInput || focused instanceof TextArea))
            return false
        return true
    }

    function controllerNavigate(direction) {
        const openCombo = window.activeControllerComboBox()
        if (openCombo) {
            // Qt's ComboBox methods automatically target highlightedIndex while
            // its popup is open, without changing the committed currentIndex.
            if (direction === "up")
                openCombo.decrementCurrentIndex()
            else if (direction === "down")
                openCombo.incrementCurrentIndex()
            // Consume all directions while the popup is open so focus cannot
            // escape to controls behind it.
            return
        }

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

        if (currentPage === "Library" && window.isDescendantOf(
                    window.activeFocusItem, libraryFilterPanel)) {
            if (direction === "left" || direction === "right") {
                window.moveDirectionalFocus(libraryFilterPanel, direction)
            } else if (direction === "down") {
                gameGrid.currentIndex = Math.max(0, gameGrid.currentIndex)
                gameGrid.forceActiveFocus()
                gameGrid.positionViewAtIndex(gameGrid.currentIndex, GridView.Contain)
            }
            return
        }

        if (currentPage === "Library" && gameGrid.count > 0) {
            let next = gameGrid.currentIndex < 0 ? 0 : gameGrid.currentIndex
            if (settingsManager.libraryView === "list") {
                if (direction === "up" && next === 0) {
                    const toolbarItems = []
                    window.collectFocusableItems(libraryFilterPanel, toolbarItems, [])
                    if (toolbarItems.length > 0)
                        toolbarItems[0].forceActiveFocus()
                    return
                }
                if (direction === "up") next--
                else if (direction === "down") next++
                next = Math.max(0, Math.min(gameGrid.count - 1, next))
                gameGrid.currentIndex = next
                gameList.positionViewAtIndex(next, ListView.Contain)
                gameList.forceActiveFocus()
                return
            }
            if (settingsManager.libraryView === "carousel") {
                if (direction === "up") {
                    const toolbarItems = []
                    window.collectFocusableItems(libraryFilterPanel, toolbarItems, [])
                    if (toolbarItems.length > 0)
                        toolbarItems[0].forceActiveFocus()
                    return
                }
                if (direction !== "left" && direction !== "right")
                    return
                next += direction === "left" ? -1 : 1
                next = Math.max(0, Math.min(gameGrid.count - 1, next))
                gameGrid.currentIndex = next
                consoleView.currentIndex = next
                consoleView.forceActiveFocus()
                return
            }
            const columns = Math.max(1, gameGrid.visibleColumns)
            const row = Math.floor(next / columns)
            if (direction === "up" && row === 0) {
                const toolbarItems = []
                window.collectFocusableItems(libraryFilterPanel, toolbarItems, [])
                if (toolbarItems.length > 0)
                    toolbarItems[0].forceActiveFocus()
                return
            }
            const column = next % columns
            const rowCount = Math.ceil(gameGrid.count / columns)
            let targetRow = row
            let targetColumn = column

            // Horizontal movement stays within the current row. Do not let
            // left/right wrap to the previous or next row at its edges.
            if (direction === "left" && column > 0)
                targetColumn--
            else if (direction === "right"
                     && column < columns - 1 && next + 1 < gameGrid.count)
                targetColumn++
            else if (direction === "up" && row > 0)
                targetRow--
            else if (direction === "down" && row + 1 < rowCount)
                targetRow++
            else
                return

            // The final row may be incomplete. Preserve the nearest valid
            // column when moving vertically into or out of that row.
            const targetRowStart = targetRow * columns
            const targetRowLength = Math.min(columns, gameGrid.count - targetRowStart)
            targetColumn = Math.min(targetColumn, Math.max(0, targetRowLength - 1))
            next = targetRowStart + targetColumn

            if (next === gameGrid.currentIndex)
                return

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
                    // Keep left/right movement within the custom-background
                    // action row before allowing left to return to the category rail.
                    if (window.isDescendantOf(focused, customBackgroundActionsRow)
                            && window.moveDirectionalFocus(settingsContentCard, direction))
                        return
                    window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus()
                    return
                }
                const moved = window.moveDirectionalFocus(settingsContentCard, direction)
                if (!moved && (direction === "up" || direction === "down"))
                    window.scrollSettingsContent(direction, 96)
                return
            }

            window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus()
        }
    }

    function controllerSelect(inputDevice) {
        activeInputDevice = inputDevice || "controller"
        if (systemMenuVisible) {
            const focused = window.focusedActionItem(window.activeFocusItem)
            if (focused && typeof focused.click === "function")
                focused.click()
            return
        }
        const openCombo = window.activeControllerComboBox()
        if (openCombo) {
            window.finishControllerComboBoxSelection(openCombo, true)
            return
        }

        const focusedAction = window.focusedActionItem(window.activeFocusItem)
        if (focusedAction && focusedAction.popup
                && typeof focusedAction.incrementCurrentIndex === "function"
                && typeof focusedAction.decrementCurrentIndex === "function") {
            window.controllerOpenComboBox = focusedAction
            focusedAction.forceActiveFocus()
            focusedAction.popup.open()
            return
        }

        if (gameDetailsPopup.visible) {
            const focused = window.focusedActionItem(window.activeFocusItem)
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
            } else if (settingsManager.libraryView === "carousel") {
                const tile = consoleView.itemAtIndex(index)
                if (tile && tile.openDetails)
                    tile.openDetails()
            } else {
                const tile = gameGrid.itemAtIndex(index)
                if (tile && tile.openDetails)
                    tile.openDetails()
            }
        } else if (currentPage === "Settings") {
            const focused = window.focusedActionItem(window.activeFocusItem)
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

    function controllerBack(inputDevice) {
        activeInputDevice = inputDevice || "controller"
        if (quitConfirmationVisible) {
            quitConfirmationVisible = false
            return
        }
        const openCombo = window.activeControllerComboBox()
        if (openCombo) {
            window.finishControllerComboBoxSelection(openCombo, false)
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

    onCurrentPageChanged: {
        if (currentPage === "Settings")
            Qt.callLater(function() { appearanceNavigationButton.forceActiveFocus() })
    }

    onWidthChanged: console.info("[Settings Geometry] window width:", width)
    onHeightChanged: console.info("[Settings Geometry] window height:", height)

    property string preFullscreenMode: settingsManager.restoreDisplayMode
    property bool pendingPreferredSizeApplication: false

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
            if (window.pendingPreferredSizeApplication) {
                window.pendingPreferredSizeApplication = false
                Qt.callLater(function() { window.applyPreferredPresentationSize() })
            }
        }
    }

    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: window.toggleFullscreen()
    }

    Shortcut {
        sequence: "PageDown"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Settings"
            && !systemMenuVisible && !uninstallDialog.visible
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.scrollSettingsContent("down", Math.max(120, settingsContentCard.height * 0.72))
        }
    }
    Shortcut {
        sequence: "PageUp"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Settings"
            && !systemMenuVisible && !uninstallDialog.visible
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.scrollSettingsContent("up", Math.max(120, settingsContentCard.height * 0.72))
        }
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Settings"
            && !systemMenuVisible && !uninstallDialog.visible
            && !libraryFolderDialog.visible && !librarySetupDialog.visible
            && !libraryBackgroundFileDialog.visible
            && !(window.activeFocusItem && window.activeFocusItem.popup
                && window.activeFocusItem.popup.visible)
        onActivated: window.controllerBack("keyboard")
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
        if (window.visibility !== Window.Windowed) {
            // Restore a normal window first. Wait for the window manager to
            // report the Windowed state before applying the requested geometry.
            window.pendingPreferredSizeApplication = true
            window.showNormal()
            return
        }

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
    // Triggers the card reveal on first library load, Grid View entry, and Library re-entry.
    property int gridRevealToken: 0
    property bool initialGridRevealPending: true

    Connections {
        target: settingsManager
        function onLibraryViewChanged() {
            if (settingsManager.libraryView === "grid") {
                window.gridRevealToken += 1
                window.initialGridRevealPending = false
            }
        }
    }
    property int launcherRevision: 0
    property string pendingUninstallProjectId: ""
    property int selectedMetadataPlatformId: 0
    property int launchBoxProgressPercent: 0
    property string launchBoxStatusMessage: launchBoxCatalogManager.catalogAvailable ? "LaunchBox catalog is ready." : ""
    property bool universalRefreshPending: false
    Connections {
        target: launchBoxCatalogManager
        function onUpdateProgress(stage, percent) {
            launchBoxStatusMessage = stage
            launchBoxProgressPercent = percent
        }
        function onUpdateCompleted(platforms, files, games) {
            launchBoxProgressPercent = 100
            launchBoxStatusMessage = "Catalog updated: " + platforms + " platforms, " + files + " file associations, " + games + " games. LaunchBox artwork for your library is being checked. Run Refresh All Metadata to match games and retrieve missing screenshots."
        }
        function onUpdateFailed(message) {
            launchBoxStatusMessage = "Update failed: " + message
        }
        function onUpdateCancelled() {
            launchBoxStatusMessage = "Catalog update cancelled. The previous catalog was preserved."
        }
    }
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

    Binding {
        target: controllerManager
        property: "navigationAccelerationEnabled"
        // Keep held left-stick and D-pad navigation consistent across all
        // Library layouts. Other pages retain the default repeat cadence.
        value: currentPage === "Library"
            && ["list", "grid", "carousel"].indexOf(settingsManager.libraryView) >= 0
    }


    // Smoothly scroll the active Library viewport with the right stick.
    // The selected game index is deliberately left untouched.
    Timer {
        id: libraryRightStickScrollTimer
        interval: 16
        repeat: true
        property real inputAmount: 0
        property real velocity: 0
        onTriggered: {
            if (window.currentPage !== "Library" || window.systemMenuVisible
                    || gameDetailsPopup.visible
                    || (settingsManager.libraryView !== "grid"
                        && settingsManager.libraryView !== "list")) {
                stop()
                inputAmount = 0
                velocity = 0
                return
            }

            const view = settingsManager.libraryView === "list" ? gameList : gameGrid
            const maxY = Math.max(0, view.contentHeight - view.height)
            const targetVelocity = inputAmount * 9.0
            if (Math.abs(inputAmount) > 0.02)
                velocity += (targetVelocity - velocity) * 0.32
            else
                velocity *= 0.88

            if (Math.abs(velocity) < 0.12 && Math.abs(inputAmount) <= 0.02) {
                velocity = 0
                stop()
                return
            }

            view.contentY = Math.max(0, Math.min(maxY, view.contentY + velocity))
            if ((view.contentY <= 0 && velocity < 0)
                    || (view.contentY >= maxY && velocity > 0)) {
                velocity = 0
                if (Math.abs(inputAmount) <= 0.02)
                    stop()
            }
        }
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
            if (gameDetailsPopup.visible)
                gameDetailsPopup.switchToAdjacentGame(-1)
            else
                window.navigateToPage("Library")
        }
        function onNextSectionRequested() {
            window.activeInputDevice = "controller"
            if (gameDetailsPopup.visible)
                gameDetailsPopup.switchToAdjacentGame(1)
            else
                window.navigateToPage("Settings")
        }
        function onRightStickScrollRequested(amount) {
            if (!controllerManager.navigationActive) {
                libraryRightStickScrollTimer.inputAmount = 0
                return
            }

            if (window.currentPage === "Library" && !window.systemMenuVisible
                    && !gameDetailsPopup.visible
                    && (settingsManager.libraryView === "grid"
                        || settingsManager.libraryView === "list")) {
                window.activeInputDevice = "controller"
                libraryRightStickScrollTimer.inputAmount = amount
                if (Math.abs(amount) > 0.02)
                    libraryRightStickScrollTimer.start()
                return
            }

            if (window.currentPage !== "Settings"
                    || window.systemMenuVisible || gameDetailsPopup.visible)
                return

            const scrollView = settingsPage.section === "Appearance" ? appearanceContent
                : settingsPage.section === "Display" ? displayContent
                : settingsPage.section === "Library" ? libraryContent
                : settingsPage.section === "Application" ? applicationContent
                : settingsPage.section === "Metadata" ? metadataContent
                : settingsPage.section === "Controller" ? controllerContent : null

            if (!scrollView || !scrollView.visible || !scrollView.contentItem)
                return

            const flickable = scrollView.contentItem
            const maxY = Math.max(0, flickable.contentHeight - flickable.height)
            if (maxY <= 0)
                return

            window.activeInputDevice = "controller"
            // Keep the existing Settings scrolling behavior unchanged.
            flickable.contentY = Math.max(0, Math.min(maxY,
                flickable.contentY + amount * 14))
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
        enabled: window.keyboardDirectionalNavigationAllowed()
            && (systemMenuVisible || currentPage === "Settings" || gameDetailsPopup.visible
                || (currentPage === "Library" && !gameDetailsPopup.visible
                    && settingsManager.libraryView !== "list"))
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("left")
        }
    }
    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        enabled: window.keyboardDirectionalNavigationAllowed()
            && (systemMenuVisible || currentPage === "Settings" || gameDetailsPopup.visible
                || (currentPage === "Library" && !gameDetailsPopup.visible
                    && settingsManager.libraryView !== "list"))
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("right")
        }
    }
    Shortcut {
        sequence: "Up"
        context: Qt.ApplicationShortcut
        enabled: window.keyboardDirectionalNavigationAllowed()
            && (systemMenuVisible || currentPage === "Settings"
                || gameDetailsPopup.visible || currentPage === "Library")
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("up")
        }
    }
    Shortcut {
        sequence: "Down"
        context: Qt.ApplicationShortcut
        enabled: window.keyboardDirectionalNavigationAllowed()
            && (systemMenuVisible || currentPage === "Settings"
                || gameDetailsPopup.visible || currentPage === "Library")
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.controllerNavigate("down")
        }
    }
    Shortcut {
        sequence: "Return"
        context: Qt.ApplicationShortcut
        enabled: window.visible
            && (currentPage === "Library" || currentPage === "Settings")
            && !systemMenuVisible && !gameDetailsPopup.visible
            && !uninstallDialog.visible && !librarySetupDialog.visible
            && window.keyboardDirectionalNavigationAllowed()
        onActivated: window.controllerSelect("keyboard")
    }
    Shortcut {
        sequence: "Enter"
        context: Qt.ApplicationShortcut
        enabled: window.visible
            && (currentPage === "Library" || currentPage === "Settings")
            && !systemMenuVisible && !gameDetailsPopup.visible
            && !uninstallDialog.visible && !librarySetupDialog.visible
            && window.keyboardDirectionalNavigationAllowed()
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
            script: {
                const previousPage = window.currentPage
                window.currentPage = window.pendingNavigationPage
                // Re-entering the Library from Settings is a fresh page
                // entrance, so replay the Grid card reveal after the page
                // switch has made the library visible.
                if (previousPage === "Settings" && window.currentPage === "Library"
                        && settingsManager.libraryView === "grid") {
                    window.gridRevealToken += 1
                }
            }
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

    function loadCachedProjectRelease(project) {
        if (!project)
            return
        const cached = githubManager.cachedLatestRelease(
            project.repositoryOwner, project.repositoryName)
        if (!cached || !cached.tagName)
            return
        let nextStates = Object.assign({}, releaseStates)
        nextStates[project.id] = {
            loading: false,
            error: "",
            release: cached,
            assets: projectCatalog.compatibleAssets(project.id, cached.assets || [])
        }
        releaseStates = nextStates
    }

    function checkProjectRelease(project) {
        if (!project || pendingReleaseRequestsForProject(project.id))
            return
        let nextStates = Object.assign({}, releaseStates)
        const previous = nextStates[project.id] || {}
        nextStates[project.id] = {
            loading: true,
            error: "",
            release: previous.release || null,
            assets: previous.assets || []
        }
        releaseStates = nextStates
        const requestId = githubManager.fetchLatestRelease(
            project.repositoryOwner, project.repositoryName)
        let pending = Object.assign({}, pendingReleaseRequests)
        pending[requestId] = project.id
        pendingReleaseRequests = pending
    }

    function pendingReleaseRequestsForProject(projectId) {
        return Object.keys(pendingReleaseRequests).some(
            requestId => pendingReleaseRequests[requestId] === projectId)
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
            const previous = states[projectId] || {}
            states[projectId] = {
                loading: false,
                error: message,
                release: previous.release || null,
                assets: previous.assets || []
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
            controllerManager.gameSessionActive = Object.keys(running).length > 0
        }
        function onLaunchFailed(projectId, message) {
            let running = Object.assign({}, window.runningLaunchers)
            delete running[projectId]
            window.runningLaunchers = running
            controllerManager.gameSessionActive = Object.keys(running).length > 0
            let errors = Object.assign({}, window.launchErrors)
            errors[projectId] = message
            window.launchErrors = errors
        }
        function onProcessFinished(projectId, exitCode, crashed) {
            let running = Object.assign({}, window.runningLaunchers)
            delete running[projectId]
            window.runningLaunchers = running
            controllerManager.gameSessionActive = Object.keys(running).length > 0
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
            if (settingsManager.automaticMetadataMatchingEnabled) {
                window.scanMessage = libraryScanner.lastError.length > 0
                    ? libraryScanner.lastError + "\nChecking metadata for discovered games..."
                    : gamesFound + " new game(s) found. Checking metadata..."
                // A problem with one platform folder must not block metadata for
                // games successfully discovered on other platforms.
                metadataMatcher.scrapeLibrary()
            } else {
                window.scanMessage = libraryScanner.lastError.length > 0
                    ? libraryScanner.lastError + "\nAutomatic metadata matching is disabled."
                    : gamesFound + " new game(s) found. Automatic metadata matching is disabled."
            }
        }
    }

    Connections {
        target: screenshotManager
        function onRefreshFinished(downloaded, skipped, withoutScreenshots, failed) {
            window.libraryRevision++
            window.scanMessage = "Screenshots refreshed: " + downloaded
                + " downloaded, " + skipped + " already available, "
                + withoutScreenshots + " unavailable from the provider, "
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
            // TheGamesDB screenshots are a separate provider-specific pass.
            // LaunchBox artwork is refreshed independently by LaunchBoxArtworkManager.
            const shouldRefreshScreenshots = metadataManager.configured
                && (window.universalRefreshPending
                    || (settingsManager.automaticArtworkDownloadsEnabled && matched > 0))
            if (window.universalRefreshPending)
                window.universalRefreshPending = false
            if (shouldRefreshScreenshots)
                screenshotManager.refreshLibraryScreenshots()
        }
    }

    Dialog {
        id: libraryFolderDialog
        title: "Choose your RecompStation game library"
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 36, 760)
        height: Math.min(parent.height - 36, 620)
        standardButtons: Dialog.NoButton
        padding: 18

        property string currentFolderPath: ""
        property string selectedFolderPath: ""
        property var directoryEntries: []
        property string browserMessage: ""
        property string newFolderParentPath: ""

        function refreshDirectory() {
            directoryEntries = platformManager.libraryDirectories(currentFolderPath)
            libraryDirectoryList.currentIndex = directoryEntries.length > 0 ? 0 : -1
            browserMessage = directoryEntries.length === 0
                ? "No subfolders in this location." : ""
        }

        function navigateTo(path) {
            currentFolderPath = path
            selectedFolderPath = path
            refreshDirectory()
        }

        onOpened: {
            const startPath = platformManager.libraryConfigured
                ? platformManager.libraryDirectory : platformManager.homeDirectory()
            navigateTo(startPath)
            Qt.callLater(function() { libraryDirectoryList.forceActiveFocus() })
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: "Choose where RecompStation should store and scan your games. You can create a folder here without leaving setup."
                color: theme.textSecondary
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "Home"
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    onClicked: {
                        libraryFolderDialog.navigateTo(platformManager.homeDirectory())
                        libraryDirectoryList.forceActiveFocus()
                    }
                }
                Button {
                    text: "Up"
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    enabled: platformManager.parentDirectory(libraryFolderDialog.currentFolderPath)
                        !== libraryFolderDialog.currentFolderPath
                    onClicked: {
                        libraryFolderDialog.navigateTo(
                            platformManager.parentDirectory(libraryFolderDialog.currentFolderPath))
                        libraryDirectoryList.forceActiveFocus()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: libraryFolderDialog.currentFolderPath
                    color: theme.textMuted
                    elide: Text.ElideMiddle
                }
                Button {
                    text: "New Folder"
                    highlighted: true
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    onClicked: {
                        libraryFolderDialog.newFolderParentPath = libraryFolderDialog.currentFolderPath
                        newLibraryFolderName.text = ""
                        newLibraryFolderDialog.open()
                        newLibraryFolderName.forceActiveFocus()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: theme.sidebarBackground
                border.color: theme.divider
                radius: 10
                clip: true

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        libraryFolderDialog.newFolderParentPath = libraryFolderDialog.currentFolderPath
                        emptyFolderContextMenu.popup()
                    }
                }

                ListView {
                    id: libraryDirectoryList
                    anchors.fill: parent
                    anchors.margins: 6
                    model: libraryFolderDialog.directoryEntries
                    clip: true
                    spacing: 3
                    focus: true
                    activeFocusOnTab: true
                    keyNavigationEnabled: true
                    highlightFollowsCurrentItem: false

                    function ensureCurrentFolderVisible() {
                        if (currentIndex < 0 || currentIndex >= count)
                            return
                        // Defer until ListView has laid out the newly selected
                        // delegate; otherwise positionViewAtIndex can run before
                        // its geometry is updated and leave the viewport in place.
                        const targetIndex = currentIndex
                        Qt.callLater(function() {
                            if (targetIndex !== libraryDirectoryList.currentIndex)
                                return
                            libraryDirectoryList.positionViewAtIndex(
                                targetIndex, ListView.Contain)
                            const item = libraryDirectoryList.itemAtIndex(targetIndex)
                            if (item) {
                                const top = item.y
                                const bottom = top + item.height
                                if (top < libraryDirectoryList.contentY)
                                    libraryDirectoryList.contentY = top
                                else if (bottom > libraryDirectoryList.contentY
                                         + libraryDirectoryList.height)
                                    libraryDirectoryList.contentY =
                                        bottom - libraryDirectoryList.height
                            }
                        })
                    }

                    onCurrentIndexChanged: ensureCurrentFolderVisible()

                    function focusFolderAt(index) {
                        if (count <= 0) {
                            currentIndex = -1
                            return
                        }
                        currentIndex = Math.max(0, Math.min(count - 1, index))
                        ensureCurrentFolderVisible()
                        const entry = currentItem
                        if (entry)
                            libraryFolderDialog.selectedFolderPath = entry.folderPath
                    }

                    Keys.onUpPressed: {
                        focusFolderAt(currentIndex < 0 ? 0 : currentIndex - 1)
                    }
                    Keys.onDownPressed: {
                        focusFolderAt(currentIndex < 0 ? 0 : currentIndex + 1)
                    }
                    Keys.onPressed: function(event) {
                        const step = Math.max(1, Math.floor(height / 47))
                        const current = currentIndex < 0 ? 0 : currentIndex
                        if (event.key === Qt.Key_Up) {
                            focusFolderAt(current - 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            focusFolderAt(current + 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_PageUp) {
                            focusFolderAt(current - step)
                            event.accepted = true
                        } else if (event.key === Qt.Key_PageDown) {
                            focusFolderAt(current + step)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Home) {
                            focusFolderAt(0)
                            event.accepted = true
                        } else if (event.key === Qt.Key_End) {
                            focusFolderAt(count - 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return
                                   || event.key === Qt.Key_Enter) {
                            if (currentItem)
                                libraryFolderDialog.navigateTo(currentItem.folderPath)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Space) {
                            if (currentItem)
                                libraryFolderDialog.selectedFolderPath = currentItem.folderPath
                            event.accepted = true
                        }
                    }

                    delegate: Rectangle {
                        required property var modelData
                        property string folderPath: modelData.path
                        width: ListView.view.width
                        height: 44
                        radius: 7
                        color: folderMouseArea.containsMouse || ListView.isCurrentItem && libraryDirectoryList.activeFocus
                            ? Qt.rgba(0.45, 0.45, 0.45, 0.16)
                            : (libraryFolderDialog.selectedFolderPath === modelData.path
                                ? Qt.rgba(0.35, 0.55, 0.85, 0.18) : "transparent")
                        border.color: ListView.isCurrentItem && libraryDirectoryList.activeFocus
                            ? theme.accent
                            : libraryFolderDialog.selectedFolderPath === modelData.path
                                ? theme.accent : "transparent"
                        border.width: ListView.isCurrentItem && libraryDirectoryList.activeFocus ? 2 : 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 10
                            spacing: 10
                            Label {
                                text: "📁"
                                color: theme.textSecondary
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: theme.textPrimary
                                elide: Text.ElideRight
                            }
                            Label {
                                text: "›"
                                color: theme.textMuted
                                font.pixelSize: 20
                            }
                        }

                        MouseArea {
                            id: folderMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                libraryDirectoryList.currentIndex = index
                                libraryDirectoryList.forceActiveFocus()
                                if (mouse.button === Qt.RightButton) {
                                    libraryFolderDialog.selectedFolderPath = modelData.path
                                    folderContextMenu.popup()
                                } else {
                                    libraryFolderDialog.selectedFolderPath = modelData.path
                                }
                            }
                            onDoubleClicked: function(mouse) {
                                if (mouse.button === Qt.LeftButton)
                                    libraryFolderDialog.navigateTo(modelData.path)
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: libraryFolderDialog.directoryEntries.length === 0
                        text: "No folders here yet. Use New Folder or right-click a folder to create one."
                        color: theme.textMuted
                        wrapMode: Text.Wrap
                        width: parent.width - 40
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: libraryFolderDialog.browserMessage
                color: theme.textMuted
                font.pixelSize: 11 * theme.fontScale
                visible: text.length > 0
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: "Selected: " + libraryFolderDialog.selectedFolderPath
                    color: theme.textMuted
                    elide: Text.ElideMiddle
                }
                Button {
                    text: "Cancel"
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    onClicked: libraryFolderDialog.reject()
                }
                Button {
                    text: "Choose This Folder"
                    highlighted: true
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    enabled: libraryFolderDialog.selectedFolderPath.length > 0
                    onClicked: {
                        if (platformManager.setLibraryDirectoryPath(
                                libraryFolderDialog.selectedFolderPath)) {
                            window.scanMessage = "Library folder saved. Scanning platform folders..."
                            libraryFolderDialog.accept()
                            librarySetupDialog.close()
                            libraryScanner.scanAll()
                        } else {
                            libraryFolderDialog.browserMessage =
                                "Could not use that folder. Check that it exists and is writable."
                        }
                    }
                }
            }
        }

        Menu {
            id: folderContextMenu
            MenuItem {
                text: "Open"
                onTriggered: libraryFolderDialog.navigateTo(
                    libraryFolderDialog.selectedFolderPath)
            }
            MenuItem {
                text: "Create New Folder"
                onTriggered: {
                    libraryFolderDialog.newFolderParentPath = libraryFolderDialog.currentFolderPath
                    newLibraryFolderName.text = ""
                    newLibraryFolderDialog.open()
                    newLibraryFolderName.forceActiveFocus()
                }
            }
        }

        Menu {
            id: emptyFolderContextMenu
            MenuItem {
                text: "Create New Folder"
                onTriggered: {
                    libraryFolderDialog.newFolderParentPath = libraryFolderDialog.currentFolderPath
                    newLibraryFolderName.text = ""
                    newLibraryFolderDialog.open()
                    newLibraryFolderName.forceActiveFocus()
                }
            }
        }
    }

    Dialog {
        id: newLibraryFolderDialog
        title: "Create New Folder"
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: newLibraryFolderName.forceActiveFocus()
        onAccepted: {
            if (platformManager.createLibraryFolder(
                    libraryFolderDialog.newFolderParentPath,
                    newLibraryFolderName.text)) {
                if (libraryFolderDialog.newFolderParentPath === libraryFolderDialog.currentFolderPath)
                    libraryFolderDialog.refreshDirectory()
                libraryFolderDialog.selectedFolderPath =
                    libraryFolderDialog.newFolderParentPath + "/" + newLibraryFolderName.text.trim()
                libraryFolderDialog.browserMessage = "Folder created."
            } else {
                libraryFolderDialog.browserMessage =
                    "Could not create that folder. Check the name, permissions, and whether it already exists."
            }
        }
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                text: "Enter a name for the new folder."
                color: theme.textSecondary
            }
            TextField {
                id: newLibraryFolderName
                Layout.fillWidth: true
                placeholderText: "Folder name"
                selectByMouse: true
            }
        }
    }

    function finishUpdatePreferencePrompt(enabled) {
        settingsManager.completeUpdatePreferencePrompt(enabled)
        updatePreferenceDialog.close()
    }

    Dialog {
        id: updatePreferenceDialog
        title: ""
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 540)
        padding: 26
        closePolicy: Popup.CloseOnEscape
        visible: !platformManager.libraryConfigured
            && !settingsManager.updatePreferencePromptCompleted
            && !window.systemMenuVisible
        standardButtons: Dialog.NoButton
        onRejected: window.finishUpdatePreferencePrompt(false)

        background: Rectangle {
            color: theme.elevatedSurface
            radius: theme.radiusLarge
            border.width: 1
            border.color: theme.divider
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: settingsManager.reducedMotion ? 0 : 220
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 0.97
                    to: 1
                    duration: settingsManager.reducedMotion ? 0 : 260
                    easing.type: Easing.OutCubic
                }
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: settingsManager.reducedMotion ? 0 : 160
                easing.type: Easing.InCubic
            }
        }

        contentItem: ColumnLayout {
            spacing: 18
            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 54
                    Layout.preferredHeight: 54
                    radius: 16
                    color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                    border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.38)
                    Label {
                        anchors.centerIn: parent
                        text: "↻"
                        color: theme.accent
                        font.pixelSize: 30
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: "Application Updates"
                        color: theme.textPrimary
                        font.pixelSize: 21 * theme.fontScale
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "A preference you can change at any time"
                        color: theme.textSecondary
                        font.pixelSize: 12 * theme.fontScale
                        wrapMode: Text.Wrap
                    }
                }
                ToolButton {
                    text: "✕"
                    Accessible.name: "Close update preferences"
                    onClicked: window.finishUpdatePreferencePrompt(false)
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Would you like RecompStation to automatically check for application updates?"
                color: theme.textPrimary
                font.pixelSize: 14 * theme.fontScale
                wrapMode: Text.Wrap
            }
            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusCard
                color: theme.sidebarBackground
                border.color: theme.divider
                implicitHeight: updateInfoColumn.implicitHeight + 26
                ColumnLayout {
                    id: updateInfoColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 13
                    spacing: 6
                    Label {
                        text: "You're always in control"
                        color: theme.textPrimary
                        font.pixelSize: 12 * theme.fontScale
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "You can change this preference later in Settings. RecompStation will never automatically download or install an application update."
                        color: theme.textMuted
                        font.pixelSize: 11 * theme.fontScale
                        wrapMode: Text.Wrap
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                Button {
                    id: updatePreferenceNoButton
                    text: "No, thanks"
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    onClicked: window.finishUpdatePreferencePrompt(false)
                }
                Button {
                    text: "Yes, check automatically"
                    highlighted: true
                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    onClicked: window.finishUpdatePreferencePrompt(true)
                }
            }
        }
        onOpened: Qt.callLater(function() { updatePreferenceNoButton.forceActiveFocus() })
    }

    Dialog {
        id: librarySetupDialog
        title: ""
        modal: false
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)
        padding: 26
        closePolicy: platformManager.libraryConfigured ? Popup.CloseOnEscape : Popup.NoAutoClose
        visible: !platformManager.libraryConfigured
            && settingsManager.updatePreferencePromptCompleted
            && window.currentPage === "Library" && !window.systemMenuVisible
        standardButtons: Dialog.NoButton

        background: Rectangle {
            color: theme.elevatedSurface
            radius: theme.radiusLarge
            border.width: 1
            border.color: theme.divider
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: settingsManager.reducedMotion ? 0 : 220
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 0.97
                    to: 1
                    duration: settingsManager.reducedMotion ? 0 : 260
                    easing.type: Easing.OutCubic
                }
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: settingsManager.reducedMotion ? 0 : 160
                easing.type: Easing.InCubic
            }
        }

        contentItem: ColumnLayout {
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 54
                    Layout.preferredHeight: 54
                    radius: 16
                    color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                    border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.38)
                    Label {
                        anchors.centerIn: parent
                        text: "▣"
                        color: theme.accent
                        font.pixelSize: 28
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: "Welcome to RecompStation"
                        color: theme.textPrimary
                        font.pixelSize: 21 * theme.fontScale
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Let's set up your game library"
                        color: theme.accent
                        font.pixelSize: 12 * theme.fontScale
                        font.weight: Font.Medium
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Choose the main folder where you keep your game files. RecompStation will organize supported systems into their own folders."
                color: theme.textSecondary
                font.pixelSize: 13 * theme.fontScale
                wrapMode: Text.Wrap
                lineHeight: 1.2
            }

            Rectangle {
                Layout.fillWidth: true
                radius: theme.radiusCard
                color: theme.sidebarBackground
                border.color: theme.divider
                implicitHeight: setupInfoColumn.implicitHeight + 26

                ColumnLayout {
                    id: setupInfoColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 13
                    spacing: 9

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Label {
                            text: "✓"
                            color: theme.success
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Your existing game files stay where they are."
                            color: theme.textPrimary
                            font.pixelSize: 12 * theme.fontScale
                            wrapMode: Text.Wrap
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Label {
                            text: "✓"
                            color: theme.success
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Platform folders are created automatically."
                            color: theme.textPrimary
                            font.pixelSize: 12 * theme.fontScale
                            wrapMode: Text.Wrap
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Label {
                            text: "✓"
                            color: theme.success
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "You can change this location later in Settings."
                            color: theme.textPrimary
                            font.pixelSize: 12 * theme.fontScale
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                Button {
                    text: "Choose Library Folder"
                    highlighted: true
                    font.pixelSize: 13 * theme.fontScale
                    padding: 13
                    onClicked: libraryFolderDialog.open()
                    background: Rectangle {
                        radius: 9
                        color: parent.down ? theme.accentPressed
                            : parent.hovered ? theme.accentHover : theme.accent
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "#ffffff"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
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
                    id: gameCoverImage
                    anchors.fill: parent
                    anchors.margins: 5
                    source: window.selectedGame ? window.selectedGame.artworkPath : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                }
                Image {
                    anchors.fill: parent
                    anchors.margins: 5
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
                currentIndex: currentPage === "Settings" ? 1 : 0
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

                            Rectangle {
                                id: libraryFilterPanel
                                // Keep the filter strip visually compact while the
                                // collection heading absorbs the flexible space to its left.
                                Layout.minimumWidth: 520
                                Layout.preferredWidth: 1080
                                Layout.maximumWidth: 1080
                                Layout.preferredHeight: 76
                                radius: 10
                                color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.82)
                                border.color: theme.divider
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 9

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 130
                                        spacing: 3
                                        Label {
                                            text: "SEARCH GAMES"
                                            color: theme.textMuted
                                            font.pixelSize: 9 * theme.fontScale
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                        }
                                        TextField {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 34
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            placeholderText: "Enter a game title"
                                            selectByMouse: true
                                            color: theme.textPrimary
                                            font.pixelSize: 12 * theme.fontScale
                                            leftPadding: 10
                                            rightPadding: 10
                                            onTextChanged: {
                                                window.librarySearchText = text
                                                gameGrid.currentIndex = 0
                                            }
                                            background: Rectangle {
                                                radius: 8
                                                color: theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.preferredWidth: 230
                                        Layout.minimumWidth: 160
                                        spacing: 3
                                        Label {
                                            text: "PLATFORM"
                                            color: theme.textMuted
                                            font.pixelSize: 9 * theme.fontScale
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 7
                                                color: parent.activeFocus
                                                    ? Qt.rgba(theme.accent.r, theme.accent.g,
                                                              theme.accent.b, 0.18)
                                                    : theme.surface
                                                border.color: parent.activeFocus
                                                    ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 34
                                            model: ["All Platforms"].concat(platformManager.platforms())
                                            currentIndex: Math.max(0, model.indexOf(window.selectedPlatform))
                                            onActivated: {
                                                window.selectedPlatform = currentText
                                                gameGrid.currentIndex = 0
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillHeight: true
                                        Layout.preferredWidth: 1
                                        color: theme.divider
                                    }

                                    ColumnLayout {
                                        Layout.preferredWidth: 150
                                        Layout.minimumWidth: 105
                                        spacing: 3
                                        Label {
                                            text: "VIEW"
                                            color: theme.textMuted
                                            font.pixelSize: 9 * theme.fontScale
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 7
                                                color: parent.activeFocus
                                                    ? Qt.rgba(theme.accent.r, theme.accent.g,
                                                              theme.accent.b, 0.18)
                                                    : theme.surface
                                                border.color: parent.activeFocus
                                                    ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 34
                                            textRole: "label"
                                            valueRole: "value"
                                            model: [
                                                { label: "Grid", value: "grid" },
                                                { label: "List", value: "list" },
                                                { label: "Carousel", value: "carousel" }
                                            ]
                                            currentIndex: Math.max(0, indexOfValue(settingsManager.libraryView))
                                            onActivated: settingsManager.libraryView = currentValue
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.preferredWidth: 170
                                        Layout.minimumWidth: 125
                                        spacing: 3
                                        Label {
                                            text: settingsManager.libraryView === "list" ? "ROW SIZE" : "CARD SIZE"
                                            color: theme.textMuted
                                            font.pixelSize: 9 * theme.fontScale
                                            font.bold: true
                                            font.letterSpacing: 0.8
                                        }
                                        ComboBox {
                                            background: Rectangle {
                                                radius: 7
                                                color: parent.activeFocus
                                                    ? Qt.rgba(theme.accent.r, theme.accent.g,
                                                              theme.accent.b, 0.18)
                                                    : theme.surface
                                                border.color: parent.activeFocus
                                                    ? theme.accent : theme.divider
                                                border.width: parent.activeFocus ? 2 : 1
                                            }
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 34
                                            textRole: "label"
                                            valueRole: "value"
                                            model: [
                                                { label: "Compact", value: "compact" },
                                                { label: "Comfortable", value: "comfortable" },
                                                { label: "Spacious", value: "spacious" }
                                            ]
                                            currentIndex: Math.max(0, indexOfValue(
                                                settingsManager.libraryView === "list"
                                                    ? settingsManager.libraryListDensity
                                                    : settingsManager.libraryView === "carousel"
                                                        ? settingsManager.libraryConsoleDensity
                                                        : settingsManager.libraryDensity))
                                            onActivated: {
                                                if (settingsManager.libraryView === "list")
                                                    settingsManager.libraryListDensity = currentValue
                                                else if (settingsManager.libraryView === "carousel")
                                                    settingsManager.libraryConsoleDensity = currentValue
                                                else
                                                    settingsManager.libraryDensity = currentValue
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            id: libraryViewViewport
                            Layout.fillWidth: true
                            Layout.fillHeight: gameGrid.count > 0
                            Layout.preferredHeight: gameGrid.count > 0 ? 760 : 0

                            GridView {
                            id: gameGrid
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            onCountChanged: {
                                if (count > 0 && window.initialGridRevealPending
                                        && settingsManager.libraryView === "grid") {
                                    startupGridRevealTimer.start()
                                }
                            }
                            // Wait until the first delegates have been created and
                            // the active display mode has produced a usable viewport.
                            // Windowed, maximized, and fullscreen modes all use the
                            // same entrance reveal; none should block startup.
                            Timer {
                                id: startupGridRevealTimer
                                interval: 250
                                repeat: false
                                onTriggered: {
                                    if (settingsManager.libraryView !== "grid" || !window.visible)
                                        return
                                    if (gameGrid.width <= 0 || gameGrid.height <= 0) {
                                        restart()
                                        return
                                    }
                                    window.gridRevealToken += 1
                                    window.initialGridRevealPending = false
                                }
                            }
                            visible: settingsManager.libraryView === "grid" || opacity > 0
                            opacity: settingsManager.libraryView === "grid" ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                            }
                            clip: true
                            // Keep scaled cards and their focus glow inside the viewport,
                            // including the first and last rows.
                            topMargin: 10
                            bottomMargin: 10
                            // Keep card widths fixed for each density. Center the complete
                            // column block with equal outer margins; GridView keeps a
                            // partially filled final row aligned to the first column.
                            readonly property real targetCardWidth:
                                settingsManager.libraryDensity === "compact" ? 220
                                : settingsManager.libraryDensity === "spacious" ? 350 : 270
                            readonly property real gridCellWidth: targetCardWidth + 14
                            readonly property int responsiveColumns:
                                Math.max(1, Math.floor(Math.max(0, parent.width - 16)
                                    / Math.max(1, gridCellWidth)))
                            // Keep 4K ultrawide grids visually balanced instead of
                            // allowing the full width to become an excessive number of columns.
                            // Smaller reference resolutions retain their natural column count.
                            readonly property int visibleColumns:
                                parent.width >= 3000
                                    ? Math.min(8, responsiveColumns) : responsiveColumns
                            readonly property real centeredOuterMargin:
                                Math.max(8, (parent.width - visibleColumns * gridCellWidth) / 2)
                            function logGridGeometry(reason) {
                                console.info("[Grid Geometry]", reason,
                                    "density:", settingsManager.libraryDensity,
                                    "viewport:", parent.width, "x", parent.height,
                                    "grid:", width, "x", height,
                                    "targetCardWidth:", targetCardWidth,
                                    "cellWidth:", cellWidth,
                                    "responsiveColumns:", responsiveColumns,
                                    "visibleColumns:", visibleColumns,
                                    "leftMargin:", leftMargin,
                                    "rightMargin:", rightMargin)
                            }
                            Component.onCompleted: Qt.callLater(function() {
                                gameGrid.logGridGeometry("completed")
                            })
                            onWidthChanged: logGridGeometry("grid width changed")
                            onCellWidthChanged: logGridGeometry("cell width changed")
                            onLeftMarginChanged: logGridGeometry("left margin changed")
                            onRightMarginChanged: logGridGeometry("right margin changed")
                            Connections {
                                target: settingsManager
                                function onLibraryDensityChanged() {
                                    Qt.callLater(function() {
                                        gameGrid.logGridGeometry("density changed")
                                    })
                                }
                            }
                            leftMargin: centeredOuterMargin
                            rightMargin: centeredOuterMargin
                            cellWidth: gridCellWidth
                            // Match the card height to its actual content: a 2:3
                            // artwork frame (the card width minus its 20 px inner
                            // horizontal margins), plus the title/platform footer,
                            // layout spacing, and outer vertical padding.
                            readonly property real gridCardHeight:
                                Math.round((targetCardWidth - 20) * 1.5 + 72)
                            cellHeight: gridCardHeight + 14
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
                                // Begin the initial Grid presentation hidden so
                                // delegates cannot flash fully visible before the
                                // startup reveal token triggers their animation.
                                property real revealOpacity:
                                    window.initialGridRevealPending
                                        && settingsManager.libraryView === "grid" ? 0 : 1
                                property real revealScale:
                                    window.initialGridRevealPending
                                        && settingsManager.libraryView === "grid" ? 0.94 : 1
                                property real revealOffsetY:
                                    window.initialGridRevealPending
                                        && settingsManager.libraryView === "grid" ? 26 : 0
                                // Use the delegate's laid-out position rather than the
                                // preferred density count: the actual column count changes
                                // with window width and the GridView's minimum cell width.
                                readonly property int revealRow:
                                    Math.round((gameTile.y - gameGrid.topMargin)
                                        / Math.max(1, gameGrid.cellHeight))
                                readonly property int revealColumn:
                                    Math.round((gameTile.x - gameGrid.leftMargin)
                                        / Math.max(1, gameGrid.cellWidth))
                                readonly property int revealBottomVisibleRow:
                                    Math.floor((gameGrid.contentY + gameGrid.height - gameGrid.topMargin)
                                        / Math.max(1, gameGrid.cellHeight))
                                // Resolve stagger timing to a finite, non-negative integer.
                                // GridView can briefly report incomplete geometry while its
                                // delegates are being created during application startup.
                                readonly property int revealDelayMs: {
                                    const row = gameTile.revealRow
                                    const column = gameTile.revealColumn
                                    const bottomRow = gameTile.revealBottomVisibleRow
                                    if (!isFinite(row) || !isFinite(column) || !isFinite(bottomRow))
                                        return 0
                                    const delay = Math.max(0, bottomRow - row) * 115
                                        + Math.max(0, column) * 38
                                    return isFinite(delay) ? Math.max(0, Math.round(delay)) : 0
                                }
                                // A delegate must never remain transparent if an animation
                                // is interrupted by a model/layout change during startup.
                                Timer {
                                    id: revealFailsafeTimer
                                    interval: Math.max(1200, gameTile.revealDelayMs + 900)
                                    repeat: false
                                    onTriggered: {
                                        if (gameTile.revealOpacity < 1) {
                                            gameTile.revealOpacity = 1
                                            gameTile.revealScale = 1
                                            gameTile.revealOffsetY = 0
                                            revealAnimation.stop()
                                        }
                                    }
                                }
                                // Grid cards are taller than List rows, so use a
                                // broader, eased fade to keep fast scrolling natural.
                                // Smoothstep avoids the linear, abrupt-looking opacity
                                // change while preserving full opacity in the center.
                                readonly property real scrollEdgeDistance: Math.min(
                                    gameTile.y + gameTile.height / 2 - gameGrid.contentY,
                                    gameGrid.contentY + gameGrid.height
                                        - (gameTile.y + gameTile.height / 2))
                                readonly property real scrollEdgeProgress: settingsManager.reducedMotion
                                    ? 1
                                    : Math.max(0, Math.min(1, scrollEdgeDistance / 240))
                                readonly property real scrollEdgeEase:
                                    scrollEdgeProgress * scrollEdgeProgress
                                    * (3 - 2 * scrollEdgeProgress)
                                readonly property real scrollEdgeOpacity:
                                    settingsManager.reducedMotion ? 1
                                    : 0.18 + 0.82 * scrollEdgeEase
                                opacity: revealOpacity * scrollEdgeOpacity
                                transform: [
                                    Scale {
                                        origin.x: gameTile.width / 2
                                        origin.y: gameTile.height / 2
                                        xScale: gameTile.revealScale
                                        yScale: gameTile.revealScale
                                    },
                                    Translate {
                                        y: gameTile.revealOffsetY
                                    }
                                ]

                                Connections {
                                    target: window
                                    function onGridRevealTokenChanged() {
                                        if (settingsManager.libraryView !== "grid")
                                            return
                                        if (settingsManager.reducedMotion) {
                                            revealFailsafeTimer.stop()
                                            gameTile.revealOpacity = 1
                                            gameTile.revealScale = 1
                                            gameTile.revealOffsetY = 0
                                            return
                                        }
                                        gameTile.revealOpacity = 0
                                        gameTile.revealScale = 0.94
                                        gameTile.revealOffsetY = 26
                                        revealFailsafeTimer.restart()
                                        revealAnimation.restart()
                                    }
                                }

                                SequentialAnimation {
                                    id: revealAnimation
                                    PauseAnimation {
                                        // Reveal the bottom row first, then work upward.
                                        // Cards within each row follow from left to right.
                                        duration: gameTile.revealDelayMs
                                    }
                                    ParallelAnimation {
                                        NumberAnimation {
                                            target: gameTile
                                            property: "revealOpacity"
                                            to: 1
                                            duration: 460
                                            easing.type: Easing.OutCubic
                                        }
                                        NumberAnimation {
                                            target: gameTile
                                            property: "revealScale"
                                            to: 1
                                            duration: 520
                                            easing.type: Easing.OutCubic
                                        }
                                        NumberAnimation {
                                            target: gameTile
                                            property: "revealOffsetY"
                                            to: 0
                                            duration: 520
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                    onFinished: revealFailsafeTimer.stop()
                                }

                                Component.onCompleted: {
                                    if (window.initialGridRevealPending
                                            && settingsManager.libraryView === "grid")
                                        revealFailsafeTimer.start()
                                }

                                readonly property bool highlighted:
                                    tileMouse.containsMouse || gameTile.GridView.isCurrentItem
                                // The blurred fill is useful for landscape covers with
                                // letterboxing, but duplicates portrait cover art. Keep
                                // portrait artwork clean so it does not ghost during fades.
                                readonly property bool useBlurredArtworkBackdrop:
                                    gameTile.platform !== "N64"
                                    && gameTileCover.status === Image.Ready
                                    && gameTileCover.sourceSize.width > gameTileCover.sourceSize.height
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
                                            // Every platform uses the same portrait
                                            // artwork window. Landscape covers remain
                                            // uncropped in front of a fitted blurred fill.
                                            Layout.preferredHeight: Math.round((gameTile.width - 20) * 1.5)
                                            Layout.minimumHeight: 96
                                            Layout.maximumHeight: Math.round((gameTile.width - 20) * 1.5)
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
                                                horizontalAlignment: Image.AlignHCenter
                                                verticalAlignment: Image.AlignVCenter
                                                asynchronous: true
                                                cache: true
                                                visible: gameTile.useBlurredArtworkBackdrop
                                                opacity: 0.42
                                            }
                                            MultiEffect {
                                                anchors.fill: parent
                                                source: artworkBackdrop
                                                blurEnabled: true
                                                blur: 0.8
                                                autoPaddingEnabled: false
                                                visible: gameTile.useBlurredArtworkBackdrop
                                            }
                                            Rectangle {
                                                anchors.fill: parent
                                                color: theme.artworkOverlay
                                                visible: gameTile.artworkPath.length > 0
                                            }
                                            Image {
                                                id: gameTileCover
                                                anchors.fill: parent
                                                // Portrait covers should fill the artwork frame.
                                                // Crop only the excess edge when their aspect ratio
                                                // differs from the shared 2:3 window. Landscape
                                                // covers retain their inset, fitted presentation.
                                                readonly property bool isPortraitArtwork:
                                                    sourceSize.height > sourceSize.width
                                                anchors.margins: isPortraitArtwork ? 1 : 5
                                                source: gameTile.artworkPath
                                                fillMode: isPortraitArtwork
                                                    ? Image.PreserveAspectCrop
                                                    : Image.PreserveAspectFit
                                                asynchronous: true
                                                cache: true
                                                visible: status === Image.Ready
                                            }
                                            Image {
                                                anchors.fill: parent
                                                anchors.margins: 5
                                                source: "qrc:/icons/game-artwork-unavailable.png"
                                                fillMode: Image.PreserveAspectFit
                                                asynchronous: true
                                                cache: true
                                                visible: gameTileCover.status !== Image.Ready
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
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }
                        }

                        ListView {
                            id: gameList
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            // List rows should use more of the available canvas
                            // while retaining a readable maximum width.
                            width: Math.min(parent.width,
                                Math.max(420, Math.min(parent.width * 0.68, 1200)))
                            visible: settingsManager.libraryView === "list" || opacity > 0
                            opacity: settingsManager.libraryView === "list" ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                            }
                            clip: true
                            // Leave room for the selected row's scale animation so its
                            // artwork and card edges are not clipped at the viewport.
                            leftMargin: 18
                            rightMargin: 18
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
                                width: gameList.width - gameList.leftMargin - gameList.rightMargin
                                readonly property int rowHeight:
                                    settingsManager.libraryListDensity === "compact" ? 104
                                    : settingsManager.libraryListDensity === "spacious" ? 144 : 128
                                height: rowHeight
                                // Match Grid View's scroll-edge treatment so both
                                // presentations share the same 140 px fade zone.
                                opacity: settingsManager.reducedMotion ? 1
                                    : Math.max(0, Math.min(1,
                                        Math.min(
                                            (listTile.y + listTile.height / 2 - gameList.contentY) / 140,
                                            (gameList.contentY + gameList.height
                                                - (listTile.y + listTile.height / 2)) / 140)))
                                focus: ListView.isCurrentItem
                                z: highlighted ? 2 : 1
                                scale: highlighted ? 1.035 : 1.0
                                transformOrigin: Item.Center
                                Behavior on scale {
                                    enabled: !settingsManager.reducedMotion
                                    NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                }
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
                                    clip: true
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop {
                                            position: 0.0
                                            color: listTile.highlighted
                                                ? Qt.rgba(theme.elevatedSurface.r, theme.elevatedSurface.g,
                                                          theme.elevatedSurface.b, 0.94)
                                                : Qt.rgba(theme.surface.r, theme.surface.g,
                                                          theme.surface.b, 0.64)
                                        }
                                        GradientStop {
                                            position: 0.58
                                            color: listTile.highlighted
                                                ? Qt.rgba(theme.elevatedSurface.r, theme.elevatedSurface.g,
                                                          theme.elevatedSurface.b, 0.68)
                                                : Qt.rgba(theme.surface.r, theme.surface.g,
                                                          theme.surface.b, 0.34)
                                        }
                                        GradientStop {
                                            position: 1.0
                                            color: listTile.highlighted
                                                ? Qt.rgba(theme.elevatedSurface.r, theme.elevatedSurface.g,
                                                          theme.elevatedSurface.b, 0.20)
                                                : Qt.rgba(theme.surface.r, theme.surface.g,
                                                          theme.surface.b, 0.035)
                                        }
                                    }
                                    border.width: listTile.highlighted ? 2 : 1
                                    border.color: listTile.highlighted ? theme.accent
                                        : Qt.rgba(theme.divider.r, theme.divider.g,
                                                  theme.divider.b, 0.28)
                                    Behavior on border.color {
                                        enabled: !settingsManager.reducedMotion
                                        ColorAnimation { duration: 170; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on border.width {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
                                    }
                                }
                                Item {
                                    id: listArtworkFrame
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: settingsManager.libraryListDensity === "compact" ? 68
                                        : settingsManager.libraryListDensity === "spacious" ? 94 : 86
                                    height: settingsManager.libraryListDensity === "compact" ? 82
                                        : settingsManager.libraryListDensity === "spacious" ? 116 : 104
                                    Image {
                                        id: listCoverImage
                                        anchors.fill: parent
                                        source: listTile.artworkPath
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        cache: true
                                        visible: status === Image.Ready
                                    }
                                    Image {
                                        anchors.fill: parent
                                        source: "qrc:/icons/game-artwork-unavailable.png"
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        cache: true
                                        visible: listCoverImage.status !== Image.Ready
                                    }
                                }
                                ColumnLayout {
                                    anchors.left: parent.left
                                    anchors.leftMargin: settingsManager.libraryListDensity === "compact" ? 90
                                        : settingsManager.libraryListDensity === "spacious" ? 122 : 114
                                    anchors.right: parent.right
                                    anchors.rightMargin: 18
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5
                                    Label { Layout.fillWidth: true; text: listTile.title; color: theme.textPrimary; font.pixelSize: (settingsManager.libraryListDensity === "compact" ? 14 : settingsManager.libraryListDensity === "spacious" ? 17 : 16) * theme.fontScale; font.bold: true; elide: Text.ElideRight }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: listTile.platform; color: theme.textSecondary; font.pixelSize: 11 * theme.fontScale }
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
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }
                        }

                        ListView {
                            id: consoleView
                            anchors.fill: parent
                            visible: settingsManager.libraryView === "carousel" || opacity > 0
                            opacity: settingsManager.libraryView === "carousel" ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 210; easing.type: Easing.OutCubic }
                            }
                            orientation: ListView.Horizontal
                            clip: true
                            model: gameGrid.model
                            keyNavigationEnabled: false
                            spacing: 24
                            // Scale card dimensions gently with the available carousel
                            // height. This preserves the density choices on smaller windows
                            // while giving fullscreen/high-resolution layouts more presence.
                            readonly property real layoutScale:
                                Math.min(1.2, Math.max(0.9, height / 900))
                            readonly property real cardWidth:
                                (settingsManager.libraryConsoleDensity === "compact" ? 220
                                : settingsManager.libraryConsoleDensity === "comfortable" ? 270 : 320)
                                * layoutScale
                            // Keep the complete hover enlargement inside the viewport.
                            readonly property real cardHeight:
                                Math.min(cardWidth * 1.48, Math.max(220, height - 64))
                            topMargin: Math.max(24, (height - cardHeight) / 2)
                            bottomMargin: topMargin
                            // Pin the current card to the same left inset throughout
                            // navigation. Matching trailing space allows the final card
                            // to settle on that exact inset too.
                            leftMargin: 34
                            rightMargin: Math.max(leftMargin, width - leftMargin - cardWidth)
                            preferredHighlightBegin: leftMargin
                            preferredHighlightEnd: preferredHighlightBegin + cardWidth
                            highlightRangeMode: ListView.StrictlyEnforceRange
                            highlightMoveDuration: settingsManager.reducedMotion ? 0 : 230
                            snapMode: ListView.NoSnap
                            currentIndex: gameGrid.currentIndex
                            // Mouse-wheel navigation complements hover selection.
                            // This is especially important with left-anchored selection:
                            // earlier cards may be off-screen when the final card is active.
                            property real wheelAccumulator: 0
                            WheelHandler {
                                target: null
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: function(event) {
                                    const delta = event.angleDelta.x !== 0 ? event.angleDelta.x
                                        : event.angleDelta.y !== 0 ? event.angleDelta.y
                                        : event.pixelDelta.x !== 0 ? event.pixelDelta.x * 4
                                        : event.pixelDelta.y * 4
                                    if (delta === 0 || consoleView.count <= 0)
                                        return

                                    // Horizontal wheel right advances; vertical wheel up
                                    // moves toward earlier cards. Accumulation avoids making
                                    // high-resolution touchpad gestures jump one card per event.
                                    consoleView.wheelAccumulator += delta
                                    const steps = Math.trunc(consoleView.wheelAccumulator / 120)
                                    if (steps !== 0) {
                                        const horizontal = event.angleDelta.x !== 0
                                            || event.pixelDelta.x !== 0
                                        const direction = horizontal ? 1 : -1
                                        consoleView.currentIndex = Math.max(0, Math.min(
                                            consoleView.count - 1,
                                            consoleView.currentIndex + direction * steps))
                                        consoleView.wheelAccumulator -= steps * 120
                                    }
                                    event.accepted = true
                                }
                            }
                            Component.onCompleted: {
                                if (currentIndex < 0 && count > 0)
                                    currentIndex = 0
                            }
                            onCountChanged: {
                                if (currentIndex < 0 && count > 0)
                                    currentIndex = 0
                            }
                            onCurrentIndexChanged: {
                                if (currentIndex !== gameGrid.currentIndex)
                                    gameGrid.currentIndex = currentIndex
                            }
                            Connections {
                                target: gameGrid
                                function onCurrentIndexChanged() {
                                    if (consoleView.currentIndex !== gameGrid.currentIndex)
                                        consoleView.currentIndex = gameGrid.currentIndex
                                }
                            }
                            delegate: Item {
                                id: consoleTile
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
                                width: consoleView.cardWidth
                                height: consoleView.cardHeight
                                readonly property bool highlighted:
                                    ListView.isCurrentItem || consoleTileMouse.containsMouse
                                readonly property real edgeDistance: Math.min(
                                    consoleTile.x + consoleTile.width / 2 - consoleView.contentX,
                                    consoleView.contentX + consoleView.width
                                        - (consoleTile.x + consoleTile.width / 2))
                                readonly property real edgeProgress: settingsManager.reducedMotion
                                    ? 1 : Math.max(0, Math.min(1, edgeDistance / 190))
                                readonly property real edgeEase:
                                    edgeProgress * edgeProgress * (3 - 2 * edgeProgress)
                                opacity: settingsManager.reducedMotion ? 1 : 0.18 + 0.82 * edgeEase
                                scale: highlighted ? 1.045 : 1
                                z: highlighted ? 2 : 1
                                Behavior on scale {
                                    enabled: !settingsManager.reducedMotion
                                    NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                                }
                                function openDetails() {
                                    window.selectedGame = {
                                        id: consoleTile.id, title: consoleTile.title,
                                        platform: consoleTile.platform, path: consoleTile.path,
                                        artworkPath: consoleTile.artworkPath,
                                        screenshots: consoleTile.screenshots,
                                        metadataTitle: consoleTile.metadataTitle,
                                        metadataOverview: consoleTile.metadataOverview,
                                        metadataReleaseDate: consoleTile.metadataReleaseDate,
                                        metadataRating: consoleTile.metadataRating
                                    }
                                    gameDetailsPopup.open()
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radiusCard + 3
                                    color: consoleTile.highlighted ? theme.elevatedSurface : theme.surface
                                    opacity: consoleTile.highlighted ? 0.96 : 0.72
                                    border.width: consoleTile.highlighted ? 2 : 1
                                    border.color: consoleTile.highlighted ? theme.accent : theme.divider
                                    Behavior on border.color {
                                        enabled: !settingsManager.reducedMotion
                                        ColorAnimation { duration: 170 }
                                    }
                                }
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 11
                                    spacing: 9
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: theme.radiusSmall
                                        color: theme.sidebarBackground
                                        clip: true
                                        Image {
                                            id: consoleCover
                                            anchors.fill: parent
                                            // Match Grid: portrait covers fill the inner frame,
                                            // while landscape covers keep their inset fitted layout.
                                            readonly property bool isPortraitArtwork:
                                                sourceSize.height > sourceSize.width
                                            anchors.margins: isPortraitArtwork ? 1 : 5
                                            source: consoleTile.artworkPath
                                            fillMode: isPortraitArtwork
                                                ? Image.PreserveAspectCrop
                                                : Image.PreserveAspectFit
                                            asynchronous: true
                                            cache: true
                                            visible: status === Image.Ready
                                        }
                                        Image {
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            source: "qrc:/icons/game-artwork-unavailable.png"
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                            cache: true
                                            visible: consoleCover.status !== Image.Ready
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: consoleTile.title
                                        color: theme.textPrimary
                                        font.pixelSize: 15 * theme.fontScale
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: consoleTile.platform
                                        color: theme.textSecondary
                                        font.pixelSize: 11 * theme.fontScale
                                        elide: Text.ElideRight
                                    }
                                }
                                MouseArea {
                                    id: consoleTileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        // A card can slide beneath a stationary pointer as the
                                        // carousel recenters. Do not treat that as a new hover
                                        // selection or it can trigger a rapid card-to-card cascade.
                                        window.activeInputDevice = "mouse"
                                    }
                                    onPositionChanged: {
                                        // Only pointer movement should change the hovered card.
                                        if (containsMouse)
                                            consoleView.currentIndex = consoleTile.index
                                    }
                                    onClicked: {
                                        gameGrid.currentIndex = consoleTile.index
                                        consoleTile.openDetails()
                                    }
                                }
                            }
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                        }
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
                                id: displayContent
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
                                id: libraryContent
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
                                        text: "LIBRARY LOCATION & SCANNING"
                                        color: theme.textMuted; font.pixelSize: 10 * theme.fontScale
                                        font.bold: true; font.letterSpacing: 1
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
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 148
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: theme.divider
                                        clip: true
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: theme.backgroundTop }
                                            GradientStop { position: 1.0; color: theme.backgroundBottom }
                                        }
                                        Image {
                                            anchors.fill: parent
                                            visible: settingsManager.libraryBackgroundType === "custom"
                                                && settingsManager.customLibraryBackground.length > 0
                                            source: settingsManager.customLibraryBackground
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            cache: true
                                            smooth: true
                                        }
                                        Image {
                                            anchors.fill: parent
                                            visible: settingsManager.libraryBackgroundType === "animated"
                                            source: libraryVisualSettings.wavePalette === "violet"
                                                ? "qrc:/qt/qml/RecompStation/assets/library-waves-violet.svg"
                                                : libraryVisualSettings.wavePalette === "emerald"
                                                    ? "qrc:/qt/qml/RecompStation/assets/library-waves-emerald.svg"
                                                    : libraryVisualSettings.wavePalette === "ember"
                                                        ? "qrc:/qt/qml/RecompStation/assets/library-waves-ember.svg"
                                                        : "qrc:/qt/qml/RecompStation/assets/library-waves.svg"
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            cache: true
                                        }
                                        Rectangle {
                                            anchors.fill: parent
                                            color: Qt.rgba(theme.windowBackground.r,
                                                theme.windowBackground.g, theme.windowBackground.b, 0.24)
                                        }
                                        Label {
                                            anchors.left: parent.left
                                            anchors.bottom: parent.bottom
                                            anchors.margins: 12
                                            text: settingsManager.libraryBackgroundType === "custom"
                                                && settingsManager.customLibraryBackground.length === 0
                                                ? "Choose an image to preview it"
                                                : settingsManager.libraryBackgroundType === "animated"
                                                    ? "Animated ambience preview"
                                                    : settingsManager.libraryBackgroundType === "custom"
                                                        ? "Custom background preview" : "Current theme background"
                                            color: theme.textPrimary
                                            font.pixelSize: 12 * theme.fontScale
                                            font.bold: true
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
                                        id: customBackgroundActionsRow
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
                                    Rectangle {
                                        Layout.fillWidth: true
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: theme.divider
                                        implicitHeight: libraryRootColumn.implicitHeight + 28
                                        ColumnLayout {
                                            id: libraryRootColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 9
                                            Label {
                                                text: "Current library folder"
                                                color: theme.textPrimary
                                                font.pixelSize: 13 * theme.fontScale
                                                font.bold: true
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: platformManager.libraryConfigured
                                                    ? platformManager.libraryDirectory
                                                    : "No library folder selected"
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.WrapAnywhere
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Changing this location does not move or delete game files. Choose an existing folder; RecompStation will create its supported platform subfolders and scan it."
                                                color: theme.textMuted
                                                font.pixelSize: 10 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                            Button {
                                                Layout.alignment: Qt.AlignRight
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                text: "Change Library Folder"
                                                enabled: platformManager.libraryConfigured
                                                onClicked: libraryFolderDialog.open()
                                                background: Rectangle {
                                                    radius: 8
                                                    color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.20)
                                                        : (parent.highlighted ? theme.accent : theme.surface)
                                                    border.color: parent.activeFocus ? theme.accent : theme.divider
                                                    border.width: parent.activeFocus ? 2 : 1
                                                }
                                            }
                                        }
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
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Place game files inside their matching platform folders. RecompStation monitors these folders and updates your library when games are added or removed."
                                        color: theme.textMuted; font.pixelSize: 11 * theme.fontScale; wrapMode: Text.Wrap
                                    }
                                }
                            }

                            ScrollView {
                                id: applicationContent
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
                                    text: "APPLICATION UPDATES"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    radius: theme.radiusCard
                                    color: theme.sidebarBackground
                                    border.color: theme.divider
                                    implicitHeight: updateSettingsColumn.implicitHeight + 28
                                    ColumnLayout {
                                        id: updateSettingsColumn
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 8
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 16
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "Check for application updates automatically"
                                                    color: theme.textPrimary
                                                    font.pixelSize: 13 * theme.fontScale
                                                    font.bold: true
                                                    wrapMode: Text.Wrap
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "Saved preference for future release-check support."
                                                    color: theme.textMuted
                                                    font.pixelSize: 11 * theme.fontScale
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                            Switch {
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                checked: settingsManager.automaticUpdateChecksEnabled
                                                onToggled: settingsManager.setAutomaticUpdateChecksEnabled(checked)
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Automatic release checking is not connected in this build. Changing this setting only saves your preference; no network requests are made."
                                            color: theme.textMuted
                                            font.pixelSize: 10 * theme.fontScale
                                            wrapMode: Text.Wrap
                                        }
                                    }
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
                                        TextField {
                                            Layout.fillWidth: true
                                            text: platformManager.dataDirectory
                                            readOnly: true
                                            selectByMouse: true
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            color: theme.textSecondary
                                            font.pixelSize: 12 * theme.fontScale
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.activeFocus ? theme.elevatedSurface : theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                            }
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
                                Label {
                                    text: "DIAGNOSTICS"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    radius: theme.radiusCard
                                    color: theme.sidebarBackground
                                    border.color: theme.divider
                                    implicitHeight: diagnosticsColumn.implicitHeight + 28
                                    ColumnLayout {
                                        id: diagnosticsColumn
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 7
                                        Label {
                                            text: "Runtime log"
                                            color: theme.textPrimary
                                            font.pixelSize: 13 * theme.fontScale
                                            font.bold: true
                                        }
                                        TextField {
                                            Layout.fillWidth: true
                                            text: platformManager.dataDirectory + "/logs/runtime.log"
                                            readOnly: true
                                            selectByMouse: true
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            color: theme.textSecondary
                                            font.pixelSize: 11 * theme.fontScale
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.activeFocus ? theme.elevatedSurface : theme.surface
                                                border.color: parent.activeFocus ? theme.accent : theme.divider
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "The log records application, library, metadata, controller, and launcher activity. It rotates at 5 MiB and retains three archived logs."
                                            color: theme.textMuted
                                            font.pixelSize: 10 * theme.fontScale
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                                Label {
                                    text: "MANAGED LAUNCHERS"
                                    color: theme.textMuted
                                    font.pixelSize: 10 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "These are Linux launchers installed and managed by RecompStation. Removing one here only removes its managed installation; game files and saves are not affected."
                                    color: theme.textSecondary
                                    font.pixelSize: 11 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Repeater {
                                        model: window.launcherRevision >= 0
                                            ? launcherDownloadManager.installedLaunchers() : []
                                        delegate: Rectangle {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            radius: theme.radiusCard
                                            color: theme.sidebarBackground
                                            border.color: theme.divider
                                            implicitHeight: launcherEntryColumn.implicitHeight + 24
                                            ColumnLayout {
                                                id: launcherEntryColumn
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.margins: 12
                                                spacing: 5
                                                Label {
                                                    text: modelData.projectId
                                                    color: theme.textPrimary
                                                    font.pixelSize: 13 * theme.fontScale
                                                    font.bold: true
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: modelData.executablePath
                                                    color: theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                    wrapMode: Text.WrapAnywhere
                                                }
                                                Button {
                                                    id: uninstallManagedLauncherButton
                                                    Layout.alignment: Qt.AlignRight
                                                    Layout.minimumWidth: 132
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "Uninstall"
                                                    Accessible.name: "Uninstall " + modelData.projectId
                                                    background: Rectangle {
                                                        radius: 7
                                                        color: uninstallManagedLauncherButton.activeFocus
                                                            ? theme.controlHover : theme.surface
                                                        border.width: uninstallManagedLauncherButton.activeFocus ? 2 : 1
                                                        border.color: uninstallManagedLauncherButton.activeFocus
                                                            ? theme.accent : theme.divider
                                                    }
                                                    contentItem: Label {
                                                        text: uninstallManagedLauncherButton.text
                                                        color: uninstallManagedLauncherButton.activeFocus
                                                            ? theme.textPrimary : theme.textSecondary
                                                        font.pixelSize: 12 * theme.fontScale
                                                        font.bold: uninstallManagedLauncherButton.activeFocus
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    onClicked: {
                                                        window.pendingUninstallProjectId = modelData.projectId
                                                        uninstallDialog.open()
                                                        Qt.callLater(function() {
                                                            uninstallDialog.standardButton(Dialog.Ok).forceActiveFocus()
                                                        })
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: launcherDownloadManager.installedLaunchers().length === 0
                                        text: "No managed launchers are currently installed."
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
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
                                id: metadataContent
                                anchors.fill: parent
                                anchors.margins: 30
                                clip: true
                                visible: settingsPage.section === "Metadata"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 16

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
                                        text: "AUTOMATION"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: automaticMetadataColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: settingsManager.automaticMetadataMatchingEnabled
                                            ? theme.accent : theme.divider
                                        ColumnLayout {
                                            id: automaticMetadataColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 8
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "Automatically match metadata"
                                                    checked: settingsManager.automaticMetadataMatchingEnabled
                                                    onToggled: settingsManager.setAutomaticMetadataMatchingEnabled(checked)
                                                    background: Rectangle {
                                                        radius: 8
                                                        color: parent.activeFocus
                                                            ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"
                                                        border.color: parent.activeFocus ? theme.accent : "transparent"
                                                        border.width: parent.activeFocus ? 2 : 0
                                                        anchors.margins: -6
                                                    }
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label {
                                                    text: settingsManager.automaticMetadataMatchingEnabled
                                                        ? "Enabled" : "Disabled"
                                                    color: settingsManager.automaticMetadataMatchingEnabled
                                                        ? "#48C78E" : theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                    font.bold: true
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Automatically identify newly discovered games after a library scan. When disabled, use Refresh All Metadata to run matching manually."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: automaticArtworkColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: settingsManager.automaticArtworkDownloadsEnabled
                                            ? theme.accent : theme.divider
                                        ColumnLayout {
                                            id: automaticArtworkColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 8
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "Automatically download artwork"
                                                    checked: settingsManager.automaticArtworkDownloadsEnabled
                                                    onToggled: settingsManager.setAutomaticArtworkDownloadsEnabled(checked)
                                                    background: Rectangle {
                                                        radius: 8
                                                        color: parent.activeFocus
                                                            ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"
                                                        border.color: parent.activeFocus ? theme.accent : "transparent"
                                                        border.width: parent.activeFocus ? 2 : 0
                                                        anchors.margins: -6
                                                    }
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label {
                                                    text: settingsManager.automaticArtworkDownloadsEnabled
                                                        ? "Enabled" : "Disabled"
                                                    color: settingsManager.automaticArtworkDownloadsEnabled
                                                        ? "#48C78E" : theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                    font.bold: true
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Download cover artwork and available gameplay screenshots automatically after games are matched. Refresh All Metadata can still retrieve them manually."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

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

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: launchBoxProviderColumn.implicitHeight + 28
                                        radius: theme.radiusCard
                                        color: theme.sidebarBackground
                                        border.color: settingsManager.launchBoxEnabled ? theme.accent : theme.divider
                                        ColumnLayout {
                                            id: launchBoxProviderColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            spacing: 9
                                            RowLayout {
                                                Layout.fillWidth: true
                                                CheckBox {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "LaunchBox Games Database"
                                                    checked: settingsManager.launchBoxEnabled
                                                    onToggled: settingsManager.setLaunchBoxEnabled(checked)
                                                    background: Rectangle {
                                                        radius: 8
                                                        color: parent.activeFocus ? Qt.rgba(0.45, 0.45, 0.45, 0.18) : "transparent"
                                                        border.color: parent.activeFocus ? theme.accent : "transparent"
                                                        border.width: parent.activeFocus ? 2 : 0
                                                        anchors.margins: -6
                                                    }
                                                }
                                                Item { Layout.fillWidth: true }
                                                Label {
                                                    text: !settingsManager.launchBoxEnabled ? "Disabled"
                                                        : launchBoxCatalogManager.catalogAvailable ? "Catalog ready" : "Enabled · catalog not downloaded"
                                                    color: launchBoxCatalogManager.catalogAvailable && settingsManager.launchBoxEnabled
                                                        ? "#48C78E" : theme.textMuted
                                                    font.pixelSize: 10 * theme.fontScale
                                                    font.bold: true
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Optional local catalog for LaunchBox metadata matching. TheGamesDB remains the primary provider. Enabling LaunchBox does not download anything automatically."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: launchBoxCatalogManager.catalogAvailable
                                                    ? "Local catalog: available"
                                                    : "Local catalog: not installed"
                                                color: theme.textMuted
                                                font.pixelSize: 11 * theme.fontScale
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: settingsManager.launchBoxLastUpdated.length > 0
                                                    ? "Last successful update: " + settingsManager.launchBoxLastUpdated
                                                    : "Last updated: never"
                                                color: theme.textMuted
                                                font.pixelSize: 11 * theme.fontScale
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 10
                                                Button {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    Layout.preferredWidth: 190
                                                    Layout.preferredHeight: 40
                                                    highlighted: true
                                                    text: launchBoxCatalogManager.busy ? "Updating Catalog…" : "Update Catalog"
                                                    enabled: settingsManager.launchBoxEnabled && !launchBoxCatalogManager.busy
                                                    onClicked: launchBoxCatalogManager.updateCatalog()
                                                }
                                                Button {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    Layout.preferredWidth: 110
                                                    Layout.preferredHeight: 40
                                                    text: "Cancel"
                                                    visible: launchBoxCatalogManager.busy
                                                    enabled: launchBoxCatalogManager.busy
                                                    onClicked: launchBoxCatalogManager.cancelUpdate()
                                                }
                                            }
                                            ProgressBar {
                                                Layout.fillWidth: true
                                                visible: launchBoxCatalogManager.busy
                                                from: 0
                                                to: 100
                                                value: launchBoxProgressPercent
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                visible: launchBoxCatalogManager.busy || launchBoxStatusMessage.length > 0
                                                text: launchBoxStatusMessage
                                                color: theme.textMuted
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                visible: launchBoxArtworkManager.status.length > 0
                                                text: launchBoxArtworkManager.status
                                                color: theme.textMuted
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: "The catalog download is approximately 106 MB compressed. Updates are manual, and the existing catalog remains available until a replacement has been validated."
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }

                                    Rectangle { Layout.fillWidth: true; height: 1; color: theme.divider }
                                    Label {
                                        text: "UNIVERSAL LIBRARY REFRESH"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Refresh missing game metadata, cover artwork, and gameplay screenshots across your library. Existing matches and downloaded images are preserved and skipped."
                                        color: theme.textSecondary
                                        font.pixelSize: 12 * theme.fontScale
                                        wrapMode: Text.Wrap
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
                                        Layout.preferredWidth: 230
                                        Layout.preferredHeight: 44
                                        text: window.universalRefreshPending || metadataMatcher.busy
                                            ? "Refreshing Library…" : "Refresh All Metadata"
                                        highlighted: true
                                        enabled: (metadataManager.configured
                                            || (settingsManager.launchBoxEnabled
                                                && launchBoxCatalogManager.catalogAvailable))
                                            && !window.universalRefreshPending
                                            && !metadataMatcher.busy
                                            && !screenshotManager.busy
                                            && !launchBoxCatalogManager.busy
                                        onClicked: {
                                            window.universalRefreshPending = true
                                            window.scanMessage = "Refreshing metadata and available artwork for your library…"
                                            metadataMatcher.scrapeLibrary(true, true)
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: metadataMatcher.status
                                        visible: metadataMatcher.status.length > 0
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: screenshotManager.status
                                        visible: screenshotManager.status.length > 0
                                        color: theme.textMuted
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }

                            ScrollView {
                                id: controllerContent
                                anchors.fill: parent
                                anchors.margins: 30
                                clip: true
                                visible: settingsPage.section === "Controller"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 16

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
                                            spacing: 8
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
                                                Button {
                                                    id: resetControllerProfileButton
                                                    text: "Reset to defaults"
                                                    enabled: controllerManager.connected
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    background: Rectangle {
                                                        radius: theme.radiusSmall
                                                        color: resetControllerProfileButton.activeFocus
                                                            ? theme.elevatedSurface : theme.surface
                                                        border.color: resetControllerProfileButton.activeFocus
                                                            ? theme.accent : theme.divider
                                                        border.width: resetControllerProfileButton.activeFocus ? 2 : 1
                                                    }
                                                    onClicked: {
                                                        settingsManager.resetControllerProfile(controllerManager.controllerGuid)
                                                        controllerManager.reloadControllerProfile()
                                                    }
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: controllerManager.connected
                                                    ? "Mappings are saved separately for this controller. Add another button or stick direction to any action."
                                                    : "Connect a controller to configure its device-specific mappings."
                                                color: theme.textSecondary
                                                font.pixelSize: 11 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    Label {
                                        text: "BUTTON MAPPINGS"
                                        color: theme.textMuted
                                        font.pixelSize: 10 * theme.fontScale
                                        font.bold: true
                                        font.letterSpacing: 1
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: controllerManager.mappingCaptureActive
                                            ? "Press the controller input you want to add. Release the button used to start capture first; capture cancels after 15 seconds."
                                            : "Choose Add binding, then press a button, D-pad direction, or left-stick direction. Existing bindings are kept."
                                        color: controllerManager.mappingCaptureActive ? theme.accent : theme.textSecondary
                                        font.pixelSize: 11 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Repeater {
                                        model: [
                                            { key: "navigateUp", label: "Navigate up" },
                                            { key: "navigateDown", label: "Navigate down" },
                                            { key: "navigateLeft", label: "Navigate left" },
                                            { key: "navigateRight", label: "Navigate right" },
                                            { key: "select", label: "Select / confirm" },
                                            { key: "back", label: "Back" },
                                            { key: "menu", label: "Open menu" },
                                            { key: "previousSection", label: "Previous section" },
                                            { key: "nextSection", label: "Next section" }
                                        ]
                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            implicitHeight: actionRow.implicitHeight + 20
                                            radius: theme.radiusSmall
                                            color: theme.sidebarBackground
                                            border.color: actionBindingButton.activeFocus
                                                || (controllerManager.mappingCaptureActive
                                                    && controllerManager.mappingCaptureAction === modelData.key)
                                                ? theme.accent : theme.divider
                                            border.width: actionBindingButton.activeFocus
                                                || (controllerManager.mappingCaptureActive
                                                    && controllerManager.mappingCaptureAction === modelData.key)
                                                ? 2 : 1
                                            RowLayout {
                                                id: actionRow
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 12
                                                spacing: 12
                                                ColumnLayout {
                                                    id: actionDetailsColumn
                                                    Layout.fillWidth: true
                                                    spacing: 3
                                                    Label {
                                                        text: modelData.label
                                                        color: theme.textPrimary
                                                        font.pixelSize: 12 * theme.fontScale
                                                        font.bold: true
                                                    }
                                                    property string actionKey: modelData.key
                                                    Flow {
                                                        Layout.fillWidth: true
                                                        spacing: 6
                                                        Repeater {
                                                            id: bindingRepeater
                                                            model: {
                                                                const revision = controllerManager.profileRevision
                                                                const profile = settingsManager.controllerProfile(controllerManager.controllerGuid)
                                                                return profile[actionDetailsColumn.actionKey] || []
                                                            }
                                                            delegate: Rectangle {
                                                                required property var modelData
                                                                required property int index
                                                                implicitWidth: bindingChipRow.implicitWidth + 12
                                                                implicitHeight: 30
                                                                radius: theme.radiusSmall
                                                                color: theme.surface
                                                                border.color: theme.divider
                                                                RowLayout {
                                                                    id: bindingChipRow
                                                                    anchors.fill: parent
                                                                    anchors.leftMargin: 6
                                                                    anchors.rightMargin: 4
                                                                    spacing: 5
                                                                    Label {
                                                                        text: modelData
                                                                        color: theme.textSecondary
                                                                        font.pixelSize: 10 * theme.fontScale
                                                                    }
                                                                    Button {
                                                                        id: removeBindingButton
                                                                        text: "×"
                                                                        enabled: bindingRepeater.count > 1 && controllerManager.connected
                                                                        activeFocusOnTab: true
                                                                        focusPolicy: Qt.StrongFocus
                                                                        implicitWidth: 24
                                                                        implicitHeight: 24
                                                                        padding: 0
                                                                        font.pixelSize: 14
                                                                        background: Rectangle {
                                                                            radius: 5
                                                                            color: removeBindingButton.activeFocus
                                                                                ? theme.elevatedSurface : "transparent"
                                                                            border.color: removeBindingButton.activeFocus
                                                                                ? theme.accent : "transparent"
                                                                            border.width: removeBindingButton.activeFocus ? 2 : 0
                                                                        }
                                                                        onClicked: {
                                                                            const profile = settingsManager.controllerProfile(controllerManager.controllerGuid)
                                                                            const bindings = profile[actionDetailsColumn.actionKey] || []
                                                                            if (bindings.length <= 1)
                                                                                return
                                                                            const updated = bindings.slice()
                                                                            updated.splice(index, 1)
                                                                            profile[actionDetailsColumn.actionKey] = updated
                                                                            if (settingsManager.saveControllerProfile(controllerManager.controllerGuid, profile))
                                                                                controllerManager.reloadControllerProfile()
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                    Label {
                                                        visible: bindingRepeater.count === 0
                                                        text: "No bindings"
                                                        color: theme.textSecondary
                                                        font.pixelSize: 10 * theme.fontScale
                                                    }
                                                }
                                                Button {
                                                    id: actionBindingButton
                                                    text: controllerManager.mappingCaptureActive
                                                        && controllerManager.mappingCaptureAction === modelData.key
                                                        ? "Listening…" : "Add binding"
                                                    enabled: controllerManager.connected
                                                        && (!controllerManager.mappingCaptureActive
                                                            || controllerManager.mappingCaptureAction === modelData.key)
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    background: Rectangle {
                                                        radius: theme.radiusSmall
                                                        color: actionBindingButton.activeFocus
                                                            ? theme.elevatedSurface : theme.surface
                                                        border.color: actionBindingButton.activeFocus
                                                            ? theme.accent : theme.divider
                                                        border.width: actionBindingButton.activeFocus ? 2 : 1
                                                    }
                                                    onClicked: controllerManager.startMappingCapture(modelData.key)
                                                }
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: controllerManager.mappingCaptureActive
                                        Button {
                                            id: cancelMappingCaptureButton
                                            text: "Cancel capture"
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            background: Rectangle {
                                                radius: theme.radiusSmall
                                                color: cancelMappingCaptureButton.activeFocus
                                                    ? theme.elevatedSurface : theme.surface
                                                border.color: cancelMappingCaptureButton.activeFocus
                                                    ? theme.accent : theme.divider
                                                border.width: cancelMappingCaptureButton.activeFocus ? 2 : 1
                                            }
                                            onClicked: controllerManager.cancelMappingCapture()
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
                width: Math.min(parent.width - 48, 520)
                height: systemMenuContent.implicitHeight + 68
                radius: theme.radiusLarge
                color: theme.sidebarBackground
                border.color: theme.divider
                border.width: 1

                ColumnLayout {
                    id: systemMenuContent
                    anchors.fill: parent
                    anchors.margins: 32
                    spacing: 14

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
                        text: "Yes, quit"
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
                        text: window.navigationHintSummary()
                        color: theme.textMuted
                        font.pixelSize: 10 * theme.fontScale
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                    window.controllerBack("keyboard")
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
