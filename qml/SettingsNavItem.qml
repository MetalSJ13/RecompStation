import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A Settings category in the left rail: icon, name and a one-line summary
// of the section's current values.
Button {
    id: item

    property string iconSource
    property string summary
    property bool current: false

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    highlighted: current
    implicitHeight: 60
    padding: 12
    Accessible.name: text + (summary.length > 0 ? ", " + summary : "")

    background: Rectangle {
        radius: 12
        color: item.current ? theme.surface
            : item.hovered ? Qt.lighter(theme.windowBackground, 1.25) : "transparent"
        border.width: item.activeFocus ? 2 : (item.current ? 1 : 0)
        border.color: item.activeFocus ? theme.accent : theme.divider
    }

    contentItem: RowLayout {
        spacing: 12
        Rectangle {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            radius: 9
            color: item.current ? theme.accent : theme.surface
            Image {
                anchors.centerIn: parent
                source: item.iconSource
                sourceSize.width: 18
                sourceSize.height: 18
                opacity: item.current ? 1.0 : 0.8
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: item.text
                color: item.current ? theme.textPrimary : theme.textSecondary
                font.pixelSize: 14 * theme.fontScale
                font.weight: item.current ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                visible: item.summary.length > 0
                text: item.summary
                color: theme.textMuted
                font.pixelSize: 11.5 * theme.fontScale
                elide: Text.ElideRight
            }
        }
    }
}
