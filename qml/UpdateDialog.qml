import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// RecompStation's own update: the offer ("0.2.0 is available"), then the
// download, the check, ready to restart, or what went wrong. One panel whose
// content follows appUpdater.state. A line under the buttons says what the
// focused one does.
Popup {
    id: dialog

    readonly property string phase: {
        const state = appUpdater.state
        if (state === "downloading" || state === "verifying" || state === "ready" || state === "failed")
            return state
        return "offer"
    }
    readonly property bool canInstall: appUpdater.canInstall
    // Sizes as people read them: "61 MB".
    function megabytes(bytes) {
        return Math.max(1, Math.round(bytes / (1024 * 1024))) + " MB"
    }
    function focusFirst() {
        Qt.callLater(function() {
            const button = dialog.phase === "offer" ? primaryOfferButton
                : dialog.phase === "downloading" ? hideButton
                : dialog.phase === "ready" ? restartButton
                : dialog.phase === "failed" ? retryButton : null
            if (button)
                button.forceActiveFocus()
            else
                panel.forceActiveFocus()
        })
    }
    // B / Esc: the gentle choice for each phase.
    function back() {
        if (phase === "offer")
            appUpdater.remindLater()
        dialog.close()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, (parent ? parent.width : 640) - 48)
    modal: true
    dim: true
    padding: 0
    closePolicy: Popup.NoAutoClose
    onOpened: {
        uiSounds.play("menu")
        focusFirst()
    }
    onPhaseChanged: if (visible) focusFirst()

    Overlay.modal: Rectangle { color: Qt.rgba(0.02, 0.03, 0.05, 0.62) }

    background: Rectangle {
        radius: 18
        color: theme.surface
        border.color: theme.divider
        border.width: 1
    }

    contentItem: FocusScope {
        id: panel
        implicitHeight: content.implicitHeight + 48
        Keys.onEscapePressed: function(event) {
            event.accepted = true
            uiSounds.play("back")
            dialog.back()
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 26
            spacing: 14

            // Title row with the app icon.
            RowLayout {
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 12
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#2d2754" }
                        GradientStop { position: 1; color: "#14151f" }
                    }
                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/RecompStation/assets/logo/recompstation-mark.svg"
                        sourceSize: Qt.size(28, 28)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: dialog.phase === "downloading" ? "Downloading " + appUpdater.latestVersion + "…"
                            : dialog.phase === "verifying" ? "Checking the download…"
                            : dialog.phase === "ready" ? "Ready: restart to finish"
                            : dialog.phase === "failed" ? "The update didn't finish"
                            : "RecompStation " + appUpdater.latestVersion + " is available"
                        color: theme.textPrimary
                        font.pixelSize: 22 * theme.fontScale
                        font.bold: true
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: dialog.phase === "offer"
                        text: "You have " + appUpdater.currentVersion
                            + (appUpdater.releaseDate.length > 0 ? " · released " + appUpdater.releaseDate : "")
                            + (dialog.canInstall && appUpdater.downloadSize > 0
                               ? " · " + dialog.megabytes(appUpdater.downloadSize) : "")
                        color: theme.textSecondary
                        font.pixelSize: 13 * theme.fontScale
                        wrapMode: Text.Wrap
                    }
                }
            }

            // What's new.
            Rectangle {
                Layout.fillWidth: true
                visible: dialog.phase === "offer"
                implicitHeight: notesColumn.implicitHeight + 24
                radius: 12
                color: theme.windowBackground
                border.color: theme.divider
                ColumnLayout {
                    id: notesColumn
                    anchors.fill: parent
                    anchors.margins: 12
                    anchors.leftMargin: 16
                    spacing: 6
                    Label {
                        text: "WHAT'S NEW"
                        color: theme.textMuted
                        font.pixelSize: 11 * theme.fontScale
                        font.bold: true
                        font.letterSpacing: 1
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.maximumHeight: 220
                        clip: true
                        text: appUpdater.releaseNotes.length > 0 ? appUpdater.releaseNotes
                            : "See the release page for what changed."
                        textFormat: appUpdater.releaseNotes.length > 0 ? Text.MarkdownText : Text.PlainText
                        color: theme.textPrimary
                        font.pixelSize: 13 * theme.fontScale
                        wrapMode: Text.Wrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }
                }
            }

            // Progress.
            ColumnLayout {
                Layout.fillWidth: true
                visible: dialog.phase === "downloading" || dialog.phase === "verifying"
                spacing: 8
                Label {
                    text: dialog.phase === "verifying" ? "Making sure it arrived complete."
                        : dialog.megabytes(appUpdater.bytesReceived) + " of "
                          + dialog.megabytes(appUpdater.bytesTotal)
                    color: theme.textSecondary
                    font.pixelSize: 13 * theme.fontScale
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 8
                    radius: 4
                    color: theme.elevatedSurface
                    Rectangle {
                        height: parent.height
                        radius: 4
                        color: dialog.phase === "verifying" ? theme.success : theme.accent
                        width: dialog.phase === "verifying" || appUpdater.bytesTotal <= 0 ? parent.width
                            : parent.width * Math.min(1, appUpdater.bytesReceived / appUpdater.bytesTotal)
                    }
                }
            }

            // One line on what happens, or what went wrong.
            Label {
                Layout.fillWidth: true
                text: dialog.phase === "offer"
                      ? (dialog.canInstall
                         ? "RecompStation updates itself and restarts when you're ready. Your games, settings and installed launchers stay as they are."
                         : "This copy can't update itself. Get the new version from the website.")
                    : dialog.phase === "ready"
                      ? "RecompStation " + appUpdater.latestVersion + " is ready. Restart whenever you like; until then you keep using this version."
                    : dialog.phase === "failed"
                      ? appUpdater.errorMessage + " Nothing changed: you're still on " + appUpdater.currentVersion + "."
                    : dialog.phase === "downloading"
                      ? "You can keep browsing while it downloads."
                    : ""
                visible: text.length > 0
                color: theme.textSecondary
                font.pixelSize: 13 * theme.fontScale
                wrapMode: Text.Wrap
            }

            // Buttons.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 16

                SettingsButton {
                    prominentFocus: true
                    id: primaryOfferButton
                    visible: dialog.phase === "offer"
                    primary: true
                    text: dialog.canInstall ? "Update now" : "Get it from the website"
                    property string explanation: dialog.canInstall
                        ? "Downloads " + appUpdater.latestVersion + " now. You choose when to restart."
                        : "Opens the release page in your browser."
                    onClicked: {
                        if (dialog.canInstall) {
                            appUpdater.startUpdate()
                        } else {
                            Qt.openUrlExternally(appUpdater.releaseUrl)
                            appUpdater.remindLater()
                            dialog.close()
                        }
                    }
                }
                SettingsButton {
                    prominentFocus: true
                    id: laterButton
                    visible: dialog.phase === "offer"
                    text: "Remind me later"
                    property string explanation: "Asks again next time you start RecompStation."
                    onClicked: dialog.back()
                }
                Item { Layout.fillWidth: true; visible: dialog.phase === "offer" }
                SettingsButton {
                    prominentFocus: true
                    id: skipButton
                    visible: dialog.phase === "offer"
                    text: "Skip " + appUpdater.latestVersion
                    property string explanation: "You won't be asked about " + appUpdater.latestVersion
                        + " again. You'll still hear about newer versions, and you can update any time from Settings › Application."
                    onClicked: {
                        appUpdater.skipLatestVersion()
                        dialog.close()
                    }
                }

                SettingsButton {
                    prominentFocus: true
                    id: hideButton
                    visible: dialog.phase === "downloading"
                    text: "Hide"
                    property string explanation: "Keeps downloading. Progress shows at the top of the screen."
                    onClicked: dialog.close()
                }
                SettingsButton {
                    prominentFocus: true
                    visible: dialog.phase === "downloading"
                    text: "Cancel"
                    property string explanation: "Stops the download. You can update later from Settings."
                    onClicked: {
                        appUpdater.cancelUpdate()
                        dialog.close()
                    }
                }

                SettingsButton {
                    prominentFocus: true
                    id: restartButton
                    visible: dialog.phase === "ready"
                    primary: true
                    text: "Restart now"
                    property string explanation: "Closes RecompStation and opens " + appUpdater.latestVersion + "."
                    onClicked: appUpdater.restartNow()
                }
                SettingsButton {
                    prominentFocus: true
                    visible: dialog.phase === "ready"
                    text: "Later"
                    property string explanation: "The next time you start RecompStation, it opens " + appUpdater.latestVersion + "."
                    onClicked: dialog.close()
                }

                SettingsButton {
                    prominentFocus: true
                    id: retryButton
                    visible: dialog.phase === "failed"
                    primary: true
                    text: "Try again"
                    property string explanation: "Downloads " + appUpdater.latestVersion + " again."
                    onClicked: appUpdater.startUpdate()
                }
                SettingsButton {
                    prominentFocus: true
                    visible: dialog.phase === "failed"
                    text: "Close"
                    property string explanation: ""
                    onClicked: dialog.close()
                }
                Item { Layout.fillWidth: true; visible: dialog.phase === "failed" }
                SettingsButton {
                    prominentFocus: true
                    visible: dialog.phase === "failed"
                    text: "Download from the website"
                    property string explanation: "Opens the release page in your browser."
                    onClicked: Qt.openUrlExternally(appUpdater.releaseUrl)
                }
            }

            // What the focused button does.
            Label {
                Layout.fillWidth: true
                readonly property Item focusedButton: {
                    const item = dialog.visible ? window.activeFocusItem : null
                    return item && item.explanation !== undefined ? item : null
                }
                text: focusedButton ? focusedButton.explanation : ""
                visible: text.length > 0
                color: theme.textMuted
                font.pixelSize: 12 * theme.fontScale
                wrapMode: Text.Wrap
            }
        }
    }
}
