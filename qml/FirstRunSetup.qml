import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Full-window first-time setup: library folder → add games → covers and
// info → ready. Shown while window.firstRunStep >= 0.
Popup {
    id: setup

    readonly property int step: window.firstRunStep
    readonly property var steps: ["Library folder", "Add games", "Covers & info", "Ready"]
    readonly property var suggested: platformManager.suggestedLibraryLocation()
    // Item that controller and arrow-key navigation moves within.
    readonly property Item navigationRoot: stepContainer

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: 0
    visible: window.firstRunStep >= 0

    function goBack() {
        if (step === 2 && gameImporter.importing)
            return
        if (step > 0 && !(step === 1 && !platformManager.libraryConfigured))
            window.firstRunStep = step - 1
    }
    function focusStep() {
        Qt.callLater(function() {
            if (step === 0) useSuggestedButton.forceActiveFocus()
            else if (step === 1) importPanel.focusFirst()
            else if (step === 2) (launchBoxDownloadButton.visible && launchBoxDownloadButton.enabled
                                  ? launchBoxDownloadButton : continueButton).forceActiveFocus()
            else finishButton.forceActiveFocus()
        })
    }
    onStepChanged: focusStep()
    onOpened: focusStep()

    component SetupButton: Button {
        id: setupButton
        activeFocusOnTab: true
        focusPolicy: Qt.StrongFocus
        leftPadding: 20
        rightPadding: 20
        implicitHeight: 44
        background: Rectangle {
            radius: 11
            color: setupButton.highlighted && !setupButton.activeFocus ? theme.accent
                : setupButton.activeFocus ? theme.elevatedSurface : theme.surface
            border.color: theme.accent
            border.width: setupButton.activeFocus ? 2 : 0
            opacity: setupButton.enabled ? 1 : 0.5
        }
        contentItem: Label {
            text: setupButton.text
            color: setupButton.highlighted && !setupButton.activeFocus ? theme.accentText : theme.textPrimary
            font.pixelSize: 14 * theme.fontScale
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: theme.backgroundTop }
            GradientStop { position: 1.0; color: theme.backgroundBottom }
        }
    }

    contentItem: Item {
        // Header and step indicator.
        RowLayout {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 28
            Image {
                source: "qrc:/qt/qml/RecompStation/assets/logo/recompstation-mark.svg"
                sourceSize: Qt.size(128, 128)
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }
            Label {
                text: "RecompStation"
                color: theme.textPrimary
                font.pixelSize: 17 * theme.fontScale
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "First-time setup"
                color: theme.textMuted
                font.pixelSize: 13 * theme.fontScale
            }
        }
        Row {
            id: stepIndicator
            anchors.top: header.bottom
            anchors.topMargin: 18
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14
            Repeater {
                model: setup.steps
                delegate: Row {
                    required property string modelData
                    required property int index
                    spacing: 14
                    Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 26; height: 26; radius: 13
                            color: index <= setup.step ? theme.accent : theme.surface
                            Label {
                                anchors.centerIn: parent
                                text: index < setup.step ? "✓" : (index + 1)
                                color: index <= setup.step ? theme.accentText : theme.textMuted
                                font.pixelSize: 12 * theme.fontScale
                                font.bold: true
                            }
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: index === setup.step ? theme.textPrimary : theme.textMuted
                            font.pixelSize: 13 * theme.fontScale
                            font.weight: index === setup.step ? Font.DemiBold : Font.Normal
                        }
                    }
                    Rectangle {
                        visible: index < setup.steps.length - 1
                        anchors.verticalCenter: parent.verticalCenter
                        width: 44; height: 2
                        color: index < setup.step ? theme.accent : theme.surface
                    }
                }
            }
        }

        Item {
            id: stepContainer
            anchors.top: stepIndicator.bottom
            anchors.topMargin: 30
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 76
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 80, 1100)

            // ---- 1. Library folder ----
            // Just the folder choice, centred; the system folders are created
            // inside it without being listed here.
            ColumnLayout {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -30
                width: Math.min(parent.width, 860)
                visible: setup.step === 0
                spacing: 12
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 72
                    Layout.bottomMargin: 10
                    source: "qrc:/qt/qml/RecompStation/assets/logo/recompstation-mark.svg"
                    sourceSize: Qt.size(256, 256)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    Accessible.ignored: true
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "Welcome to RecompStation"
                    color: theme.textPrimary
                    font.pixelSize: 34 * theme.fontScale
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 18
                    horizontalAlignment: Text.AlignHCenter
                    text: "First, where should your games live? RecompStation keeps one folder per system inside it."
                    color: theme.textSecondary
                    font.pixelSize: 15 * theme.fontScale
                    wrapMode: Text.Wrap
                }
                SettingRow {
                    title: setup.suggested.path.replace(platformManager.homeDirectory(), "~")
                    description: "Suggested · " + window.formatBytes(setup.suggested.freeBytes) + " free on this drive · "
                        + (setup.suggested.exists ? "already exists; nothing in it is changed or removed"
                                                  : "a new folder will be created")
                    SetupButton {
                        id: useSuggestedButton
                        text: "Use this folder"
                        highlighted: true
                        onClicked: {
                            if (platformManager.useLibraryLocation(setup.suggested.path))
                                window.firstRunStep = 1
                            else
                                setupMessage.text = "Could not create that folder. Choose another one."
                        }
                    }
                }
                SettingRow {
                    title: "Choose another folder…"
                    description: "An existing folder, an external drive, or wherever your games already are."
                    SetupButton {
                        text: "Browse"
                        onClicked: window.openLibraryFolderBrowser()
                    }
                }
                Label {
                    id: setupMessage
                    Layout.fillWidth: true
                    visible: text.length > 0
                    color: theme.error
                    font.pixelSize: 13 * theme.fontScale
                }
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    horizontalAlignment: Text.AlignHCenter
                    text: "You can change this later in Settings › Library."
                    color: theme.textMuted
                    font.pixelSize: 12.5 * theme.fontScale
                }
            }

            // ---- 2. Add games ----
            ColumnLayout {
                anchors.fill: parent
                visible: setup.step === 1
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "Bring in your games"
                    color: theme.textPrimary
                    font.pixelSize: 30 * theme.fontScale
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 12
                    horizontalAlignment: Text.AlignHCenter
                    text: "Point RecompStation at folders that already have games. It works out the system for each file and sorts them into your library."
                    color: theme.textSecondary
                    font.pixelSize: 14.5 * theme.fontScale
                    wrapMode: Text.Wrap
                }
                ImportGamesPanel {
                    id: importPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    secondaryText: "Skip for now"
                    onSecondaryRequested: window.firstRunStep = 2
                    onImportStarted: window.firstRunStep = 2
                }
            }

            // ---- 3. Covers and info ----
            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                visible: setup.step === 2
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "Covers, descriptions and screenshots"
                    color: theme.textPrimary
                    font.pixelSize: 30 * theme.fontScale
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 14
                    horizontalAlignment: Text.AlignHCenter
                    text: gameImporter.importing
                        ? "Your games are being added in the background. Meanwhile, choose where game details come from."
                        : "Choose where game details come from. You can change this later in Settings › Metadata."
                    color: theme.textSecondary
                    font.pixelSize: 14.5 * theme.fontScale
                    wrapMode: Text.Wrap
                }
                Rectangle {
                    id: launchBoxSetupCard
                    Layout.fillWidth: true
                    implicitHeight: launchBoxSetupColumn.implicitHeight + 28
                    radius: 12
                    color: theme.sidebarBackground
                    border.width: window.isDescendantOf(window.activeFocusItem, launchBoxSetupCard) ? 2 : 0
                    border.color: theme.accent
                    ColumnLayout {
                        id: launchBoxSetupColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        anchors.leftMargin: 18
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Label {
                                    text: "LaunchBox Games Database · recommended"
                                    color: theme.textPrimary
                                    font.pixelSize: 14.5 * theme.fontScale
                                    font.weight: Font.DemiBold
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: launchBoxCatalogManager.catalogAvailable && settingsManager.launchBoxEnabled
                                            && !launchBoxCatalogManager.busy
                                        ? "Installed (" + window.formatBytes(launchBoxCatalogManager.catalogSizeBytes) + "). Games are matched offline."
                                        : "Offline catalog: no account or key. Downloads about 106 MB once, then matches games without internet."
                                    color: theme.textMuted
                                    font.pixelSize: 12 * theme.fontScale
                                    wrapMode: Text.Wrap
                                }
                            }
                            SetupButton {
                                id: launchBoxDownloadButton
                                visible: !launchBoxCatalogManager.busy
                                highlighted: !launchBoxCatalogManager.catalogAvailable
                                enabled: !(launchBoxCatalogManager.catalogAvailable && settingsManager.launchBoxEnabled)
                                text: launchBoxCatalogManager.catalogAvailable && settingsManager.launchBoxEnabled ? "✓ Ready"
                                    : launchBoxCatalogManager.catalogAvailable ? "Turn on" : "Download"
                                onClicked: {
                                    settingsManager.setLaunchBoxEnabled(true)
                                    if (!launchBoxCatalogManager.catalogAvailable)
                                        launchBoxCatalogManager.updateCatalog()
                                }
                            }
                        }
                        LaunchBoxProgress {
                            Layout.fillWidth: true
                        }
                    }
                }
                SettingRow {
                    title: "TheGamesDB · optional"
                    description: settingsManager.theGamesDbKeySource !== "none"
                        ? "Key saved. TheGamesDB fills in games LaunchBox doesn't know."
                        : "Online; fills in games LaunchBox doesn't know. Needs a free API key from the TheGamesDB forums."
                    RowLayout {
                        spacing: 8
                        TextField {
                            id: setupKeyField
                            visible: settingsManager.theGamesDbKeySource === "none"
                            Layout.preferredWidth: 260
                            activeFocusOnTab: true
                            placeholderText: "Paste API key"
                            echoMode: TextInput.Password
                            color: theme.textPrimary
                            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                            onAccepted: saveKeyButton.clicked()
                        }
                        SetupButton {
                            id: saveKeyButton
                            visible: settingsManager.theGamesDbKeySource === "none"
                            enabled: setupKeyField.text.trim().length > 0
                            text: "Save key"
                            onClicked: {
                                if (settingsManager.saveTheGamesDbApiKey(setupKeyField.text)) {
                                    setupKeyField.text = ""
                                    keyMessage.text = ""
                                } else {
                                    keyMessage.text = "That doesn't look like a TheGamesDB key (16–128 letters and digits)."
                                }
                            }
                        }
                    }
                }
                Label {
                    id: keyMessage
                    Layout.fillWidth: true
                    visible: text.length > 0
                    color: theme.error
                    font.pixelSize: 12.5 * theme.fontScale
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    Layout.preferredHeight: 10
                    radius: 5
                    visible: gameImporter.importing
                    color: theme.surface
                    Rectangle {
                        width: parent.width * (gameImporter.bytesTotal > 0
                            ? Math.min(1, gameImporter.bytesDone / gameImporter.bytesTotal) : 0)
                        height: parent.height
                        radius: 5
                        color: theme.accent
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: gameImporter.importing || gameImporter.status.length > 0
                    text: gameImporter.importing
                        ? "Adding game " + (gameImporter.completedCount + 1) + " of " + gameImporter.totalCount
                            + (gameImporter.currentName.length > 0 ? " · " + gameImporter.currentName : "")
                        : gameImporter.status
                    color: theme.textMuted
                    font.pixelSize: 12.5 * theme.fontScale
                    elide: Text.ElideRight
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    Item { Layout.fillWidth: true }
                    SetupButton {
                        text: "Back"
                        enabled: !gameImporter.importing
                        onClicked: setup.goBack()
                    }
                    SetupButton {
                        id: continueButton
                        text: "Continue"
                        highlighted: true
                        onClicked: window.firstRunStep = 3
                    }
                }
            }

            // ---- 4. Ready ----
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width, 640)
                visible: setup.step === 3
                spacing: 14
                ControllerOutline {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 145
                    kind: window.controllerLayout
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "You're all set"
                    color: theme.textPrimary
                    font.pixelSize: 32 * theme.fontScale
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: (gameImporter.importing
                            ? "Your games are still being added; they appear in the Library as they arrive. "
                            : "")
                        + "Add more games any time from Settings › Library, or drop files onto RecompStation."
                    color: theme.textSecondary
                    font.pixelSize: 15 * theme.fontScale
                    wrapMode: Text.Wrap
                }
                SetupButton {
                    id: finishButton
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 10
                    text: "Go to my library"
                    highlighted: true
                    onClicked: window.finishFirstRun()
                }
            }
        }

        // Hint bar, as on every page.
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 26
            text: window.navigationHintSummary()
            color: theme.textMuted
            font.pixelSize: 12 * theme.fontScale
        }
    }
}
