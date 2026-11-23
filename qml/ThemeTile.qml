import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A theme choice in Settings > Appearance: a miniature Library drawn in the
// theme's own colors, its name, and an "In use" badge. Activating it applies
// the theme straight away.
Button {
    id: tile

    property string themeId
    property string title
    property string description
    property bool inUse: false
    readonly property var colors: theme.paletteFor(themeId)

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: 196
    padding: 10
    Accessible.name: title + (inUse ? ", in use" : "")

    background: Rectangle {
        radius: 14
        color: tile.hovered && !tile.activeFocus ? Qt.lighter(theme.sidebarBackground, 1.1)
            : theme.sidebarBackground
        border.width: tile.activeFocus ? 2 : (tile.inUse ? 1 : 0)
        border.color: tile.activeFocus ? theme.accent : theme.divider
    }

    contentItem: ColumnLayout {
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 96
            radius: 9
            color: tile.colors.windowBackground
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * 0.14
                color: tile.colors.sidebarBackground
                Rectangle {
                    x: 8; y: 12
                    width: parent.width - 16; height: 6; radius: 3
                    color: tile.colors.accent
                }
            }
            Row {
                x: parent.width * 0.14 + 12
                y: 10
                spacing: 6
                Rectangle { width: 60; height: 14; radius: 7; color: tile.colors.accent }
                Rectangle { width: 44; height: 14; radius: 7; color: tile.colors.surface }
                Rectangle { width: 44; height: 14; radius: 7; color: tile.colors.surface }
            }
            Row {
                x: parent.width * 0.14 + 12
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                spacing: 7
                Repeater {
                    model: 5
                    Rectangle {
                        required property int index
                        anchors.bottom: parent ? parent.bottom : undefined
                        width: 34
                        height: index % 2 === 0 ? 46 : 40
                        radius: 4
                        color: tile.colors.surface
                        border.width: index === 1 ? 2 : 0
                        border.color: tile.colors.accent
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: tile.title
                    color: theme.textPrimary
                    font.pixelSize: 14 * theme.fontScale
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: tile.description
                    color: theme.textMuted
                    font.pixelSize: 11.5 * theme.fontScale
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                visible: tile.inUse
                Layout.preferredHeight: 24
                Layout.preferredWidth: inUseLabel.implicitWidth + 18
                radius: 12
                color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.18)
                Label {
                    id: inUseLabel
                    anchors.centerIn: parent
                    text: "✓ In use"
                    color: theme.accentHover
                    font.pixelSize: 11 * theme.fontScale
                    font.bold: true
                }
            }
        }
    }
}
