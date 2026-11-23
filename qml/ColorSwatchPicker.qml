import QtQuick
import QtQuick.Controls

// A row of color swatches that behaves like ValueCycler: left/right (D-pad,
// arrow keys) move the selection, and a click picks a swatch directly.
// options: [{ label: "Ocean", value: "ocean", color: "#3d8bff" }, ...]
Item {
    id: picker

    property var options: []
    property var value
    signal picked(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === value)
                return i
        }
        return -1
    }
    readonly property bool canStepBack: currentIndex > 0
    readonly property bool canStepForward: currentIndex < options.length - 1

    // Used by Main.qml's controller routing, as for ValueCycler.
    function stepValue(delta) {
        if (!enabled || options.length === 0)
            return false
        const next = Math.max(0, Math.min(options.length - 1,
            (currentIndex < 0 ? 0 : currentIndex + delta)))
        if (next === currentIndex)
            return false
        picked(options[next].value)
        return true
    }

    implicitWidth: swatchRow.implicitWidth + 16
    implicitHeight: swatchRow.implicitHeight + 12
    activeFocusOnTab: true
    Accessible.role: Accessible.List
    Accessible.name: currentIndex >= 0 ? options[currentIndex].label : ""

    Keys.onLeftPressed: (event) => { picker.stepValue(-1); event.accepted = true }
    Keys.onRightPressed: (event) => { picker.stepValue(1); event.accepted = true }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: picker.activeFocus ? theme.elevatedSurface : "transparent"
        border.width: picker.activeFocus ? 2 : 0
        border.color: theme.accent
    }

    Row {
        id: swatchRow
        anchors.centerIn: parent
        spacing: 10
        Repeater {
            model: picker.options
            delegate: Column {
                required property var modelData
                required property int index
                readonly property bool selected: index === picker.currentIndex
                spacing: 4
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 34
                    height: 34
                    radius: 17
                    color: modelData.color
                    border.width: parent.selected ? 3 : 0
                    border.color: theme.textPrimary
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: width / 2
                        visible: parent.parent.selected
                        color: "transparent"
                        border.width: 2
                        border.color: theme.windowBackground
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            picker.forceActiveFocus()
                            picker.picked(modelData.value)
                        }
                    }
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.label
                    color: parent.selected ? theme.textPrimary : theme.textMuted
                    font.pixelSize: 10.5 * theme.fontScale
                }
            }
        }
    }
}
