import QtQuick
import QtQuick.Controls

// On/off switch in the theme's colors: a pill track that fills with the
// accent when on, a round knob that slides, and an accent focus ring for
// controller and keyboard navigation. Toggles with A / Enter / Space or a click.
Switch {
    id: control

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    padding: 4
    implicitWidth: 60
    implicitHeight: 36
    text: ""

    indicator: Rectangle {
        id: track
        x: control.leftPadding + (control.availableWidth - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 48
        height: 26
        radius: height / 2
        color: control.checked ? theme.accent : theme.controlBackground
        border.width: control.checked ? 0 : 1
        border.color: theme.divider
        opacity: control.enabled ? 1.0 : 0.45
        Behavior on color {
            enabled: !settingsManager.reducedMotion
            ColorAnimation { duration: 140 }
        }

        Rectangle {
            id: knob
            width: 20
            height: 20
            radius: 10
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? theme.accentText : theme.textSecondary
            Behavior on x {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
        }

        // Focus ring just outside the track.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: height / 2
            color: "transparent"
            border.width: 2
            border.color: theme.accent
            visible: control.visualFocus || control.activeFocus
        }
    }

    background: Item {}
    contentItem: Item {}
}
