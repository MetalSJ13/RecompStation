import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A yes-or-no question in the app's own style (Uninstall…?, Switch to the
// Windows launcher?). It sits above everything, Game Details included, and
// keeps the controller and keyboard inside it until it is answered: the
// buttons take focus, B / Esc means Cancel. Cancel is focused first, so
// pressing A by accident never removes anything.
Popup {
    id: dialog

    property string title: ""
    property string message: ""
    property string confirmText: "OK"
    property string cancelText: "Cancel"
    // Draws the confirm button in red, for actions that remove something.
    property bool destructive: false
    // Which button has focus when the dialog opens.
    property bool focusConfirm: false

    signal accepted()
    signal rejected()

    function accept() {
        dialog.close()
        dialog.accepted()
    }
    function reject() {
        dialog.close()
        dialog.rejected()
    }
    // B / Esc.
    function back() {
        reject()
    }
    function focusFirst() {
        Qt.callLater(function() {
            (dialog.focusConfirm ? confirmButton : cancelButton).forceActiveFocus()
        })
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    z: 500
    width: Math.min(560, (parent ? parent.width : 560) - 48)
    modal: true
    dim: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose
    onOpened: {
        uiSounds.play("menu")
        focusFirst()
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0.02, 0.03, 0.05, 0.62) }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: settingsManager.reducedMotion ? 0 : 160; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: settingsManager.reducedMotion ? 0 : 200; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: settingsManager.reducedMotion ? 0 : 120 }
    }

    background: Rectangle {
        radius: 18
        color: theme.surface
        border.color: theme.divider
        border.width: 1
    }

    contentItem: FocusScope {
        id: panel
        implicitHeight: content.implicitHeight + 52
        Keys.onEscapePressed: function(event) {
            event.accepted = true
            uiSounds.play("back")
            dialog.back()
        }
        // Enter answers with the focused button, like A on a controller.
        function clickFocused(event) {
            event.accepted = true
            if (confirmButton.activeFocus)
                confirmButton.clicked()
            else
                cancelButton.clicked()
        }
        Keys.onReturnPressed: function(event) { clickFocused(event) }
        Keys.onEnterPressed: function(event) { clickFocused(event) }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 28
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: dialog.title
                color: theme.textPrimary
                font.pixelSize: 22 * theme.fontScale
                font.bold: true
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: dialog.message
                visible: text.length > 0
                color: theme.textSecondary
                font.pixelSize: 14 * theme.fontScale
                lineHeight: 1.15
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 16

                Item { Layout.fillWidth: true }
                SettingsButton {
                    id: cancelButton
                    prominentFocus: true
                    text: dialog.cancelText
                    KeyNavigation.right: confirmButton
                    onClicked: dialog.reject()
                }
                SettingsButton {
                    id: confirmButton
                    prominentFocus: true
                    primary: !dialog.destructive
                    danger: dialog.destructive
                    text: dialog.confirmText
                    KeyNavigation.left: cancelButton
                    onClicked: dialog.accept()
                }
            }
        }
    }
}
