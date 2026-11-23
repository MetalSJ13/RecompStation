import QtQuick
import QtQuick.Controls

// Full-window screenshot viewer. open() with a list of screenshots and a
// starting index; Left/Right (D-pad, keyboard, or the edge buttons) page
// through them; B, Escape, or the close button return. Main.qml routes
// controller input here while it is visible. themeTokens is the app Theme.
Popup {
    id: root

    property var sources: []
    property int index: 0
    property QtObject themeTokens: null
    property bool reducedMotion: false
    // Item to refocus when the viewer closes (the thumbnail that opened it).
    property Item returnFocusItem: null
    signal shown(int index)

    function show(screenshots, startIndex, focusItem) {
        sources = screenshots || []
        if (sources.length === 0)
            return
        index = Math.max(0, Math.min(sources.length - 1, startIndex))
        returnFocusItem = focusItem || null
        open()
    }
    function step(delta) {
        if (sources.length < 2)
            return
        index = (index + delta + sources.length) % sources.length
    }

    onIndexChanged: if (visible) shown(index)
    onOpened: {
        shown(index)
        viewerContent.forceActiveFocus()
    }
    onClosed: {
        if (returnFocusItem)
            returnFocusItem.forceActiveFocus()
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.reducedMotion ? 0 : 200 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: root.reducedMotion ? 0 : 160 }
    }

    background: Rectangle {
        color: Qt.rgba(0.02, 0.03, 0.05, 0.95)
    }

    component EdgeButton: Button {
        id: edgeButton
        property string glyph: ""
        focusPolicy: Qt.NoFocus
        width: 56
        height: 56
        background: Rectangle {
            radius: width / 2
            color: edgeButton.hovered ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.12)
        }
        contentItem: Label {
            text: edgeButton.glyph
            color: "white"
            font.pixelSize: 26
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }


    contentItem: Item {
        id: viewerContent
        focus: true
        Keys.onLeftPressed: root.step(-1)
        Keys.onRightPressed: root.step(1)

        Image {
            id: viewerImage
            anchors.fill: parent
            anchors.margins: 72
            anchors.bottomMargin: 88
            source: root.sources.length > 0 ? root.sources[root.index] : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            smooth: true
            mipmap: true
        }

        // Clicking the dimmed area outside the image closes the viewer.
        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: root.close()
        }

        EdgeButton {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: root.sources.length > 1
            glyph: "‹"
            Accessible.name: "Previous screenshot"
            onClicked: root.step(-1)
        }
        EdgeButton {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: root.sources.length > 1
            glyph: "›"
            Accessible.name: "Next screenshot"
            onClicked: root.step(1)
        }
        EdgeButton {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            glyph: "×"
            Accessible.name: "Close screenshot"
            onClicked: root.close()
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 22
            height: 40
            width: viewerHint.implicitWidth + 36
            radius: 20
            color: Qt.rgba(1, 1, 1, 0.08)
            border.color: Qt.rgba(1, 1, 1, 0.16)
            Label {
                id: viewerHint
                anchors.centerIn: parent
                text: (root.sources.length > 1
                        ? (root.index + 1) + " of " + root.sources.length + "     ◀ ▶  Browse     " : "")
                    + "B / Esc  Close"
                color: root.themeTokens ? root.themeTokens.textSecondary : "#b5bbcd"
                font.pixelSize: 12
            }
        }
    }
}
