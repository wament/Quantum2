// Phase 0.4 vision-critical presentation seam: U1 (hover vs selection), U2 (pause panel), U3 (list reflow),
// U6 (card fan + hand-total pill) and U11 (one cursor coordinate basis).
//
// These assert on the SAME layout values GameRenderer consumes when it draws, so a drawn row can never drift
// from its hover target and no list can grow past its footer. Everything here is pure geometry: no window, no
// font, no render target -- the same headless contract as test_replay_layout.cpp / test_hud_layout.cpp.
//
// The invariant that ties it to the screen: for every supported world size, the row centre the renderer draws
// at is the row centre the hit-test resolves, and no drawn rect overlaps the footer/panel boundary.

#include <catch2/catch_all.hpp>

#include <string>
#include <vector>

// Include order matters: Game.hpp owns AppMode and includes GameRenderer.hpp from inside its own guard, so
// GameRenderer.hpp must not be the first of the pair to be included (the repo convention, see test_rendering.cpp).
#include "Constants.hpp"
#include "Game.hpp"
#include "GameRenderer.hpp"
#include "GameRules.hpp"
#include "PresentationLayout.hpp"

using Catch::Approx;
using namespace PresentationLayout;

namespace {

// The world sizes the game can end up in. gameView is a fixed 1600x900, but resizeView() letterboxes it, and
// the layout helpers are resolution-parameterised precisely so the invariant can be checked at a squashed world
// too -- that is the case that used to break (Achievements' footer at window-pixel h-28 vs rows at world 96..816).
std::vector<sf::Vector2f> worldSizes()
{
    return {
        sf::Vector2f(1600.f, 900.f),  // the default
        sf::Vector2f(1280.f, 800.f),
        sf::Vector2f(900.f, 560.f),   // the squashed case from the playtest
        sf::Vector2f(800.f, 1200.f),  // tall/narrow
    };
}

float cxOf(const sf::Vector2f& ws) { return ws.x / 2.f; }

// A centred text block, as every one of these screens places it.
sf::Rect<float> textRect(float cx, float cy, float w, float h)
{
    return sf::Rect<float>(sf::Vector2f(cx - w / 2.f, cy - h / 2.f), sf::Vector2f(w, h));
}

// The footer hint strip: a full-width band starting at its own y and 16px tall.
sf::Rect<float> footerRect(float worldW, float footerY)
{
    return sf::Rect<float>(sf::Vector2f(0.f, footerY), sf::Vector2f(worldW, 16.f));
}

} // namespace

// ---------------------------------------------------------------------------
// U1 -- hover and selection must be visually distinct
// ---------------------------------------------------------------------------

TEST_CASE("U1: hover is not the same visual state as selection", "[U1][menu][hover]")
{
    // The defect: drawMenuItem used `if (hovered || selected)` -> both gold, hover only added Bold, so two rows
    // looked selected at once and Enter was ambiguous about what a click would do.
    REQUIRE(menuRowVisualFor(true, false) == MenuRowVisual::Hovered);
    REQUIRE(menuRowVisualFor(false, true) == MenuRowVisual::Selected);
    REQUIRE(menuRowVisualFor(false, false) == MenuRowVisual::Normal);

    // Gold is reserved for selection: hovering must NOT resolve to the selected state.
    REQUIRE(menuRowVisualFor(true, false) != MenuRowVisual::Selected);

    // Selection wins the tie (cursor over the keyboard-selected row -> it is the selected row).
    REQUIRE(menuRowVisualFor(true, true) == MenuRowVisual::Selected);

    // Hover carries its own affordances: a background pill and a leading marker.
    const sf::Rect<float> pill = menuRowPill(800.f, 300.f, 160.f, 24.f);
    REQUIRE(pill.size.x > 160.f);
    REQUIRE(pill.size.y > 24.f);
    REQUIRE(rectInside(textRect(800.f, 300.f, 160.f, 24.f), pill));
    REQUIRE(std::string(menuHoverMarker()).size() >= 1u);
}

// ---------------------------------------------------------------------------
// U2 -- the pause menu needs a panel that contains its rows
// ---------------------------------------------------------------------------

