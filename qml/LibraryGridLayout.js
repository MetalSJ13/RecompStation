.pragma library
.import "ArtworkLayout.js" as ArtworkLayout

// Geometry and navigation for the sectioned (All Platforms) Library Grid.
//
// The library model is a flat list ordered by platform, then title. Each run of
// consecutive games on the same platform becomes one section with a header and
// its own row height, taken from that platform's box-art shape. Every section
// shares the same card width and column count, so a flat model index maps to
// (section, row, column) arithmetically and the shared selection index stays
// the single source of truth for List, Carousel and Game Details.

// Fallback shape for platforms without a defined box-art ratio.
var defaultFrameRatio = 5 / 7

// Runs of consecutive games sharing a platform: [{ platform, start, count }].
function buildSections(games) {
    var sections = []
    if (!games)
        return sections
    for (var i = 0; i < games.length; ++i) {
        var platform = games[i] && games[i].platform ? games[i].platform : ""
        var last = sections.length > 0 ? sections[sections.length - 1] : null
        if (last && last.platform === platform)
            last.count++
        else
            sections.push({ platform: platform, start: i, count: 1 })
    }
    return sections
}

function frameRatioForPlatform(platform) {
    var ratio = ArtworkLayout.platformAspectRatio(platform)
    return ratio > 0 ? ratio : defaultFrameRatio
}

// options: { columns, cardWidth, gap, captionHeight, headerHeight,
//            sectionSpacing, topMargin, bottomMargin }
// Returns { sections: [...], contentHeight }. Each section gains top,
// artworkHeight, cardHeight, rowPitch, rows, height.
function layoutSections(sections, options) {
    var columns = Math.max(1, options.columns)
    var gap = options.gap
    var y = options.topMargin
    var laidOut = []
    for (var i = 0; i < sections.length; ++i) {
        var section = sections[i]
        var artworkHeight = Math.round(options.cardWidth / frameRatioForPlatform(section.platform))
        var cardHeight = artworkHeight + options.captionHeight
        var rowPitch = cardHeight + gap
        var rows = Math.ceil(section.count / columns)
        var height = options.headerHeight + rows * rowPitch - gap
        laidOut.push({
            platform: section.platform,
            start: section.start,
            count: section.count,
            top: y,
            artworkHeight: artworkHeight,
            cardHeight: cardHeight,
            rowPitch: rowPitch,
            rows: rows,
            height: height
        })
        y += height + (i + 1 < sections.length ? options.sectionSpacing : 0)
    }
    return { sections: laidOut, contentHeight: y + options.bottomMargin }
}

// Locates a flat model index: { section, local, row, column } or null.
function locate(laidOutSections, columns, index) {
    columns = Math.max(1, columns)
    for (var i = 0; i < laidOutSections.length; ++i) {
        var section = laidOutSections[i]
        if (index >= section.start && index < section.start + section.count) {
            var local = index - section.start
            return {
                section: i,
                local: local,
                row: Math.floor(local / columns),
                column: local % columns
            }
        }
    }
    return null
}

// Card rectangle in content coordinates (x relative to the column block).
function cardRect(laidOutSections, columns, cellWidth, cardWidth, headerHeight, index) {
    var position = locate(laidOutSections, columns, index)
    if (!position)
        return null
    var section = laidOutSections[position.section]
    return {
        x: position.column * cellWidth,
        y: section.top + headerHeight + position.row * section.rowPitch,
        width: cardWidth,
        height: section.cardHeight,
        sectionTop: section.top,
        firstRow: position.row === 0
    }
}

// Sentinel results for moveIndex.
var noMove = -1
var leaveToToolbar = -2

// Directional movement. Left/right stay within the current row (no wrap).
// Up/down move between rows and continue into the previous/next section,
// keeping the nearest valid column. Up from the first row of the first
// section returns leaveToToolbar.
function moveIndex(laidOutSections, columns, index, direction) {
    columns = Math.max(1, columns)
    var position = locate(laidOutSections, columns, index)
    if (!position)
        return laidOutSections.length > 0 && laidOutSections[0].count > 0 ? 0 : noMove
    var section = laidOutSections[position.section]
    var rowStart = position.row * columns
    var rowLength = Math.min(columns, section.count - rowStart)

    if (direction === "left")
        return position.column > 0 ? index - 1 : noMove
    if (direction === "right")
        return position.column + 1 < rowLength ? index + 1 : noMove

    var targetSection = position.section
    var targetRow = position.row
    if (direction === "up") {
        if (position.row > 0) {
            targetRow--
        } else if (position.section > 0) {
            targetSection--
            targetRow = laidOutSections[targetSection].rows - 1
        } else {
            return leaveToToolbar
        }
    } else if (direction === "down") {
        if (position.row + 1 < section.rows) {
            targetRow++
        } else if (position.section + 1 < laidOutSections.length) {
            targetSection++
            targetRow = 0
        } else {
            return noMove
        }
    } else {
        return noMove
    }

    var target = laidOutSections[targetSection]
    var targetRowStart = targetRow * columns
    var targetRowLength = Math.min(columns, target.count - targetRowStart)
    var column = Math.min(position.column, Math.max(0, targetRowLength - 1))
    return target.start + targetRowStart + column
}

