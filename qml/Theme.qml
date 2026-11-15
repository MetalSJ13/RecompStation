import QtQuick

QtObject {
    // RecompStation's initial dark visual foundation.
    // Keep these tokens centralized so curated themes can be added later.

    readonly property color windowBackground: "#11131b"
    readonly property color sidebarBackground: "#191c27"
    readonly property color surface: "#222635"
    readonly property color elevatedSurface: "#2a3042"
    readonly property color controlBackground: "#30374b"
    readonly property color divider: "#343b50"

    readonly property color accent: "#8174e8"
    readonly property color accentHover: "#978cf2"
    readonly property color accentPressed: "#6e61d4"

    readonly property color textPrimary: "#f4f5fb"
    readonly property color textSecondary: "#b5bbcd"
    readonly property color textMuted: "#929bb1"
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