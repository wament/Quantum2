// Pure pixel-geometry helpers for the menu / pause / list / hand presentation, shared by GameRenderer (drawing)
// and its hover hit-tests. Header-only and free of any SFML *runtime* type (only Vector2/Rect value types), so it is
// fully headless-testable exactly like ReplayLayout.hpp -- no window, no font, no render target.
//
// WHY THIS EXISTS (Phase 0.4 U1, U2, U3, U6, U11). Those five defects were all the same root cause wearing
// different clothes: the renderer drew rows in one coordinate space and hit-tested them in another, and the
// list screens mixed window pixels with world units inside a single screen. Concretely:
//
//   Game owns `gameView`, a FIXED 1600x900 world rect (Game.cpp:100-111) whose *viewport* is letterboxed into
//   the OS window by GameRenderer::resizeView(). So "window pixels" and "world units" are the same numbers only
//   at exactly 1600x900. Every site below that read `window.getSize()` while drawing into that view was drawing
//   at the wrong place as soon as the window was resized, and every hit-test that used raw
//   `sf::Mouse::getPosition` (window pixels) against a world-space row was pointing at a row that was not there.
//
// The rule this file enforces: **one basis -- the world (view) rect.** Every row position is computed from the
// world size passed in, and every cursor is converted with `window.mapPixelToCoords(...)` before it is compared.
// Callers must pass `game.gameView.getSize()`, never `game.window.getSize()`.
//
// The layout functions are resolution-parameterised (they take the world w/h as arguments) so a test can assert
// the no-overlap invariant at 1600x900 AND at a squashed 900x560 world, which is what "it reflows" means here.

#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cstddef>
#include <string> // std::string / std::to_string -- this header must compile standalone (see below)
#include <vector>

#include "Constants.hpp"

