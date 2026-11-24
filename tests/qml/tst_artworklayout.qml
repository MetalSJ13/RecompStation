import QtQuick
import QtTest
import "../../qml/ArtworkLayout.js" as ArtworkLayout

TestCase {
    name: "ArtworkLayout"

    function test_platformAspectRatios() {
        fuzzyCompare(ArtworkLayout.platformAspectRatio("N64"), 1.4, 0.0001)
        compare(ArtworkLayout.platformAspectRatio("PS1"), 1)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("GameCube"), 5 / 7, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("Wii"), 5 / 7, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("Xbox"), 5 / 7, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("GB"), 0.92, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("GBA"), 0.92, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("SNES"), 1.4, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("PS2"), 5 / 7, 0.0001)
        fuzzyCompare(ArtworkLayout.platformAspectRatio("Wii U"), 5 / 7, 0.0001)
        compare(ArtworkLayout.placeholderSystemName("N64"), "NINTENDO 64")
        compare(ArtworkLayout.placeholderSystemName("GBA"), "GAME BOY ADVANCE")
        compare(ArtworkLayout.placeholderSystemName("Dreamcast"), "DREAMCAST")
        verify(ArtworkLayout.placeholderBandColor("SNES") !== ArtworkLayout.placeholderBandColor("N64"))
        compare(ArtworkLayout.placeholderBandColor("Unknown"), "#2a2f45")
        compare(ArtworkLayout.platformAspectRatio("Dreamcast"), 0)
        compare(ArtworkLayout.platformAspectRatio(""), 0)
    }

    // "measured" is the Grid's existing behavior and must stay unchanged.
    function test_measuredModeFollowsImage() {
        compare(ArtworkLayout.frameAspectRatio("measured", "N64", 1.38, 0.66), 1.38)
        compare(ArtworkLayout.frameAspectRatio("measured", "GameCube", 0.7, 0.66), 0.7)
    }

    function test_measuredModeFallsBackToViewportWithoutImage() {
        compare(ArtworkLayout.frameAspectRatio("measured", "N64", 0, 0.66), 0.66)
    }

    function test_platformModeUsesPlatformShapeForTypicalCovers() {
        // A North American N64 scan close to 7:5 snaps to the platform shape.
        fuzzyCompare(ArtworkLayout.frameAspectRatio("platform", "N64", 1.36, 0.66), 1.4, 0.0001)
        fuzzyCompare(ArtworkLayout.frameAspectRatio("platform", "GameCube", 0.70, 0.66), 5 / 7, 0.0001)
        compare(ArtworkLayout.frameAspectRatio("platform", "PS1", 1.05, 0.66), 1)
    }

    function test_platformModeKeepsClearlyDifferentCovers() {
        // A PAL N64 box is portrait and keeps its own ratio.
        compare(ArtworkLayout.frameAspectRatio("platform", "N64", 0.71, 0.66), 0.71)
        // A landscape image on a disc-case platform keeps its own ratio.
        compare(ArtworkLayout.frameAspectRatio("platform", "Wii", 1.33, 0.66), 1.33)
    }

    function test_platformRatioToleranceBoundary() {
        // Within 12% the cover is cropped to the platform frame...
        compare(ArtworkLayout.frameAspectRatio("platform", "PS1", 1.10, 0.66), 1)
        // ...beyond it the cover keeps its own shape rather than lose more art.
        compare(ArtworkLayout.frameAspectRatio("platform", "PS1", 1.15, 0.66), 1.15)
    }

    function test_platformModeWithoutImageUsesPlatformShape() {
        fuzzyCompare(ArtworkLayout.frameAspectRatio("platform", "N64", 0, 0.66), 1.4, 0.0001)
    }

    function test_platformModeUnknownPlatformFallsBack() {
        compare(ArtworkLayout.frameAspectRatio("platform", "Dreamcast", 1.2, 0.66), 1.2)
        compare(ArtworkLayout.frameAspectRatio("platform", "Dreamcast", 0, 0.66), 0.66)
    }

    function test_fitFrameMatchesPreviousGridFormula() {
        // Comfortable Grid artwork viewport: 250 x 375 with a 4 px inset.
        const landscape = ArtworkLayout.fitFrame(242, 367, 1.4)
        compare(landscape.width, 242)
        fuzzyCompare(landscape.height, 242 / 1.4, 0.0001)

        const portrait = ArtworkLayout.fitFrame(242, 367, 0.6)
        fuzzyCompare(portrait.width, 367 * 0.6, 0.0001)
        compare(portrait.height, 367)
    }

    function test_fitFrameGuardsAgainstZeroRatio() {
        const frame = ArtworkLayout.fitFrame(242, 367, 0)
        fuzzyCompare(frame.width, 367 * 0.01, 0.0001)
        compare(frame.height, 367)
    }
}
