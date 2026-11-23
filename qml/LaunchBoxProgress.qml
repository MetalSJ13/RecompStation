import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Progress of a LaunchBox catalog download and rebuild: which stage it is in
// (downloading, unpacking, building the catalog), a progress bar (moving
// stripe when the size is not known yet), Cancel, and errors with Retry.
// Reads the state kept in Main.qml (window.launchBox*).
ColumnLayout {
    id: progress

    readonly property string stage: window.launchBoxStage
    readonly property bool active: launchBoxCatalogManager.busy
    // 0..1, or -1 while the amount of work is unknown.
    readonly property real fraction: {
        if (stage === "downloading")
            return window.launchBoxTotalBytes > 0
                ? 0.5 * window.launchBoxReceivedBytes / window.launchBoxTotalBytes : -1
        if (stage === "extracting")
            return -1
        if (stage === "indexing")
            return 0.6 + 0.4 * window.launchBoxProgressPercent / 100
        if (stage === "ready")
            return 1
        return 0
    }
    readonly property string label: {
        switch (stage) {
        case "downloading":
            return window.launchBoxTotalBytes > 0
                ? "Step 1 of 3 · Downloading · " + window.formatBytes(window.launchBoxReceivedBytes)
                    + " of " + window.formatBytes(window.launchBoxTotalBytes)
                : "Step 1 of 3 · Downloading · " + window.formatBytes(window.launchBoxReceivedBytes)
        case "extracting":
        {
            const unpacked = /\(([^)]*)\)/.exec(window.launchBoxStatusMessage)
            return "Step 2 of 3 · Unpacking the catalog" + (unpacked ? " · " + unpacked[1] + " so far" : "")
        }
        case "indexing":
            return "Step 3 of 3 · Building the catalog for your systems · " + window.launchBoxProgressPercent + "%"
        case "ready":
            return "Catalog ready."
        case "failed":
            return window.launchBoxStatusMessage
        case "cancelled":
            return "Cancelled. Any catalog you had before is unchanged."
        }
        return ""
    }

    visible: active || stage === "failed" || stage === "cancelled"
    spacing: 8

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 8
        radius: 4
        color: theme.surface
        clip: true
        visible: progress.active
        Rectangle {
            visible: progress.fraction >= 0
            width: parent.width * Math.max(0, Math.min(1, progress.fraction))
            height: parent.height
            radius: 4
            color: theme.accent
            Behavior on width {
                enabled: !settingsManager.reducedMotion
                NumberAnimation { duration: 300 }
            }
        }
        // Moving stripe while the size of the remaining work is unknown.
        Rectangle {
            id: stripe
            visible: progress.fraction < 0
            width: parent.width * 0.3
            height: parent.height
            radius: 4
            color: theme.accent
            SequentialAnimation on x {
                running: stripe.visible && !settingsManager.reducedMotion
                loops: Animation.Infinite
                NumberAnimation { from: -stripe.width; to: stripe.parent.width; duration: 1400; easing.type: Easing.InOutQuad }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        BusyIndicator {
            visible: progress.active
            running: visible
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
        }
        Label {
            Layout.fillWidth: true
            text: progress.label
            color: progress.stage === "failed" ? theme.error : theme.textSecondary
            font.pixelSize: 12.5 * theme.fontScale
            wrapMode: Text.Wrap
        }
        Button {
            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus
            visible: progress.active
            text: "Cancel"
            onClicked: launchBoxCatalogManager.cancelUpdate()
        }
        Button {
            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus
            visible: !progress.active && (progress.stage === "failed" || progress.stage === "cancelled")
            text: "Retry"
            onClicked: launchBoxCatalogManager.updateCatalog()
        }
    }
    Label {
        Layout.fillWidth: true
        visible: progress.active
        text: "This takes a few minutes. You can keep going; it finishes in the background."
        color: theme.textMuted
        font.pixelSize: 11.5 * theme.fontScale
        wrapMode: Text.Wrap
    }
}
