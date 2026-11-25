import QtQuick
import QtTest
import "../../qml" as App

// Loads the controller drawing for every layout, so a QML error in it (such
// as a property given the wrong type) fails the tests instead of stopping
// the app at startup.
Item {
    id: root
    width: 400
    height: 300

    // The colors ControllerOutline reads from the app's theme.
    QtObject {
        id: theme
        property color textSecondary: "#b5bbcd"
        property color textPrimary: "#f4f5fb"
        property color accent: "#8174e8"
        property color surface: "#222635"
        property color elevatedSurface: "#2a3042"
        property color sidebarBackground: "#191c27"
    }

    Component {
        id: outlineComponent
        App.ControllerOutline {
            width: 206
            height: 136
        }
    }

    TestCase {
        name: "ControllerOutline"
        when: windowShown

        function test_everyLayoutLoads_data() {
            return [
                { tag: "xbox", kind: "xbox" },
                { tag: "playstation", kind: "playstation" },
                { tag: "nintendo", kind: "nintendo" },
                { tag: "generic", kind: "generic" }
            ]
        }

        function test_everyLayoutLoads(data) {
            const outline = createTemporaryObject(outlineComponent, root,
                                                  { kind: data.kind, activeInputs: ["a", "dpad-up", "leftstick"] })
            verify(outline !== null, "ControllerOutline failed to load for " + data.kind)
            compare(outline.filledDrawing, data.kind === "xbox" || data.kind === "playstation")
            verify(outline.isLit(["a"]))
            verify(!outline.isLit(["b"]))
        }
    }
}
