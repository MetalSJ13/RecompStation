import QtQuick
import QtQuick.Controls

// The Settings button style: a rounded surface tile with a DemiBold label,
// an accent ring on keyboard or controller focus, and an optional leading
// glyph in the accent color ("+" on Add). Disabled buttons fade.
Button {
    id: control

    property string glyph: ""
    property bool compact: false

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    leftPadding: compact ? 12 : 16
    rightPadding: compact ? 12 : 16
    implicitHeight: compact ? 34 : 38
    Accessible.name: text

    background: Rectangle {
        radius: 9
        color: control.activeFocus ? theme.elevatedSurface
            : control.hovered ? theme.controlHover : theme.surface
        border.color: control.activeFocus ? theme.accent : theme.divider
        border.width: control.activeFocus ? 2 : 1
        opacity: control.enabled ? 1 : 0.5
    }
    contentItem: Row {
        spacing: 7
        opacity: control.enabled ? 1 : 0.5
        Label {
            visible: control.glyph.length > 0
            anchors.verticalCenter: parent.verticalCenter
            text: control.glyph
            color: theme.accent
            font.pixelSize: 15 * theme.fontScale
            font.bold: true
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: theme.textPrimary
            font.pixelSize: 13 * theme.fontScale
            font.weight: Font.DemiBold
        }
    }
}