TEST_CASE("U2: pause panel contains every pause row, the title and the hint", "[U2][pause][panel]")
{
    const int rows = static_cast<int>(pauseMenuItems().size());
    REQUIRE(rows == 3);  // Resume / Restart / Quit to Menu

    for (const sf::Vector2f& ws : worldSizes())
    {
        const PausePanelLayout L = pausePanelLayout(ws.x, ws.y, rows);
        const float cx = cxOf(ws);

        // The panel must be on screen.
        REQUIRE(L.panel.position.x >= 0.f);
        REQUIRE(L.panel.position.y >= 0.f);
        REQUIRE(L.panel.position.x + L.panel.size.x <= ws.x);
        REQUIRE(L.panel.position.y + L.panel.size.y <= ws.y);

        // Title, every row and the instruction line sit inside the panel -- this is the "Quit to Menu collides
        // with the seat-name labels" fix: the rows now sit on an opaque panel, not on the felt.
        REQUIRE(rectInside(textRect(cx, L.titleY, 200.f, 36.f), L.panel));
        REQUIRE(rectInside(textRect(cx, L.instrY, 320.f, L.instrHeight), L.panel));
        for (int i = 0; i < rows; ++i)
        {
            const float rowY = pauseRowYAt(ws.y, i);
            REQUIRE(rectInside(rowRect(cx, rowY, 240.f, L.rowHalfHeight), L.panel));
        }

        // Rows are strictly ordered top to bottom.
        for (int i = 1; i < rows; ++i)
            REQUIRE(pauseRowYAt(ws.y, i) > pauseRowYAt(ws.y, i - 1));
    }
}

// ---------------------------------------------------------------------------
// U3 -- Achievements and Game Rules reflow
// ---------------------------------------------------------------------------

TEST_CASE("U3: achievements list never overlaps its footer, at any world size", "[U3][reflow][overlap]")
{
    const int rows = 12;  // Achievement::Id has 12 entries

    for (const sf::Vector2f& ws : worldSizes())
    {
        const ListLayout L = achievementListLayout(ws.x, ws.y, rows);
        const float cx = cxOf(ws);

        // The regression: 12 rows of 60px from y=96 ran to 816 while the footer sat at h-28 = 532 at 900x560.
        REQUIRE(L.footerY <= ws.y);
        REQUIRE(L.startRowY > L.titleY);
        REQUIRE(L.rowHeight >= 26.f);

        // No row the renderer is allowed to draw may touch the footer strip.
        const sf::Rect<float> footer = footerRect(ws.x, L.footerY);
        for (int i = 0; i < L.visibleRows; ++i)
        {
            const sf::Rect<float> row =
                rowRect(cx, L.startRowY + static_cast<float>(i) * L.rowHeight, 600.f, L.rowHeight / 2.f);
            REQUIRE_FALSE(rectsOverlap(row, footer));
        }

        // The list stays usable: at the real world size all 12 achievements are visible.
        if (ws.x == 1600.f && ws.y == 900.f)
            REQUIRE(L.visibleRows >= rows);
    }
}

TEST_CASE("U3: game rules rows never overlap the instruction line, at any world size", "[U3][reflow][overlap]")
{
    const int rows = static_cast<int>(gameRuleRows().size());
    REQUIRE(rows == 7);

    for (const sf::Vector2f& ws : worldSizes())
    {
        const ListLayout L = gameRulesLayout(ws.x, ws.y, rows);
        const float cx = cxOf(ws);

        // The regression: rows ran to 444 while the instruction line sat at h*0.78 = 436.8 at 900x560.
        REQUIRE(L.footerY <= ws.y);
        REQUIRE(L.startRowY > L.titleY + 36.f);
        REQUIRE(L.rowHeight >= 22.f);

        // A 24pt rule line is ~30px tall, so its band is +/-15 around the row centre.
        const sf::Rect<float> instr = rowRect(cx, L.footerY, 420.f, 8.f);
        for (int i = 0; i < L.visibleRows; ++i)
        {
            const sf::Rect<float> row =
                rowRect(cx, L.startRowY + static_cast<float>(i) * L.rowHeight, 840.f, 15.f);
            REQUIRE_FALSE(rectsOverlap(row, instr));
        }
    }
}

