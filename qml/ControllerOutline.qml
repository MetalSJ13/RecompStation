import QtQuick
import QtQuick.Shapes
import "ControllerShapes.js" as ControllerShapes

// Line drawing of a controller layout ("xbox", "playstation", "nintendo" or
// "generic"), in theme colors and without logos. Elements whose input token
// is in activeInputs light up, for the Settings button test.
Item {
    id: root

    property string kind: "generic"
    property var activeInputs: []
    property color lineColor: theme.textSecondary
    property color accentColor: theme.accent
    property color litColor: theme.accent
    property real lineWidth: 2.2   // in the 200 x 130 drawing

    readonly property real drawingScale: Math.min(width / 206, height / 136)
    implicitWidth: 206
    implicitHeight: 136

    function isLit(tokens) {
        for (let i = 0; i < tokens.length; ++i) {
            if (root.activeInputs.indexOf(tokens[i]) >= 0)
                return true
        }
        return false
    }

    // Multisampling keeps the thin strokes smooth at any size.
    layer.enabled: true
    layer.samples: 4

    Item {
        width: 206
        height: 136
        anchors.centerIn: parent
        scale: root.drawingScale

        Repeater {
            model: ControllerShapes.elements(root.kind)
            delegate: Shape {
                required property var modelData
                readonly property bool lit: modelData.tokens.length > 0 && root.isLit(modelData.tokens)
                x: 3
                y: 3
                width: 200
                height: 130
                ShapePath {
                    strokeColor: parent.lit ? root.litColor
                        : modelData.role === "accent" ? root.accentColor : root.lineColor
                    // In drawing units, so lines scale with the drawing.
                    strokeWidth: modelData.role === "body" ? root.lineWidth : root.lineWidth * 0.8
                    fillColor: parent.lit && modelData.role !== "body"
                        ? Qt.rgba(root.litColor.r, root.litColor.g, root.litColor.b, 0.45) : "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: modelData.path }
                }
            }
        }
    }
}
