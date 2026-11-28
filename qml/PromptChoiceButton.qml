import QtQuick
import QtQuick.Controls

// A choice in a short prompt (Install this launcher? Yes / No), drawn so the
// focused one is obvious from the couch: it grows a little and gets a thick
// light ring, while the other stays flat. Yes is filled with the accent.
Button {
    id: control

    property bool primary: false
    // A mark drawn in the label's colour, such as ✓ or ✕.
    property string glyph: ""

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: 46
    leftPadding: 22
    rightPadding: 22
    Accessible.name: text
    scale: activeFocus ? 1.06 : 1
    Behavior on scale {
        enabled: !settingsManager.reducedMotion
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    contentItem: Row {
        spacing: 8
        Label {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.glyph.length > 0
            text: control.glyph
            color: control.primary ? theme.accentText : theme.textPrimary
            font.pixelSize: 15 * theme.fontScale
            font.bold: true
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.primary ? theme.accentText : theme.textPrimary
            font.pixelSize: 14 * theme.fontScale
            font.weight: Font.DemiBold
        }
    }

    background: Rectangle {
        implicitWidth: 104
        radius: height / 2
        color: control.primary
            ? (control.down ? theme.accentPressed : control.hovered || control.activeFocus ? theme.accentHover : theme.accent)
            : (control.activeFocus ? theme.elevatedSurface : control.hovered ? theme.controlHover : theme.controlBackground)
        border.width: control.activeFocus ? 3 : 1
        border.color: control.activeFocus ? theme.textPrimary : theme.divider
        opacity: control.enabled ? 1 : 0.5

        // An outer glow ring in the accent, outside the button's edge.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -5
            radius: height / 2
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
}
