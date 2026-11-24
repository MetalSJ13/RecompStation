.pragma library

// Pure cover-frame calculations shared by GameArtwork.qml and its tests.

// A measured image ratio more than this far (relative) from the platform's
// expected shape is treated as a different box format and keeps its own ratio.
// Within the tolerance the image is cropped to the platform frame, so this
// also caps how much of a cover can be trimmed (about 6% per edge).
var platformRatioTolerance = 0.12

// Typical North American box-art shapes (width / height). Returns 0 for
// platforms without a defined shape so callers fall back to the image.
function platformAspectRatio(platformName) {
    switch (platformName) {
    case "GB":
    case "GBC":
    case "GBA": return 0.92
    case "SNES":
    case "N64": return 7 / 5
    case "PS1": return 1
    case "GameCube":
    case "Wii":
    case "Wii U":
    case "PS2":
    case "Xbox":
    case "Xbox 360": return 5 / 7
    default: return 0
    }
}

// mode: "measured" follows the image; "platform" follows the platform shape
// unless the measured ratio clearly differs. measuredRatio is 0 when no image
// is loaded. viewportRatio is the final fallback.
function frameAspectRatio(mode, platformName, measuredRatio, viewportRatio) {
    if (mode === "platform") {
        var expected = platformAspectRatio(platformName)
        if (expected > 0) {
            if (measuredRatio > 0
                    && Math.abs(measuredRatio - expected) / expected > platformRatioTolerance)
                return measuredRatio
            return expected
        }
    }
    return measuredRatio > 0 ? measuredRatio : viewportRatio
}

// Largest frame of the given ratio that fits inside the available box.
function fitFrame(availableWidth, availableHeight, ratio) {
    var safeRatio = Math.max(0.01, ratio)
    return {
        width: Math.min(availableWidth, availableHeight * safeRatio),
        height: Math.min(availableHeight, availableWidth / safeRatio)
    }
}

// Name printed on a placeholder cover's band.
function placeholderSystemName(platformName) {
    var names = {
        "GB": "GAME BOY", "GBC": "GAME BOY COLOR", "GBA": "GAME BOY ADVANCE",
        "SNES": "SUPER NINTENDO", "N64": "NINTENDO 64", "GameCube": "GAMECUBE",
        "Wii": "WII", "Wii U": "WII U", "PS1": "PLAYSTATION", "PS2": "PLAYSTATION 2",
        "Xbox": "XBOX", "Xbox 360": "XBOX 360"
    }
    return names[platformName] || String(platformName || "").toUpperCase()
}

// Band colour of a placeholder cover: muted, one per system.
function placeholderBandColor(platformName) {
    var colors = {
        "GB": "#3c4a3a", "GBC": "#4a3a5c", "GBA": "#2f4a63", "SNES": "#4a3a6b",
        "N64": "#2f3a63", "GameCube": "#3a2f63", "Wii": "#4a4f5c", "Wii U": "#2f5263",
        "PS1": "#3b3f4f", "PS2": "#2a3550", "Xbox": "#2f4a36", "Xbox 360": "#3a5236"
    }
    return colors[platformName] || "#2a2f45"
}