namespace PresentationLayout
{

// The world height the hand-placed menu positions were authored against (Game::gameView is a fixed 1600x900).
// Scaling an authored y by worldH / kDesignWorldHeight leaves it pixel-identical at the default size and reflows
// it proportionally at any other, which is what lets the menu clear its hint line at a squashed world (U3/U11).
inline constexpr float kDesignWorldHeight = 900.f;

// ---------------------------------------------------------------------------
// U11 -- one unified row hit-test
// ---------------------------------------------------------------------------

// The clickable band of one menu row: its vertical centre, how far above/below the centre the row still
// answers, and how far left/right of the shared horizontal centre the band extends.
struct RowBand
{
    float centreY = 0.f;
    float halfHeight = 0.f;
    float halfWidth = 0.f;
};

// Index of the row band containing `worldCursor` (already in WORLD units), or -1 for none. Rows are
// horizontally centred on `worldCx`, so the band is symmetric about it. When the cursor is inside two
// overlapping bands the one whose centre is nearest wins, so a point exactly between two rows resolves
// unambiguously. This one function replaces the three divergent hit-tests that used three different
// coordinate bases (U11); main menu, pause menu and rules rows all route through it.
inline int hitTestRowBand(const sf::Vector2f &worldCursor, float worldCx, const std::vector<RowBand> &bands)
{
    int best = -1;
    float bestDistance = 1e9f;

    for (std::size_t i = 0; i < bands.size(); ++i)
    {
        const float dy = std::abs(worldCursor.y - bands[i].centreY);
        const float dx = std::abs(worldCursor.x - worldCx);
        if (dy <= bands[i].halfHeight && dx <= bands[i].halfWidth && dy < bestDistance)
        {
            bestDistance = dy;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// U1 -- hover and selection must not look the same
// ---------------------------------------------------------------------------

// What a menu row should look like. Gold is RESERVED for the keyboard-selected row: a row the mouse is merely
// hovering gets a background pill + a leading marker instead, so "the row under the cursor" and "the row Enter
// will activate" can never be the same colour and the player can tell what a click will do.
enum class MenuRowVisual
{
    Normal,
    Hovered,
    Selected
};

// Selection wins a tie: when the cursor is over the row the keyboard has already selected, it is the selected
// row (gold) -- the pill is drawn underneath it as a "you can click this too" affordance, not a second highlight.
inline MenuRowVisual menuRowVisualFor(bool hovered, bool selected)
{
    if (selected)
        return MenuRowVisual::Selected;
    if (hovered)
        return MenuRowVisual::Hovered;
    return MenuRowVisual::Normal;
}

// Leading marker drawn in front of a hovered row. Deliberately ASCII. The first version used a typographic
// chevron "›" (U+203A) and it rendered as tofu boxes on screen -- NOT because the glyph is missing
// (assets/fonts/noto/NotoSans-Regular.ttf has it: cmap U+203A -> "guilsinglright", verified with fontTools).
// The cause is SFML 3's string decoding: sf::String(const std::string&) is documented as an *ANSI* string
// converted through std::locale (SFML/include/SFML/System/String.hpp:173), so a UTF-8 literal in the source
// decodes to three code points -- 0xE2, 0x80, 0xBA -- and the U+0080 control character has no glyph.
// Non-ASCII text must be passed as sf::String(U"...") / std::u32string, never as a UTF-8 const char*.
// ">" is the ASCII marker the rules panel already uses for its selected row, so it is proven to render.
inline const char *menuHoverMarker() { return "> "; }

// Every string drawn with sf::Text must be ASCII-only unless it is built as a UTF-32 sf::String, for the
// decoding reason above. This predicate is what a headless test asserts on the hint helpers.
inline bool isSfmlSafeAscii(const std::string &s)
{
    for (const unsigned char c : s)
        if (c > 0x7Fu)
            return false;
    return true;
}

// Name-entry hint line (U7 screen). Extracted from drawNameEntry so the ASCII rule is testable; the inline
// literal used to carry two U+2022 bullets, which decoded to "â<control>¢" on screen.
inline std::string nameEntryHintText(unsigned remainingChars)
{
    return "Enter = confirm   |   Esc = skip   |   " + std::to_string(remainingChars) + " left";
}

// Background pill behind a hovered row, centred on (worldCx, rowY) and sized from the row's own text bounds so
// it hugs the label at any string length.
inline sf::Rect<float> menuRowPill(float worldCx, float rowY, float textWidth, float textHeight)
{
    constexpr float kPadX = 20.f;
    constexpr float kPadY = 9.f;
    const float w = textWidth + 2.f * kPadX;
    const float h = textHeight + 2.f * kPadY;
    return sf::Rect<float>(sf::Vector2f(worldCx - w / 2.f, rowY - h / 2.f), sf::Vector2f(w, h));
}


// ---------------------------------------------------------------------------
// U2 -- the pause menu needs a panel
// ---------------------------------------------------------------------------

// Everything drawPauseMenu places, in world units, plus the rounded panel that must sit behind it. Bare text
// over the alpha-dimmed felt used to leave "Quit to Menu" colliding with the seat-name labels drawn under it.
struct PausePanelLayout
{
    sf::Rect<float> panel{};  // rounded rect with padding, centred horizontally
    float titleY = 0.f;
    float firstRowY = 0.f;
    float rowSpacing = 0.f;
    float rowHalfHeight = 0.f;
    float instrY = 0.f;
    float instrHeight = 0.f;
    int rowCount = 0;
};

// Row centre of pause row `i`, derived from the same fractions the panel rect is computed from, so drawing,
// hit-testing and the panel can never disagree about where a row sits.
inline float pauseRowYAt(float worldH, int index, float firstFrac = 0.40f, float spacingFrac = 0.08f)
{
    return worldH * (firstFrac + static_cast<float>(index) * spacingFrac);
}

inline PausePanelLayout pausePanelLayout(float worldW, float worldH, int rowCount,
                                         float firstFrac = 0.40f, float spacingFrac = 0.08f)
{
    constexpr float kTitleFrac = 0.25f;
    constexpr float kInstrFrac = 0.68f;
    constexpr float kInstrFont = 14.f;
    constexpr float kRowHalf = 20.f;      // covers a size-24 line with room to spare
    constexpr float kPadY = 30.f;
    constexpr float kPanelW = 380.f;

    PausePanelLayout L;
    L.rowCount = rowCount;
    L.titleY = worldH * kTitleFrac;
    L.firstRowY = worldH * firstFrac;
    L.rowSpacing = worldH * spacingFrac;
    L.rowHalfHeight = kRowHalf;
    L.instrY = worldH * kInstrFrac;
    L.instrHeight = kInstrFont;

    // Panel spans title top -> instruction bottom, plus padding, and is wide enough for the longest row.
    const float top = L.titleY - kPadY;
    const float bottom = L.instrY + L.instrHeight + kPadY;
    L.panel = sf::Rect<float>(sf::Vector2f(worldW / 2.f - kPanelW / 2.f, top),
                              sf::Vector2f(kPanelW, bottom - top));
    return L;
}

// ---------------------------------------------------------------------------
// U3 -- the two screens that did not reflow
// ---------------------------------------------------------------------------

// A vertical list that must fit between its header and its footer hint at ANY world size.
struct ListLayout
{
    float titleY = 0.f;
    float startRowY = 0.f;
    float rowHeight = 0.f;
    float footerY = 0.f;
    int visibleRows = 0;  // rows that fit above the footer; the renderer draws at most this many
    bool fits = true;     // rowCount <= visibleRows
};

// Achievements list. Previously: y = 96 fixed, rowH = 60 fixed, footer at window-pixel h-28 -- the same screen
// reading two different spaces, so at a squashed world the footer landed in the middle of the list (12 rows of
// 60px from y=96 run to 816 while the footer sat at 532). Row height is now derived from the space between
// header and footer, clamped to a readable band, and the renderer clips to visibleRows so overlap is
// impossible by construction.
inline ListLayout achievementListLayout(float worldW, float worldH, int rowCount)
{
    constexpr float kTitleY = 40.f;
    constexpr float kHeaderBottom = 96.f;  // top of the first row
    constexpr float kFooterFromBottom = 28.f;
    constexpr float kFooterGap = 12.f;     // breathing room between last row and footer
    constexpr float kMaxRowH = 60.f;       // the original spacing, kept as the upper bound
    constexpr float kMinRowH = 26.f;       // below this a name + description line stops being readable

    ListLayout L;
    L.titleY = kTitleY;
    L.footerY = worldH - kFooterFromBottom;
    L.startRowY = kHeaderBottom;

    const float available = L.footerY - kFooterGap - kHeaderBottom;
    const int n = std::max(0, rowCount);

    float rowH = (n > 0) ? available / static_cast<float>(n) : kMaxRowH;
    rowH = std::min(kMaxRowH, std::max(kMinRowH, rowH));
    L.rowHeight = rowH;

    L.visibleRows = (rowH > 0.f) ? static_cast<int>(available / rowH) : 0;
    L.fits = (n <= L.visibleRows);
    return L;
}

// Custom Game Rules panel. Previously: startY = h*0.30 (fractional) with spacing = 46 fixed and the instruction
// line at h*0.78 -- at a squashed world the instruction line landed inside the last rule row (rows ran to 444
// while the instruction sat at 436.8). Spacing is now derived from the space between the first row and footer.
inline ListLayout gameRulesLayout(float worldW, float worldH, int rowCount)
{
    constexpr float kTitleFrac = 0.12f;
    constexpr float kStartFrac = 0.22f;
    constexpr float kFooterFrac = 0.86f;
    constexpr float kFooterGap = 12.f;
    constexpr float kMaxSpacing = 46.f;  // the original spacing, kept as the upper bound
    constexpr float kMinSpacing = 22.f;

    ListLayout L;
    L.titleY = worldH * kTitleFrac;
    L.startRowY = worldH * kStartFrac;
    L.footerY = worldH * kFooterFrac;

    const float available = L.footerY - kFooterGap - L.startRowY;
    const int n = std::max(0, rowCount);

    float spacing = (n > 0) ? available / static_cast<float>(n) : kMaxSpacing;
    spacing = std::min(kMaxSpacing, std::max(kMinSpacing, spacing));
    L.rowHeight = spacing;

    L.visibleRows = (spacing > 0.f) ? static_cast<int>(available / spacing) : 0;
    L.fits = (n <= L.visibleRows);
    return L;
}

// ---------------------------------------------------------------------------
// U6 -- card fan overlap and hand-total legibility
// ---------------------------------------------------------------------------

// Horizontal/vertical step between successive cards of one hand. It used to be 16px on a 64px card, so a third
// card showed only a quarter of itself and a fourth less; 34px leaves just over half of every card visible.
inline constexpr float kCardFanStep = 34.f;

// Full width of an n-card fan (left edge of card 0 to right edge of the last card).
inline float cardFanWidth(int cardCount)
{
    if (cardCount <= 0)
        return 0.f;
    return static_cast<float>(cardCount - 1) * kCardFanStep + static_cast<float>(Constants::CARD_WIDTH);
}

// Left-edge shift that keeps an n-card fan centred on the seat's anchor, so a long fan grows symmetrically
// instead of fanning right into the neighbour. Exactly -(n-1)*step/2, which lands the fan's centre on the
// anchor's centre.
inline float cardFanCenterShift(int cardCount)
{
    if (cardCount <= 1)
        return 0.f;
    return -0.5f * kCardFanStep * static_cast<float>(cardCount - 1);
}

// Dark backing pill behind a hand total, centred on (cx, cy) and sized from the number's own text bounds.
// Totals used to be bare white 16pt straight on the green felt arc, which is the lowest-contrast placement
// in the game.
inline sf::Rect<float> totalPillRect(float cx, float cy, float textWidth, float textHeight)
{
    constexpr float kPadX = 8.f;
    constexpr float kPadY = 4.f;
    const float w = textWidth + 2.f * kPadX;
    const float h = textHeight + 2.f * kPadY;
    return sf::Rect<float>(sf::Vector2f(cx - w / 2.f, cy - h / 2.f), sf::Vector2f(w, h));
}

// ---------------------------------------------------------------------------
// Shared rect predicates, used by the layout tests to assert "no overlap" / "stays inside".
// ---------------------------------------------------------------------------
inline bool rectsOverlap(const sf::Rect<float> &a, const sf::Rect<float> &b)
{
    const float aR = a.position.x + a.size.x;
    const float aB = a.position.y + a.size.y;
    const float bR = b.position.x + b.size.x;
    const float bB = b.position.y + b.size.y;
    return a.position.x < bR && b.position.x < aR && a.position.y < bB && b.position.y < aB;
}

// True when `inner` lies completely inside `outer` (with `tolerance` of slack).
inline bool rectInside(const sf::Rect<float> &inner, const sf::Rect<float> &outer, float tolerance = 0.5f)
{
    const float iR = inner.position.x + inner.size.x;
    const float iB = inner.position.y + inner.size.y;
    const float oR = outer.position.x + outer.size.x;
    const float oB = outer.position.y + outer.size.y;
    return inner.position.x >= outer.position.x - tolerance &&
           inner.position.y >= outer.position.y - tolerance &&
           iR <= oR + tolerance && iB <= oB + tolerance;
}

// A row's drawn rect, for overlap assertions: centred on (worldCx, centreY), `width` wide, `halfHeight` tall
// above and below the centre.
inline sf::Rect<float> rowRect(float worldCx, float centreY, float width, float halfHeight)
{
    return sf::Rect<float>(sf::Vector2f(worldCx - width / 2.f, centreY - halfHeight),
                           sf::Vector2f(width, 2.f * halfHeight));
}

} // namespace PresentationLayout

