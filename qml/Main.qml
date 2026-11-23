import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs
import QtCore
import "ArtworkLayout.js" as ArtworkLayout
import "LibraryGridLayout.js" as LibraryGridLayout

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
        // Retired SVG wave palette; read once to carry the choice over.
        property string wavePalette: "migrated"
        // Shader waves: a preset id from window.waveColorPresets or "custom".
        property string waveColor: "ocean"
        property int waveCustomHue: 220
        property string waveMotion: "normal"
        property string waveQuality: "auto"
        // Custom wallpaper: "fill" (crop to cover) or "fit" (whole image
        // over a blurred copy), and how much to darken it (percent).
        property string wallpaperFit: "fill"
        property int wallpaperDim: 15
        Component.onCompleted: {
            if (wavePalette !== "migrated") {
                waveColor = wavePalette === "violet" ? "violet"
                    : wavePalette === "emerald" ? "aurora" : "ocean"
                wavePalette = "migrated"
            }
        }
    }

    function inputHintItems() {
        if (activeInputDevice === "controller" && controllerManager.connected) {
            let selectButton = "A"
            let backButton = "B"
            let switchButtons = "LB / RB"
            let detailsButton = "Y"
            let selectColor = "#48C78E"
            let backColor = "#E58B78"
            if (controllerManager.controllerType === "playstation") {
                selectButton = "✕"
                backButton = "○"
                switchButtons = "L1 / R1"
                detailsButton = "△"
                selectColor = "#55A9F4"
                backColor = "#F06B8B"
            } else if (controllerManager.controllerType === "nintendo") {
                selectButton = "B"
                backButton = "A"
                switchButtons = "L / R"
                detailsButton = "X"
                selectColor = "#E58B78"
                backColor = "#48A8E8"
            } else if (controllerManager.controllerType !== "xbox") {
                selectButton = "S"
                backButton = "E"
                switchButtons = "L / R"
                detailsButton = "N"
            }
            const libraryActive = currentPage === "Library" && !gameDetailsPopup.visible
            const carouselPlays = libraryActive
                && (settingsManager.libraryView === "carousel" || settingsManager.libraryView === "list")
                && window.libraryGameInstalled((gameGrid.model || [])[Math.max(0, gameGrid.currentIndex)])
            const items = [
                { icon: "qrc:/icons/dpad.svg", key: "", label: "Navigate", color: "#B8C7D9", round: false },
                { icon: "", key: selectButton, label: carouselPlays ? "Play" : "Select", color: selectColor, round: true },
                { icon: "", key: backButton, label: "Back", color: backColor, round: true },
                { icon: "", key: switchButtons,
                  label: gameDetailsPopup.visible ? "Switch game"
                      : currentPage === "Library" ? "Platform" : "Section",
                  color: "#A9B7CA", round: false }
            ]
            if (libraryActive)
                items.splice(2, 0, { icon: "", key: detailsButton, label: "Details", color: "#E8C27F", round: true })
            return items
        }
        if (activeInputDevice === "mouse") {
            return [
                { icon: "qrc:/icons/mouse.svg", key: "", label: "Select", color: "#B8C7D9", round: false },
                { icon: "", key: "↕", label: "Navigate", color: "#A9B7CA", round: false },
                { icon: "", key: "Esc", label: "Back", color: "#A9B7CA", round: false }
            ]
        }
        if (currentPage === "Library" && !gameDetailsPopup.visible) {
            return [
                { icon: "", key: "← ↑ ↓ →", label: "Navigate", color: "#A9B7CA", round: false },
                { icon: "", key: "Enter", label: (settingsManager.libraryView === "carousel"
                    || settingsManager.libraryView === "list")
                    && window.libraryGameInstalled((gameGrid.model || [])[Math.max(0, gameGrid.currentIndex)])
                    ? "Play" : "Select",
                  color: "#A9B7CA", round: false },
                { icon: "", key: "I", label: "Details", color: "#A9B7CA", round: false },
                { icon: "", key: "Q / E", label: "Platform", color: "#A9B7CA", round: false },
                { icon: "", key: "/", label: "Search", color: "#A9B7CA", round: false },
                { icon: "", key: "F11", label: "Fullscreen", color: "#A9B7CA", round: false },
                { icon: "", key: "Ctrl+Q", label: "Quit", color: "#A9B7CA", round: false }
            ]
        }
        const keyboardItems = [
            { icon: "", key: "← ↑ ↓ →", label: "Navigate", color: "#A9B7CA", round: false },
            { icon: "", key: "Enter", label: "Select", color: "#A9B7CA", round: false },
            { icon: "", key: "Esc", label: "Back", color: "#A9B7CA", round: false },
            { icon: "", key: "F11", label: "Fullscreen", color: "#A9B7CA", round: false },
            { icon: "", key: "Ctrl+Q", label: "Quit", color: "#A9B7CA", round: false }
        ]
        if (currentPage === "Settings" && !gameDetailsPopup.visible)
            keyboardItems.splice(3, 0, { icon: "", key: "Q / E", label: "Section", color: "#A9B7CA", round: false })
        return keyboardItems
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
            : settingsPage.section === "Background" ? backgroundContent
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
        case "Background": return backgroundNavigationButton
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

    function libraryKeyboardShortcutsAllowed() {
        return window.visible && currentPage === "Library"
            && !systemMenuVisible && !gameDetailsPopup.visible
            && !uninstallDialog.visible && !librarySetupDialog.visible
            && window.keyboardDirectionalNavigationAllowed()
    }

    // Q/E switch Settings sections, except while typing in a text field.
    function settingsKeyboardShortcutsAllowed() {
        const focused = window.activeFocusItem
        const typing = focused && (focused instanceof TextInput || focused instanceof TextEdit)
        return window.visible && currentPage === "Settings"
            && !systemMenuVisible && !typing
            && !controllerManager.mappingCaptureActive
            && window.keyboardDirectionalNavigationAllowed()
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

        if (screenshotViewer.visible) {
            if (direction === "left")
                screenshotViewer.step(-1)
            else if (direction === "right")
                screenshotViewer.step(1)
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
            if (window.moveDirectionalFocus(gameDetailsPopup.contentFrame, direction))
                return
            if (direction === "up" || direction === "down") {
                const bar = gameDetailsPopup.infoScroll.ScrollBar.vertical
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
                window.focusLibraryContent()
            }
            return
        }

        if (currentPage === "Library" && gameGrid.count > 0) {
            let next = gameGrid.currentIndex < 0 ? 0 : gameGrid.currentIndex
            if (settingsManager.libraryView === "shelves") {
                const focusToolbar = function() {
                    const toolbarItems = []
                    window.collectFocusableItems(libraryFilterPanel, toolbarItems, [])
                    if (toolbarItems.length > 0)
                        toolbarItems[0].forceActiveFocus()
                }
                // Spotlight actions: left/right between buttons, up to the
                // toolbar, down to the shelves.
                if (window.isDescendantOf(window.activeFocusItem, shelvesSpotlight)) {
                    if (direction === "left" || direction === "right")
                        window.moveDirectionalFocus(spotlightActions, direction)
                    else if (direction === "up")
                        focusToolbar()
                    else if (direction === "down")
                        shelvesView.focusShelves()
                    return
                }
                const moved = LibraryGridLayout.moveShelfIndex(
                    shelvesView.sections, next, direction, shelvesView.shelfMemory)
                if (moved === LibraryGridLayout.toSpotlight) {
                    shelvesView.focusSpotlight()
                    return
                }
                if (moved < 0)
                    return
                gameGrid.currentIndex = moved
                shelvesView.positionAtIndex(moved)
                shelvesFlick.forceActiveFocus()
                return
            }
            if (settingsManager.libraryView === "list") {
                // Preview screenshots: left/right between them, left from the
                // first or up/down returns to the list.
                if (window.isDescendantOf(window.activeFocusItem, listPreviewShots)) {
                    if (direction === "left" || direction === "right") {
                        if (!window.moveDirectionalFocus(listPreviewShots, direction) && direction === "left")
                            gameList.forceActiveFocus()
                    } else {
                        gameList.forceActiveFocus()
                    }
                    return
                }
                if (direction === "right") {
                    const shots = []
                    window.collectFocusableItems(listPreviewShots, shots, [])
                    if (shots.length > 0)
                        shots[0].forceActiveFocus()
                    return
                }
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
            if (window.sectionedLibraryGrid) {
                const moved = LibraryGridLayout.moveIndex(
                    sectionedGrid.layout.sections, sectionedGrid.columns, next, direction)
                if (moved === LibraryGridLayout.leaveToToolbar) {
                    const toolbarItems = []
                    window.collectFocusableItems(libraryFilterPanel, toolbarItems, [])
                    if (toolbarItems.length > 0)
                        toolbarItems[0].forceActiveFocus()
                    return
                }
                if (moved < 0 || moved === gameGrid.currentIndex)
                    return
                gameGrid.currentIndex = moved
                sectionedGrid.positionAtIndex(moved)
                sectionedGrid.forceActiveFocus()
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
            const sections = window.settingsSections
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
                // Left/right change the value of a focused ◀ value ▶ row. At
                // the first value, left falls through to the category rail.
                const stepper = window.focusedValueStepper(focused)
                if (stepper && (direction === "right"
                        || (direction === "left" && stepper.canStepBack))) {
                    stepper.stepValue(direction === "left" ? -1 : 1)
                    return
                }
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
        // Toolbar buttons (platform pills, search, view, size) handle Select
        // themselves instead of opening the current game.
        if (currentPage === "Library"
                && window.isDescendantOf(window.activeFocusItem, libraryFilterPanel)) {
            const toolbarAction = window.focusedActionItem(window.activeFocusItem)
            if (toolbarAction && typeof toolbarAction.click === "function")
                toolbarAction.click()
            return
        }
        if (screenshotViewer.visible)
            return
        // List preview screenshots open the full-screen viewer.
        if (currentPage === "Library"
                && window.isDescendantOf(window.activeFocusItem, listPreview)) {
            const previewAction = window.focusedActionItem(window.activeFocusItem)
            if (previewAction && typeof previewAction.click === "function")
                previewAction.click()
            return
        }
        // Spotlight buttons (Play, Details) in the Shelves view.
        if (currentPage === "Library"
                && window.isDescendantOf(window.activeFocusItem, shelvesSpotlight)) {
            const spotlightAction = window.focusedActionItem(window.activeFocusItem)
            if (spotlightAction && typeof spotlightAction.click === "function")
                spotlightAction.click()
            return
        }
        if (currentPage === "Library" && gameGrid.count > 0) {
            const index = gameGrid.currentIndex < 0 ? 0 : gameGrid.currentIndex
            if (settingsManager.libraryView === "shelves") {
                window.openLibraryGameDetails(gameGrid.model[index])
                return
            }
            if (settingsManager.libraryView === "list") {
                window.libraryPrimaryAction(index)
            } else if (settingsManager.libraryView === "carousel") {
                window.libraryPrimaryAction(index)
            } else if (window.sectionedLibraryGrid) {
                window.openLibraryGameDetails(gameGrid.model[index])
            } else {
                const tile = gameGrid.itemAtIndex(index)
                if (tile && tile.openDetails)
                    tile.openDetails()
            }
        } else if (currentPage === "Settings") {
            const stepper = window.focusedValueStepper(window.activeFocusItem)
            if (stepper) {
                stepper.stepValue(1)
                return
            }
            const focused = window.focusedActionItem(window.activeFocusItem)
            if (focused && focused.popup) {
                if (focused.popup.visible) {
                    focused.popup.close()
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
        if (screenshotViewer.visible) {
            screenshotViewer.close()
            return
        }
        if (gameDetailsPopup.visible)
            gameDetailsPopup.close()
        else if (currentPage === "Library"
                 && window.isDescendantOf(window.activeFocusItem, listPreviewShots))
            gameList.forceActiveFocus()
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
        if (controllerManager.mappingCaptureActive && currentPage !== "Settings")
            controllerManager.cancelMappingCapture()
        if (currentPage === "Settings")
            Qt.callLater(function() { window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus() })
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
    // Library keyboard shortcuts, matching the controller: Q/E cycle platform
    // filters (LB/RB), "/" or Ctrl+F opens search. They are
    // disabled while a text field or open popup owns the keyboard.
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: window.settingsKeyboardShortcutsAllowed()
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.cycleSettingsSection(-1)
        }
    }
    Shortcut {
        sequence: "E"
        context: Qt.ApplicationShortcut
        enabled: window.settingsKeyboardShortcutsAllowed()
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.cycleSettingsSection(1)
        }
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: window.libraryKeyboardShortcutsAllowed()
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.cycleLibraryPlatform(-1)
        }
    }
    Shortcut {
        sequence: "E"
        context: Qt.ApplicationShortcut
        enabled: window.libraryKeyboardShortcutsAllowed()
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.cycleLibraryPlatform(1)
        }
    }
    Shortcut {
        sequence: "I"
        context: Qt.ApplicationShortcut
        enabled: window.libraryKeyboardShortcutsAllowed()
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.openSelectedGameDetails()
        }
    }
    Shortcut {
        sequences: ["/", "Ctrl+F"]
        context: Qt.ApplicationShortcut
        enabled: window.libraryKeyboardShortcutsAllowed()
        onActivated: {
            window.activeInputDevice = "keyboard"
            window.openLibrarySearch()
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: window.visible && currentPage === "Settings"
            && !systemMenuVisible && !uninstallDialog.visible
            && !libraryFolderDialog.visible && !librarySetupDialog.visible
            && !libraryBackgroundFileDialog.visible
            // A text field (the TheGamesDB key) handles Escape itself.
            && !(window.activeFocusItem instanceof TextInput)
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
    // After a restart from Settings (for example to apply a new interface
    // scale), reopen Settings at the section the user was on.
    Component.onCompleted: {
        if (startupSettingsSection.length > 0
                && window.settingsSections.indexOf(startupSettingsSection) >= 0) {
            window.currentPage = "Settings"
            settingsPage.section = startupSettingsSection
            Qt.callLater(function() {
                window.settingsNavigationButtonForSection(startupSettingsSection).forceActiveFocus()
            })
        }
    }
    // Theme shown in the Appearance preview pane (follows the focused tile).
    property string previewThemeId: settingsManager.themeName
    readonly property var settingsSections: ["Appearance", "Display", "Background", "Library", "Metadata", "Controller", "Application"]
    // Settings pages stop growing past this width; the rest of a wide screen
    // goes to the preview pane.
    readonly property real settingsContentMaxWidth: 880
    readonly property bool interfaceScaleRestartPending: !interfaceScaleInfo.external
        && window.interfaceScalePercentFor(settingsManager.interfaceScale) !== interfaceScaleInfo.applied

    function interfaceScalePercentFor(setting) {
        return setting === "auto" ? interfaceScaleInfo.auto : parseInt(setting)
    }
    function interfaceScaleLabel(setting) {
        return setting === "auto" ? "Auto · " + interfaceScaleInfo.auto + "%" : setting + "%"
    }
    function themeTitle(themeId) {
        for (let i = 0; i < theme.presets.length; ++i) {
            if (theme.presets[i].id === themeId)
                return theme.presets[i].title
        }
        return themeId
    }
    function cycleSettingsSection(step) {
        const sections = window.settingsSections
        const index = Math.max(0, sections.indexOf(settingsPage.section))
        settingsPage.section = sections[(index + step + sections.length) % sections.length]
        Qt.callLater(function() {
            window.settingsNavigationButtonForSection(settingsPage.section).forceActiveFocus()
        })
    }
    // One-line summary under each category in the Settings rail.
    function settingsSectionSummary(section) {
        switch (section) {
        case "Appearance":
            return window.themeTitle(settingsManager.themeName) + " · "
                + settingsManager.fontSize.charAt(0).toUpperCase() + settingsManager.fontSize.slice(1) + " text"
        case "Display": {
            const mode = settingsManager.displayMode
            return mode.charAt(0).toUpperCase() + mode.slice(1) + " · "
                + (interfaceScaleInfo.external ? "Scale from environment"
                    : window.interfaceScaleLabel(settingsManager.interfaceScale))
        }
        case "Background": {
            const type = settingsManager.libraryBackgroundType
            if (type === "animated")
                return "Waves · " + window.wavePreset(libraryVisualSettings.waveColor).label
            if (type === "custom")
                return settingsManager.customLibraryBackground.length > 0 ? "Image" : "Image · none chosen"
            return "Gradient"
        }
        case "Library": {
            if (!platformManager.libraryConfigured)
                return "No library folder"
            const parts = platformManager.libraryDirectory.split("/")
            return parts[parts.length - 1] || platformManager.libraryDirectory
        }
        case "Metadata": {
            const providers = []
            if (settingsManager.theGamesDbEnabled) providers.push("TheGamesDB")
            if (settingsManager.launchBoxEnabled) providers.push("LaunchBox")
            return providers.length > 0 ? providers.join(" + ") : "No providers enabled"
        }
        case "Controller":
            return controllerManager.connected ? controllerManager.controllerName : "No controller connected"
        case "Application":
            return "Version " + Qt.application.version + " · Logs"
        }
        return ""
    }
    readonly property var waveColorPresets: [
        { label: "Ocean", value: "ocean", color: "#3d8bff" },
        { label: "Violet", value: "violet", color: "#8a6bff" },
        { label: "Aurora", value: "aurora", color: "#2fd4a8" },
        { label: "Sunset", value: "sunset", color: "#ff7a59" },
        { label: "Rose", value: "rose", color: "#ff5c9a" },
        { label: "Ice", value: "ice", color: "#bcd7ff" },
        { label: "Theme", value: "theme", color: theme.accent },
        { label: "Custom", value: "custom", color: Qt.hsva(libraryVisualSettings.waveCustomHue / 360, 0.72, 1.0, 1.0) }
    ]
    readonly property var waveHueOptions: [
        { label: "Red", value: 0 }, { label: "Orange", value: 25 }, { label: "Amber", value: 42 },
        { label: "Yellow", value: 55 }, { label: "Lime", value: 85 }, { label: "Green", value: 130 },
        { label: "Teal", value: 165 }, { label: "Cyan", value: 185 }, { label: "Sky", value: 200 },
        { label: "Blue", value: 220 }, { label: "Indigo", value: 245 }, { label: "Purple", value: 270 },
        { label: "Magenta", value: 300 }, { label: "Pink", value: 330 }
    ]
    function wavePreset(value) {
        for (let i = 0; i < waveColorPresets.length; ++i) {
            if (waveColorPresets[i].value === value)
                return waveColorPresets[i]
        }
        return waveColorPresets[0]
    }
    readonly property color waveColorValue: wavePreset(libraryVisualSettings.waveColor).color
    readonly property real waveSpeed: libraryVisualSettings.waveMotion === "calm" ? 0.6
        : libraryVisualSettings.waveMotion === "lively" ? 1.7 : 1.0
    // Auto: full quality up to 2560 × 1440 physical pixels, Balanced above.
    readonly property string waveQualityResolved: libraryVisualSettings.waveQuality !== "auto"
        ? libraryVisualSettings.waveQuality
        : (Screen.width * Screen.height * Screen.devicePixelRatio * Screen.devicePixelRatio > 3686400
            ? "balanced" : "high")
    readonly property real waveRenderScale: waveQualityResolved === "low" ? 0.5
        : waveQualityResolved === "balanced" ? 0.75 : 1.0
    readonly property int waveFps: waveQualityResolved === "low" ? 30 : 60
    // A ValueCycler (or anything with stepValue) at or above the focused item.
    function focusedValueStepper(item) {
        let current = item
        while (current) {
            if (typeof current.stepValue === "function")
                return current
            current = current.parent
        }
        return null
    }
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
    property string theGamesDbKeyMessage: ""
    function formatBytes(bytes) {
        if (!bytes || bytes <= 0)
            return "0 MB"
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
        return Math.max(1, Math.round(bytes / (1024 * 1024))) + " MB"
    }
    function formatShortDate(isoText) {
        const date = new Date(isoText)
        return isNaN(date.getTime()) ? isoText : date.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
    }
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
            launchBoxStatusMessage = "Catalog updated: " + games + " games for your platforms. Unmatched games in your library are being checked against it now."
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
    property string scanMessage: "Your library updates automatically when supported game files are added."
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
                        && settingsManager.libraryView !== "list"
                        && settingsManager.libraryView !== "shelves")) {
                stop()
                inputAmount = 0
                velocity = 0
                return
            }

            const view = settingsManager.libraryView === "list" ? gameList
                : settingsManager.libraryView === "shelves" ? shelvesFlick
                : window.sectionedLibraryGrid ? sectionedGrid : gameGrid
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
        // LB / RB: switch games inside Game Details, cycle platform filters on
        // the Library page, and switch sections in Settings.
        function onPreviousSectionRequested() {
            window.activeInputDevice = "controller"
            if (gameDetailsPopup.visible)
                gameDetailsPopup.switchToAdjacentGame(-1)
            else if (currentPage === "Library" && !window.systemMenuVisible)
                window.cycleLibraryPlatform(-1)
            else if (currentPage === "Settings" && !window.systemMenuVisible
                     && !controllerManager.mappingCaptureActive)
                window.cycleSettingsSection(-1)
        }
        function onNextSectionRequested() {
            window.activeInputDevice = "controller"
            if (gameDetailsPopup.visible)
                gameDetailsPopup.switchToAdjacentGame(1)
            else if (currentPage === "Library" && !window.systemMenuVisible)
                window.cycleLibraryPlatform(1)
            else if (currentPage === "Settings" && !window.systemMenuVisible
                     && !controllerManager.mappingCaptureActive)
                window.cycleSettingsSection(1)
        }
        function onDetailsRequested() {
            window.activeInputDevice = "controller"
            if (currentPage === "Library" && !gameDetailsPopup.visible
                    && !window.systemMenuVisible)
                window.openSelectedGameDetails()
        }
        function onRightStickScrollRequested(amount) {
            if (!controllerManager.navigationActive) {
                libraryRightStickScrollTimer.inputAmount = 0
                return
            }

            if (window.currentPage === "Library" && !window.systemMenuVisible
                    && !gameDetailsPopup.visible
                    && (settingsManager.libraryView === "grid"
                        || settingsManager.libraryView === "list"
                        || settingsManager.libraryView === "shelves")) {
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
                : settingsPage.section === "Background" ? backgroundContent
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

    // All Platforms in Grid view uses the sectioned grid: one section per
    // platform, each with its own row height. Platform filters keep the
    // single GridView. gameGrid.currentIndex stays the shared selection.
    readonly property bool sectionedLibraryGrid:
        settingsManager.libraryView === "grid" && selectedPlatform === "All Platforms"

    // Game counts per platform for the toolbar pills, from one library query.
    readonly property var libraryPlatformCounts: {
        window.libraryRevision
        const counts = { "All Platforms": 0 }
        for (const game of databaseManager.games("")) {
            counts["All Platforms"]++
            counts[game.platform] = (counts[game.platform] || 0) + 1
        }
        return counts
    }

    // Display names for the internal platform identifiers.
    function platformDisplayName(platform) {
        const names = { "N64": "Nintendo 64", "PS1": "PlayStation", "GameCube": "GameCube",
                        "Wii": "Wii", "Xbox": "Xbox" }
        return names[platform] || platform
    }

    // Shoulder-button names for the controller in use, as [previous, next].
    function platformCycleButtonLabels() {
        if (controllerManager.controllerType === "playstation")
            return ["L1", "R1"]
        if (controllerManager.controllerType === "xbox")
            return ["LB", "RB"]
        return ["L", "R"]
    }

    // Select a Library platform filter and return to the top of the results.
    function selectLibraryPlatform(platform) {
        if (window.selectedPlatform === platform)
            return
        window.selectedPlatform = platform
        gameGrid.currentIndex = 0
        Qt.callLater(function() {
            gameGrid.positionViewAtBeginning()
            sectionedGrid.contentY = 0
            gameList.positionViewAtBeginning()
        })
    }

    // Step through All Platforms and each platform, wrapping at either end.
    function cycleLibraryPlatform(step) {
        const platforms = ["All Platforms"].concat(platformManager.platforms())
        const index = Math.max(0, platforms.indexOf(window.selectedPlatform))
        const next = (index + step + platforms.length) % platforms.length
        window.selectLibraryPlatform(platforms[next])
    }

    // Move focus from the toolbar (or search field) into the current
    // Library view: the spotlight in Shelves, otherwise the selected game.
    function focusLibraryContent() {
        if (gameGrid.count === 0)
            return
        gameGrid.currentIndex = Math.max(0, gameGrid.currentIndex)
        if (settingsManager.libraryView === "shelves") {
            shelvesView.focusSpotlight()
        } else if (settingsManager.libraryView === "list") {
            gameList.forceActiveFocus()
            gameList.positionViewAtIndex(gameGrid.currentIndex, ListView.Contain)
        } else if (settingsManager.libraryView === "carousel") {
            consoleView.currentIndex = gameGrid.currentIndex
            consoleView.forceActiveFocus()
        } else if (window.sectionedLibraryGrid) {
            sectionedGrid.forceActiveFocus()
            sectionedGrid.positionAtIndex(gameGrid.currentIndex)
        } else {
            gameGrid.forceActiveFocus()
            gameGrid.positionViewAtIndex(gameGrid.currentIndex, GridView.Contain)
        }
    }

    // Ratings arrive either as an age rating ("T - Teen") or as a numeric
    // community score ("4.7578..."). Show numbers as a short star score.
    function formatGameRating(value) {
        const text = value === undefined || value === null ? "" : String(value).trim()
        if (text.length === 0)
            return ""
        const number = Number(text)
        if (!isNaN(number) && isFinite(number))
            return number > 0 ? "★ " + number.toFixed(1) : ""
        return text
    }

    // Supported recompilation project for a Library model entry, or null.
    function libraryGameProject(game) {
        if (!game)
            return null
        const projects = projectCatalog.projectsForGame(
            game.metadataTitle ? game.metadataTitle : game.title, game.platform)
        return projects.length > 0 ? projects[0] : null
    }
    function libraryGameInstalled(game) {
        const project = window.libraryGameProject(game)
        return project !== null && window.installedLauncherProjectIds[project.id] === true
    }

    // Carousel and List primary action (A / Enter, or clicking the selected
    // Carousel cover): play
    // when the game's recompilation is installed, otherwise open Game Details.
    function libraryPrimaryAction(index) {
        const games = gameGrid.model || []
        const game = games[Math.max(0, Math.min(games.length - 1, index))]
        if (!game)
            return
        const project = window.libraryGameProject(game)
        if (project !== null && window.installedLauncherProjectIds[project.id] === true) {
            if (!window.runningLaunchers[project.id])
                window.launchSupportedGame(project.id)
            return
        }
        window.openLibraryGameDetails(game)
    }

    // Y / I: open Game Details for the selected game in any Library view.
    function openSelectedGameDetails() {
        const games = gameGrid.model || []
        if (games.length === 0)
            return
        window.openLibraryGameDetails(games[Math.max(0, Math.min(games.length - 1, gameGrid.currentIndex))])
    }

    // Open and focus the Library search field.
    function openLibrarySearch() {
        librarySearchControl.openRequested = true
        Qt.callLater(function() { librarySearchField.forceActiveFocus() })
    }

    // Open Game Details for an entry of the Library model (gameGrid.model).
    function openLibraryGameDetails(game) {
        if (!game)
            return
        window.selectedGame = {
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
        gameDetailsPopup.open()
    }

    // Project ids with a managed launcher installation. installedLaunchers()
    // reads the launcher directories from disk, so Library cards share this
    // single lookup instead of querying once per card.
    readonly property var installedLauncherProjectIds: {
        launcherRevision
        const ids = {}
        for (const entry of launcherDownloadManager.installedLaunchers())
            ids[entry.projectId] = true
        return ids
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
        function onScanStarted() {
            window.scanMessage = "Scanning your game library..."
        }
        function onPlatformScanStarted(platform, platformIndex, platformCount) {
            window.scanMessage = "Scanning " + platform + " (" + platformIndex
                + " of " + platformCount + ")..."
        }
        function onPlatformScanCompleted(platform, gamesFound, unsupportedFiles) {
            window.scanMessage = platform + " scanned: " + gamesFound
                + " new game(s), " + unsupportedFiles + " unsupported file(s) ignored."
        }
        function onScanCompleted(gamesFound) {
            window.libraryRevision++
            if (settingsManager.automaticMetadataMatchingEnabled) {
                window.scanMessage = libraryScanner.lastError.length > 0
                    ? libraryScanner.lastError + "\nChecking metadata for discovered games..."
                    : gamesFound + " new game(s) found. " + libraryScanner.lastUnsupportedCount
                        + " unsupported file(s) ignored. Checking metadata..."
                // A problem with one platform folder must not block metadata for
                // games successfully discovered on other platforms.
                metadataMatcher.scrapeLibrary()
            } else {
                window.scanMessage = libraryScanner.lastError.length > 0
                    ? libraryScanner.lastError + "\nAutomatic metadata matching is disabled."
                    : gamesFound + " new game(s) found. " + libraryScanner.lastUnsupportedCount
                        + " unsupported file(s) ignored. Automatic metadata matching is disabled."
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
                            libraryScanner.scanAllAsync()
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

    GameDetailsPage {
        id: gameDetailsPopup
    }

    // Full-screen screenshot viewer (List preview). Paging updates the List
    // preview header so it shows the screenshot last viewed.
    ScreenshotViewer {
        id: screenshotViewer
        themeTokens: theme
        reducedMotion: settingsManager.reducedMotion
        onShown: function(index) {
            if (settingsManager.libraryView === "list")
                listPreviewBackdrop.show(index)
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

            // Custom wallpaper, loaded at the window's size (Fill crops to
            // cover; Fit shows the whole image over a blurred copy) and dimmed
            // by the user's chosen amount.
            WallpaperBackground {
                anchors.fill: parent
                visible: currentPage === "Library"
                    && settingsManager.libraryBackgroundType === "custom"
                    && settingsManager.customLibraryBackground.length > 0
                source: visible ? settingsManager.customLibraryBackground : ""
                fitMode: libraryVisualSettings.wallpaperFit
                dimPercent: libraryVisualSettings.wallpaperDim
                dimColor: theme.windowBackground
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

            // Waves: drawn live by a GPU shader (WaveBackground.qml). The animation
            // pauses (keeping its current frame) under Reduced motion, while a game
            // is running, and when the window is inactive or minimized.
            WaveBackground {
                id: libraryWaveLayer
                anchors.fill: parent
                visible: currentPage === "Library"
                    && settingsManager.libraryBackgroundType === "animated"
                waveColor: window.waveColorValue
                topColor: theme.backgroundTop
                bottomColor: theme.backgroundBottom
                speed: window.waveSpeed
                quality: window.waveRenderScale
                fps: window.waveFps
                running: !settingsManager.reducedMotion && !controllerManager.gameSessionActive
                    && window.active && window.visibility !== Window.Minimized
            }

            // Hero carousel backdrop: the selected game's first screenshot fills
            // the window (or its blurred cover when it has none). Two layers
            // cross-fade when the selection changes.
            Item {
                id: carouselHeroBackdrop
                anchors.fill: parent
                readonly property bool active: currentPage === "Library"
                    && settingsManager.libraryView === "carousel" && gameGrid.count > 0
                visible: opacity > 0
                opacity: active ? 1 : 0
                Behavior on opacity {
                    enabled: !settingsManager.reducedMotion
                    NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
                }
                readonly property var game: consoleView.currentGame
                readonly property bool hasScreenshot: game !== null
                    && game.screenshots !== undefined && game.screenshots.length > 0
                readonly property string source: !game ? ""
                    : hasScreenshot ? game.screenshots[0] : game.artworkPath
                property bool showFirst: true
                Component.onCompleted: {
                    heroLayerA.source = source
                    heroLayerA.blurred = !hasScreenshot
                }
                onSourceChanged: {
                    if (showFirst) {
                        heroLayerB.source = source
                        heroLayerB.blurred = !hasScreenshot
                    } else {
                        heroLayerA.source = source
                        heroLayerA.blurred = !hasScreenshot
                    }
                    showFirst = !showFirst
                }

                Repeater {
                    model: 2
                    delegate: Item {
                        required property int index
                        anchors.fill: parent
                        opacity: (index === 0) === carouselHeroBackdrop.showFirst ? 1 : 0
                        Behavior on opacity {
                            enabled: !settingsManager.reducedMotion
                            NumberAnimation { duration: 420; easing.type: Easing.InOutQuad }
                        }
                        Image {
                            id: heroLayerImage
                            anchors.fill: parent
                            source: index === 0 ? heroLayerA.source : heroLayerB.source
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            visible: false
                        }
                        MultiEffect {
                            anchors.fill: parent
                            source: heroLayerImage
                            blurEnabled: index === 0 ? heroLayerA.blurred : heroLayerB.blurred
                            blur: 1.0
                            blurMax: 64
                            autoPaddingEnabled: false
                            opacity: heroLayerImage.status === Image.Ready
                                ? ((index === 0 ? heroLayerA.blurred : heroLayerB.blurred) ? 0.5 : 1) : 0
                        }
                    }
                }
                QtObject { id: heroLayerA; property string source: ""; property bool blurred: false }
                QtObject { id: heroLayerB; property string source: ""; property bool blurred: false }

                // Keep the header and the details under the row legible.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.78) }
                        GradientStop { position: 0.22; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.18) }
                        GradientStop { position: 0.45; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.28) }
                        GradientStop { position: 0.72; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.88) }
                        GradientStop { position: 1.0; color: Qt.rgba(theme.backgroundBottom.r, theme.backgroundBottom.g, theme.backgroundBottom.b, 0.98) }
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
                anchors.topMargin: 98
                currentIndex: currentPage === "Settings" ? 1 : 0
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        // Tighten only the Grid heading-to-card gap as density decreases.
                        // List and Carousel retain their existing spacing.
                        spacing: settingsManager.libraryView === "grid"
                            ? (settingsManager.libraryDensity === "compact" ? 0
                                : settingsManager.libraryDensity === "comfortable" ? 2 : 8)
                            : 18

                        // The hero carousel shows the selected game instead of the
                        // collection heading.
                        RowLayout {
                            visible: settingsManager.libraryView !== "carousel"
                            Layout.fillWidth: true
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                RowLayout {
                                    spacing: 12
                                    Layout.alignment: Qt.AlignLeft
                                    Label {
                                        text: selectedPlatform === "All Platforms" ? "Your Collection" : selectedPlatform
                                        font.pixelSize: 32 * theme.fontScale
                                        font.bold: true
                                        color: theme.textPrimary
                                    }
                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 10
                                        color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.12)
                                        border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.28)
                                        border.width: 1
                                        implicitWidth: collectionCountLabel.implicitWidth + 18
                                        implicitHeight: collectionCountLabel.implicitHeight + 8
                                        Label {
                                            id: collectionCountLabel
                                            anchors.centerIn: parent
                                            text: gameGrid.count + (gameGrid.count === 1 ? " GAME" : " GAMES")
                                            color: theme.accentHover
                                            font.pixelSize: 10 * theme.fontScale
                                            font.bold: true
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
                            // End the grid above the floating input-hint bar so it
                            // never covers the last visible row.
                            anchors.bottomMargin: hintBarClearance
                            readonly property real hintBarClearance: 36
                            anchors.horizontalCenter: parent.horizontalCenter
                            // Constrain the GridView itself to the intended column
                            // block. Margins alone do not limit GridView's layout width.
                            width: Math.min(parent.width, visibleColumns * gridCellWidth)
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
                            // Hidden while the sectioned All Platforms grid is shown;
                            // it still owns the model and the shared selection.
                            visible: (settingsManager.libraryView === "grid"
                                      && !window.sectionedLibraryGrid) || opacity > 0
                            opacity: settingsManager.libraryView === "grid"
                                && !window.sectionedLibraryGrid ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                            }
                            clip: true
                            // Keep drag gestures disabled; mouse-wheel scrolling is
                            // handled separately so pointer navigation does not interfere
                            // with keyboard/controller focus movement.
                            interactive: false
                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: function(event) {
                                    if (settingsManager.libraryView !== "grid"
                                            || gameGrid.contentHeight <= gameGrid.height) {
                                        event.accepted = true
                                        return
                                    }

                                    // Use precise trackpad deltas when available. Standard
                                    // mouse wheels report 120 units per notch, so translate
                                    // those into a comfortable fraction of one grid row.
                                    const delta = event.pixelDelta.y !== 0
                                        ? event.pixelDelta.y
                                        : (event.angleDelta.y / 120)
                                            * gameGrid.cellHeight * 0.85
                                    const maxContentY = Math.max(
                                        0, gameGrid.contentHeight - gameGrid.height)
                                    gameGrid.contentY = Math.max(0, Math.min(
                                        maxContentY, gameGrid.contentY - delta))
                                    event.accepted = true
                                }
                            }
                            // Keep scaled cards and their focus glow inside the viewport,
                            // including the first and last rows.
                            // Combined with the outer layout gap, this produces
                            // an 8 / 10 / 16 px heading-to-card gap by density.
                            // The focused card scales slightly and its focus ring sits
                            // outside the artwork, so leave room for both in the first
                            // and last rows.
                            topMargin: 16
                            bottomMargin: 18
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
                            // Keep 4K and ultrawide grids visually balanced instead of
                            // allowing the full width to become an excessive number of
                            // columns. Each card size has its own cap so smaller cards
                            // still add columns: Compact 10, Comfortable 8, Spacious 6.
                            // Smaller reference resolutions retain their natural count.
                            readonly property int wideScreenColumnCap:
                                settingsManager.libraryDensity === "compact" ? 10
                                : settingsManager.libraryDensity === "spacious" ? 6 : 8
                            readonly property int visibleColumns:
                                parent.width >= 3000
                                    ? Math.min(wideScreenColumnCap, responsiveColumns)
                                    : responsiveColumns
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
                            leftMargin: 0
                            rightMargin: 0
                            cellWidth: gridCellWidth
                            // Artwork-forward cards: the cover art is the card. The
                            // artwork area takes the frame shape of the selected
                            // platform (N64 7:5, PS1 1:1, disc cases 5:7). With All
                            // Platforms it uses the tallest shape (5:7), and each cover
                            // sits in its own platform frame on a shared bottom edge so
                            // the captions line up.
                            readonly property real gridFrameRatio: {
                                if (selectedPlatform !== "All Platforms") {
                                    const ratio = ArtworkLayout.platformAspectRatio(selectedPlatform)
                                    if (ratio > 0)
                                        return ratio
                                }
                                return 5 / 7
                            }
                            readonly property real gridArtworkHeight:
                                Math.round(targetCardWidth / gridFrameRatio)
                            // Two caption lines (title and status) plus spacing,
                            // scaled with the user's font size preference.
                            readonly property real gridCaptionHeight:
                                Math.ceil(12 + 38 * theme.fontScale)
                            readonly property real gridCardHeight:
                                gridArtworkHeight + gridCaptionHeight
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
                                    // Match the ROM file name and the metadata title shown
                                    // on Grid cards.
                                    : games.filter(game => game.title.toLowerCase().includes(query)
                                        || (game.metadataTitle || "").toLowerCase().includes(query))
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

                                LibraryCoverCard {
                                    anchors.fill: parent
                                    title: gameTile.title
                                    platform: gameTile.platform
                                    artworkPath: gameTile.artworkPath
                                    metadataTitle: gameTile.metadataTitle
                                    artworkHeight: gameGrid.gridArtworkHeight
                                    focused: gameTile.GridView.isCurrentItem
                                    hovered: tileMouse.containsMouse
                                    reducedMotion: settingsManager.reducedMotion
                                    themeTokens: theme
                                    installedProjectIds: window.installedLauncherProjectIds
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

                        // All Platforms Grid: one section per platform, each with a
                        // header and its own row height (N64 7:5, PS1 1:1, disc
                        // cases 5:7). Geometry and navigation come from
                        // LibraryGridLayout.js. Selection is gameGrid.currentIndex.
                        Flickable {
                            id: sectionedGrid
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: gameGrid.hintBarClearance
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: gameGrid.width
                            visible: window.sectionedLibraryGrid || opacity > 0
                            opacity: window.sectionedLibraryGrid ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                            }
                            clip: true
                            // Pointer drags are disabled as in the Grid; the mouse
                            // wheel is handled below.
                            interactive: false
                            contentWidth: width
                            contentHeight: layout.contentHeight
                            boundsBehavior: Flickable.StopAtBounds

                            readonly property int columns: Math.max(1, gameGrid.visibleColumns)
                            readonly property real cardWidth: gameGrid.targetCardWidth
                            readonly property real cellWidth: gameGrid.gridCellWidth
                            readonly property real headerHeight: Math.ceil(44 * theme.fontScale)
                            readonly property var layout: LibraryGridLayout.layoutSections(
                                LibraryGridLayout.buildSections(gameGrid.model || []), {
                                    columns: columns,
                                    cardWidth: cardWidth,
                                    gap: 14,
                                    captionHeight: gameGrid.gridCaptionHeight,
                                    headerHeight: headerHeight,
                                    sectionSpacing: 26,
                                    topMargin: gameGrid.topMargin,
                                    bottomMargin: gameGrid.bottomMargin
                                })

                            function positionAtIndex(index) {
                                const rect = LibraryGridLayout.cardRect(
                                    layout.sections, columns, cellWidth, cardWidth,
                                    headerHeight, index)
                                contentY = LibraryGridLayout.contentYForCard(
                                    rect, contentY, height, contentHeight,
                                    gameGrid.topMargin, gameGrid.bottomMargin)
                            }

                            // Keep the scroll position valid when the library,
                            // search results, or card size change.
                            onLayoutChanged: contentY = Math.max(0, Math.min(
                                contentY, Math.max(0, contentHeight - height)))

                            Keys.onReturnPressed: window.openLibraryGameDetails(
                                gameGrid.model[Math.max(0, gameGrid.currentIndex)])
                            Keys.onEnterPressed: window.openLibraryGameDetails(
                                gameGrid.model[Math.max(0, gameGrid.currentIndex)])

                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: function(event) {
                                    if (!window.sectionedLibraryGrid
                                            || sectionedGrid.contentHeight <= sectionedGrid.height) {
                                        event.accepted = true
                                        return
                                    }
                                    const delta = event.pixelDelta.y !== 0
                                        ? event.pixelDelta.y
                                        : (event.angleDelta.y / 120)
                                            * gameGrid.cellHeight * 0.85
                                    const maxContentY = Math.max(
                                        0, sectionedGrid.contentHeight - sectionedGrid.height)
                                    sectionedGrid.contentY = Math.max(0, Math.min(
                                        maxContentY, sectionedGrid.contentY - delta))
                                    event.accepted = true
                                }
                            }

                            Repeater {
                                model: sectionedGrid.layout.sections
                                delegate: Item {
                                    id: gridSection
                                    required property var modelData
                                    x: 0
                                    y: modelData.top
                                    width: sectionedGrid.width
                                    height: modelData.height

                                    Row {
                                        id: sectionHeader
                                        height: sectionedGrid.headerHeight
                                        spacing: 10
                                        Label {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: window.platformDisplayName(gridSection.modelData.platform)
                                            color: theme.textPrimary
                                            font.pixelSize: 20 * theme.fontScale
                                            font.bold: true
                                        }
                                        Label {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: gridSection.modelData.count
                                                + (gridSection.modelData.count === 1 ? " game" : " games")
                                            color: theme.textMuted
                                            font.pixelSize: 13 * theme.fontScale
                                        }
                                    }

                                    Repeater {
                                        model: gridSection.modelData.count
                                        delegate: Item {
                                            id: sectionCard
                                            required property int index
                                            readonly property int gameIndex:
                                                gridSection.modelData.start + index
                                            readonly property var game: gameGrid.model
                                                ? gameGrid.model[gameIndex] : null
                                            readonly property int row:
                                                Math.floor(index / sectionedGrid.columns)
                                            readonly property int column:
                                                index % sectionedGrid.columns
                                            readonly property bool isCurrent:
                                                gameGrid.currentIndex === gameIndex
                                            x: column * sectionedGrid.cellWidth
                                            y: sectionedGrid.headerHeight
                                                + row * gridSection.modelData.rowPitch
                                            width: sectionedGrid.cardWidth
                                            height: gridSection.modelData.cardHeight
                                            z: isCurrent ? 2 : 1
                                            scale: isCurrent ? 1.035 : 1.0
                                            Behavior on scale {
                                                enabled: !settingsManager.reducedMotion
                                                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                            }

                                            // Same edge fade as the Grid: smoothstep on the
                                            // card center's distance from the viewport edges.
                                            readonly property real contentTop: gridSection.y + y
                                            readonly property real scrollEdgeDistance: Math.min(
                                                contentTop + height / 2 - sectionedGrid.contentY,
                                                sectionedGrid.contentY + sectionedGrid.height
                                                    - (contentTop + height / 2))
                                            readonly property real scrollEdgeProgress:
                                                settingsManager.reducedMotion ? 1
                                                : Math.max(0, Math.min(1, scrollEdgeDistance / 240))
                                            readonly property real scrollEdgeOpacity:
                                                settingsManager.reducedMotion ? 1
                                                : 0.18 + 0.82 * scrollEdgeProgress * scrollEdgeProgress
                                                    * (3 - 2 * scrollEdgeProgress)

                                            // Startup / view-switch reveal, matching the Grid:
                                            // bottom visible rows first, then upward, left to right.
                                            property real revealOpacity:
                                                window.initialGridRevealPending
                                                    && window.sectionedLibraryGrid ? 0 : 1
                                            property real revealScale:
                                                window.initialGridRevealPending
                                                    && window.sectionedLibraryGrid ? 0.94 : 1
                                            property real revealOffsetY:
                                                window.initialGridRevealPending
                                                    && window.sectionedLibraryGrid ? 26 : 0
                                            readonly property int revealDelayMs: {
                                                const viewportBottom = sectionedGrid.contentY + sectionedGrid.height
                                                const rowsFromBottom = Math.max(0, Math.floor(
                                                    (viewportBottom - contentTop - height)
                                                    / Math.max(1, gridSection.modelData.rowPitch)))
                                                const delay = rowsFromBottom * 115 + column * 38
                                                return isFinite(delay) ? Math.max(0, Math.round(delay)) : 0
                                            }
                                            opacity: revealOpacity * scrollEdgeOpacity
                                            transform: Translate { y: sectionCard.revealOffsetY }

                                            Timer {
                                                id: sectionRevealFailsafe
                                                interval: Math.max(1200, sectionCard.revealDelayMs + 900)
                                                repeat: false
                                                onTriggered: {
                                                    if (sectionCard.revealOpacity < 1) {
                                                        sectionRevealAnimation.stop()
                                                        sectionCard.revealOpacity = 1
                                                        sectionCard.revealScale = 1
                                                        sectionCard.revealOffsetY = 0
                                                    }
                                                }
                                            }
                                            Connections {
                                                target: window
                                                function onGridRevealTokenChanged() {
                                                    if (!window.sectionedLibraryGrid)
                                                        return
                                                    if (settingsManager.reducedMotion) {
                                                        sectionRevealFailsafe.stop()
                                                        sectionCard.revealOpacity = 1
                                                        sectionCard.revealScale = 1
                                                        sectionCard.revealOffsetY = 0
                                                        return
                                                    }
                                                    sectionCard.revealOpacity = 0
                                                    sectionCard.revealScale = 0.94
                                                    sectionCard.revealOffsetY = 26
                                                    sectionRevealFailsafe.restart()
                                                    sectionRevealAnimation.restart()
                                                }
                                            }
                                            SequentialAnimation {
                                                id: sectionRevealAnimation
                                                PauseAnimation { duration: sectionCard.revealDelayMs }
                                                ParallelAnimation {
                                                    NumberAnimation {
                                                        target: sectionCard; property: "revealOpacity"
                                                        to: 1; duration: 460; easing.type: Easing.OutCubic
                                                    }
                                                    NumberAnimation {
                                                        target: sectionCard; property: "revealScale"
                                                        to: 1; duration: 520; easing.type: Easing.OutCubic
                                                    }
                                                    NumberAnimation {
                                                        target: sectionCard; property: "revealOffsetY"
                                                        to: 0; duration: 520; easing.type: Easing.OutCubic
                                                    }
                                                }
                                                onFinished: sectionRevealFailsafe.stop()
                                            }
                                            Component.onCompleted: {
                                                if (window.initialGridRevealPending
                                                        && window.sectionedLibraryGrid)
                                                    sectionRevealFailsafe.start()
                                            }

                                            LibraryCoverCard {
                                                anchors.fill: parent
                                                scale: sectionCard.revealScale
                                                title: sectionCard.game ? sectionCard.game.title : ""
                                                platform: sectionCard.game ? sectionCard.game.platform : ""
                                                artworkPath: sectionCard.game ? sectionCard.game.artworkPath : ""
                                                metadataTitle: sectionCard.game && sectionCard.game.metadataTitle
                                                    ? sectionCard.game.metadataTitle : ""
                                                artworkHeight: gridSection.modelData.artworkHeight
                                                focused: sectionCard.isCurrent
                                                hovered: sectionCardMouse.containsMouse
                                                reducedMotion: settingsManager.reducedMotion
                                                themeTokens: theme
                                                installedProjectIds: window.installedLauncherProjectIds
                                            }

                                            MouseArea {
                                                id: sectionCardMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onEntered: {
                                                    gameGrid.currentIndex = sectionCard.gameIndex
                                                    window.activeInputDevice = "mouse"
                                                }
                                                onClicked: {
                                                    gameGrid.currentIndex = sectionCard.gameIndex
                                                    window.openLibraryGameDetails(sectionCard.game)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Shelves view: a spotlight panel for the selected game, then
                        // one horizontal shelf per platform. Cards share a height per
                        // card size; each width follows the platform's box shape.
                        // Selection is gameGrid.currentIndex; navigation comes from
                        // LibraryGridLayout.moveShelfIndex.
                        Item {
                            id: shelvesView
                            anchors.fill: parent
                            anchors.bottomMargin: gameGrid.hintBarClearance
                            visible: settingsManager.libraryView === "shelves" || opacity > 0
                            opacity: settingsManager.libraryView === "shelves" ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                            }

                            readonly property var sections:
                                LibraryGridLayout.buildSections(gameGrid.model || [])
                            // Last focused card per shelf: { platform: localIndex }.
                            property var shelfMemory: ({})
                            readonly property real cardHeight:
                                settingsManager.libraryDensity === "compact" ? 150
                                : settingsManager.libraryDensity === "spacious" ? 236 : 190
                            readonly property real headerHeight: Math.ceil(34 * theme.fontScale)
                            // Room for the focus ring and the focused card's scale.
                            readonly property real cardPadding: 14
                            readonly property real rowHeight:
                                headerHeight + cardHeight + cardPadding * 2
                            readonly property real rowSpacing: 10
                            readonly property var currentGame: {
                                const games = gameGrid.model || []
                                if (games.length === 0)
                                    return null
                                return games[Math.max(0, Math.min(games.length - 1, gameGrid.currentIndex))]
                            }

                            Connections {
                                target: gameGrid
                                function onCurrentIndexChanged() {
                                    const s = LibraryGridLayout.sectionIndexOf(
                                        shelvesView.sections, gameGrid.currentIndex)
                                    if (s < 0)
                                        return
                                    const memory = Object.assign({}, shelvesView.shelfMemory)
                                    memory[shelvesView.sections[s].platform] =
                                        gameGrid.currentIndex - shelvesView.sections[s].start
                                    shelvesView.shelfMemory = memory
                                }
                            }
                            onSectionsChanged: shelfMemory = ({})

                            function focusShelves() {
                                gameGrid.currentIndex = Math.max(0, gameGrid.currentIndex)
                                shelvesFlick.forceActiveFocus()
                                positionAtIndex(gameGrid.currentIndex)
                            }
                            function focusSpotlight() {
                                const target = spotlightPlayButton.visible
                                    ? spotlightPlayButton : spotlightDetailsButton
                                target.forceActiveFocus()
                            }
                            function positionAtIndex(index) {
                                const s = LibraryGridLayout.sectionIndexOf(sections, index)
                                if (s < 0)
                                    return
                                shelvesFlick.contentY = LibraryGridLayout.contentYForRow(
                                    s * (rowHeight + rowSpacing), rowHeight,
                                    shelvesFlick.contentY, shelvesFlick.height,
                                    shelvesFlick.contentHeight)
                                const row = shelfRepeater.itemAt(s)
                                if (row && row.shelfList) {
                                    const list = row.shelfList
                                    const local = index - sections[s].start
                                    list.positionViewAtIndex(local, ListView.Contain)
                                    // Keep the shelf's margins at either end so the
                                    // first and last cards align with the other shelves.
                                    if (local === 0)
                                        list.contentX = -list.leftMargin
                                    else if (local === sections[s].count - 1)
                                        list.contentX = Math.max(-list.leftMargin,
                                            list.contentWidth + list.rightMargin - list.width)
                                }
                            }

                            // Spotlight panel.
                            Rectangle {
                                id: shelvesSpotlight
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: Math.round(Math.max(220, Math.min(300, shelvesView.height * 0.34)))
                                radius: theme.radiusLarge
                                color: theme.sidebarBackground
                                clip: true

                                readonly property var game: shelvesView.currentGame
                                readonly property string backdropSource: game
                                    ? (game.screenshots && game.screenshots.length > 0
                                        ? game.screenshots[0] : game.artworkPath)
                                    : ""
                                readonly property bool backdropIsCover: game
                                    ? !(game.screenshots && game.screenshots.length > 0) : false
                                readonly property var project: {
                                    if (!game)
                                        return null
                                    const projects = projectCatalog.projectsForGame(
                                        game.metadataTitle ? game.metadataTitle : game.title,
                                        game.platform)
                                    return projects.length > 0 ? projects[0] : null
                                }
                                readonly property bool installed: project !== null
                                    && window.installedLauncherProjectIds[project.id] === true
                                readonly property bool running: project !== null
                                    && window.runningLaunchers[project.id] === true

                                // Blurred screenshot slideshow behind the text (blurred
                                // cover when there are none); the box cover sits on the right.
                                ScreenshotBackdrop {
                                    id: spotlightBackdrop
                                    anchors.fill: parent
                                    alignment: "right"
                                    showSharpImage: false
                                    fillOpacity: 0.7
                                    sources: shelvesSpotlight.game && shelvesSpotlight.game.screenshots
                                        ? shelvesSpotlight.game.screenshots : []
                                    fallbackSource: shelvesSpotlight.game ? shelvesSpotlight.game.artworkPath : ""
                                    cycling: shelvesView.visible && currentPage === "Library"
                                        && !gameDetailsPopup.visible
                                    reducedMotion: settingsManager.reducedMotion
                                }
                                // Legibility gradient behind the text.
                                Rectangle {
                                    anchors.fill: parent
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.0; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.96) }
                                        GradientStop { position: 0.42; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.82) }
                                        GradientStop { position: 0.75; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.25) }
                                        GradientStop { position: 1.0; color: Qt.rgba(theme.windowBackground.r, theme.windowBackground.g, theme.windowBackground.b, 0.1) }
                                    }
                                }

                                ColumnLayout {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 30
                                    width: Math.min(760, parent.width * 0.55)
                                    spacing: 8

                                    Row {
                                        spacing: 8
                                        Rectangle {
                                            height: 24
                                            width: spotlightPlatformLabel.implicitWidth + 20
                                            radius: 12
                                            color: Qt.rgba(theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.12)
                                            Label {
                                                id: spotlightPlatformLabel
                                                anchors.centerIn: parent
                                                text: shelvesSpotlight.game
                                                    ? window.platformDisplayName(shelvesSpotlight.game.platform).toUpperCase() : ""
                                                color: theme.textPrimary
                                                font.pixelSize: 11 * theme.fontScale
                                                font.bold: true
                                                font.letterSpacing: 0.6
                                            }
                                        }
                                        Rectangle {
                                            visible: shelvesSpotlight.project !== null
                                            height: 24
                                            width: spotlightStatusLabel.implicitWidth + 20
                                            radius: 12
                                            color: shelvesSpotlight.installed
                                                ? Qt.rgba(theme.success.r, theme.success.g, theme.success.b, 0.16)
                                                : Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                                            Label {
                                                id: spotlightStatusLabel
                                                anchors.centerIn: parent
                                                text: !shelvesSpotlight.project ? ""
                                                    : shelvesSpotlight.project.name
                                                        + (shelvesSpotlight.installed ? " · Installed" : " · Available")
                                                color: shelvesSpotlight.installed ? theme.success : theme.accentHover
                                                font.pixelSize: 11 * theme.fontScale
                                                font.bold: true
                                            }
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: !shelvesSpotlight.game ? ""
                                            : (shelvesSpotlight.game.metadataTitle || shelvesSpotlight.game.title)
                                        color: theme.textPrimary
                                        font.pixelSize: 38 * theme.fontScale
                                        font.bold: true
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        readonly property string year: shelvesSpotlight.game
                                            && shelvesSpotlight.game.metadataReleaseDate
                                            ? String(shelvesSpotlight.game.metadataReleaseDate).substring(0, 4) : ""
                                        readonly property string rating: shelvesSpotlight.game
                                            ? window.formatGameRating(shelvesSpotlight.game.metadataRating) : ""
                                        visible: text.length > 0
                                        text: [year, rating].filter(part => part.length > 0).join("  ·  ")
                                        color: theme.textSecondary
                                        font.pixelSize: 14 * theme.fontScale
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: shelvesSpotlight.game && shelvesSpotlight.game.metadataOverview
                                            ? shelvesSpotlight.game.metadataOverview : ""
                                        color: theme.textSecondary
                                        font.pixelSize: 13 * theme.fontScale
                                        wrapMode: Text.WordWrap
                                        elide: Text.ElideRight
                                        maximumLineCount: 3
                                        verticalAlignment: Text.AlignTop
                                    }

                                    RowLayout {
                                        id: spotlightActions
                                        spacing: 10
                                        Button {
                                            id: spotlightPlayButton
                                            visible: shelvesSpotlight.installed
                                            enabled: !shelvesSpotlight.running
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            highlighted: true
                                            text: shelvesSpotlight.running ? "Running…" : "Play"
                                            icon.source: "qrc:/icons/play.svg"
                                            icon.width: 16
                                            icon.height: 16
                                            implicitHeight: 44
                                            Layout.preferredWidth: 130
                                            background: Rectangle {
                                                radius: height / 2
                                                color: spotlightPlayButton.enabled ? theme.accent : theme.controlBackground
                                                border.width: spotlightPlayButton.activeFocus ? 3 : 0
                                                border.color: theme.textPrimary
                                            }
                                            onClicked: {
                                                if (shelvesSpotlight.project)
                                                    window.launchSupportedGame(shelvesSpotlight.project.id)
                                            }
                                        }
                                        Button {
                                            id: spotlightDetailsButton
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            text: "Details"
                                            implicitHeight: 44
                                            Layout.preferredWidth: 120
                                            background: Rectangle {
                                                radius: height / 2
                                                color: spotlightDetailsButton.hovered ? theme.controlHover : theme.controlBackground
                                                border.width: spotlightDetailsButton.activeFocus ? 2 : 0
                                                border.color: theme.accent
                                            }
                                            onClicked: window.openLibraryGameDetails(shelvesView.currentGame)
                                        }
                                    }
                                }

                                // The selected game's cover at its true shape.
                                GameArtwork {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 36
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: parent.height - 56
                                    width: Math.round(height * 1.4)
                                    visible: parent.width > 1100
                                    source: shelvesSpotlight.game ? shelvesSpotlight.game.artworkPath : ""
                                    platform: shelvesSpotlight.game ? shelvesSpotlight.game.platform : ""
                                    themeTokens: theme
                                    reducedMotion: settingsManager.reducedMotion
                                    frameMode: "platform"
                                    frameInset: 0
                                    showViewportBackground: false
                                }
                            }

                            Flickable {
                                id: shelvesFlick
                                anchors.top: shelvesSpotlight.bottom
                                anchors.topMargin: 14
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                clip: true
                                interactive: false
                                contentWidth: width
                                contentHeight: shelvesColumn.height
                                boundsBehavior: Flickable.StopAtBounds

                                Keys.onReturnPressed: window.openLibraryGameDetails(shelvesView.currentGame)
                                Keys.onEnterPressed: window.openLibraryGameDetails(shelvesView.currentGame)

                                // One handler for the whole area: vertical wheel movement
                                // scrolls the shelves; horizontal movement (tilt wheel or
                                // trackpad) scrolls the shelf under the pointer.
                                WheelHandler {
                                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                    onWheel: function(event) {
                                        const dx = event.pixelDelta.x !== 0 ? event.pixelDelta.x
                                            : (event.angleDelta.x / 120) * shelvesView.cardHeight
                                        const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y
                                            : (event.angleDelta.y / 120) * shelvesView.rowHeight * 0.6
                                        if (Math.abs(dx) > Math.abs(dy)) {
                                            const rowIndex = Math.floor((event.y + shelvesFlick.contentY)
                                                / (shelvesView.rowHeight + shelvesView.rowSpacing))
                                            const row = shelfRepeater.itemAt(rowIndex)
                                            if (row && row.shelfList) {
                                                const list = row.shelfList
                                                const minX = -list.leftMargin
                                                const maxX = Math.max(minX,
                                                    list.contentWidth + list.rightMargin - list.width)
                                                list.contentX = Math.max(minX, Math.min(maxX, list.contentX - dx))
                                            }
                                        } else {
                                            const maxY = Math.max(0, shelvesFlick.contentHeight - shelvesFlick.height)
                                            shelvesFlick.contentY = Math.max(0, Math.min(maxY, shelvesFlick.contentY - dy))
                                        }
                                        event.accepted = true
                                    }
                                }

                                Column {
                                    id: shelvesColumn
                                    width: shelvesFlick.width
                                    spacing: shelvesView.rowSpacing

                                    Repeater {
                                        id: shelfRepeater
                                        model: shelvesView.sections
                                        delegate: Item {
                                            id: shelfRow
                                            required property var modelData
                                            required property int index
                                            property alias shelfList: shelfList
                                            readonly property real cardRatio:
                                                LibraryGridLayout.frameRatioForPlatform(modelData.platform)
                                            width: shelvesColumn.width
                                            height: shelvesView.rowHeight

                                            Row {
                                                x: shelvesView.cardPadding
                                                height: shelvesView.headerHeight
                                                spacing: 10
                                                Label {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: window.platformDisplayName(shelfRow.modelData.platform)
                                                    color: theme.textPrimary
                                                    font.pixelSize: 19 * theme.fontScale
                                                    font.bold: true
                                                }
                                                Label {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: shelfRow.modelData.count
                                                        + (shelfRow.modelData.count === 1 ? " game" : " games")
                                                    color: theme.textMuted
                                                    font.pixelSize: 13 * theme.fontScale
                                                }
                                            }

                                            ListView {
                                                id: shelfList
                                                anchors.top: parent.top
                                                anchors.topMargin: shelvesView.headerHeight
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: shelvesView.cardHeight + shelvesView.cardPadding * 2
                                                orientation: ListView.Horizontal
                                                spacing: 16
                                                leftMargin: shelvesView.cardPadding
                                                rightMargin: shelvesView.cardPadding
                                                clip: true
                                                interactive: false
                                                keyNavigationEnabled: false
                                                boundsBehavior: Flickable.StopAtBounds
                                                model: shelfRow.modelData.count
                                                // Start at the leading margin so every shelf's first
                                                // card lines up with its header.
                                                Component.onCompleted: contentX = -leftMargin

                                                delegate: Item {
                                                    id: shelfCard
                                                    required property int index
                                                    readonly property int gameIndex: shelfRow.modelData.start + index
                                                    readonly property var game: gameGrid.model
                                                        ? gameGrid.model[gameIndex] : null
                                                    readonly property bool isCurrent: gameGrid.currentIndex === gameIndex
                                                    width: Math.round(shelvesView.cardHeight * shelfRow.cardRatio)
                                                    height: shelfList.height
                                                    z: isCurrent ? 2 : 1
                                                    scale: isCurrent ? 1.05 : 1.0
                                                    Behavior on scale {
                                                        enabled: !settingsManager.reducedMotion
                                                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                                    }

                                                    GameArtwork {
                                                        anchors.centerIn: parent
                                                        width: parent.width
                                                        height: shelvesView.cardHeight
                                                        source: shelfCard.game ? shelfCard.game.artworkPath : ""
                                                        platform: shelfCard.game ? shelfCard.game.platform : ""
                                                        highlighted: shelfCard.isCurrent || shelfCardMouse.containsMouse
                                                        focused: shelfCard.isCurrent
                                                        hovered: shelfCardMouse.containsMouse
                                                        reducedMotion: settingsManager.reducedMotion
                                                        themeTokens: theme
                                                        frameMode: "platform"
                                                        frameInset: 0
                                                        showViewportBackground: false
                                                        showFocusRing: true
                                                    }

                                                    MouseArea {
                                                        id: shelfCardMouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        Accessible.name: shelfCard.game
                                                            ? (shelfCard.game.metadataTitle || shelfCard.game.title) : ""
                                                        onEntered: {
                                                            gameGrid.currentIndex = shelfCard.gameIndex
                                                            window.activeInputDevice = "mouse"
                                                        }
                                                        onClicked: {
                                                            gameGrid.currentIndex = shelfCard.gameIndex
                                                            window.openLibraryGameDetails(shelfCard.game)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ListView {
                            id: gameList
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: gameGrid.hintBarClearance
                            // Rows take the left part of the canvas; the preview pane
                            // (listPreview) uses the rest.
                            width: Math.min(parent.width,
                                Math.max(420, Math.min(parent.width * 0.45, 900)))
                            // All Platforms groups rows under platform headers, like the
                            // sectioned Grid. Headers are not focusable.
                            section.property: window.selectedPlatform === "All Platforms" ? "platform" : ""
                            section.criteria: ViewSection.FullString
                            section.delegate: Item {
                                required property string section
                                width: gameList.width - gameList.leftMargin - gameList.rightMargin
                                height: Math.ceil(42 * theme.fontScale)
                                Row {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 8
                                    spacing: 10
                                    Label {
                                        text: window.platformDisplayName(parent.parent.section)
                                        color: theme.textPrimary
                                        font.pixelSize: 17 * theme.fontScale
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        visible: window.librarySearchText.trim().length === 0
                                        readonly property int total:
                                            window.libraryPlatformCounts[parent.parent.section] || 0
                                        text: total + (total === 1 ? " game" : " games")
                                        color: theme.textMuted
                                        font.pixelSize: 12 * theme.fontScale
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
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
                                readonly property int rowHeight: Math.round(thumbHeight
                                    + (settingsManager.libraryListDensity === "compact" ? 20 : 28))
                                height: rowHeight
                                // Use a shorter eased fade than Grid View because List
                                // rows are much shorter, while keeping the same minimum
                                // edge visibility and Reduced Motion behavior.
                                readonly property real scrollEdgeDistance: Math.min(
                                    listTile.y + listTile.height / 2 - gameList.contentY,
                                    gameList.contentY + gameList.height
                                        - (listTile.y + listTile.height / 2))
                                readonly property real scrollEdgeProgress: settingsManager.reducedMotion
                                    ? 1
                                    : Math.max(0, Math.min(1, scrollEdgeDistance / 120))
                                readonly property real scrollEdgeEase:
                                    scrollEdgeProgress * scrollEdgeProgress
                                    * (3 - 2 * scrollEdgeProgress)
                                readonly property real scrollEdgeOpacity:
                                    settingsManager.reducedMotion ? 1
                                    : 0.18 + 0.82 * scrollEdgeEase
                                opacity: scrollEdgeOpacity
                                focus: ListView.isCurrentItem
                                z: highlighted ? 2 : 1
                                scale: ListView.isCurrentItem ? 1.02 : 1.0
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
                                Keys.onReturnPressed: window.libraryPrimaryAction(listTile.index)
                                Keys.onEnterPressed: window.libraryPrimaryAction(listTile.index)
                                readonly property real thumbHeight:
                                    settingsManager.libraryListDensity === "compact" ? 60
                                    : settingsManager.libraryListDensity === "spacious" ? 108 : 84
                                // Wide enough for a 7:5 N64 cover, so every title starts
                                // at the same x whatever the cover shape.
                                readonly property real thumbColumnWidth: Math.round(thumbHeight * 1.4)
                                readonly property var project: window.libraryGameProject({
                                    title: listTile.title, metadataTitle: listTile.metadataTitle,
                                    platform: listTile.platform })
                                readonly property bool installed: project !== null
                                    && window.installedLauncherProjectIds[project.id] === true
                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radiusCard
                                    color: listTile.highlighted
                                        ? theme.elevatedSurface
                                        : Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.42)
                                    border.width: listTile.ListView.isCurrentItem ? 2 : 0
                                    border.color: theme.accent
                                    Behavior on color {
                                        enabled: !settingsManager.reducedMotion
                                        ColorAnimation { duration: 170; easing.type: Easing.OutCubic }
                                    }
                                }
                                GameArtwork {
                                    id: listThumbnail
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: listTile.thumbColumnWidth
                                    height: listTile.thumbHeight
                                    source: listTile.artworkPath
                                    platform: listTile.platform
                                    themeTokens: theme
                                    reducedMotion: settingsManager.reducedMotion
                                    frameMode: "platform"
                                    frameInset: 0
                                    showViewportBackground: false
                                }
                                Column {
                                    anchors.left: listThumbnail.right
                                    anchors.leftMargin: 16
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Label {
                                        width: parent.width
                                        text: listTile.metadataTitle.length > 0 ? listTile.metadataTitle : listTile.title
                                        color: theme.textPrimary
                                        font.pixelSize: (settingsManager.libraryListDensity === "compact" ? 15 : 16) * theme.fontScale
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        width: parent.width
                                        text: window.platformDisplayName(listTile.platform)
                                            + (listTile.metadataReleaseDate.length >= 4
                                                ? "  ·  " + listTile.metadataReleaseDate.substring(0, 4) : "")
                                        color: theme.textMuted
                                        font.pixelSize: 12 * theme.fontScale
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        width: parent.width
                                        visible: listTile.project !== null
                                            && settingsManager.libraryListDensity !== "compact"
                                        text: listTile.installed ? "Recomp installed" : "Recompilation available"
                                        color: listTile.installed ? theme.success : theme.accentHover
                                        font.pixelSize: 12 * theme.fontScale
                                        font.bold: true
                                        elide: Text.ElideRight
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

                        // List preview pane: follows the selected game. Screenshot
                        // header (blurred cover when none), cover at its true shape,
                        // chips, title, year and rating, overview, up to three
                        // screenshot thumbnails, then Play and Details.
                        Rectangle {
                            id: listPreview
                            anchors.left: gameList.right
                            anchors.leftMargin: 24
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: 6
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: gameGrid.hintBarClearance
                            readonly property bool active: settingsManager.libraryView === "list"
                                && gameGrid.count > 0 && width > 360
                            visible: active || opacity > 0
                            opacity: active ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                            }
                            radius: theme.radiusLarge
                            color: theme.sidebarBackground
                            border.width: 1
                            border.color: Qt.rgba(theme.divider.r, theme.divider.g, theme.divider.b, 0.6)
                            clip: true

                            readonly property var game: {
                                const games = gameGrid.model || []
                                return games.length > 0
                                    ? games[Math.max(0, Math.min(games.length - 1, gameGrid.currentIndex))] : null
                            }
                            readonly property var shots: game && game.screenshots ? game.screenshots : []
                            readonly property bool hasScreenshot: shots.length > 0
                            readonly property var project: window.libraryGameProject(game)
                            readonly property bool installed: window.libraryGameInstalled(game)
                            readonly property bool running: project !== null
                                && window.runningLaunchers[project.id] === true
                            readonly property real coverHeight: Math.round(Math.max(140, Math.min(220, height * 0.3)))

                            Item {
                                id: listPreviewHeader
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: Math.round(parent.height * 0.42)
                                clip: true
                                ScreenshotBackdrop {
                                    id: listPreviewBackdrop
                                    anchors.fill: parent
                                    alignment: "center"
                                    sources: listPreview.shots
                                    fallbackSource: listPreview.game ? listPreview.game.artworkPath : ""
                                    cycling: listPreview.active && currentPage === "Library"
                                        && !gameDetailsPopup.visible && !screenshotViewer.visible
                                    reducedMotion: settingsManager.reducedMotion
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    gradient: Gradient {
                                        GradientStop { position: 0.35; color: Qt.rgba(theme.sidebarBackground.r, theme.sidebarBackground.g, theme.sidebarBackground.b, 0) }
                                        GradientStop { position: 0.78; color: Qt.rgba(theme.sidebarBackground.r, theme.sidebarBackground.g, theme.sidebarBackground.b, 0.85) }
                                        GradientStop { position: 1.0; color: theme.sidebarBackground }
                                    }
                                }
                            }

                            GameArtwork {
                                id: listPreviewCover
                                x: 28
                                y: listPreviewHeader.height - Math.round(listPreview.coverHeight * 0.55)
                                height: listPreview.coverHeight
                                width: Math.round(height * LibraryGridLayout.frameRatioForPlatform(
                                    listPreview.game ? listPreview.game.platform : ""))
                                source: listPreview.game ? listPreview.game.artworkPath : ""
                                platform: listPreview.game ? listPreview.game.platform : ""
                                themeTokens: theme
                                reducedMotion: settingsManager.reducedMotion
                                frameMode: "platform"
                                frameInset: 0
                                showViewportBackground: false
                            }

                            Column {
                                anchors.left: listPreviewCover.right
                                anchors.leftMargin: 24
                                anchors.right: parent.right
                                anchors.rightMargin: 28
                                anchors.bottom: listPreviewCover.bottom
                                spacing: 8
                                Row {
                                    spacing: 8
                                    Rectangle {
                                        height: 24
                                        width: listPreviewPlatformLabel.implicitWidth + 20
                                        radius: 12
                                        color: Qt.rgba(theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.14)
                                        Label {
                                            id: listPreviewPlatformLabel
                                            anchors.centerIn: parent
                                            text: listPreview.game
                                                ? window.platformDisplayName(listPreview.game.platform).toUpperCase() : ""
                                            color: theme.textPrimary
                                            font.pixelSize: 11 * theme.fontScale
                                            font.bold: true
                                            font.letterSpacing: 0.6
                                        }
                                    }
                                    Rectangle {
                                        visible: listPreview.project !== null
                                        height: 24
                                        width: Math.min(listPreviewStatusLabel.implicitWidth + 20,
                                            listPreview.width - listPreviewCover.width - 200)
                                        radius: 12
                                        color: listPreview.installed
                                            ? Qt.rgba(theme.success.r, theme.success.g, theme.success.b, 0.16)
                                            : Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                                        Label {
                                            id: listPreviewStatusLabel
                                            anchors.centerIn: parent
                                            width: Math.min(implicitWidth, parent.width - 20)
                                            text: !listPreview.project ? ""
                                                : listPreview.project.name + (listPreview.installed ? " · Installed" : " · Available")
                                            color: listPreview.installed ? theme.success : theme.accentHover
                                            font.pixelSize: 11 * theme.fontScale
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                Label {
                                    width: parent.width
                                    text: listPreview.game
                                        ? (listPreview.game.metadataTitle || listPreview.game.title) : ""
                                    color: theme.textPrimary
                                    font.pixelSize: 28 * theme.fontScale
                                    font.bold: true
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                                Label {
                                    width: parent.width
                                    readonly property string year: listPreview.game && listPreview.game.metadataReleaseDate
                                        ? String(listPreview.game.metadataReleaseDate).substring(0, 4) : ""
                                    readonly property string rating: listPreview.game
                                        ? window.formatGameRating(listPreview.game.metadataRating) : ""
                                    visible: text.length > 0
                                    text: [year, rating].filter(part => part.length > 0).join("  ·  ")
                                    color: theme.textSecondary
                                    font.pixelSize: 14 * theme.fontScale
                                }
                                Row {
                                    id: listPreviewActions
                                    topPadding: 6
                                    spacing: 10
                                    Button {
                                        id: listPreviewPlayButton
                                        visible: listPreview.installed
                                        enabled: !listPreview.running
                                        focusPolicy: Qt.NoFocus
                                        text: listPreview.running ? "Running…" : "Play"
                                        icon.source: "qrc:/icons/play.svg"
                                        icon.width: 16
                                        icon.height: 16
                                        implicitHeight: 44
                                        width: 130
                                        background: Rectangle {
                                            radius: height / 2
                                            color: listPreviewPlayButton.enabled ? theme.accent : theme.controlBackground
                                            border.width: 3
                                            border.color: theme.textPrimary
                                        }
                                        onClicked: window.libraryPrimaryAction(gameGrid.currentIndex)
                                    }
                                    Button {
                                        id: listPreviewDetailsButton
                                        focusPolicy: Qt.NoFocus
                                        text: "Details"
                                        implicitHeight: 44
                                        width: 120
                                        background: Rectangle {
                                            radius: height / 2
                                            color: listPreviewDetailsButton.hovered ? theme.controlHover : theme.controlBackground
                                            border.width: listPreview.installed ? 0 : 3
                                            border.color: theme.textPrimary
                                        }
                                        onClicked: window.openLibraryGameDetails(listPreview.game)
                                    }
                                }
                            }

                            Label {
                                id: listPreviewOverview
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 28
                                anchors.rightMargin: 28
                                anchors.top: listPreviewCover.bottom
                                anchors.topMargin: 20
                                text: listPreview.game && listPreview.game.metadataOverview
                                    ? listPreview.game.metadataOverview : ""
                                visible: text.length > 0
                                color: theme.textSecondary
                                font.pixelSize: 14 * theme.fontScale
                                lineHeight: 1.2
                                wrapMode: Text.WordWrap
                                maximumLineCount: 4
                                elide: Text.ElideRight
                            }

                            Column {
                                id: listPreviewShots
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 28
                                anchors.rightMargin: 28
                                anchors.top: listPreviewOverview.visible ? listPreviewOverview.bottom : listPreviewCover.bottom
                                anchors.topMargin: 20
                                spacing: 8
                                // Three thumbnails share the pane width, limited by the
                                // height left below the overview; hidden when too small.
                                readonly property real thumbWidth: Math.floor(Math.min(
                                    (width - 20) / 3,
                                    Math.max(0, listPreview.height - y - 24 - 28 * theme.fontScale) * 16 / 9))
                                visible: listPreview.hasScreenshot ? thumbWidth >= 120 : y + height <= listPreview.height - 20
                                Label {
                                    text: "SCREENSHOTS"
                                    color: theme.textMuted
                                    font.pixelSize: 11 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 0.8
                                }
                                Row {
                                    spacing: 10
                                    Repeater {
                                        model: listPreview.shots.slice(0, 3)
                                        // Right from the list (or a click) reaches these; A
                                        // opens the screenshot full screen.
                                        delegate: Button {
                                            id: listShotButton
                                            required property string modelData
                                            required property int index
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            width: listPreviewShots.thumbWidth
                                            height: Math.round(width * 9 / 16)
                                            padding: 0
                                            Accessible.name: "Screenshot " + (index + 1)
                                            contentItem: Item {}
                                            background: Rectangle {
                                                radius: theme.radiusSmall
                                                color: theme.surface
                                                clip: true
                                                border.width: listShotButton.activeFocus ? 3
                                                    : listShotButton.index === listPreviewBackdrop.currentIndex ? 2 : 0
                                                border.color: listShotButton.activeFocus ? theme.accent : theme.textPrimary
                                                Image {
                                                    anchors.fill: parent
                                                    anchors.margins: parent.border.width
                                                    source: listShotButton.modelData
                                                    fillMode: Image.PreserveAspectCrop
                                                    asynchronous: true
                                                    cache: true
                                                }
                                            }
                                            onClicked: screenshotViewer.show(listPreview.shots, index, listShotButton)
                                        }
                                    }
                                    Label {
                                        visible: !listPreview.hasScreenshot
                                        text: "No screenshots yet. Run Refresh All Metadata in Settings to fetch them."
                                        color: theme.textMuted
                                        font.pixelSize: 13 * theme.fontScale
                                    }
                                }
                            }
                        }

                        // Hero carousel: the selected game's screenshot fills the
                        // window (carouselHeroBackdrop), one row of covers crosses the
                        // screen with the selected cover centered and enlarged, and the
                        // selected game's details and actions sit beneath it. Card
                        // positions come from LibraryGridLayout.carouselPositions.
                        // Selection mirrors gameGrid.currentIndex.
                        Item {
                            id: consoleView
                            anchors.fill: parent
                            // Run edge to edge across the window.
                            anchors.leftMargin: -30
                            anchors.rightMargin: -30
                            anchors.bottomMargin: gameGrid.hintBarClearance
                            visible: settingsManager.libraryView === "carousel" || opacity > 0
                            opacity: settingsManager.libraryView === "carousel" ? 1 : 0
                            Behavior on opacity {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 210; easing.type: Easing.OutCubic }
                            }
                            clip: true

                            property int currentIndex: -1
                            readonly property int count: games.length
                            readonly property var games: gameGrid.model || []
                            readonly property var currentGame: count > 0
                                ? games[Math.max(0, Math.min(count - 1, currentIndex))] : null

                            // Kept for callers shared with the list-based views; card
                            // positions follow currentIndex automatically.
                            function positionViewAtIndex(index, mode) {}

                            onCountChanged: {
                                if (count > 0 && currentIndex < 0)
                                    currentIndex = Math.max(0, gameGrid.currentIndex)
                            }
                            onCurrentIndexChanged: {
                                if (currentIndex !== gameGrid.currentIndex)
                                    gameGrid.currentIndex = currentIndex
                            }
                            Component.onCompleted: currentIndex = Math.max(0, gameGrid.currentIndex)
                            Connections {
                                target: gameGrid
                                function onCurrentIndexChanged() {
                                    if (consoleView.currentIndex !== gameGrid.currentIndex)
                                        consoleView.currentIndex = gameGrid.currentIndex
                                }
                            }
                            Keys.onReturnPressed: window.libraryPrimaryAction(consoleView.currentIndex)
                            Keys.onEnterPressed: window.libraryPrimaryAction(consoleView.currentIndex)

                            // Sizes scale gently with the available height.
                            readonly property real layoutScale:
                                Math.min(1.25, Math.max(0.85, height / 760))
                            readonly property real baseHeight: Math.round(
                                (settingsManager.libraryConsoleDensity === "compact" ? 170
                                : settingsManager.libraryConsoleDensity === "spacious" ? 250 : 210)
                                * layoutScale)
                            readonly property real selectedHeight: Math.round(baseHeight * 1.43)
                            readonly property real gap: Math.round(30 * layoutScale)
                            readonly property real infoHeight: Math.ceil(196 * theme.fontScale)
                            // Center the row and the details beneath it as one block,
                            // biased slightly low so more of the backdrop shows above.
                            readonly property real blockHeight: selectedHeight + 26 + infoHeight
                            readonly property real rowCenterY: Math.round(Math.max(
                                selectedHeight / 2 + 16,
                                (height - blockHeight) * 0.6 + selectedHeight / 2))

                            function ratioAt(index) {
                                const game = games[index]
                                return LibraryGridLayout.frameRatioForPlatform(game ? game.platform : "")
                            }
                            readonly property var baseWidths: {
                                const widths = []
                                for (let i = 0; i < games.length; ++i)
                                    widths.push(Math.round(baseHeight * ratioAt(i)))
                                return widths
                            }
                            readonly property var positions: LibraryGridLayout.carouselPositions(
                                baseWidths, Math.max(0, currentIndex),
                                Math.round(selectedHeight * ratioAt(Math.max(0, currentIndex))),
                                gap, width / 2)

                            // Horizontal wheel or trackpad gestures step the selection.
                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                property real accumulated: 0
                                onWheel: function(event) {
                                    const delta = event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y
                                    accumulated += delta
                                    if (Math.abs(accumulated) >= 120) {
                                        const step = accumulated > 0 ? -1 : 1
                                        accumulated = 0
                                        consoleView.currentIndex = Math.max(0, Math.min(
                                            consoleView.count - 1, consoleView.currentIndex + step))
                                        window.activeInputDevice = "mouse"
                                    }
                                    event.accepted = true
                                }
                            }

                            Repeater {
                                model: consoleView.count
                                delegate: Item {
                                    id: carouselCard
                                    required property int index
                                    readonly property var game: consoleView.games[index]
                                    readonly property bool isCurrent: index === consoleView.currentIndex
                                    readonly property real cardHeight:
                                        isCurrent ? consoleView.selectedHeight : consoleView.baseHeight
                                    readonly property real centerDistance:
                                        Math.abs(x + width / 2 - consoleView.width / 2)
                                    x: consoleView.positions[index] !== undefined ? consoleView.positions[index] : 0
                                    y: consoleView.rowCenterY - height / 2
                                    width: Math.round(cardHeight * consoleView.ratioAt(index))
                                    height: cardHeight
                                    z: isCurrent ? 2 : 1
                                    // Skip cards well outside the window.
                                    visible: x + width > -consoleView.width * 0.25
                                        && x < consoleView.width * 1.25
                                    opacity: isCurrent ? 1
                                        : Math.max(0.16, 0.86 - centerDistance / (consoleView.width * 0.62))
                                    Behavior on x {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on width {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on height {
                                        enabled: !settingsManager.reducedMotion
                                        NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
                                    }

                                    GameArtwork {
                                        anchors.fill: parent
                                        source: carouselCard.game ? carouselCard.game.artworkPath : ""
                                        platform: carouselCard.game ? carouselCard.game.platform : ""
                                        highlighted: carouselCard.isCurrent || carouselCardMouse.containsMouse
                                        focused: carouselCard.isCurrent
                                        hovered: carouselCardMouse.containsMouse
                                        reducedMotion: settingsManager.reducedMotion
                                        themeTokens: theme
                                        frameMode: "platform"
                                        frameInset: 0
                                        showViewportBackground: false
                                        showFocusRing: true
                                    }

                                    // Hover only highlights: the row recenters under the
                                    // pointer, so hovering must not change the selection.
                                    // Click selects a card, or runs the primary action on
                                    // the selected one.
                                    MouseArea {
                                        id: carouselCardMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        Accessible.name: carouselCard.game
                                            ? (carouselCard.game.metadataTitle || carouselCard.game.title) : ""
                                        onEntered: window.activeInputDevice = "mouse"
                                        onClicked: {
                                            if (carouselCard.isCurrent)
                                                window.libraryPrimaryAction(carouselCard.index)
                                            else
                                                consoleView.currentIndex = carouselCard.index
                                        }
                                    }
                                }
                            }

                            // Selected game details and actions.
                            ColumnLayout {
                                id: carouselInfo
                                readonly property var game: consoleView.currentGame
                                readonly property var project: window.libraryGameProject(game)
                                readonly property bool installed: window.libraryGameInstalled(game)
                                readonly property bool running: project !== null
                                    && window.runningLaunchers[project.id] === true
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: consoleView.rowCenterY + consoleView.selectedHeight / 2 + 26
                                width: Math.min(parent.width - 120, 900)
                                spacing: 8
                                visible: game !== null

                                Row {
                                    Layout.alignment: Qt.AlignHCenter
                                    spacing: 8
                                    Rectangle {
                                        height: 24
                                        width: carouselPlatformLabel.implicitWidth + 20
                                        radius: 12
                                        color: Qt.rgba(theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.14)
                                        Label {
                                            id: carouselPlatformLabel
                                            anchors.centerIn: parent
                                            text: carouselInfo.game
                                                ? window.platformDisplayName(carouselInfo.game.platform).toUpperCase() : ""
                                            color: theme.textPrimary
                                            font.pixelSize: 11 * theme.fontScale
                                            font.bold: true
                                            font.letterSpacing: 0.6
                                        }
                                    }
                                    Rectangle {
                                        visible: carouselInfo.project !== null
                                        height: 24
                                        width: carouselStatusLabel.implicitWidth + 20
                                        radius: 12
                                        color: carouselInfo.installed
                                            ? Qt.rgba(theme.success.r, theme.success.g, theme.success.b, 0.16)
                                            : Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                                        Label {
                                            id: carouselStatusLabel
                                            anchors.centerIn: parent
                                            text: !carouselInfo.project ? ""
                                                : carouselInfo.project.name
                                                    + (carouselInfo.installed ? " · Installed" : " · Available")
                                            color: carouselInfo.installed ? theme.success : theme.accentHover
                                            font.pixelSize: 11 * theme.fontScale
                                            font.bold: true
                                        }
                                    }
                                    Rectangle {
                                        height: 24
                                        width: carouselPositionLabel.implicitWidth + 20
                                        radius: 12
                                        color: Qt.rgba(theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.08)
                                        Label {
                                            id: carouselPositionLabel
                                            anchors.centerIn: parent
                                            text: (consoleView.currentIndex + 1) + " of " + consoleView.count
                                            color: theme.textSecondary
                                            font.pixelSize: 11 * theme.fontScale
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    text: carouselInfo.game
                                        ? (carouselInfo.game.metadataTitle || carouselInfo.game.title) : ""
                                    color: theme.textPrimary
                                    font.pixelSize: 36 * theme.fontScale
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    readonly property string year: carouselInfo.game && carouselInfo.game.metadataReleaseDate
                                        ? String(carouselInfo.game.metadataReleaseDate).substring(0, 4) : ""
                                    readonly property string rating: carouselInfo.game
                                        ? window.formatGameRating(carouselInfo.game.metadataRating) : ""
                                    visible: text.length > 0
                                    text: [year, rating].filter(part => part.length > 0).join("  ·  ")
                                    color: theme.textSecondary
                                    font.pixelSize: 14 * theme.fontScale
                                }

                                Row {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.topMargin: 6
                                    spacing: 10
                                    Button {
                                        id: carouselPlayButton
                                        visible: carouselInfo.installed
                                        enabled: !carouselInfo.running
                                        focusPolicy: Qt.NoFocus
                                        text: carouselInfo.running ? "Running…" : "Play"
                                        icon.source: "qrc:/icons/play.svg"
                                        icon.width: 16
                                        icon.height: 16
                                        implicitHeight: 44
                                        width: 130
                                        background: Rectangle {
                                            radius: height / 2
                                            color: carouselPlayButton.enabled ? theme.accent : theme.controlBackground
                                            border.width: 3
                                            border.color: theme.textPrimary
                                        }
                                        onClicked: window.libraryPrimaryAction(consoleView.currentIndex)
                                    }
                                    Button {
                                        id: carouselDetailsButton
                                        focusPolicy: Qt.NoFocus
                                        text: "Details"
                                        implicitHeight: 44
                                        width: 120
                                        background: Rectangle {
                                            radius: height / 2
                                            color: carouselDetailsButton.hovered ? theme.controlHover : theme.controlBackground
                                            border.width: carouselInfo.installed ? 0 : 3
                                            border.color: theme.textPrimary
                                        }
                                        onClicked: window.openLibraryGameDetails(carouselInfo.game)
                                    }
                                }
                            }
                        }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: gameGrid.count === 0
                            radius: theme.radiusLarge
                            // Apply transparency to the panel fill only. Child text
                            // retains its own fully opaque theme colors.
                            color: Qt.rgba(theme.surface.r, theme.surface.g,
                                           theme.surface.b, 0.78)
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
                                    text: "Place supported game files in your platform folders. RecompStation discovers them automatically and keeps your collection up to date."
                                    color: theme.textSecondary
                                    font.pixelSize: 13 * theme.fontScale
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: libraryScanner.scanning
                                    text: window.scanMessage
                                    color: theme.accent
                                    font.pixelSize: 12 * theme.fontScale
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
                    onSectionChanged: {
                        console.info("[Settings QML] Section changed:", section)
                        window.previewThemeId = settingsManager.themeName
                        if (controllerManager.mappingCaptureActive && section !== "Controller")
                            controllerManager.cancelMappingCapture()
                    }

                    RowLayout {
                        id: settingsLayoutRow
                        anchors.fill: parent
                        spacing: 22

                        // Category rail: each entry shows a summary of its section's values.
                        Item {
                            id: settingsNavigationPanel
                            Layout.preferredWidth: 280
                            Layout.minimumWidth: 240
                            Layout.maximumWidth: 280
                            Layout.fillHeight: true

                            ColumnLayout {
                                id: settingsNavigationColumn
                                anchors.fill: parent
                                anchors.topMargin: 6
                                spacing: 4

                                Label {
                                    Layout.leftMargin: 12
                                    Layout.bottomMargin: 10
                                    text: "SETTINGS"
                                    color: theme.textMuted
                                    font.pixelSize: 11 * theme.fontScale
                                    font.bold: true
                                    font.letterSpacing: 1.4
                                }
                                SettingsNavItem {
                                    id: appearanceNavigationButton
                                    Layout.fillWidth: true
                                    text: "Appearance"
                                    iconSource: "qrc:/icons/appearance.svg"
                                    summary: window.settingsSectionSummary("Appearance")
                                    current: settingsPage.section === "Appearance"
                                    onClicked: settingsPage.section = "Appearance"
                                }
                                SettingsNavItem {
                                    id: displayNavigationButton
                                    Layout.fillWidth: true
                                    text: "Display"
                                    iconSource: "qrc:/icons/display.svg"
                                    summary: window.settingsSectionSummary("Display")
                                    current: settingsPage.section === "Display"
                                    onClicked: settingsPage.section = "Display"
                                }
                                SettingsNavItem {
                                    id: backgroundNavigationButton
                                    Layout.fillWidth: true
                                    text: "Background"
                                    iconSource: "qrc:/icons/background.svg"
                                    summary: window.settingsSectionSummary("Background")
                                    current: settingsPage.section === "Background"
                                    onClicked: settingsPage.section = "Background"
                                }
                                SettingsNavItem {
                                    id: libraryNavigationButton
                                    Layout.fillWidth: true
                                    text: "Library"
                                    iconSource: "qrc:/icons/library.svg"
                                    summary: window.settingsSectionSummary("Library")
                                    current: settingsPage.section === "Library"
                                    onClicked: settingsPage.section = "Library"
                                }
                                SettingsNavItem {
                                    id: metadataNavigationButton
                                    Layout.fillWidth: true
                                    text: "Metadata"
                                    iconSource: "qrc:/icons/metadata.svg"
                                    summary: window.settingsSectionSummary("Metadata")
                                    current: settingsPage.section === "Metadata"
                                    onClicked: settingsPage.section = "Metadata"
                                }
                                SettingsNavItem {
                                    id: controllerNavigationButton
                                    Layout.fillWidth: true
                                    text: "Controller"
                                    iconSource: "qrc:/icons/gamepad.svg"
                                    summary: window.settingsSectionSummary("Controller")
                                    current: settingsPage.section === "Controller"
                                    onClicked: settingsPage.section = "Controller"
                                }
                                SettingsNavItem {
                                    id: applicationNavigationButton
                                    Layout.fillWidth: true
                                    text: "Application"
                                    iconSource: "qrc:/icons/application.svg"
                                    summary: window.settingsSectionSummary("Application")
                                    current: settingsPage.section === "Application"
                                    onClicked: settingsPage.section = "Application"
                                }
                                Item { Layout.fillHeight: true }
                            }
                        }

                        Rectangle {
                            id: settingsContentCard
                            Layout.minimumWidth: 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            clip: true

                            ScrollView {
                                id: appearanceContent
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Appearance"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: appearanceContent.availableWidth
                                    spacing: 8

                                    Label {
                                        text: "Appearance"
                                        color: theme.textPrimary
                                        font.pixelSize: 30 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: 8
                                        text: "Themes apply as soon as you pick one. Focus a tile to preview it."
                                        color: theme.textSecondary
                                        font.pixelSize: 14 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    SettingsGroupLabel { text: "THEME" }

                                    GridLayout {
                                        id: themeTileGrid
                                        Layout.fillWidth: true
                                        columns: width >= 560 ? 2 : 1
                                        columnSpacing: 12
                                        rowSpacing: 12
                                        Repeater {
                                            model: theme.presets
                                            delegate: ThemeTile {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                Layout.preferredWidth: 1
                                                themeId: modelData.id
                                                title: modelData.title
                                                description: modelData.description
                                                inUse: settingsManager.themeName === modelData.id
                                                // Preview the focused or hovered tile; fall back to the theme in use.
                                                onActiveFocusChanged: window.previewThemeId = activeFocus || hovered
                                                    ? modelData.id : settingsManager.themeName
                                                onHoveredChanged: window.previewThemeId = activeFocus || hovered
                                                    ? modelData.id : settingsManager.themeName
                                                onClicked: settingsManager.themeName = modelData.id
                                            }
                                        }
                                    }

                                    SettingsGroupLabel { text: "TEXT & MOTION" }
                                    SettingRow {
                                        title: "Text size"
                                        description: "Applied on top of the interface scale in Display."
                                        ValueCycler {
                                            id: fontSizeSelector
                                            options: [
                                                { label: "Small", value: "small" },
                                                { label: "Medium", value: "medium" },
                                                { label: "Large", value: "large" }
                                            ]
                                            value: settingsManager.fontSize
                                            onPicked: (value) => settingsManager.fontSize = value
                                        }
                                    }
                                    SettingRow {
                                        title: "Reduced motion"
                                        description: "Turns off animated backgrounds, carousel slides and screenshot cycling."
                                        ToggleSwitch {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            checked: settingsManager.reducedMotion
                                            onToggled: settingsManager.reducedMotion = checked
                                            Accessible.name: "Reduced motion"
                                        }
                                    }
                                    Item { Layout.preferredHeight: 24 }
                                }
                            }

                            ScrollView {
                                id: displayContent
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Display"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: displayContent.availableWidth
                                    spacing: 8

                                    Label {
                                        text: "Display"
                                        color: theme.textPrimary
                                        font.pixelSize: 30 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: 8
                                        text: "How RecompStation fills your screen. Your monitor resolution is never changed."
                                        color: theme.textSecondary
                                        font.pixelSize: 14 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    SettingsGroupLabel { text: "SCALING" }
                                    SettingRow {
                                        title: "Interface scale"
                                        description: interfaceScaleInfo.external
                                            ? "QT_SCALE_FACTOR=" + interfaceScaleInfo.environmentValue + " is set in your environment, so RecompStation leaves scaling to it."
                                            : "Auto picks a size from your screen height. Text, covers and spacing scale together."
                                        ValueCycler {
                                            id: interfaceScaleSelector
                                            preferredWidth: 250
                                            enabled: !interfaceScaleInfo.external
                                            options: settingsManager.interfaceScaleOptions().map(function(value) {
                                                return { label: window.interfaceScaleLabel(value), value: value }
                                            })
                                            value: settingsManager.interfaceScale
                                            onPicked: (value) => settingsManager.interfaceScale = value
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        visible: window.interfaceScaleRestartPending
                                        implicitHeight: scaleRestartRow.implicitHeight + 24
                                        radius: 12
                                        color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.12)
                                        border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.45)
                                        RowLayout {
                                            id: scaleRestartRow
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            anchors.leftMargin: 18
                                            spacing: 14
                                            Label {
                                                Layout.fillWidth: true
                                                text: "Restart RecompStation to switch from " + interfaceScaleInfo.applied + "% to "
                                                    + window.interfaceScalePercentFor(settingsManager.interfaceScale)
                                                    + "%. It reopens on this page."
                                                color: theme.textPrimary
                                                font.pixelSize: 13 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                            Button {
                                                id: restartForScaleButton
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                text: "Restart now"
                                                highlighted: true
                                                onClicked: settingsManager.requestApplicationRestart("Display")
                                            }
                                        }
                                    }
                                    SettingsGroupLabel { text: "WINDOW" }
                                    SettingRow {
                                        title: "Window mode"
                                        description: "Fullscreen gives the console experience. F11 switches at any time."
                                        ValueCycler {
                                            id: displayModeSelector
                                            options: [
                                                { label: "Fullscreen", value: "fullscreen" },
                                                { label: "Maximized", value: "maximized" },
                                                { label: "Windowed", value: "windowed" }
                                            ]
                                            value: settingsManager.displayMode
                                            onPicked: (mode) => {
                                                if (mode === "fullscreen" && window.visibility !== Window.FullScreen) {
                                                    window.preFullscreenMode = window.visibility === Window.Maximized
                                                        ? "maximized" : "windowed"
                                                    settingsManager.setRestoreDisplayMode(window.preFullscreenMode)
                                                }
                                                settingsManager.setDisplayMode(mode)
                                                window.applyDisplayMode(mode)
                                            }
                                        }
                                    }
                                    SettingRow {
                                        title: "Window size"
                                        description: settingsManager.displayMode === "windowed"
                                            ? "Sizes larger than your screen are scaled down to fit, keeping their shape."
                                            : "Used when the window mode is Windowed."
                                        dimmed: settingsManager.displayMode !== "windowed"
                                        ValueCycler {
                                            id: presentationSizeSelector
                                            enabled: settingsManager.displayMode === "windowed"
                                            options: [
                                                { label: "Keep current size", value: "system" },
                                                { label: "1280 × 720", value: "1280x720" },
                                                { label: "1920 × 1080", value: "1920x1080" },
                                                { label: "1920 × 1440 · 4:3", value: "1920x1440" },
                                                { label: "3440 × 1440 · 21:9", value: "3440x1440" },
                                                { label: "3840 × 2160", value: "3840x2160" },
                                                { label: "5120 × 2160 · 21:9", value: "5120x2160" }
                                            ]
                                            value: settingsManager.preferredPresentationSize
                                            onPicked: (size) => {
                                                settingsManager.setPreferredPresentationSize(size)
                                                if (size !== "system")
                                                    window.applyPreferredPresentationSize()
                                            }
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 6
                                        Layout.leftMargin: 4
                                        visible: !settingsPreviewPane.visible
                                        text: (interfaceScaleInfo.screenName || "This screen") + ": " + interfaceScaleInfo.logicalWidth
                                            + " × " + interfaceScaleInfo.logicalHeight + " · Auto scale " + interfaceScaleInfo.auto + "%"
                                        color: theme.textMuted
                                        font.pixelSize: 12 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Item { Layout.preferredHeight: 16 }
                                }
                            }

                            ScrollView {
                                id: backgroundContent
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Background"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: backgroundContent.availableWidth
                                    spacing: 8

                                    Label {
                                        text: "Background"
                                        color: theme.textPrimary
                                        font.pixelSize: 30 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: 8
                                        text: "The backdrop behind your Library."
                                        color: theme.textSecondary
                                        font.pixelSize: 14 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    SettingsGroupLabel { text: "STYLE" }
                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: width >= 520 ? 3 : 1
                                        columnSpacing: 10
                                        rowSpacing: 10
                                        Repeater {
                                            model: [
                                                { value: "static", title: "Gradient", description: "Static, follows the theme" },
                                                { value: "animated", title: "Waves", description: "Animated ribbons" },
                                                { value: "custom", title: "Image", description: "Your own wallpaper" }
                                            ]
                                            delegate: BackgroundStyleTile {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                Layout.preferredWidth: 1
                                                styleValue: modelData.value
                                                title: modelData.title
                                                description: modelData.description
                                                current: settingsManager.libraryBackgroundType === modelData.value
                                                wallpaper: settingsManager.customLibraryBackground
                                                waveColor: window.waveColorValue
                                                onClicked: settingsManager.libraryBackgroundType = modelData.value
                                            }
                                        }
                                    }

                                    SettingsGroupLabel {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                        text: "WAVES"
                                    }
                                    SettingRow {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                        title: "Color"
                                        description: libraryVisualSettings.waveColor === "theme" ? "Follows your theme's accent."
                                            : libraryVisualSettings.waveColor === "custom" ? "Pick the hue below." : ""
                                        ColorSwatchPicker {
                                            options: window.waveColorPresets
                                            value: libraryVisualSettings.waveColor
                                            onPicked: (color) => libraryVisualSettings.waveColor = color
                                        }
                                    }
                                    SettingRow {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                            && libraryVisualSettings.waveColor === "custom"
                                        title: "Custom hue"
                                        description: "Any color around the wheel."
                                        ValueCycler {
                                            options: window.waveHueOptions
                                            value: libraryVisualSettings.waveCustomHue
                                            onPicked: (hue) => libraryVisualSettings.waveCustomHue = hue
                                        }
                                    }
                                    SettingRow {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                        title: "Motion"
                                        description: settingsManager.reducedMotion
                                            ? "Paused while Reduced motion is on (Appearance)."
                                            : "Calm drifts slowly; Lively moves faster."
                                        ValueCycler {
                                            options: [
                                                { label: "Calm", value: "calm" },
                                                { label: "Normal", value: "normal" },
                                                { label: "Lively", value: "lively" }
                                            ]
                                            value: libraryVisualSettings.waveMotion
                                            onPicked: (motion) => libraryVisualSettings.waveMotion = motion
                                        }
                                    }
                                    SettingRow {
                                        visible: settingsManager.libraryBackgroundType === "animated"
                                        title: "Quality"
                                        description: "Balanced renders at 75% resolution; Low at half resolution and 30 fps, for older or integrated graphics."
                                        ValueCycler {
                                            options: [
                                                { label: "Auto · " + (window.waveQualityResolved === "balanced" ? "Balanced" : "High"), value: "auto" },
                                                { label: "High", value: "high" },
                                                { label: "Balanced", value: "balanced" },
                                                { label: "Low", value: "low" }
                                            ]
                                            value: libraryVisualSettings.waveQuality
                                            onPicked: (quality) => libraryVisualSettings.waveQuality = quality
                                        }
                                    }

                                    SettingsGroupLabel {
                                        visible: settingsManager.libraryBackgroundType === "custom"
                                        text: "IMAGE"
                                    }
                                    SettingRow {
                                        id: customBackgroundActionsRow
                                        visible: settingsManager.libraryBackgroundType === "custom"
                                        title: settingsManager.customLibraryBackground.length > 0 ? "Wallpaper" : "Choose a wallpaper"
                                        description: settingsManager.customLibraryBackground.length > 0
                                            ? decodeURIComponent(settingsManager.customLibraryBackground.toString().split("/").pop())
                                            : "PNG, JPEG or WebP. Large images are loaded at your screen's size to save memory."
                                        RowLayout {
                                            spacing: 8
                                            Button {
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                text: settingsManager.customLibraryBackground.length > 0 ? "Choose another" : "Choose image"
                                                highlighted: settingsManager.customLibraryBackground.length === 0
                                                onClicked: libraryBackgroundFileDialog.open()
                                                background: Rectangle {
                                                    radius: 9
                                                    color: parent.activeFocus ? theme.elevatedSurface
                                                        : (parent.highlighted ? theme.accent : theme.surface)
                                                    border.color: theme.accent
                                                    border.width: parent.activeFocus ? 2 : 0
                                                }
                                                contentItem: Label {
                                                    text: parent.text
                                                    color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                    opacity: parent.enabled ? 1 : 0.5
                                                    font.pixelSize: 13 * theme.fontScale
                                                    font.weight: Font.DemiBold
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                leftPadding: 16
                                                rightPadding: 16
                                                implicitHeight: 38
                                            }
                                            Button {
                                                activeFocusOnTab: true
                                                focusPolicy: Qt.StrongFocus
                                                visible: settingsManager.customLibraryBackground.length > 0
                                                text: "Clear"
                                                onClicked: settingsManager.setCustomLibraryBackground("")
                                                background: Rectangle {
                                                    radius: 9
                                                    color: parent.activeFocus ? theme.elevatedSurface
                                                        : (parent.highlighted ? theme.accent : theme.surface)
                                                    border.color: theme.accent
                                                    border.width: parent.activeFocus ? 2 : 0
                                                }
                                                contentItem: Label {
                                                    text: parent.text
                                                    color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                    opacity: parent.enabled ? 1 : 0.5
                                                    font.pixelSize: 13 * theme.fontScale
                                                    font.weight: Font.DemiBold
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                leftPadding: 16
                                                rightPadding: 16
                                                implicitHeight: 38
                                            }
                                        }
                                    }
                                    SettingRow {
                                        visible: settingsManager.libraryBackgroundType === "custom"
                                        dimmed: settingsManager.customLibraryBackground.length === 0
                                        title: "Fit"
                                        description: "Fill covers the screen and crops the edges. Fit shows the whole image over a blurred copy of itself."
                                        ValueCycler {
                                            options: [
                                                { label: "Fill", value: "fill" },
                                                { label: "Fit", value: "fit" }
                                            ]
                                            value: libraryVisualSettings.wallpaperFit
                                            onPicked: (fit) => libraryVisualSettings.wallpaperFit = fit
                                        }
                                    }
                                    SettingRow {
                                        visible: settingsManager.libraryBackgroundType === "custom"
                                        dimmed: settingsManager.customLibraryBackground.length === 0
                                        title: "Dim"
                                        description: "Darkens the wallpaper so covers and text stay readable on bright images."
                                        ValueCycler {
                                            options: [0, 15, 30, 45, 60].map(function(percent) {
                                                return { label: percent === 0 ? "Off" : percent + "%", value: percent }
                                            })
                                            value: libraryVisualSettings.wallpaperDim
                                            onPicked: (percent) => libraryVisualSettings.wallpaperDim = percent
                                        }
                                    }
                                    Item { Layout.preferredHeight: 16 }
                                }
                            }

                            ScrollView {
                                id: libraryContent
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Library"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: libraryContent.availableWidth
                                    spacing: 8

                                    Label {
                                        text: "Library"
                                        color: theme.textPrimary
                                        font.pixelSize: 30 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: 8
                                        text: "Where your games live and how RecompStation finds them."
                                        color: theme.textSecondary
                                        font.pixelSize: 14 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    SettingsGroupLabel { text: "LOCATION" }
                                    SettingRow {
                                        title: "Library folder"
                                        description: platformManager.libraryConfigured
                                            ? platformManager.libraryDirectory
                                            : "No library folder selected yet."
                                        Button {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            text: "Change folder"
                                            enabled: platformManager.libraryConfigured
                                            onClicked: libraryFolderDialog.open()
                                        background: Rectangle {
                                            radius: 9
                                            color: parent.activeFocus ? theme.elevatedSurface
                                                : (parent.highlighted ? theme.accent : theme.surface)
                                            border.color: theme.accent
                                            border.width: parent.activeFocus ? 2 : 0
                                        }
                                        contentItem: Label {
                                            text: parent.text
                                            color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                            opacity: parent.enabled ? 1 : 0.5
                                            font.pixelSize: 13 * theme.fontScale
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        leftPadding: 16
                                        rightPadding: 16
                                        implicitHeight: 38
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 4
                                        text: "Changing the folder doesn't move or delete game files. RecompStation creates a subfolder for each platform and scans it."
                                        color: theme.textMuted
                                        font.pixelSize: 12 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 4
                                        spacing: 8
                                        Repeater {
                                            model: platformManager.platforms()
                                            delegate: Rectangle {
                                                required property string modelData
                                                radius: 13
                                                color: theme.surface
                                                width: platformChip.implicitWidth + 24
                                                height: 28
                                                Label {
                                                    id: platformChip
                                                    anchors.centerIn: parent
                                                    text: window.platformDisplayName(modelData)
                                                    color: theme.textSecondary
                                                    font.pixelSize: 11.5 * theme.fontScale
                                                }
                                            }
                                        }
                                    }
                                    SettingsGroupLabel { text: "SCANNING" }
                                    SettingRow {
                                        title: "Scan library on startup"
                                        description: "Check the platform folders for new or removed games when RecompStation starts. Folders are also watched while it runs."
                                        ToggleSwitch {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            checked: settingsManager.scanOnStartup
                                            onToggled: settingsManager.setScanOnStartup(checked)
                                            Accessible.name: "Scan library on startup"
                                        }
                                    }
                                    SettingRow {
                                        title: "Scan now"
                                        description: window.scanMessage.length > 0 ? window.scanMessage
                                            : "Place game files in their platform folders, then scan."
                                        Button {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            text: libraryScanner.scanning ? "Scanning…" : "Scan library"
                                            highlighted: true
                                            enabled: platformManager.libraryConfigured && !libraryScanner.scanning
                                            onClicked: libraryScanner.scanAllAsync()
                                        background: Rectangle {
                                            radius: 9
                                            color: parent.activeFocus ? theme.elevatedSurface
                                                : (parent.highlighted ? theme.accent : theme.surface)
                                            border.color: theme.accent
                                            border.width: parent.activeFocus ? 2 : 0
                                        }
                                        contentItem: Label {
                                            text: parent.text
                                            color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                            opacity: parent.enabled ? 1 : 0.5
                                            font.pixelSize: 13 * theme.fontScale
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        leftPadding: 16
                                        rightPadding: 16
                                        implicitHeight: 38
                                        }
                                    }
                                    Item { Layout.preferredHeight: 16 }
                                }
                            }

                            ScrollView {
                                id: applicationContent
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Application"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: applicationContent.availableWidth
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
                                            ToggleSwitch {
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
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Metadata"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: metadataContent.availableWidth
                                    spacing: 8

                                    Label {
                                        text: "Metadata"
                                        color: theme.textPrimary
                                        font.pixelSize: 30 * theme.fontScale
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: 8
                                        text: "Where game names, descriptions, covers and screenshots come from."
                                        color: theme.textSecondary
                                        font.pixelSize: 14 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    SettingsGroupLabel { text: "PROVIDERS" }

                                    // TheGamesDB: online, needs a key.
                                    Rectangle {
                                        id: theGamesDbCard
                                        Layout.fillWidth: true
                                        implicitHeight: theGamesDbColumn.implicitHeight + 28
                                        radius: 12
                                        color: theme.sidebarBackground
                                        border.width: window.isDescendantOf(window.activeFocusItem, theGamesDbCard) ? 2 : 0
                                        border.color: theme.accent
                                        property bool editingKey: false
                                        readonly property string keySource: settingsManager.theGamesDbKeySource
                                        readonly property string connection: metadataManager.connectionState

                                        ColumnLayout {
                                            id: theGamesDbColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            anchors.leftMargin: 18
                                            spacing: 10
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 12
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 3
                                                    Label {
                                                        text: "TheGamesDB  <font color=\"" + theme.textMuted + "\">· online</font>"
                                                        textFormat: Text.StyledText
                                                        color: theme.textPrimary
                                                        font.pixelSize: 14.5 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                    }
                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: "Descriptions, ratings, covers and gameplay screenshots. Needs a free API key."
                                                        color: theme.textMuted
                                                        font.pixelSize: 12 * theme.fontScale
                                                        wrapMode: Text.Wrap
                                                    }
                                                }
                                                StatusPill {
                                                    text: !settingsManager.theGamesDbEnabled ? "Off"
                                                        : theGamesDbCard.connection === "connected" ? "● Connected"
                                                        : theGamesDbCard.connection === "checking" ? "Checking…"
                                                        : theGamesDbCard.connection === "unconfigured" ? "No key"
                                                        : "● Can't connect"
                                                    tone: !settingsManager.theGamesDbEnabled ? "neutral"
                                                        : theGamesDbCard.connection === "connected" ? "good"
                                                        : theGamesDbCard.connection === "error" ? "bad" : "warning"
                                                }
                                                ToggleSwitch {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    checked: settingsManager.theGamesDbEnabled
                                                    onToggled: settingsManager.setTheGamesDbEnabled(checked)
                                                    Accessible.name: "Use TheGamesDB"
                                                }
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8
                                                visible: settingsManager.theGamesDbEnabled
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 38
                                                    radius: 9
                                                    color: theme.windowBackground
                                                    border.width: theGamesDbKeyField.activeFocus ? 2 : 1
                                                    border.color: theGamesDbKeyField.activeFocus ? theme.accent : theme.divider
                                                    Label {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 12
                                                        anchors.rightMargin: 12
                                                        visible: !theGamesDbCard.editingKey
                                                        verticalAlignment: Text.AlignVCenter
                                                        elide: Text.ElideRight
                                                        text: theGamesDbCard.keySource === "none" ? "No key yet"
                                                            : "••••••••••••••••••••" + settingsManager.theGamesDbKeyHint
                                                        color: theGamesDbCard.keySource === "none" ? theme.textMuted : theme.textSecondary
                                                        font.family: "monospace"
                                                        font.pixelSize: 13 * theme.fontScale
                                                    }
                                                    TextField {
                                                        id: theGamesDbKeyField
                                                        anchors.fill: parent
                                                        visible: theGamesDbCard.editingKey
                                                        activeFocusOnTab: theGamesDbCard.editingKey
                                                        placeholderText: "Paste your TheGamesDB API key"
                                                        echoMode: TextInput.Password
                                                        passwordMaskDelay: 600
                                                        font.family: "monospace"
                                                        font.pixelSize: 13 * theme.fontScale
                                                        color: theme.textPrimary
                                                        background: Item {}
                                                        leftPadding: 12
                                                        rightPadding: 12
                                                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                                                        onAccepted: theGamesDbSaveKeyButton.clicked()
                                                        Keys.onEscapePressed: (event) => {
                                                            theGamesDbCard.editingKey = false
                                                            theGamesDbKeyChangeButton.forceActiveFocus()
                                                            event.accepted = true
                                                        }
                                                    }
                                                }
                                                Button {
                                                    id: theGamesDbKeyChangeButton
                                                    visible: !theGamesDbCard.editingKey && theGamesDbCard.keySource !== "environment"
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: theGamesDbCard.keySource === "none" ? "Add key" : "Change key"
                                                    highlighted: theGamesDbCard.keySource === "none"
                                                    onClicked: {
                                                        theGamesDbKeyField.text = ""
                                                        theGamesDbCard.editingKey = true
                                                        window.theGamesDbKeyMessage = ""
                                                        Qt.callLater(function() { theGamesDbKeyField.forceActiveFocus() })
                                                    }
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                                Button {
                                                    id: theGamesDbSaveKeyButton
                                                    visible: theGamesDbCard.editingKey
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "Save"
                                                    highlighted: true
                                                    onClicked: {
                                                        if (settingsManager.saveTheGamesDbApiKey(theGamesDbKeyField.text)) {
                                                            window.theGamesDbKeyMessage = "Key saved. Checking it with TheGamesDB…"
                                                            theGamesDbCard.editingKey = false
                                                            theGamesDbKeyField.text = ""
                                                            Qt.callLater(function() { theGamesDbKeyChangeButton.forceActiveFocus() })
                                                        } else {
                                                            window.theGamesDbKeyMessage = "That doesn't look like a TheGamesDB key: it should be 16 to 128 letters and digits."
                                                        }
                                                    }
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                                Button {
                                                    visible: theGamesDbCard.editingKey
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "Cancel"
                                                    onClicked: {
                                                        theGamesDbCard.editingKey = false
                                                        theGamesDbKeyField.text = ""
                                                        window.theGamesDbKeyMessage = ""
                                                        theGamesDbKeyChangeButton.forceActiveFocus()
                                                    }
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                                Button {
                                                    visible: !theGamesDbCard.editingKey && theGamesDbCard.keySource !== "none"
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: metadataManager.connectionState === "checking" ? "Testing…" : "Test"
                                                    enabled: metadataManager.connectionState !== "checking"
                                                    onClicked: {
                                                        window.theGamesDbKeyMessage = ""
                                                        metadataManager.refreshProvider()
                                                    }
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                                Button {
                                                    visible: !theGamesDbCard.editingKey && theGamesDbCard.keySource === "saved"
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    text: "Remove"
                                                    onClicked: {
                                                        settingsManager.clearTheGamesDbApiKey()
                                                        window.theGamesDbKeyMessage = "Saved key removed."
                                                    }
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                visible: settingsManager.theGamesDbEnabled && text.length > 0
                                                text: {
                                                    if (window.theGamesDbKeyMessage.length > 0 && theGamesDbCard.connection !== "connected"
                                                            && theGamesDbCard.connection !== "error")
                                                        return window.theGamesDbKeyMessage
                                                    const parts = []
                                                    if (theGamesDbCard.keySource === "environment")
                                                        parts.push("Using the key from RECOMPSTATION_TGDB_API_KEY")
                                                    else if (theGamesDbCard.keySource === "saved")
                                                        parts.push("Key saved for your user only")
                                                    if (theGamesDbCard.connection === "connected" && metadataManager.remainingAllowance >= 0)
                                                        parts.push(metadataManager.remainingAllowance.toLocaleString(Qt.locale(), "f", 0)
                                                            + " requests left this month")
                                                    if (theGamesDbCard.connection === "error")
                                                        parts.push(metadataManager.connectionError)
                                                    return parts.join(" · ")
                                                }
                                                color: theGamesDbCard.connection === "error" ? theme.error : theme.textMuted
                                                font.pixelSize: 11.5 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    // LaunchBox: offline catalog, no key.
                                    Rectangle {
                                        id: launchBoxCard
                                        Layout.fillWidth: true
                                        implicitHeight: launchBoxColumn.implicitHeight + 28
                                        radius: 12
                                        color: theme.sidebarBackground
                                        border.width: window.isDescendantOf(window.activeFocusItem, launchBoxCard) ? 2 : 0
                                        border.color: theme.accent

                                        ColumnLayout {
                                            id: launchBoxColumn
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 14
                                            anchors.leftMargin: 18
                                            spacing: 10
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 12
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 3
                                                    Label {
                                                        text: "LaunchBox Games Database  <font color=\"" + theme.textMuted + "\">· offline catalog</font>"
                                                        textFormat: Text.StyledText
                                                        color: theme.textPrimary
                                                        font.pixelSize: 14.5 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                    }
                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: "Matches by ROM file name and title, with covers and screenshots. Works without a key or internet once downloaded (about 106 MB)."
                                                        color: theme.textMuted
                                                        font.pixelSize: 12 * theme.fontScale
                                                        wrapMode: Text.Wrap
                                                    }
                                                }
                                                StatusPill {
                                                    text: !settingsManager.launchBoxEnabled ? "Off"
                                                        : launchBoxCatalogManager.busy ? "Updating " + window.launchBoxProgressPercent + "%"
                                                        : launchBoxCatalogManager.catalogAvailable
                                                            ? "● Installed · " + window.formatBytes(launchBoxCatalogManager.catalogSizeBytes)
                                                            : "Not downloaded"
                                                    tone: !settingsManager.launchBoxEnabled ? "neutral"
                                                        : launchBoxCatalogManager.catalogAvailable ? "good" : "warning"
                                                }
                                                ToggleSwitch {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    checked: settingsManager.launchBoxEnabled
                                                    onToggled: settingsManager.setLaunchBoxEnabled(checked)
                                                    Accessible.name: "Use LaunchBox"
                                                }
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8
                                                visible: settingsManager.launchBoxEnabled
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: launchBoxCatalogManager.busy || window.launchBoxStatusMessage.length > 0
                                                            && !launchBoxCatalogManager.catalogAvailable
                                                        ? window.launchBoxStatusMessage
                                                        : settingsManager.launchBoxLastUpdated.length > 0
                                                            ? "Updated " + window.formatShortDate(settingsManager.launchBoxLastUpdated)
                                                                + " · indexes " + platformManager.platforms().map(window.platformDisplayName).join(", ")
                                                            : "Download the catalog to start matching with LaunchBox."
                                                    color: theme.textMuted
                                                    font.pixelSize: 11.5 * theme.fontScale
                                                    wrapMode: Text.Wrap
                                                }
                                                Button {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    visible: !launchBoxCatalogManager.busy
                                                    text: launchBoxCatalogManager.catalogAvailable ? "Update catalog" : "Download catalog"
                                                    highlighted: !launchBoxCatalogManager.catalogAvailable
                                                    onClicked: launchBoxCatalogManager.updateCatalog()
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                                Button {
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    visible: launchBoxCatalogManager.busy
                                                    text: "Cancel"
                                                    onClicked: launchBoxCatalogManager.cancelUpdate()
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                visible: settingsManager.launchBoxEnabled && launchBoxArtworkManager.status.length > 0
                                                text: launchBoxArtworkManager.status
                                                color: theme.textMuted
                                                font.pixelSize: 11.5 * theme.fontScale
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    SettingRow {
                                        title: "Ask first"
                                        description: settingsManager.metadataProviderOrder === "launchbox"
                                            ? "LaunchBox is checked first; TheGamesDB fills in games it doesn't know."
                                            : "TheGamesDB is checked first; LaunchBox fills in games it can't match or when it's unreachable."
                                        ValueCycler {
                                            options: [
                                                { label: "LaunchBox", value: "launchbox" },
                                                { label: "TheGamesDB", value: "thegamesdb" }
                                            ]
                                            value: settingsManager.metadataProviderOrder
                                            onPicked: (order) => settingsManager.setMetadataProviderOrder(order)
                                        }
                                    }

                                    SettingsGroupLabel { text: "AUTOMATION" }
                                    SettingRow {
                                        title: "Match new games automatically"
                                        description: "After every scan, including when files are added while RecompStation is open. Games skipped because a provider was unreachable are retried an hour later."
                                        ToggleSwitch {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            checked: settingsManager.automaticMetadataMatchingEnabled
                                            onToggled: settingsManager.setAutomaticMetadataMatchingEnabled(checked)
                                            Accessible.name: "Match new games automatically"
                                        }
                                    }
                                    SettingRow {
                                        title: "Download covers and screenshots"
                                        description: "Right after a game is matched."
                                        ToggleSwitch {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            checked: settingsManager.automaticArtworkDownloadsEnabled
                                            onToggled: settingsManager.setAutomaticArtworkDownloadsEnabled(checked)
                                            Accessible.name: "Download covers and screenshots automatically"
                                        }
                                    }

                                    SettingsGroupLabel { text: "WHOLE LIBRARY" }
                                    SettingRow {
                                        title: "Refresh all metadata"
                                        description: metadataMatcher.busy && metadataMatcher.totalCount > 0
                                            ? "Matching " + metadataMatcher.completedCount + " of " + metadataMatcher.totalCount + "…"
                                            : "Retries every game without a match, and fetches missing covers and screenshots. Existing matches are kept."
                                        Button {
                                            activeFocusOnTab: true
                                            focusPolicy: Qt.StrongFocus
                                            text: window.universalRefreshPending || metadataMatcher.busy ? "Refreshing…" : "Refresh"
                                            highlighted: true
                                            enabled: (metadataManager.configured
                                                || (settingsManager.launchBoxEnabled && launchBoxCatalogManager.catalogAvailable))
                                                && !window.universalRefreshPending
                                                && !metadataMatcher.busy
                                                && !screenshotManager.busy
                                                && !launchBoxCatalogManager.busy
                                            onClicked: {
                                                window.universalRefreshPending = true
                                                window.scanMessage = "Refreshing metadata and available artwork for your library…"
                                                metadataMatcher.scrapeLibrary(true, true)
                                            }
                                                    background: Rectangle {
                                                        radius: 9
                                                        color: parent.activeFocus ? theme.elevatedSurface
                                                            : (parent.highlighted ? theme.accent : theme.surface)
                                                        border.color: theme.accent
                                                        border.width: parent.activeFocus ? 2 : 0
                                                    }
                                                    contentItem: Label {
                                                        text: parent.text
                                                        color: parent.highlighted && !parent.activeFocus ? theme.accentText : theme.textPrimary
                                                        opacity: parent.enabled ? 1 : 0.5
                                                        font.pixelSize: 13 * theme.fontScale
                                                        font.weight: Font.DemiBold
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    leftPadding: 16
                                                    rightPadding: 16
                                                    implicitHeight: 38
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 4
                                        visible: !settingsPreviewPane.visible && metadataMatcher.status.length > 0
                                        text: metadataMatcher.status
                                        color: theme.textMuted
                                        font.pixelSize: 12 * theme.fontScale
                                        wrapMode: Text.Wrap
                                    }
                                    Item { Layout.preferredHeight: 16 }
                                }
                            }

                            ScrollView {
                                id: controllerContent
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 6
                                anchors.bottomMargin: 64
                                anchors.leftMargin: 8
                                width: Math.min(parent.width - 16, window.settingsContentMaxWidth)
                                clip: true
                                visible: settingsPage.section === "Controller"
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: controllerContent.availableWidth
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
                                                ToggleSwitch {
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
                                                        controllerManager.resetConnectedControllerProfile()
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
                                            ? "Press a controller button, trigger, or click either analog stick. Release the input used to start capture first; capture cancels after 15 seconds."
                                            : "Choose Add binding, then press a button, trigger, D-pad direction, move a stick, or press a stick to click it. Existing bindings are kept."
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
                                            { key: "nextSection", label: "Next section" },
                                            { key: "details", label: "Game details" }
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
                                                                        text: modelData === "lefttrigger" ? "Left trigger"
                                                                            : modelData === "righttrigger" ? "Right trigger"
                                                                            : modelData === "leftstick-click" ? "L3 · Left stick click"
                                                                            : modelData === "rightstick-click" ? "R3 · Right stick click"
                                                                            : modelData
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
                                                        ? "Listening…"
                                                        : controllerManager.mappingCaptureReplaceExisting
                                                            && controllerManager.mappingCaptureAction === modelData.key
                                                            ? "Replacing…"
                                                            : "Add binding"
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
                                                    onClicked: controllerManager.startMappingCapture(modelData.key, false)
                                                }
                                                Button {
                                                    text: "Replace"
                                                    enabled: controllerManager.connected
                                                        && (!controllerManager.mappingCaptureActive
                                                            || controllerManager.mappingCaptureAction === modelData.key)
                                                    activeFocusOnTab: true
                                                    focusPolicy: Qt.StrongFocus
                                                    background: Rectangle {
                                                        radius: theme.radiusSmall
                                                        color: activeFocus ? theme.elevatedSurface : theme.surface
                                                        border.color: activeFocus ? theme.accent : theme.divider
                                                        border.width: activeFocus ? 2 : 1
                                                    }
                                                    onClicked: controllerManager.startMappingCapture(modelData.key, true)
                                                }
                                                Button {
                                                    id: cancelMappingCaptureButton
                                                    text: "Cancel"
                                                    visible: controllerManager.mappingCaptureActive
                                                        && controllerManager.mappingCaptureAction === modelData.key
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

                        // Right-hand pane: a live preview or reference for the current section.
                        // Hidden when the window is too narrow to give the rows room.
                        Rectangle {
                            id: settingsPreviewPane
                            readonly property bool hasContent: settingsPage.section === "Appearance"
                                || settingsPage.section === "Display"
                                || settingsPage.section === "Background"
                                || settingsPage.section === "Metadata"
                            visible: hasContent && settingsLayoutRow.width >= 1180
                            Layout.preferredWidth: Math.max(340, Math.min(460, settingsLayoutRow.width * 0.28))
                            Layout.fillHeight: true
                            radius: 16
                            color: theme.sidebarBackground
                            border.color: theme.divider
                            clip: true

                            readonly property var previewGame: {
                                const games = gameGrid.model || []
                                return games.length > 0 ? games[Math.max(0, Math.min(games.length - 1, gameGrid.currentIndex))] : null
                            }
                            readonly property var previewColors: theme.paletteFor(window.previewThemeId)

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 10
                                visible: settingsPage.section === "Appearance"

                                SettingsGroupLabel {
                                    Layout.topMargin: 0
                                    text: "LIVE PREVIEW · " + window.themeTitle(window.previewThemeId).toUpperCase()
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: width * 0.62
                                    radius: 12
                                    color: settingsPreviewPane.previewColors.windowBackground
                                    border.color: settingsPreviewPane.previewColors.divider
                                    clip: true
                                    Behavior on color { enabled: !settingsManager.reducedMotion; ColorAnimation { duration: 160 } }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 34
                                        color: settingsPreviewPane.previewColors.sidebarBackground
                                        Row {
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: 12
                                            spacing: 6
                                            Rectangle { width: 48; height: 12; radius: 6; color: settingsPreviewPane.previewColors.accent }
                                            Rectangle { width: 34; height: 12; radius: 6; color: settingsPreviewPane.previewColors.surface }
                                            Rectangle { width: 34; height: 12; radius: 6; color: settingsPreviewPane.previewColors.surface }
                                        }
                                    }
                                    GameArtwork {
                                        id: themePreviewCover
                                        x: 14
                                        y: 48
                                        width: parent.width * 0.32
                                        height: parent.height - 62
                                        source: settingsPreviewPane.previewGame ? settingsPreviewPane.previewGame.artworkPath : ""
                                        platform: settingsPreviewPane.previewGame ? settingsPreviewPane.previewGame.platform : ""
                                        reducedMotion: settingsManager.reducedMotion
                                        themeTokens: theme
                                        frameAlignment: "center"
                                    }
                                    Column {
                                        anchors.left: themePreviewCover.right
                                        anchors.leftMargin: 14
                                        anchors.right: parent.right
                                        anchors.rightMargin: 14
                                        y: 56
                                        spacing: 6
                                        Label {
                                            width: parent.width
                                            text: settingsPreviewPane.previewGame
                                                ? (settingsPreviewPane.previewGame.metadataTitle || settingsPreviewPane.previewGame.title) : "Your game"
                                            color: "#f4f5fb"
                                            font.pixelSize: 15 * theme.fontScale
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            width: parent.width
                                            text: settingsPreviewPane.previewGame
                                                ? window.platformDisplayName(settingsPreviewPane.previewGame.platform) : "Platform"
                                            color: settingsPreviewPane.previewColors.textSecondary
                                            font.pixelSize: 11 * theme.fontScale
                                            elide: Text.ElideRight
                                        }
                                        Row {
                                            spacing: 8
                                            topPadding: 8
                                            Rectangle {
                                                width: 84; height: 28; radius: 7
                                                color: settingsPreviewPane.previewColors.accent
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "▶  Play"
                                                    color: settingsPreviewPane.previewColors.accentText
                                                    font.pixelSize: 11.5 * theme.fontScale
                                                    font.bold: true
                                                }
                                            }
                                            Rectangle {
                                                width: 70; height: 28; radius: 7
                                                color: settingsPreviewPane.previewColors.elevatedSurface
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "Details"
                                                    color: "#f4f5fb"
                                                    font.pixelSize: 11.5 * theme.fontScale
                                                }
                                            }
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "The preview follows the focused theme. Select it to apply it everywhere."
                                    color: theme.textSecondary
                                    font.pixelSize: 12.5 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Item { Layout.fillHeight: true }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 10
                                visible: settingsPage.section === "Metadata"

                                SettingsGroupLabel { Layout.topMargin: 0; text: "HOW MATCHING WORKS" }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Each new game is looked up by its ROM file name and title, in the order you chose. A game is only matched when one result clearly fits; otherwise it stays unmatched rather than getting the wrong cover."
                                    color: theme.textSecondary
                                    font.pixelSize: 12.5 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                SettingsGroupLabel { text: "LATEST RUN" }
                                Label {
                                    Layout.fillWidth: true
                                    text: metadataMatcher.status.length > 0 ? metadataMatcher.status : "Nothing has run yet in this session."
                                    color: theme.textPrimary
                                    font.pixelSize: 12.5 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: screenshotManager.status.length > 0
                                    text: screenshotManager.status
                                    color: theme.textMuted
                                    font.pixelSize: 12 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                SettingsGroupLabel { text: "GETTING A THEGAMESDB KEY" }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Request a free key on the TheGamesDB forums, then choose Add key and paste it. It is stored in its own file in RecompStation's config folder, readable only by your user. If RECOMPSTATION_TGDB_API_KEY is set in your environment, that key is used instead."
                                    color: theme.textSecondary
                                    font.pixelSize: 12.5 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Item { Layout.fillHeight: true }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 10
                                visible: settingsPage.section === "Background"

                                SettingsGroupLabel { Layout.topMargin: 0; text: "LIVE PREVIEW" }
                                Rectangle {
                                    id: backgroundPreviewFrame
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: width * 9 / 16
                                    radius: 12
                                    clip: true
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: theme.backgroundTop }
                                        GradientStop { position: 1.0; color: theme.backgroundBottom }
                                    }
                                        WaveBackground {
                                            anchors.fill: parent
                                            visible: settingsManager.libraryBackgroundType === "animated"
                                            waveColor: window.waveColorValue
                                            topColor: theme.backgroundTop
                                            bottomColor: theme.backgroundBottom
                                            speed: window.waveSpeed
                                            quality: window.waveRenderScale
                                            fps: window.waveFps
                                            running: settingsPreviewPane.visible && settingsPage.section === "Background"
                                                && !settingsManager.reducedMotion && window.active
                                        }
                                    WallpaperBackground {
                                        anchors.fill: parent
                                        visible: settingsManager.libraryBackgroundType === "custom"
                                            && settingsManager.customLibraryBackground.length > 0
                                        source: visible ? settingsManager.customLibraryBackground : ""
                                        fitMode: libraryVisualSettings.wallpaperFit
                                        dimPercent: libraryVisualSettings.wallpaperDim
                                        dimColor: theme.windowBackground
                                    }
                                    // Stand-in covers, so the dim level can be judged against cards.
                                    Row {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 12
                                        spacing: 7
                                        Repeater {
                                            model: 6
                                            Rectangle {
                                                required property int index
                                                anchors.bottom: parent ? parent.bottom : undefined
                                                width: backgroundPreviewFrame.width * 0.11
                                                height: width * (index % 2 === 0 ? 1.4 : 1.15)
                                                radius: 4
                                                color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.92)
                                                border.width: index === 1 ? 2 : 0
                                                border.color: theme.accent
                                            }
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: settingsManager.libraryBackgroundType === "custom"
                                        ? "Your image is loaded at your screen's size. Fill crops the edges; Fit keeps the whole image."
                                        : settingsManager.libraryBackgroundType === "animated"
                                            ? "Drawn live by your graphics card. It pauses while a game is running and under Reduced motion."
                                            : "A quiet gradient in your theme's colors."
                                    color: theme.textSecondary
                                    font.pixelSize: 12.5 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Item { Layout.fillHeight: true }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 10
                                visible: settingsPage.section === "Display"

                                SettingsGroupLabel { Layout.topMargin: 0; text: "AUTO SCALE BY SCREEN HEIGHT" }
                                Repeater {
                                    model: [
                                        { label: "Below 900p (720p, Steam Deck)", percent: 85 },
                                        { label: "1080p · 1200p", percent: 100 },
                                        { label: "1440p · 1600p", percent: 125 },
                                        { label: "1800p", percent: 150 },
                                        { label: "2160p (4K)", percent: 175 },
                                        { label: "2880p and above", percent: 200 }
                                    ]
                                    delegate: RowLayout {
                                        required property var modelData
                                        readonly property bool matches: modelData.percent === interfaceScaleInfo.auto
                                        Layout.fillWidth: true
                                        Label {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: parent.matches ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 13 * theme.fontScale
                                        }
                                        Label {
                                            text: modelData.percent + "%"
                                            color: parent.matches ? theme.accent : theme.textSecondary
                                            font.pixelSize: 13 * theme.fontScale
                                            font.bold: true
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 6
                                    text: "Heights are measured after your desktop's own scaling, so a 4K screen already set to 200% in your desktop counts as 1080p and is not scaled twice."
                                    color: theme.textSecondary
                                    font.pixelSize: 12.5 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                SettingsGroupLabel { text: "THIS SCREEN" }
                                Label {
                                    Layout.fillWidth: true
                                    text: (interfaceScaleInfo.screenName || "Current display") + " · "
                                        + interfaceScaleInfo.logicalWidth + " × " + interfaceScaleInfo.logicalHeight
                                    color: theme.textPrimary
                                    font.pixelSize: 13.5 * theme.fontScale
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Auto would use " + interfaceScaleInfo.auto + "% · now using " + interfaceScaleInfo.applied + "%"
                                        + (interfaceScaleInfo.external ? " (from QT_SCALE_FACTOR)" : "")
                                    color: theme.textMuted
                                    font.pixelSize: 12 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                                Item { Layout.fillHeight: true }
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
            height: 88
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

                // Compact Library toolbar: platform pills (LB/RB cycle them on
                // a controller), an expanding search control, a segmented view
                // switcher, and a card-size button. Keeps the libraryFilterPanel
                // id because directional focus and Select route through it.
                Item {
                    id: libraryFilterPanel
                    visible: currentPage === "Library"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 430
                    Layout.preferredHeight: 44

                    RowLayout {
                        anchors.fill: parent
                        spacing: 12

                        // LB / RB hints flank the pills while a controller is active.
                        Label {
                            visible: window.activeInputDevice === "controller"
                                && controllerManager.connected
                            text: window.platformCycleButtonLabels()[0]
                            color: theme.textMuted
                            font.pixelSize: 10 * theme.fontScale
                            font.bold: true
                            leftPadding: 6
                            rightPadding: 6
                            topPadding: 2
                            bottomPadding: 2
                            background: Rectangle {
                                radius: 4
                                color: "transparent"
                                border.color: theme.divider
                            }
                        }

                        // Grows to fit the pills and clips them only when the
                        // window is too narrow, so RB stays right after them.
                        Item {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 120
                            Layout.maximumWidth: platformPillRow.implicitWidth
                            Layout.preferredHeight: 36
                            clip: true

                            Row {
                                id: platformPillRow
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Repeater {
                                    model: ["All Platforms"].concat(platformManager.platforms())
                                    delegate: Button {
                                        id: platformPill
                                        required property string modelData
                                        readonly property bool selected:
                                            window.selectedPlatform === modelData
                                        readonly property int gameCount:
                                            window.libraryPlatformCounts[modelData] || 0
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        height: 34
                                        leftPadding: 14
                                        rightPadding: 14
                                        topPadding: 0
                                        bottomPadding: 0
                                        Accessible.name: (modelData === "All Platforms" ? "All platforms" : modelData)
                                            + ", " + gameCount + (gameCount === 1 ? " game" : " games")
                                        contentItem: Row {
                                            spacing: 6
                                            Label {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: platformPill.modelData === "All Platforms"
                                                    ? "All" : platformPill.modelData
                                                color: platformPill.selected ? theme.windowBackground
                                                    : platformPill.gameCount === 0 ? theme.textSubtle
                                                    : theme.textPrimary
                                                font.pixelSize: 13 * theme.fontScale
                                                font.bold: platformPill.selected
                                            }
                                            Label {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: platformPill.gameCount
                                                color: platformPill.selected
                                                    ? Qt.rgba(theme.windowBackground.r, theme.windowBackground.g,
                                                              theme.windowBackground.b, 0.7)
                                                    : theme.textMuted
                                                font.pixelSize: 12 * theme.fontScale
                                            }
                                        }
                                        background: Rectangle {
                                            radius: height / 2
                                            color: platformPill.selected ? theme.textPrimary
                                                : platformPill.hovered ? theme.controlHover
                                                : Qt.rgba(theme.surface.r, theme.surface.g,
                                                          theme.surface.b, 0.8)
                                            border.width: platformPill.activeFocus ? 2 : 0
                                            border.color: theme.accent
                                        }
                                        onClicked: window.selectLibraryPlatform(modelData)
                                    }
                                }
                            }
                        }

                        Label {
                            visible: window.activeInputDevice === "controller"
                                && controllerManager.connected
                            text: window.platformCycleButtonLabels()[1]
                            color: theme.textMuted
                            font.pixelSize: 10 * theme.fontScale
                            font.bold: true
                            leftPadding: 6
                            rightPadding: 6
                            topPadding: 2
                            bottomPadding: 2
                            background: Rectangle {
                                radius: 4
                                color: "transparent"
                                border.color: theme.divider
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Search: a round button that expands into a field. It stays
                        // open while it has focus or holds text.
                        Item {
                            id: librarySearchControl
                            // Set by the button so the field is visible before it takes focus.
                            property bool openRequested: false
                            readonly property bool open: openRequested
                                || librarySearchField.activeFocus || librarySearchField.text.length > 0
                            Layout.preferredWidth: open ? 240 : 36
                            Layout.preferredHeight: 36
                            Behavior on Layout.preferredWidth {
                                enabled: !settingsManager.reducedMotion
                                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                            }

                            TextField {
                                id: librarySearchField
                                anchors.fill: parent
                                visible: librarySearchControl.open
                                activeFocusOnTab: librarySearchControl.open
                                focusPolicy: Qt.StrongFocus
                                placeholderText: "Search games"
                                selectByMouse: true
                                color: theme.textPrimary
                                font.pixelSize: 12 * theme.fontScale
                                leftPadding: 34
                                rightPadding: 10
                                Accessible.name: "Search games"
                                onActiveFocusChanged: {
                                    if (!activeFocus)
                                        librarySearchControl.openRequested = false
                                }
                                // Keyboard: Enter or Down moves to the results; Escape
                                // clears the search, then leaves the field.
                                onAccepted: window.focusLibraryContent()
                                Keys.onDownPressed: function(event) {
                                    window.activeInputDevice = "keyboard"
                                    window.focusLibraryContent()
                                    event.accepted = true
                                }
                                Keys.onEscapePressed: function(event) {
                                    window.activeInputDevice = "keyboard"
                                    if (text.length > 0)
                                        text = ""
                                    else
                                        window.focusLibraryContent()
                                    event.accepted = true
                                }
                                onTextChanged: {
                                    window.librarySearchText = text
                                    gameGrid.currentIndex = 0
                                }
                                background: Rectangle {
                                    radius: height / 2
                                    color: theme.surface
                                    border.color: librarySearchField.activeFocus ? theme.accent : theme.divider
                                    border.width: librarySearchField.activeFocus ? 2 : 1
                                }
                            }
                            Button {
                                id: librarySearchButton
                                anchors.fill: parent
                                visible: !librarySearchControl.open
                                activeFocusOnTab: !librarySearchControl.open
                                focusPolicy: Qt.StrongFocus
                                Accessible.name: "Search games"
                                padding: 0
                                contentItem: Item {}
                                background: Rectangle {
                                    radius: height / 2
                                    color: librarySearchButton.hovered ? theme.controlHover
                                        : Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.8)
                                    border.width: librarySearchButton.activeFocus ? 2 : 0
                                    border.color: theme.accent
                                }
                                onClicked: window.openLibrarySearch()
                            }
                            // Magnifier glyph shared by both states.
                            Item {
                                width: 16
                                height: 16
                                x: librarySearchControl.open ? 12 : (parent.width - width) / 2
                                anchors.verticalCenter: parent.verticalCenter
                                Rectangle {
                                    x: 0; y: 0
                                    width: 11; height: 11
                                    radius: 5.5
                                    color: "transparent"
                                    border.width: 2
                                    border.color: theme.textSecondary
                                }
                                Rectangle {
                                    x: 10; y: 9
                                    width: 2; height: 7
                                    radius: 1
                                    rotation: -45
                                    color: theme.textSecondary
                                }
                            }
                        }

                        // View switcher.
                        Rectangle {
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: viewSegmentRow.implicitWidth + 6
                            radius: height / 2
                            color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.8)

                            Row {
                                id: viewSegmentRow
                                anchors.centerIn: parent
                                spacing: 2
                                Repeater {
                                    model: [
                                        { label: "Shelves", value: "shelves" },
                                        { label: "Grid", value: "grid" },
                                        { label: "List", value: "list" },
                                        { label: "Carousel", value: "carousel" }
                                    ]
                                    delegate: Button {
                                        id: viewSegment
                                        required property var modelData
                                        readonly property bool selected:
                                            settingsManager.libraryView === modelData.value
                                        activeFocusOnTab: true
                                        focusPolicy: Qt.StrongFocus
                                        height: 30
                                        leftPadding: 14
                                        rightPadding: 14
                                        topPadding: 0
                                        bottomPadding: 0
                                        Accessible.name: modelData.label + " view"
                                        contentItem: Label {
                                            text: viewSegment.modelData.label
                                            color: viewSegment.selected ? theme.textPrimary : theme.textSecondary
                                            font.pixelSize: 12 * theme.fontScale
                                            font.bold: viewSegment.selected
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            radius: height / 2
                                            color: viewSegment.selected ? theme.controlBackground
                                                : viewSegment.hovered ? theme.controlHover : "transparent"
                                            border.width: viewSegment.activeFocus ? 2 : 0
                                            border.color: theme.accent
                                        }
                                        onClicked: settingsManager.libraryView = modelData.value
                                    }
                                }
                            }
                        }

                        // Card or row size for the current view. Each press steps to
                        // the next size; the label shows the current one.
                        Button {
                            id: librarySizeButton
                            readonly property var sizes: ["compact", "comfortable", "spacious"]
                            readonly property var sizeLabels: ({
                                compact: "Compact", comfortable: "Comfortable", spacious: "Spacious"
                            })
                            readonly property string currentSize:
                                settingsManager.libraryView === "list" ? settingsManager.libraryListDensity
                                : settingsManager.libraryView === "carousel" ? settingsManager.libraryConsoleDensity
                                : settingsManager.libraryDensity
                            activeFocusOnTab: true
                            focusPolicy: Qt.StrongFocus
                            Layout.preferredHeight: 36
                            leftPadding: 12
                            rightPadding: 14
                            topPadding: 0
                            bottomPadding: 0
                            Accessible.name: (settingsManager.libraryView === "list" ? "Row size: " : "Card size: ")
                                + (sizeLabels[currentSize] || currentSize)
                            contentItem: Row {
                                spacing: 8
                                // Size glyph: three squares growing left to right.
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Repeater {
                                        model: [6, 8, 10]
                                        delegate: Rectangle {
                                            required property int modelData
                                            required property int index
                                            anchors.bottom: parent ? parent.bottom : undefined
                                            width: modelData
                                            height: modelData
                                            radius: 2
                                            color: index <= librarySizeButton.sizes.indexOf(librarySizeButton.currentSize)
                                                ? theme.textPrimary : theme.textSubtle
                                        }
                                    }
                                }
                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: librarySizeButton.sizeLabels[librarySizeButton.currentSize]
                                        || librarySizeButton.currentSize
                                    color: theme.textPrimary
                                    font.pixelSize: 12 * theme.fontScale
                                }
                            }
                            background: Rectangle {
                                radius: height / 2
                                color: librarySizeButton.hovered ? theme.controlHover
                                    : Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.8)
                                border.width: librarySizeButton.activeFocus ? 2 : 0
                                border.color: theme.accent
                            }
                            onClicked: {
                                const index = sizes.indexOf(currentSize)
                                const next = sizes[(index + 1) % sizes.length]
                                if (settingsManager.libraryView === "list")
                                    settingsManager.libraryListDensity = next
                                else if (settingsManager.libraryView === "carousel")
                                    settingsManager.libraryConsoleDensity = next
                                else
                                    settingsManager.libraryDensity = next
                            }
                        }
                    }
                }

                // Keeps the page buttons right-aligned when the Library toolbar
                // is hidden (Settings).
                Item {
                    Layout.fillWidth: true
                    visible: !libraryFilterPanel.visible
                }

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
