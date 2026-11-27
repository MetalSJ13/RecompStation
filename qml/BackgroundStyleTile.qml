import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A background style choice in Settings > Background: a small preview of the
// style, its name and a short description. Also used, compact (no
// description), for the Still background's patterns.
Button {
    id: tile

    property string styleValue
    property string title
    property string description
    property bool current: false
    property url wallpaper
    property color waveColor: "#3d8bff"
    // The Still pattern shown in the preview ("gradient", "crt", ...).
    property string pattern: "gradient"
    // The Moving scene shown in the preview ("waves", "stars", ...).
    property string scene: "waves"
    property bool compact: false

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: compact ? 104 : 150
    padding: 8
    Accessible.name: title + (current ? ", selected" : "")

    background: Rectangle {
        radius: 12
        color: tile.hovered && !tile.activeFocus ? Qt.lighter(theme.sidebarBackground, 1.1)
            : theme.sidebarBackground
        border.width: tile.activeFocus || tile.current ? 2 : 0
        border.color: tile.activeFocus ? theme.accent
            : Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.55)
    }

    contentItem: ColumnLayout {
        spacing: 6
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            radius: 8
            clip: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.backgroundTop }
                GradientStop { position: 1.0; color: theme.backgroundBottom }
            }
            RetroBackground {
                anchors.fill: parent
                visible: tile.styleValue === "static" && tile.pattern !== "gradient"
                pattern: visible ? tile.pattern : "gradient"
                accent: theme.accent
                topColor: theme.backgroundTop
                bottomColor: theme.backgroundBottom
            }
            // A still frame of the Moving scene in the chosen color.
            WaveBackground {
                anchors.fill: parent
                visible: tile.styleValue === "animated"
                scene: tile.scene
                waveColor: tile.waveColor
                topColor: theme.backgroundTop
                bottomColor: theme.backgroundBottom
                quality: 0.5
                running: false
            }
            Image {
                anchors.fill: parent
                visible: tile.styleValue === "custom" && tile.wallpaper.toString().length > 0
                source: visible ? tile.wallpaper : ""
                sourceSize.width: 320
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
            Label {
                anchors.centerIn: parent
                visible: tile.styleValue === "custom" && tile.wallpaper.toString().length === 0
                text: "No image yet"
                color: theme.textMuted
                font.pixelSize: 11 * theme.fontScale
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            text: tile.title
            color: theme.textPrimary
            font.pixelSize: (tile.compact ? 12.5 : 13.5) * theme.fontScale
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            visible: !tile.compact
            text: tile.description
            color: theme.textMuted
            font.pixelSize: 11 * theme.fontScale
            elide: Text.ElideRight
        }
    }
}
