import QtQuick
import QtQuick.Controls

// The Settings button style: a rounded surface tile with a DemiBold label,
// an accent ring on keyboard or controller focus, and an optional leading
// glyph in the accent color ("+" on Add). Disabled buttons fade.
Button {
    id: control

    property string glyph: ""
    property bool compact: false
    // The main action of a dialog or row: filled with the accent.
    property bool primary: false
    // For dialogs read from the couch: the focused button grows a little
    // and gets a thick light ring with an accent glow outside it.
    property bool prominentFocus: false

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    leftPadding: compact ? 12 : 16
    rightPadding: compact ? 12 : 16
    implicitHeight: compact ? 34 : 38
    Accessible.name: text
    scale: prominentFocus && activeFocus ? 1.05 : 1
    Behavior on scale {
        enabled: !settingsManager.reducedMotion
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    background: Rectangle {
        radius: 9
        color: control.primary ? (control.hovered || control.activeFocus ? theme.accentHover : theme.accent)
            : control.activeFocus ? theme.elevatedSurface
            : control.hovered ? theme.controlHover : theme.surface
        border.color: control.activeFocus
            ? (control.primary || control.prominentFocus ? theme.textPrimary : theme.accent) : theme.divider
        border.width: control.activeFocus ? (control.prominentFocus ? 3 : 2) : 1
        opacity: control.enabled ? 1 : 0.5

        Rectangle {
            visible: control.prominentFocus
            anchors.fill: parent
            anchors.margins: -5
            radius: parent.radius + 5
            color: "transparent"
            border.width: 2
            border.color: theme.accent
            opacity: control.activeFocus ? 0.55 : 0
            Behavior on opacity {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 140 }
            }
        }
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
            color: control.primary ? theme.accentText : theme.textPrimary
            font.pixelSize: 13 * theme.fontScale
            font.weight: Font.DemiBold
        }
    }
}
