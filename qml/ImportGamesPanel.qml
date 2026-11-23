import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Add games": folders to look in on the left, the games found on the right
// (system detected per file, unsure ones unticked with a system picker), copy
// or move, and progress while games are added. Used by first-time setup, by
// Settings > Library, and when files are dropped onto the window.
Item {
    id: panel

    property string secondaryText: "Close"
    signal secondaryRequested()
    signal importStarted()

    readonly property var candidates: gameImporter.candidates
    readonly property int selectedCount: { candidates; return gameImporter.selectedCount() }
    readonly property real selectedBytes: { candidates; return gameImporter.selectedBytes() }
    readonly property int unsureCount: {
        let count = 0
        for (const item of candidates)
            count += (!item.sure && !item.exists) ? 1 : 0
        return count
    }
    readonly property var platformCounts: {
        const counts = {}
        for (const item of candidates) {
            if (item.selected)
                counts[item.platform] = (counts[item.platform] || 0) + 1
        }
        return counts
    }
    readonly property string downloadsFolder: platformManager.homeDirectory() + "/Downloads"

    function focusFirst() {
        (gameImporter.sources.length === 0 ? lookInDownloadsButton : addFolderButton).forceActiveFocus()
    }

    component PanelButton: Button {
        id: panelButton
        activeFocusOnTab: true
        focusPolicy: Qt.StrongFocus
        leftPadding: 16
        rightPadding: 16
        implicitHeight: 40
        background: Rectangle {
            radius: 10
            color: panelButton.highlighted && !panelButton.activeFocus ? theme.accent
                : panelButton.activeFocus ? theme.elevatedSurface : theme.surface
            border.color: theme.accent
            border.width: panelButton.activeFocus ? 2 : 0
            opacity: panelButton.enabled ? 1 : 0.5
        }
        contentItem: Label {
            text: panelButton.text
            color: panelButton.highlighted && !panelButton.activeFocus ? theme.accentText : theme.textPrimary
            font.pixelSize: 13.5 * theme.fontScale
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.bottomMargin: footer.height + 16
        spacing: 18

        // ---- Where to look ----
        ColumnLayout {
            Layout.preferredWidth: 320
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            spacing: 8
            SettingsGroupLabel { Layout.topMargin: 0; text: "LOOK IN" }
            Repeater {
                model: gameImporter.sources
                delegate: Rectangle {
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: 52
                    radius: 10
                    color: theme.sidebarBackground
                    border.width: removeSourceButton.activeFocus ? 2 : 0
                    border.color: theme.accent
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 6
                        Label {
                            Layout.fillWidth: true
                            text: modelData.replace(platformManager.homeDirectory(), "~")
                            color: theme.textPrimary
                            font.pixelSize: 13 * theme.fontScale
                            font.weight: Font.DemiBold
                            elide: Text.ElideMiddle
                        }
                        Button {
                            id: removeSourceButton
                            activeFocusOnTab: true
                            focusPolicy: Qt.StrongFocus
                            enabled: !gameImporter.importing
                            text: "✕"
                            flat: true
                            Accessible.name: "Stop looking in " + modelData
                            onClicked: gameImporter.removeSource(modelData)
                        }
                    }
                }
            }
            PanelButton {
                id: lookInDownloadsButton
                Layout.fillWidth: true
                visible: gameImporter.sources.indexOf(panel.downloadsFolder) < 0
                enabled: !gameImporter.importing
                text: "Look in Downloads"
                onClicked: gameImporter.addSource(panel.downloadsFolder)
            }
            PanelButton {
                id: addFolderButton
                Layout.fillWidth: true
                enabled: !gameImporter.importing
                text: "+ Add a folder…"
                onClicked: window.openSourceFolderBrowser()
            }
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 4
                text: "Folders are searched with their subfolders. You can also drop files or folders onto RecompStation."
                color: theme.textMuted
                font.pixelSize: 11.5 * theme.fontScale
                wrapMode: Text.Wrap
            }

            SettingsGroupLabel { text: "THEN" }
            SettingRow {
                title: "Copy or move"
                description: window.importMoves ? "Originals are moved into your library." : "Your originals stay where they are."
                ValueCycler {
                    preferredWidth: 130
                    enabled: !gameImporter.importing
                    options: [{ label: "Copy", value: false }, { label: "Move", value: true }]
                    value: window.importMoves
                    onPicked: (move) => window.importMoves = move
                }
            }
            Item { Layout.fillHeight: true }
        }

        // ---- What was found ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: theme.sidebarBackground

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    spacing: 8
                    Label {
                        text: gameImporter.scanning ? "Looking for games…"
                            : panel.candidates.length === 1 ? "1 game" : panel.candidates.length + " games"
                        color: theme.textPrimary
                        font.pixelSize: 15 * theme.fontScale
                        font.bold: true
                    }
                    Label {
                        visible: panel.selectedCount > 0
                        text: "· " + panel.selectedCount + " selected · " + window.formatBytes(panel.selectedBytes)
                        color: theme.textMuted
                        font.pixelSize: 12.5 * theme.fontScale
                    }
                    Item { Layout.fillWidth: true }
                    Repeater {
                        model: Object.keys(panel.platformCounts)
                        delegate: StatusPill {
                            required property string modelData
                            text: window.platformDisplayName(modelData) + "  " + panel.platformCounts[modelData]
                            tone: "neutral"
                        }
                    }
                }

                ListView {
                    id: candidateList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    model: panel.candidates
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: candidateRow
                        required property var modelData
                        required property int index
                        readonly property bool focusWithin: selectBox.activeFocus || platformPicker.activeFocus
                        width: candidateList.width
                        height: 46
                        radius: 9
                        color: focusWithin ? theme.surface : "transparent"
                        border.width: focusWithin ? 2 : 0
                        border.color: theme.accent
                        onFocusWithinChanged: if (focusWithin) candidateList.positionViewAtIndex(index, ListView.Contain)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 12
                            spacing: 12
                            CheckBox {
                                id: selectBox
                                activeFocusOnTab: true
                                focusPolicy: Qt.StrongFocus
                                checked: candidateRow.modelData.selected
                                enabled: !gameImporter.importing && !candidateRow.modelData.exists
                                    && candidateRow.modelData.platform.length > 0
                                onClicked: gameImporter.setCandidateSelected(candidateRow.index, checked)
                                Accessible.name: candidateRow.modelData.name
                            }
                            Label {
                                Layout.fillWidth: true
                                text: candidateRow.modelData.name
                                color: candidateRow.modelData.exists ? theme.textMuted : theme.textPrimary
                                font.pixelSize: 13 * theme.fontScale
                                elide: Text.ElideMiddle
                            }
                            StatusPill {
                                visible: candidateRow.modelData.exists
                                text: "Already in library"
                                tone: "neutral"
                            }
                            StatusPill {
                                visible: candidateRow.modelData.sure && !candidateRow.modelData.exists
                                text: window.platformDisplayName(candidateRow.modelData.platform)
                                tone: "good"
                            }
                            ValueCycler {
                                id: platformPicker
                                visible: !candidateRow.modelData.sure && !candidateRow.modelData.exists
                                preferredWidth: 170
                                implicitHeight: 34
                                enabled: !gameImporter.importing
                                options: [{ label: "Pick system", value: "" }].concat(
                                    platformManager.platforms().map(function(platform) {
                                        return { label: window.platformDisplayName(platform), value: platform }
                                    }))
                                value: candidateRow.modelData.platform
                                onPicked: (platform) => gameImporter.setCandidatePlatform(candidateRow.index, platform)
                            }
                            Label {
                                Layout.preferredWidth: 72
                                horizontalAlignment: Text.AlignRight
                                text: window.formatBytes(candidateRow.modelData.size)
                                color: theme.textMuted
                                font.pixelSize: 12.5 * theme.fontScale
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width * 0.8
                        visible: panel.candidates.length === 0 && !gameImporter.scanning
                        horizontalAlignment: Text.AlignHCenter
                        text: gameImporter.sources.length === 0
                            ? "Add a folder that has games in it. N64 ROMs inside .zip or .7z archives are unpacked."
                            : "No games for N64, PS1, GameCube, Wii or Xbox were found there."
                        color: theme.textMuted
                        font.pixelSize: 13 * theme.fontScale
                        wrapMode: Text.Wrap
                    }
                    BusyIndicator {
                        anchors.centerIn: parent
                        running: gameImporter.scanning && panel.candidates.length === 0
                        visible: running
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    visible: panel.unsureCount > 0
                    text: panel.unsureCount === 1
                        ? "1 file could be more than one system. Pick its system to add it, or leave it unticked."
                        : panel.unsureCount + " files could be more than one system. Pick their system to add them, or leave them unticked."
                    color: theme.warning
                    font.pixelSize: 12 * theme.fontScale
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    // ---- Progress and actions ----
    ColumnLayout {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 8
            radius: 4
            visible: gameImporter.importing
            color: theme.surface
            Rectangle {
                width: parent.width * (gameImporter.bytesTotal > 0
                    ? Math.min(1, gameImporter.bytesDone / gameImporter.bytesTotal) : 0)
                height: parent.height
                radius: 4
                color: theme.accent
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: gameImporter.importing
                    ? (window.importMoves ? "Moving " : "Copying ") + (gameImporter.completedCount + 1)
                        + " of " + gameImporter.totalCount
                        + (gameImporter.currentName.length > 0 ? " · " + gameImporter.currentName : "")
                    : gameImporter.status
                color: theme.textSecondary
                font.pixelSize: 12.5 * theme.fontScale
                elide: Text.ElideRight
            }
            Label {
                visible: !gameImporter.importing && panel.selectedCount > 0
                    && gameImporter.libraryFreeBytes() >= 0
                    && gameImporter.libraryFreeBytes() < panel.selectedBytes && !window.importMoves
                text: "Not enough free space in your library folder."
                color: theme.error
                font.pixelSize: 12.5 * theme.fontScale
            }
            PanelButton {
                visible: gameImporter.importing
                text: "Stop"
                onClicked: gameImporter.cancel()
            }
            PanelButton {
                id: secondaryButton
                visible: !gameImporter.importing && panel.secondaryText.length > 0
                text: panel.secondaryText
                onClicked: panel.secondaryRequested()
            }
            PanelButton {
                id: importButton
                visible: !gameImporter.importing
                highlighted: true
                enabled: panel.selectedCount > 0 && !gameImporter.scanning
                text: panel.selectedCount === 1 ? "Add 1 game" : "Add " + panel.selectedCount + " games"
                onClicked: {
                    gameImporter.importSelected(window.importMoves)
                    panel.importStarted()
                }
            }
        }
    }
}
