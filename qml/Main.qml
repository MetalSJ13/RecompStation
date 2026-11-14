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
    color: "#15151d"

    property string currentPage: "Library"
    property string selectedPlatform: "All Games"
    property int libraryRevision: 0
    property int selectedMetadataPlatformId: 0
    property int selectedLocalGameId: 0
    property string metadataAssociationMessage: ""
    property string scanMessage: "Scan your platform folders to discover games."

    palette.window: "#15151d"
    palette.windowText: "#ffffff"
    palette.button: "#292936"
    palette.buttonText: "#ffffff"
    palette.highlight: "#8060e8"
    palette.highlightedText: "#ffffff"

    Connections {
        target: libraryScanner
        function onScanCompleted(gamesFound) {
            window.libraryRevision++
            window.scanMessage = libraryScanner.lastError.length > 0
                ? libraryScanner.lastError
                : gamesFound + " new game(s) added to your library."
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 225
            Layout.fillHeight: true
            color: "#1d1d28"

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
                        color: "#8060e8"
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

                Rectangle { Layout.fillWidth: true; height: 1; color: "#343440" }
                Label { text: "LIBRARY"; color: "#888899"; font.pixelSize: 11; font.bold: true }

                Button {
                    Layout.fillWidth: true
                    text: "Game Library"
                    highlighted: currentPage === "Library"
                    onClicked: {
                        selectedPlatform = "All Games"
                        currentPage = "Library"
                    }
                }
                Button {
                    Layout.fillWidth: true
                    text: "Metadata Search"
                    highlighted: currentPage === "Metadata Search"
                    onClicked: currentPage = "Metadata Search"
                }
                Button {
                    Layout.fillWidth: true
                    text: "Platforms"
                    highlighted: currentPage === "Platforms"
                    onClicked: currentPage = "Platforms"
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#343440" }
                Label { text: "MANAGEMENT"; color: "#888899"; font.pixelSize: 11; font.bold: true }
                Button {
                    Layout.fillWidth: true
                    text: "Launchers"
                    highlighted: currentPage === "Launchers"
                    onClicked: currentPage = "Launchers"
                }
                Item { Layout.fillHeight: true }
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
            color: "#15151d"

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
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Label {
                                    text: selectedPlatform === "All Games" ? "Game Library" : selectedPlatform
                                    font.pixelSize: 30
                                    font.bold: true
                                    color: "white"
                                }
                                Label {
                                    text: "Manage and launch your recompiled games."
                                    color: "#9999aa"
                                    font.pixelSize: 14
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

                        Label {
                            Layout.fillWidth: true
                            text: scanMessage
                            color: "#a98cff"
                            wrapMode: Text.Wrap
                        }

                        ListView {
                            id: gameList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 10
                            model: {
                                window.libraryRevision
                                databaseManager.games(selectedPlatform === "All Games" ? "" : selectedPlatform)
                            }
                            delegate: Rectangle {
                                required property string title
                                required property string platform
                                required property string path
                                width: gameList.width
                                height: 76
                                radius: 10
                                color: "#252532"
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 3
                                    Label {
                                        text: title
                                        color: "white"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                    Label {
                                        text: platform + "  •  " + path
                                        color: "#9999aa"
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            visible: gameList.count === 0
                            text: "No games found. Add supported game files to a platform folder, then scan."
                            color: "#9999aa"
                            wrapMode: Text.Wrap
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
                            color: "#252532"
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
                                width: metadataResultsList.width
                                height: resultContent.implicitHeight + 28
                                radius: 10
                                color: "#252532"
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
                                    Button {
                                        text: "Associate with selected ROM"
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
                                                rating
                                            )
                                            if (saved) {
                                                window.libraryRevision++
                                                window.metadataAssociationMessage = "Metadata associated with the selected ROM."
                                            } else {
                                                window.metadataAssociationMessage = databaseManager.lastError
                                            }
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
                                    color: "#252532"
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
                        spacing: 20
                        Label { text: "Launcher Manager"; color: "white"; font.pixelSize: 30; font.bold: true }
                        Label { text: "Manage your installed recompilation projects."; color: "#9999aa" }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: "#20202c"
                            Label {
                                anchors.centerIn: parent
                                text: "No launchers registered yet."
                                color: "#9999aa"
                                font.pixelSize: 16
                            }
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
                            color: "#252532"
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
