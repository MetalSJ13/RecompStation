import QtQuick
import QtQuick.Controls

// A controller-friendly "◀ value ▶" selector used by the Settings rows.
// Left/right (D-pad, arrow keys) or clicking the arrows steps through
// `options`; Enter/A also steps forward. `picked(value)` fires on each step;
// the owner saves it, and `value` (bound to the setting) follows.
Item {
    id: cycler

    // [{ label: "Medium", value: "medium" }, ...]
    property var options: []
    property var value
    property bool wrap: false
    property int preferredWidth: 240
    signal picked(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === value)
                return i
        }
        return -1
    }
    readonly property string currentLabel: currentIndex >= 0 ? options[currentIndex].label : ""
    readonly property bool canStepBack: options.length > 1 && (wrap || currentIndex > 0)
    readonly property bool canStepForward: options.length > 1
        && (wrap || currentIndex < options.length - 1)

    // Main.qml's controller routing looks for this function to turn
    // left/right into value changes instead of focus movement.
    function stepValue(delta) {
        if (!enabled || options.length === 0)
            return false
        let next = currentIndex < 0 ? 0 : currentIndex + delta
        if (wrap)
            next = (next + options.length) % options.length
        else
            next = Math.max(0, Math.min(options.length - 1, next))
        if (next === currentIndex)
            return false
        // Owners bind `value` to their setting and update it in picked().
        picked(options[next].value)
        return true
    }

    implicitWidth: preferredWidth
    implicitHeight: 42
    activeFocusOnTab: true
    opacity: enabled ? 1.0 : 0.45

    Accessible.role: Accessible.SpinBox
    Accessible.name: currentLabel

    Keys.onLeftPressed: (event) => { cycler.stepValue(-1); event.accepted = true }
    Keys.onRightPressed: (event) => { cycler.stepValue(1); event.accepted = true }
    Keys.onReturnPressed: (event) => { cycler.stepValue(1); event.accepted = true }
    Keys.onEnterPressed: (event) => { cycler.stepValue(1); event.accepted = true }
    Keys.onSpacePressed: (event) => { cycler.stepValue(1); event.accepted = true }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: cycler.activeFocus ? theme.elevatedSurface : theme.surface
        border.width: cycler.activeFocus ? 2 : 0
        border.color: theme.accent
    }

    Item {
        id: backArrow
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 38
        Label {
            anchors.centerIn: parent
            text: "◀"
            color: cycler.canStepBack ? theme.accent : theme.textSubtle
            font.pixelSize: 13 * theme.fontScale
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: { cycler.forceActiveFocus(); cycler.stepValue(-1) }
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * backArrow.width
        spacing: 4
        Label {
            width: parent.width
            text: cycler.currentLabel
            color: theme.textPrimary
            font.pixelSize: 14 * theme.fontScale
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 4
            visible: cycler.options.length > 1 && cycler.options.length <= 10
            Repeater {
                model: cycler.options.length
                Rectangle {
                    required property int index
                    width: index === cycler.currentIndex ? 14 : 5
                    height: 3
                    radius: 1.5
                    color: index === cycler.currentIndex ? theme.accent : theme.controlHover
                    Behavior on width {
                        enabled: !settingsManager.reducedMotion
                        NumberAnimation { duration: 120 }
                    }
                }
            }
        }
    }

    Item {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: backArrow.width
        Label {
            anchors.centerIn: parent
            text: "▶"
            color: cycler.canStepForward ? theme.accent : theme.textSubtle
            font.pixelSize: 13 * theme.fontScale
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: { cycler.forceActiveFocus(); cycler.stepValue(1) }
        }
    }
}