// ---------------------------------------------------------------------------
// U6 -- card fan step and hand-total pill
// ---------------------------------------------------------------------------

TEST_CASE("U6: card fan shows more than a sliver of each card", "[U6][cards][fan]")
{
    // It used to be 16px on a 64px card: a third card showed a quarter of itself.
    REQUIRE(kCardFanStep > 16.f);
    REQUIRE(kCardFanStep >= static_cast<float>(Constants::CARD_WIDTH) / 2.f);

    // Fan width grows by exactly one step per card; a 1-card fan is one card wide.
    REQUIRE(cardFanWidth(0) == 0.f);
    REQUIRE(cardFanWidth(1) == Approx(static_cast<float>(Constants::CARD_WIDTH)));
    REQUIRE(cardFanWidth(3) - cardFanWidth(2) == Approx(kCardFanStep));

    // The centring shift lands the fan's centre on the anchor's centre.
    for (int n = 1; n <= 8; ++n)
    {
        const float anchor = 800.f;
        const float left = anchor + cardFanCenterShift(n);
        const float fanCentre = left + cardFanWidth(n) / 2.f;
        const float anchorCentre = anchor + static_cast<float>(Constants::CARD_WIDTH) / 2.f;
        REQUIRE(fanCentre == Approx(anchorCentre));
    }
}

TEST_CASE("U6: hand total gets a dark pill that fully backs the number", "[U6][cards][total]")
{
    const sf::Rect<float> pill = totalPillRect(500.f, 300.f, 22.f, 18.f);
    REQUIRE(rectInside(textRect(500.f, 300.f, 22.f, 18.f), pill));
    REQUIRE(pill.size.x > 22.f);
    REQUIRE(pill.size.y > 18.f);
}


// ---------------------------------------------------------------------------
// U11 -- one coordinate basis for every menu hit-test
// ---------------------------------------------------------------------------

TEST_CASE("U11: every menu hit-test resolves the row the renderer draws, at every world size",
          "[U11][menu][hover][resize]")
{
    GameRenderer renderer;

    for (const sf::Vector2f& ws : worldSizes())
    {
        const float cx = cxOf(ws);

        // Main menu: the drawn row centre must be the hovered index.
        for (int i = 0; i < static_cast<int>(mainMenuItems().size()); ++i)
        {
            const float y = renderer.mainMenuRowYForTest(i, ws);
            REQUIRE(y >= 0.f);
            REQUIRE(renderer.hoveredMainRowAt(sf::Vector2f(cx, y), ws) == i);
        }

        // Pause menu. This is the one that used raw sf::Mouse::getPosition (window pixels) against a
        // world-space pauseRowY, so it drifted the moment the window was resized.
        for (int i = 0; i < static_cast<int>(pauseMenuItems().size()); ++i)
        {
            const float y = renderer.pauseMenuRowYForTest(i, ws);
            REQUIRE(renderer.hoveredPauseRowAt(sf::Vector2f(cx, y), ws) == i);
        }

        // Game rules rows.
        for (int i = 0; i < static_cast<int>(gameRuleRows().size()); ++i)
        {
            const float y = renderer.gameRulesRowYForTest(i, ws);
            REQUIRE(renderer.hoveredRulesRowAt(sf::Vector2f(cx, y), ws) == i);
        }

        // A cursor well outside every band resolves to nothing rather than to a stale row.
        REQUIRE(renderer.hoveredMainRowAt(sf::Vector2f(cx, ws.y - 2.f), ws) == -1);
        REQUIRE(renderer.hoveredPauseRowAt(sf::Vector2f(4.f, ws.y / 2.f), ws) == -1);
        REQUIRE(renderer.hoveredRulesRowAt(sf::Vector2f(cx, 2.f), ws) == -1);
    }
}

TEST_CASE("U11: the main menu reflows so its last row and its hint stay on screen", "[U11][U3][menu][reflow]")
{
    GameRenderer renderer;
    const int last = static_cast<int>(mainMenuItems().size()) - 1;

    for (const sf::Vector2f& ws : worldSizes())
    {
        const float lastRowY = renderer.mainMenuRowYForTest(last, ws);
        REQUIRE(lastRowY >= 0.f);
        REQUIRE(lastRowY + 20.f <= ws.y);
        REQUIRE(mainMenuHintYAt(ws.y) > lastRowY);
        REQUIRE(mainMenuHintYAt(ws.y) + 16.f <= ws.y);
    }
}

