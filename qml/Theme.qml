import QtQuick

QtObject {
    readonly property string selectedTheme: window.pendingThemeName
    readonly property real fontScale: settingsManager.fontSize === "small" ? 0.9
        : settingsManager.fontSize === "large" ? 1.2 : 1.0

    // Each curated theme defines a complete set of coordinated surfaces,
    // accents and text colors. Settings > Appearance lists `presets` in order.
    // Text tokens meet WCAG AA (4.5:1) where they are used: textMuted on
    // raised panels and controls, the accent as text on cards (2026-11-24).
    // textSubtle is only for disabled controls, which WCAG exempts.
    readonly property var palettes: ({
        "electric-blue": {
            backgroundTop: "#172b40", windowBackground: "#101722", backgroundBottom: "#0b111b",
            sidebarBackground: "#172231", surface: "#202e40", elevatedSurface: "#293b51",
            controlBackground: "#30445d", controlHover: "#3b526d", divider: "#34465c",
            accent: "#55a8f5", accentHover: "#7bbcff", accentPressed: "#378bd8", accentText: "#0b111b",
            textSecondary: "#b5bbcd", textMuted: "#a8afc1", textSubtle: "#777f95"
        },
        "midnight-violet": {
            backgroundTop: "#171a2a", windowBackground: "#11131b", backgroundBottom: "#0d0f17",
            sidebarBackground: "#191c27", surface: "#222635", elevatedSurface: "#2a3042",
            controlBackground: "#30374b", controlHover: "#3b435a", divider: "#343b50",
            accent: "#968cec", accentHover: "#aca4f2", accentPressed: "#8174e8", accentText: "#0d0f17",
            textSecondary: "#b5bbcd", textMuted: "#97a0b5", textSubtle: "#777f95"
        },
        "aurora": {
            backgroundTop: "#123036", windowBackground: "#0c1a1c", backgroundBottom: "#081214",
            sidebarBackground: "#11252a", surface: "#173239", elevatedSurface: "#1f3c45",
            controlBackground: "#274952", controlHover: "#305661", divider: "#2b4a52",
            accent: "#3fd8b8", accentHover: "#6ee7cd", accentPressed: "#26b899", accentText: "#06201b",
            textSecondary: "#b2c6c4", textMuted: "#9fb5b4", textSubtle: "#6f8886"
        },
        "carbon": {
            backgroundTop: "#1c1d21", windowBackground: "#111214", backgroundBottom: "#0a0a0c",
            sidebarBackground: "#18191c", surface: "#212328", elevatedSurface: "#2a2d33",
            controlBackground: "#33363d", controlHover: "#3e424a", divider: "#373a41",
            accent: "#ff5c6c", accentHover: "#ff8592", accentPressed: "#e0404f", accentText: "#1a0a0c",
            textSecondary: "#bdbfc6", textMuted: "#9c9ea7", textSubtle: "#787a83"
        }
    })
    readonly property var presets: [
        { id: "electric-blue", title: "Electric Blue", description: "Cool layered blues, electric accent" },
        { id: "midnight-violet", title: "Midnight Violet", description: "Deep ink with violet highlights" },
        { id: "aurora", title: "Aurora", description: "Deep teal night, mint accent" },
        { id: "carbon", title: "Carbon", description: "Neutral graphite, crimson accent" }
    ]
    readonly property var colors: palettes[selectedTheme] || palettes["midnight-violet"]
    function paletteFor(themeId) { return palettes[themeId] || palettes["midnight-violet"] }

    readonly property color backgroundTop: colors.backgroundTop
    readonly property color windowBackground: colors.windowBackground
    readonly property color backgroundBottom: colors.backgroundBottom

    readonly property color sidebarBackground: colors.sidebarBackground
    readonly property color surface: colors.surface
    readonly property color elevatedSurface: colors.elevatedSurface
    readonly property color controlBackground: colors.controlBackground
    readonly property color controlHover: colors.controlHover
    readonly property color divider: colors.divider

    readonly property color accent: colors.accent
    readonly property color accentHover: colors.accentHover
    readonly property color accentPressed: colors.accentPressed
    // Text drawn on top of the accent color (primary buttons, selected pills).
    readonly property color accentText: colors.accentText

    readonly property color textPrimary: "#f4f5fb"
    readonly property color textSecondary: colors.textSecondary
    readonly property color textMuted: colors.textMuted
    readonly property color textSubtle: colors.textSubtle

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