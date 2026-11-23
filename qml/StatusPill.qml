import QtQuick
import QtQuick.Controls

// Small rounded status label: tone is "good", "warning", "bad" or "neutral".
Rectangle {
    id: pill
    property alias text: label.text
    property string tone: "neutral"
    readonly property color toneColor: tone === "good" ? theme.success
        : tone === "warning" ? theme.warning
        : tone === "bad" ? theme.error : theme.textMuted

    implicitWidth: label.implicitWidth + 20
    implicitHeight: 24
    radius: 12
    color: Qt.rgba(toneColor.r, toneColor.g, toneColor.b, 0.14)

    Label {
        id: label
        anchors.centerIn: parent
        color: pill.toneColor
        font.pixelSize: 11 * theme.fontScale
        font.bold: true
    }
}
