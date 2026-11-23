import QtQuick
import QtTest
import "../../qml/LibraryGridLayout.js" as GridLayout

TestCase {
    name: "LibraryGridLayout"

    // 3 GameCube games followed by 5 N64 games, 4 columns:
    //   GameCube: [0 1 2]
    //   N64:      [3 4 5 6]
    //             [7]
    function games() {
        const list = []
        for (let i = 0; i < 3; ++i) list.push({ platform: "GameCube" })
        for (let i = 0; i < 5; ++i) list.push({ platform: "N64" })
        return list
    }

    function options() {
        return { columns: 4, cardWidth: 350, gap: 14, captionHeight: 50,
                 headerHeight: 44, sectionSpacing: 22, topMargin: 16, bottomMargin: 18 }
    }

    function layout() {
        return GridLayout.layoutSections(GridLayout.buildSections(games()), options())
    }

    function test_buildSectionsGroupsConsecutivePlatforms() {
        const sections = GridLayout.buildSections(games())
        compare(sections.length, 2)
        compare(sections[0].platform, "GameCube")
        compare(sections[0].start, 0)
        compare(sections[0].count, 3)
        compare(sections[1].platform, "N64")
        compare(sections[1].start, 3)
        compare(sections[1].count, 5)
        compare(GridLayout.buildSections([]).length, 0)
        compare(GridLayout.buildSections(null).length, 0)
    }

    function test_sectionsUsePlatformRowHeights() {
        const result = layout()
        // GameCube 5:7 -> 350 * 7 / 5 = 490; N64 7:5 -> 250.
        compare(result.sections[0].artworkHeight, 490)
        compare(result.sections[1].artworkHeight, 250)
        compare(result.sections[0].rows, 1)
        compare(result.sections[1].rows, 2)
        compare(result.sections[0].top, 16)
        // header 44 + one row (540) - trailing gap is not added: 44 + 554 - 14.
        compare(result.sections[0].height, 44 + 554 - 14)
        compare(result.sections[1].top, 16 + result.sections[0].height + 22)
        compare(result.contentHeight,
                result.sections[1].top + result.sections[1].height + 18)
    }

    function test_unknownPlatformFallsBackToDiscCaseShape() {
        const result = GridLayout.layoutSections(
            GridLayout.buildSections([{ platform: "Dreamcast" }]), options())
        compare(result.sections[0].artworkHeight, 490)
    }

    function test_locateMapsFlatIndexToSectionRowColumn() {
        const sections = layout().sections
        const seventh = GridLayout.locate(sections, 4, 7)
        compare(seventh.section, 1)
        compare(seventh.row, 1)
        compare(seventh.column, 0)
        compare(GridLayout.locate(sections, 4, 99), null)
    }

    function test_horizontalMovementStaysInRow() {
        const sections = layout().sections
        compare(GridLayout.moveIndex(sections, 4, 1, "right"), 2)
        compare(GridLayout.moveIndex(sections, 4, 2, "right"), GridLayout.noMove)
        compare(GridLayout.moveIndex(sections, 4, 3, "left"), GridLayout.noMove)
        compare(GridLayout.moveIndex(sections, 4, 6, "right"), GridLayout.noMove)
    }

    function test_verticalMovementCrossesSections() {
        const sections = layout().sections
        // Down from GameCube column 2 lands on N64 row 0, column 2.
        compare(GridLayout.moveIndex(sections, 4, 2, "down"), 5)
        // Down within N64 clamps to the short final row.
        compare(GridLayout.moveIndex(sections, 4, 6, "down"), 7)
        compare(GridLayout.moveIndex(sections, 4, 7, "down"), GridLayout.noMove)
        // Up from N64 row 0, column 3 clamps to GameCube's last column.
        compare(GridLayout.moveIndex(sections, 4, 6, "up"), 2)
        compare(GridLayout.moveIndex(sections, 4, 7, "up"), 3)
    }

    function test_upFromFirstRowLeavesToToolbar() {
        const sections = layout().sections
        compare(GridLayout.moveIndex(sections, 4, 0, "up"), GridLayout.leaveToToolbar)
    }

    function test_cardRectAndScrollTarget() {
        const result = layout()
        const rect = GridLayout.cardRect(result.sections, 4, 364, 350, 44, 7)
        compare(rect.x, 0)
        compare(rect.y, result.sections[1].top + 44 + result.sections[1].rowPitch)
        compare(rect.height, 300)
        verify(!rect.firstRow)

        // Already visible: unchanged.
        const maxY = result.contentHeight - 400
        compare(GridLayout.contentYForCard(rect, maxY, 400, result.contentHeight, 16, 18), maxY)
        // Below the viewport: scroll so the card's bottom padding is visible.
        const below = GridLayout.contentYForCard(rect, 0, 600, result.contentHeight, 16, 18)
        compare(below, rect.y + rect.height + 18 - 600)

        // First row of a section scrolls its header into view too.
        const first = GridLayout.cardRect(result.sections, 4, 364, 350, 44, 3)
        verify(first.firstRow)
        compare(GridLayout.contentYForCard(first, first.y + 100, 600, result.contentHeight, 16, 18),
                result.sections[1].top)
    }

    // Shelves: GameCube [0 1 2], N64 [3 4 5 6 7].
    function test_shelfHorizontalMovementStaysOnShelf() {
        const sections = GridLayout.buildSections(games())
        compare(GridLayout.moveShelfIndex(sections, 0, "right", {}), 1)
        compare(GridLayout.moveShelfIndex(sections, 2, "right", {}), GridLayout.noMove)
        compare(GridLayout.moveShelfIndex(sections, 3, "left", {}), GridLayout.noMove)
        compare(GridLayout.moveShelfIndex(sections, 7, "right", {}), GridLayout.noMove)
    }

    function test_shelfVerticalMovementUsesMemory() {
        const sections = GridLayout.buildSections(games())
        // No memory: land on the first card of the target shelf.
        compare(GridLayout.moveShelfIndex(sections, 1, "down", {}), 3)
        compare(GridLayout.moveShelfIndex(sections, 6, "up", {}), 0)
        // Memory returns to the last focused card, clamped to the shelf.
        compare(GridLayout.moveShelfIndex(sections, 1, "down", { "N64": 3 }), 6)
        compare(GridLayout.moveShelfIndex(sections, 6, "up", { "GameCube": 9 }), 2)
        compare(GridLayout.moveShelfIndex(sections, 7, "down", {}), GridLayout.noMove)
    }

    function test_shelfUpFromFirstShelfGoesToSpotlight() {
        const sections = GridLayout.buildSections(games())
        compare(GridLayout.moveShelfIndex(sections, 2, "up", {}), GridLayout.toSpotlight)
    }

    function test_shelfIndexOutsideSectionsStartsAtFirstCard() {
        const sections = GridLayout.buildSections(games())
        compare(GridLayout.moveShelfIndex(sections, -1, "right", {}), 0)
        compare(GridLayout.moveShelfIndex([], -1, "right", {}), GridLayout.noMove)
        compare(GridLayout.sectionIndexOf(sections, 4), 1)
        compare(GridLayout.sectionIndexOf(sections, 42), -1)
    }

    function test_contentYForRow() {
        // Visible already.
        compare(GridLayout.contentYForRow(100, 200, 50, 400, 2000), 50)
        // Above the viewport.
        compare(GridLayout.contentYForRow(100, 200, 300, 400, 2000), 100)
        // Below the viewport.
        compare(GridLayout.contentYForRow(900, 200, 300, 400, 2000), 700)
        // Clamped to the content end.
        compare(GridLayout.contentYForRow(1900, 200, 0, 400, 2000), 1600)
    }

    function test_carouselCentersSelectedCard() {
        // Widths: portrait 150, landscape 294, portrait 150.
        const xs = GridLayout.carouselPositions([150, 294, 150], 1, 420, 30, 800)
        compare(xs[1], 800 - 210)
        compare(xs[2], 800 + 210 + 30)
        compare(xs[0], 800 - 210 - 30 - 150)
    }

    function test_carouselSelectedAtEnds() {
        const first = GridLayout.carouselPositions([100, 100, 100], 0, 140, 20, 500)
        compare(first[0], 430)
        compare(first[1], 590)
        compare(first[2], 710)
        const last = GridLayout.carouselPositions([100, 100, 100], 2, 140, 20, 500)
        compare(last[2], 430)
        compare(last[1], 310)
        compare(last[0], 190)
    }

    function test_carouselClampsSelectionAndHandlesEmpty() {
        compare(GridLayout.carouselPositions([], 0, 100, 10, 0).length, 0)
        const xs = GridLayout.carouselPositions([100, 100], 5, 140, 20, 500)
        compare(xs[1], 430)
    }
}