TEST_CASE("U11: the renderer's layout accessors return the same values as the pure helpers",
          "[U11][layout][single-source]")
{
    GameRenderer renderer;
    const sf::Vector2f ws(900.f, 560.f);

    // One source of truth: the renderer must not keep its own copy of the geometry it draws with.
    REQUIRE(renderer.cardFanStepForTest() == kCardFanStep);
    REQUIRE(renderer.cardFanWidthForTest(5) == cardFanWidth(5));
    REQUIRE(renderer.menuRowVisualForTest(true, false) == menuRowVisualFor(true, false));
    REQUIRE(renderer.menuRowVisualForTest(true, true) == menuRowVisualFor(true, true));

    const ListLayout ach = renderer.achievementListLayoutForTest(12, ws);
    const ListLayout achRef = achievementListLayout(ws.x, ws.y, 12);
    REQUIRE(ach.rowHeight == Approx(achRef.rowHeight));
    REQUIRE(ach.visibleRows == achRef.visibleRows);
    REQUIRE(ach.footerY == Approx(achRef.footerY));

    const ListLayout rules = renderer.gameRulesLayoutForTest(7, ws);
    const ListLayout rulesRef = gameRulesLayout(ws.x, ws.y, 7);
    REQUIRE(rules.rowHeight == Approx(rulesRef.rowHeight));
    REQUIRE(rules.visibleRows == rulesRef.visibleRows);

    const PausePanelLayout pp = renderer.pausePanelLayoutForTest(ws);
    const PausePanelLayout ppRef = pausePanelLayout(ws.x, ws.y, 3);
    REQUIRE(pp.panel.size.x == Approx(ppRef.panel.size.x));
    REQUIRE(pp.panel.position.y == Approx(ppRef.panel.position.y));
    REQUIRE(pp.instrY == Approx(ppRef.instrY));
}

// ---------------------------------------------------------------------------
// U13 -- text that reaches sf::Text must be ASCII (found while verifying U1 on screen)
// ---------------------------------------------------------------------------

TEST_CASE("U13: every hint string that reaches sf::Text is ASCII-only", "[presentation][U13][text]")
{
    // sf::String(const std::string&) is documented as an ANSI string converted through std::locale (SFML 3,
    // include/SFML/System/String.hpp:173). A UTF-8 literal in the source therefore decodes to SEVERAL code
    // points, and the 0x80-range byte in the middle has no glyph -- the tofu boxes seen on screen. The shipped
    // UI font additionally has no U+2191/U+2193 at all (fontTools: assets/fonts/noto/NotoSans-Regular.ttf).
    // Both facts make ASCII the correct form for these lines; a non-ASCII label must be built as
    // sf::String(U"...")/std::u32string instead, never as a UTF-8 const char*.
    Game game(true); // headless: the hint helpers are pure

    REQUIRE(isSfmlSafeAscii(game.bettingControlsHint()));
    REQUIRE(isSfmlSafeAscii(game.sideBetControlsHint()));
    REQUIRE(isSfmlSafeAscii(nameEntryHintText(Constants::MAX_PLAYER_NAME_LENGTH)));
    REQUIRE(isSfmlSafeAscii(menuHoverMarker()));

    // Pin the replacements so the fix cannot silently regress to the mojibake originals.
    REQUIRE(game.bettingControlsHint().find("Up Increase Bet") != std::string::npos);
    REQUIRE(game.bettingControlsHint().find("Down Decrease Bet") != std::string::npos);
    REQUIRE(game.bettingControlsHint().find("\xE2\x86\x91") == std::string::npos); // no UTF-8 up arrow
    REQUIRE(game.bettingControlsHint().find("\xE2\x86\x93") == std::string::npos); // no UTF-8 down arrow
    REQUIRE(nameEntryHintText(3).find("3 left") != std::string::npos);
    REQUIRE(nameEntryHintText(3).find("\xE2\x80\xA2") == std::string::npos); // no UTF-2022 bullet
}
