import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "LibraryGridLayout.js" as LibraryGridLayout

// Shown for about a second when Play prepares a launcher: what RecompStation
// just did (checked the copy, gave it to the launcher, set fullscreen) while
// the game opens. Reads theme, window and projectCatalog from Main.qml.
Popup {
    id: overlay

    property string projectId: ""
    property var steps: []
    property string artworkPath: ""
    property string platform: ""
    property bool opening: true

    function show(id, preparedSteps) {
        projectId = id
        steps = preparedSteps
        opening = true
        closeTimer.stop()
        safetyTimer.restart()
        open()
    }
    function started(id) {
        if (id !== projectId || !visible)
            return
        opening = false
        closeTimer.restart()
    }
    function hideFor(id) {
        if (id === projectId)
            close()
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0
    onClosed: { closeTimer.stop(); safetyTimer.stop() }

    Timer { id: closeTimer; interval: 1200; onTriggered: overlay.close() }
    Timer { id: safetyTimer; interval: 10000; onTriggered: overlay.close() }

    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: settingsManager.reducedMotion ? 0 : 160 } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: settingsManager.reducedMotion ? 0 : 200 } }

    background: Rectangle {
        color: Qt.rgba(theme.windowBackground.r * 0.6, theme.windowBackground.g * 0.6, theme.windowBackground.b * 0.6, 0.94)
    }

    contentItem: Item {
        RowLayout {
            anchors.centerIn: parent
            spacing: 44

            GameArtwork {
                visible: overlay.artworkPath.length > 0
                Layout.preferredHeight: 220
                Layout.preferredWidth: Math.round(220 * LibraryGridLayout.frameRatioForPlatform(overlay.platform))
                source: overlay.artworkPath
                platform: overlay.platform
                themeTokens: theme
                reducedMotion: settingsManager.reducedMotion
                frameMode: "platform"
                frameInset: 0
                showViewportBackground: false
            }

            ColumnLayout {
                Layout.preferredWidth: 520
                spacing: 4

                Label {
                    text: "STARTING"
                    color: theme.textMuted
                    font.pixelSize: 12 * theme.fontScale
                    font.bold: true
                    font.letterSpacing: 1.4
                }
                Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 14
                    text: overlay.projectId.length > 0 ? (projectCatalog.project(overlay.projectId).name || "") : ""
                    color: theme.textPrimary
                    font.pixelSize: 34 * theme.fontScale
                    font.bold: true
                    elide: Text.ElideRight
                }

                Repeater {
                    model: overlay.steps
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        spacing: 14
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 1
                            width: 22; height: 22; radius: 11
                            color: modelData.warning ? theme.warning : theme.success
                            Label {
                                anchors.centerIn: parent
                                text: modelData.warning ? "!" : "✓"
                                color: theme.windowBackground
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: theme.textPrimary
                                font.pixelSize: 15 * theme.fontScale
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: modelData.detail || ""
                                color: theme.textMuted
                                font.pixelSize: 12.5 * theme.fontScale
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.topMargin: 10
                    spacing: 14
                    BusyIndicator {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        running: overlay.visible && overlay.opening
                        visible: overlay.opening
                    }
                    Rectangle {
                        visible: !overlay.opening
                        width: 22; height: 22; radius: 11
                        color: theme.success
                        Label { anchors.centerIn: parent; text: "✓"; color: theme.windowBackground; font.pixelSize: 12; font.bold: true }
                    }
                    Label {
                        text: overlay.opening ? "Opening the game…" : "Started"
                        color: theme.textPrimary
                        font.pixelSize: 15 * theme.fontScale
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }
}
