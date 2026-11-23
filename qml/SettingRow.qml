import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One Settings row: title and description on the left, a control on the
// right. The row lights up while its control has focus, and clicking
// anywhere on the row focuses the control.
Rectangle {
    id: row

    property string title
    property string description
    property bool dimmed: false
    default property alias control: controlSlot.data
    readonly property Item controlItem: controlSlot.children.length > 0 ? controlSlot.children[0] : null
    readonly property bool focusWithin: controlItem !== null && controlItem.activeFocus

    Layout.fillWidth: true
    implicitHeight: Math.max(64, textColumn.implicitHeight + 24, controlSlot.childrenRect.height + 20)
    radius: 12
    color: focusWithin ? Qt.lighter(theme.sidebarBackground, 1.12) : theme.sidebarBackground
    border.width: focusWithin ? 2 : 0
    border.color: theme.accent
    opacity: dimmed ? 0.5 : 1.0

    MouseArea {
        anchors.fill: parent
        onClicked: if (row.controlItem) row.controlItem.forceActiveFocus()
    }

    ColumnLayout {
        id: textColumn
        anchors.left: parent.left
        anchors.right: controlSlot.left
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        Label {
            Layout.fillWidth: true
            text: row.title
            color: theme.textPrimary
            font.pixelSize: 14.5 * theme.fontScale
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            visible: row.description.length > 0
            text: row.description
            color: theme.textMuted
            font.pixelSize: 12 * theme.fontScale
            wrapMode: Text.Wrap
        }
    }

    Item {
        id: controlSlot
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        width: childrenRect.width
        height: childrenRect.height
    }
}
