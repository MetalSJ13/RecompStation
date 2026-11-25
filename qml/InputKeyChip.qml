import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// One button or key in a hint: a round chip for face buttons, a rounded
// box for everything else. PlayStation face buttons (✕ ○ □ △) are drawn as
// shapes, because the font characters sit at different heights and never
// centre in the circle; other keys are text.
Rectangle {
    id: chip

    property string key: ""          // text shown ("A", "LB / RB", "Esc")
    property string token: ""        // controller input ("a", "b", "x", "y", …)
    property color keyColor: theme.textSecondary
    property bool round: false
    property real size: 23
    property bool bare: false        // no fill or border, for use inside another chip
    property bool strikeout: false

    readonly property bool drawsSymbol: round && window.controllerLayout === "playstation"
        && ["a", "b", "x", "y"].indexOf(token) >= 0

    implicitWidth: bare && !drawsSymbol ? keyLabel.implicitWidth : Math.max(size - 1, keyLabel.implicitWidth + 12)
    implicitHeight: size
    radius: round ? size / 2 : 5
    color: bare ? "transparent" : round ? Qt.rgba(0.22, 0.68, 0.50, 0.16) : Qt.rgba(0.58, 0.68, 0.82, 0.12)
    border.color: keyColor
    border.width: bare ? 0 : 1

    Label {
        id: keyLabel
        anchors.centerIn: parent
        visible: !chip.drawsSymbol
        text: chip.key
        color: chip.keyColor
        font.pixelSize: Math.round(chip.size * (chip.bare ? 0.52 : 0.43)) * theme.fontScale
        font.weight: Font.Bold
        font.strikeout: chip.strikeout
    }

    // The symbol, centred, in a box 40% of the chip.
    Shape {
        id: symbol
        visible: chip.drawsSymbol
        anchors.centerIn: parent
        width: chip.size
        height: chip.size
        // Smooth edges at this small size.
        layer.enabled: chip.drawsSymbol
        layer.samples: 4
        readonly property real c: chip.size / 2
        readonly property real s: chip.size * 0.2
        ShapePath {
            strokeColor: chip.keyColor
            strokeWidth: Math.max(1.4, chip.size * 0.072)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: {
                    const c = symbol.c
                    const s = symbol.s
                    switch (chip.token) {
                    case "a":   // cross
                        return "M " + (c - s) + " " + (c - s) + " L " + (c + s) + " " + (c + s)
                            + " M " + (c + s) + " " + (c - s) + " L " + (c - s) + " " + (c + s)
                    case "b": { // circle
                        const r = s * 1.05
                        return "M " + (c - r) + " " + c + " A " + r + " " + r + " 0 1 0 " + (c + r) + " " + c
                            + " A " + r + " " + r + " 0 1 0 " + (c - r) + " " + c
                    }
                    case "x": { // square
                        const h = s * 0.95
                        return "M " + (c - h) + " " + (c - h) + " L " + (c + h) + " " + (c - h)
                            + " L " + (c + h) + " " + (c + h) + " L " + (c - h) + " " + (c + h) + " Z"
                    }
                    case "y":   // triangle, centred by its middle, not its base
                        return "M " + c + " " + (c - s * 1.15) + " L " + (c + s * 1.1) + " " + (c + s * 0.75)
                            + " L " + (c - s * 1.1) + " " + (c + s * 0.75) + " Z"
                    default:
                        return ""
                    }
                }
            }
        }
    }
}
