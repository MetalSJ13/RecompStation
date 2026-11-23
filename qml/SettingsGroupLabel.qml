import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Small uppercase heading above a group of Settings rows.
Label {
    Layout.fillWidth: true
    Layout.topMargin: 14
    Layout.bottomMargin: 2
    color: theme.textMuted
    font.pixelSize: 11 * theme.fontScale
    font.bold: true
    font.letterSpacing: 1.4
}
