
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

    // Application theme

    palette.window: "#15151d"
    palette.windowText: "#ffffff"
    palette.button: "#292936"
    palette.buttonText: "#ffffff"
    palette.highlight: "#8060e8"
    palette.highlightedText: "#ffffff"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Navigation sidebar

        Rectangle {
            Layout.preferredWidth: 225
            Layout.fillHeight: true
            color: "#1d1d28"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                // Application branding

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

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#343440"
                }

                Label {
                    text: "LIBRARY"
                    color: "#888899"
                    font.pixelSize: 11
                    font.bold: true
                }

                Button {
                    Layout.fillWidth: true
                    text: "Game Library"
                    highlighted: currentPage === "Library"
                    onClicked: currentPage = "Library"
                    focusPolicy: Qt.StrongFocus
                }

                Button {
                    Layout.fillWidth: true
                    text: "Platforms"
                    highlighted: currentPage === "Platforms"
                    onClicked: currentPage = "Platforms"
                    focusPolicy: Qt.StrongFocus
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#343440"
                }

                Label {
                    text: "MANAGEMENT"
                    color: "#888899"
                    font.pixelSize: 11
                    font.bold: true
                }

                Button {
                    Layout.fillWidth: true
                    text: "Launchers"
                    highlighted: currentPage === "Launchers"
                    onClicked: currentPage = "Launchers"
                }

                Item {
                    Layout.fillHeight: true
                }

                Button {
                    Layout.fillWidth: true
                    text: "Settings"
                    highlighted: currentPage === "Settings"
                    onClicked: currentPage = "Settings"
                }
            }
        }

        // Main content area

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
                        case "Platforms": return 1
                        case "Launchers": return 2
                        case "Settings": return 3
                        default: return 0
                    }
                }

                // LIBRARY PAGE

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20

                        Label {
                            text: "Game Library"
                            font.pixelSize: 30
                            font.bold: true
                            color: "white"
                        }

                        Label {
                            text: "Manage and launch your recompiled games."
                            color: "#9999aa"
                            font.pixelSize: 14
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#20202c"
                            radius: 12

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 12

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "No games found"
                                    font.pixelSize: 22
                                    font.bold: true
                                    color: "white"
                                }

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "Your game library will appear here."
                                    color: "#9999aa"
                                }

                                Button {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "Browse Platforms"
                                    onClicked: currentPage = "Platforms"
                                }
                            }
                        }
                    }
                }

                // PLATFORMS PAGE

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20

                        Label {
                            text: "Platforms"
                            font.pixelSize: 30
                            font.bold: true
                            color: "white"
                        }

                        Label {
                            text: "Browse your supported console platforms."
                            color: "#9999aa"
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 15
                            columnSpacing: 15

                            Repeater {
                                model: platformManager.platforms()

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 115

                                    radius: 12
                                    color: "#252532"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 18
                                        spacing: 5

                                        Label {
                                            text: modelData
                                            color: "white"
                                            font.pixelSize: 20
                                            font.bold: true
                                        }

                                        Label {
                                            text: "Game directory"
                                            color: "#9999aa"
                                            font.pixelSize: 12
                                        }

                                        Item {
                                            Layout.fillHeight: true
                                        }

                                        Label {
                                            text: "Open platform"
                                            color: "#a98cff"
                                            font.pixelSize: 12
                                        }
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
                            text: "Game folders: " +
                            platformManager.dataDirectory +
                            "/games/"
                            color: "#777788"
                            font.pixelSize: 11
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                // LAUNCHERS PAGE

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20

                        Label {
                            text: "Launcher Manager"
                            color: "white"
                            font.pixelSize: 30
                            font.bold: true
                        }

                        Label {
                            text: "Manage your installed recompilation projects."
                            color: "#9999aa"
                        }

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

                // SETTINGS PAGE

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 20

                        Label {
                            text: "Settings"
                            color: "white"
                            font.pixelSize: 30
                            font.bold: true
                        }

                        Label {
                            text: "Application configuration"
                            color: "#9999aa"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 100
                            radius: 12
                            color: "#252532"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 15

                                Label {
                                    text: "Application data directory"
                                    color: "white"
                                    font.bold: true
                                }

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
