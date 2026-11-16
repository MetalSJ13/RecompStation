import QtQuick

QtObject {
    readonly property string selectedTheme: settingsManager.themeName

    readonly property color windowBackground: selectedTheme === "electric-blue" ? "#101722"
        : selectedTheme === "ember" ? "#191411" : "#11131b"
    readonly property color sidebarBackground: selectedTheme === "electric-blue" ? "#172231"
        : selectedTheme === "ember" ? "#241b16" : "#191c27"
    readonly property color surface: selectedTheme === "electric-blue" ? "#202e40"
        : selectedTheme === "ember" ? "#30231c" : "#222635"
    readonly property color elevatedSurface: selectedTheme === "electric-blue" ? "#293b51"
        : selectedTheme === "ember" ? "#3b2b22" : "#2a3042"
    readonly property color controlBackground: selectedTheme === "electric-blue" ? "#30445d"
        : selectedTheme === "ember" ? "#493326" : "#30374b"
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
    readonly property color success: "#8bd6ad"
    readonly property color warning: "#e8c27f"
    readonly property color error: "#ee929b"

    readonly property int spacingTight: 6
    readonly property int spacingNormal: 12
    readonly property int spacingLarge: 20
    readonly property int spacingPage: 30

    readonly property int radiusSmall: 6
    readonly property int radiusCard: 12
    readonly property int radiusLarge: 16
}