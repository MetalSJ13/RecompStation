import QtQuick

QtObject {
    readonly property string selectedTheme: window.pendingThemeName
    readonly property real fontScale: settingsManager.fontSize === "small" ? 0.9
        : settingsManager.fontSize === "large" ? 1.2 : 1.0

    // Each curated theme defines a complete set of coordinated surfaces,
    // accents, text colors, and status colors.
    readonly property color backgroundTop: selectedTheme === "electric-blue" ? "#172b40"
        : selectedTheme === "ember" ? "#2b1d16" : "#171a2a"
    readonly property color windowBackground: selectedTheme === "electric-blue" ? "#101722"
        : selectedTheme === "ember" ? "#191411" : "#11131b"
    readonly property color backgroundBottom: selectedTheme === "electric-blue" ? "#0b111b"
        : selectedTheme === "ember" ? "#130f0d" : "#0d0f17"

    readonly property color sidebarBackground: selectedTheme === "electric-blue" ? "#172231"
        : selectedTheme === "ember" ? "#241b16" : "#191c27"
    readonly property color surface: selectedTheme === "electric-blue" ? "#202e40"
        : selectedTheme === "ember" ? "#30231c" : "#222635"
    readonly property color elevatedSurface: selectedTheme === "electric-blue" ? "#293b51"
        : selectedTheme === "ember" ? "#3b2b22" : "#2a3042"
    readonly property color controlBackground: selectedTheme === "electric-blue" ? "#30445d"
        : selectedTheme === "ember" ? "#493326" : "#30374b"
    readonly property color controlHover: selectedTheme === "electric-blue" ? "#3b526d"
        : selectedTheme === "ember" ? "#59402f" : "#3b435a"
    readonly property color divider: selectedTheme === "electric-blue" ? "#34465c"
        : selectedTheme === "ember" ? "#4a382c" : "#343b50"

    readonly property color accent: selectedTheme === "electric-blue" ? "#55a8f5"
        : selectedTheme === "ember" ? "#d58b59" : "#8174e8"
    readonly property color accentHover: selectedTheme === "electric-blue" ? "#7bbcff"
        : selectedTheme === "ember" ? "#e6a875" : "#978cf2"
    readonly property color accentPressed: selectedTheme === "electric-blue" ? "#378bd8"
        : selectedTheme === "ember" ? "#bd7041" : "#6e61d4"

    readonly property color textPrimary: "#f4f5fb"
    readonly property color textSecondary: selectedTheme === "ember" ? "#c9b9ad" : "#b5bbcd"
    readonly property color textMuted: selectedTheme === "ember" ? "#a99789" : "#929bb1"
    readonly property color textSubtle: selectedTheme === "ember" ? "#817064" : "#777f95"

    readonly property color success: "#8bd6ad"
    readonly property color warning: "#e8c27f"
    readonly property color warningText: "#f3d59b"
    readonly property color warningSurface: "#5b4525"
    readonly property color neutralStatusSurface: "#3b3d4c"
    readonly property color error: "#ee929b"

    // Artwork overlays use the active theme's base tone instead of a fixed
    // violet-black tint, keeping cover treatments consistent across themes.
    readonly property color artworkFade: Qt.rgba(
        windowBackground.r, windowBackground.g, windowBackground.b, 0.8)
    readonly property color artworkOverlay: Qt.rgba(
        windowBackground.r, windowBackground.g, windowBackground.b, 0.6)

    readonly property int spacingTight: 6
    readonly property int spacingNormal: 12
    readonly property int spacingLarge: 20
    readonly property int spacingPage: 30

    readonly property int radiusSmall: 6
    readonly property int radiusCard: 12
    readonly property int radiusLarge: 16
}