// contentY that brings a card (and, for a section's first row, its header)
// fully into view with the given padding. Returns the current value when the
// card is already visible.
function contentYForCard(rect, contentY, viewportHeight, contentHeight, topPadding, bottomPadding) {
    if (!rect)
        return contentY
    var top = rect.firstRow ? rect.sectionTop : rect.y - topPadding
    var bottom = rect.y + rect.height + bottomPadding
    var maxY = Math.max(0, contentHeight - viewportHeight)
    var next = contentY
    if (top < next)
        next = top
    else if (bottom > next + viewportHeight)
        next = Math.min(top, bottom - viewportHeight)
    return Math.max(0, Math.min(maxY, next))
}

// ---------------------------------------------------------------------------
// Shelves view: one horizontal row per section (platform).

// Sentinel: move focus up into the spotlight panel.
var toSpotlight = -3

// Section index containing a flat model index, or -1.
function sectionIndexOf(sections, index) {
    for (var i = 0; i < sections.length; ++i) {
        if (index >= sections[i].start && index < sections[i].start + sections[i].count)
            return i
    }
    return -1
}

// Directional movement between shelves. Left/right move along a shelf without
// wrapping. Up/down move to the previous/next shelf and return to the card
// last focused on that shelf (memory: { platform: localIndex }), or its first
// card. Up from the first shelf returns toSpotlight.
function moveShelfIndex(sections, index, direction, memory) {
    var s = sectionIndexOf(sections, index)
    if (s < 0)
        return sections.length > 0 && sections[0].count > 0 ? sections[0].start : noMove
    var section = sections[s]
    var local = index - section.start

    if (direction === "left")
        return local > 0 ? index - 1 : noMove
    if (direction === "right")
        return local + 1 < section.count ? index + 1 : noMove

    var target
    if (direction === "up") {
        if (s === 0)
            return toSpotlight
        target = sections[s - 1]
    } else if (direction === "down") {
        if (s + 1 >= sections.length)
            return noMove
        target = sections[s + 1]
    } else {
        return noMove
    }

    var remembered = memory && memory[target.platform] !== undefined
        ? memory[target.platform] : 0
    return target.start + Math.max(0, Math.min(target.count - 1, remembered))
}

// Vertical scroll position that brings a shelf row fully into view. rowTop and
// rowHeight are in content coordinates.
function contentYForRow(rowTop, rowHeight, contentY, viewportHeight, contentHeight) {
    var maxY = Math.max(0, contentHeight - viewportHeight)
    var next = contentY
    if (rowTop < next)
        next = rowTop
    else if (rowTop + rowHeight > next + viewportHeight)
        next = rowTop + rowHeight - viewportHeight
    return Math.max(0, Math.min(maxY, next))
}

// ---------------------------------------------------------------------------
// Hero carousel: one row with the selected cover centered and enlarged.

// Left edge (x) of every card. widths are the unselected card widths; the
// selected card uses selectedWidth and is centered on centerX. Neighbors are
// laid out outward from it with a constant gap.
function carouselPositions(widths, selectedIndex, selectedWidth, gap, centerX) {
    var count = widths ? widths.length : 0
    var xs = new Array(count)
    if (count === 0)
        return xs
    var selected = Math.max(0, Math.min(count - 1, selectedIndex))
    xs[selected] = centerX - selectedWidth / 2
    var x = xs[selected] + selectedWidth + gap
    for (var i = selected + 1; i < count; ++i) {
        xs[i] = x
        x += widths[i] + gap
    }
    x = xs[selected] - gap
    for (var j = selected - 1; j >= 0; --j) {
        x -= widths[j]
        xs[j] = x
        x -= gap
    }
    return xs
}
