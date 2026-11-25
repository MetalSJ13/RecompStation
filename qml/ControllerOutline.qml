import QtQuick
import QtQuick.Shapes
import "ControllerShapes.js" as ControllerShapes

// Drawing of a controller layout ("xbox", "playstation", "nintendo" or
// "generic"), in theme colors and without logos. Elements whose input token
// is in activeInputs light up, for the Settings button test.
Item {
    id: root

    property string kind: "generic"
    property var activeInputs: []
    property color lineColor: theme.textSecondary
    property color accentColor: theme.accent
    property color litColor: theme.accent
    property color bodyColor: theme.elevatedSurface
    property color wellColor: theme.windowBackground
    property color partColor: theme.controlBackground
    property color labelColor: theme.textPrimary
    property real lineWidth: 2.2   // in the 200 x 130 drawing

    // Xbox and PlayStation are filled drawings; the others are line drawings.
    readonly property bool filledDrawing: kind === "xbox" || kind === "playstation"
    // The filled drawings start lower in their box (no shoulder buttons),
    // so they are moved up to sit centred.
    readonly property real drawingOffset: filledDrawing ? -6 : 0
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
                readonly property string role: modelData.role
                readonly property bool lit: modelData.tokens.length > 0 && root.isLit(modelData.tokens)
                x: 3
                y: 3 + root.drawingOffset
                width: 200
                height: 130
                ShapePath {
                    strokeColor: parent.role === "glyph" ? (parent.lit ? root.wellColor : root.labelColor)
                        : parent.lit ? root.litColor
                        : parent.role === "accent" ? root.accentColor : root.lineColor
                    // In drawing units, so lines scale with the drawing.
                    strokeWidth: !root.filledDrawing
                        ? (parent.role === "body" ? root.lineWidth : root.lineWidth * 0.8)
                        : parent.role === "body" ? root.lineWidth * 0.65
                        : parent.role === "trim" ? root.lineWidth * 0.29
                        : parent.role === "well" ? root.lineWidth * 0.38
                        : parent.role === "accent" ? root.lineWidth * 0.5
                        : root.lineWidth * 0.45
                    fillColor: !root.filledDrawing
                        ? (parent.lit && parent.role !== "body"
                           ? Qt.rgba(root.litColor.r, root.litColor.g, root.litColor.b, 0.45) : "transparent")
                        : parent.role === "body" ? root.bodyColor
                        : parent.role === "well" ? root.wellColor
                        : parent.role === "trim" || parent.role === "glyph" ? "transparent"
                        : parent.lit ? root.litColor
                        : root.filledDrawing ? root.partColor
                        : "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: modelData.path }
                }
            }
        }

        // Letters on face buttons (Xbox).
        Repeater {
            model: ControllerShapes.labels(root.kind)
            delegate: Text {
                required property var modelData
                readonly property bool lit: root.isLit(modelData.tokens)
                x: 3 + modelData.x - width / 2
                y: 3 + root.drawingOffset + modelData.y - height / 2
                text: modelData.text
                color: lit ? root.wellColor : root.labelColor
                // pixelSize takes whole numbers only: drawn at 12 px and
                // scaled to the drawing's 6.2.
                font.pixelSize: 12
                scale: 6.2 / 12
                font.bold: true
                renderType: Text.QtRendering
            }
        }
    }
}
