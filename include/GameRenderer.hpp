#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <map>
#include <tuple>
#include <vector>
#include <functional>
#include <optional>
#include <algorithm>

// AppMode drives GameRenderer::draw()'s unified mode-dispatch table (modeTable()/modeEntryForTest())
// header needs the enum even though it only forward-declares Game for its draw(Game&) methods. Game.hpp defines AppMode
// before including this header, so this self-reference resolves without a circular-include cycle.
#include "Game.hpp"
#include "PresentationLayout.hpp"

// One source of truth for menu contents and row positions, shared by the renderer (drawing + hover)
// and InputHandler (click selection)
struct MainMenuItem { MenuOption option; float y; };

inline const std::vector<MainMenuItem> &mainMenuItems()
{
    static const std::vector<MainMenuItem> items = {
        {MenuOption::PlayGame,      220.f},
        {MenuOption::DailyChallenge, 260.f},
        {MenuOption::PlayerCount,   300.f},
        {MenuOption::GameSpeed,     340.f},
        {MenuOption::ReviewHistory, 380.f},
        {MenuOption::StrategyTrainer, 420.f},
        {MenuOption::Tournament, 460.f},
        {MenuOption::Achievements, 500.f},
        {MenuOption::GameRules, 540.f},
        {MenuOption::Statistics, 580.f},
    };
    return items;
}

// Vertical centre of main-menu row `index` at a given WORLD height. The table above stores the design-height (900)
// positions; scaling by worldH/900 keeps them pixel-identical at the default window and reflows the whole menu
// proportionally at any other, so the drawn row and its hover band can never separate (U11) and the last row
// always clears the hint line below it (U3). Both drawing and hit-testing call this -- one source of truth.
inline float mainMenuRowYAt(int index, float worldH)
{
    const auto &items = mainMenuItems();
    if (index < 0 || index >= static_cast<int>(items.size()))
        return -1.f;
    return items[static_cast<std::size_t>(index)].y * (worldH / PresentationLayout::kDesignWorldHeight);
}

// y of the "Up/Down: navigate ..." hint line that sits below the last menu row, so it can never land on a row.
inline float mainMenuHintYAt(float worldH)
{
    const int last = static_cast<int>(mainMenuItems().size()) - 1;
    return mainMenuRowYAt(last, worldH) + 45.f * (worldH / PresentationLayout::kDesignWorldHeight);
}
// Same shared table pattern as mainMenuItems(): one table drives both drawing/hover (renderer) and
// click selection (input). Fractional Y keeps each row's centre stable regardless of window height.
struct PauseMenuItem { PauseOption option; const char *label; float yFrac; };

inline const std::vector<PauseMenuItem> &pauseMenuItems()
{
    static const std::vector<PauseMenuItem> items = {
        {PauseOption::Resume,      "Resume",       0.40f},
        {PauseOption::Restart,     "Restart",      0.48f},
        {PauseOption::QuitToMenu,  "Quit to Menu", 0.56f},
    };
    return items;
}

// One source of truth for how many per-player round summaries can be drawn safely. The summary overlay renders one
// string per seat but indexes BOTH playerSummaries[i] and game.players[i], and feeds i into getPlayerPosition (which
// slots it against players.size()). It must therefore not iterate past the smaller of the two counts -- B9 made that a
// shared helper so the renderer and its test agree on one definition. When the vectors agree (the normal case) this is
// effectively a no change; when they diverge (e.g. a seat cleared out from under a stale summary list) it keeps every
// access in bounds and getPlayerPosition's slotMap lookup safe too.
inline size_t summaryOverlayRowCount(size_t summaryCount, size_t playerCount)
{
    return std::min(summaryCount, playerCount);
}

// Extra fan width a seat's split hands add beyond its single un-split hand (16px per extra hand), computed so an empty
// or divergent-state seat (zero hands) cannot underflow size_t into a huge width: a lone or missing hand contributes
// zero extra. Shared with drawSummaryOverlay so the renderer and its test agree on the layout.
inline float summaryRowFanWidth(size_t handCount)
{
    const size_t extraHands = (handCount > 1) ? (handCount - 1) : 0;
    return static_cast<float>(extraHands) * 16.f;
}

class Game;

// Per-player split-hand layout, allocated once per frame
struct PlayerHandLayout {
    std::vector<float> leftX;   // screen-x left edge of each hand's first card (one entry per split hand)
    std::vector<float> offsetY; // screen-y drop for each hand relative to its seat (staggered split hands)
    float cx = 0.f;             // group centre x (used to draw the active-player highlight)
    float cw = 0.f;             // group width
};

// One row of the unified AppMode->{backdrop, render} table consumed by GameRenderer::draw(): one place per
// mode so a mode's clear color and its draw fn can't drift apart. `backdrop` is an explicit clear color, or nullopt for
// modes that default to a plain clear + backgroundSprite felt; `render` is the per-mode presentation invoked after clearing.
// Exposed alongside modeEntryForTest()
struct ModeEntry {
    std::optional<sf::Color> backdrop;
    std::function<void(Game&)> render;
};

class GameRenderer
{
public:
    void draw(Game &game);
    // return the AppMode->{backdrop, render} entry from the unified dispatch table
    // assert each mode clears to its documented backdrop color and routes to a render fn without opening an SFML window.
    // Mirrors the *ForTest() pattern
    ModeEntry modeEntryForTest(AppMode appMode);
    void resizeView(sf::RenderWindow &window, sf::View &view);

    // Menu-item index under the cursor, or -1; shared source of truth with input selection: see mainMenuItems().
    int getHoveredMenuItemIndex(Game &game) const;
    // Same table pattern as the main menu.
    int getHoveredPauseMenuItemIndex(Game &game) const;
    // rules-row index under the cursor, or -1 -- shared source of truth with input hover so
    // pointing at a rule selects it, exactly like getHoveredMenuItemIndex() does for the main menu.
    int getHoveredGameRulesRowIndex(Game &game) const;

    // ---- Presentation seam: pure layout + hit-testing (U1, U2, U3, U6, U11) ----
    // Every one of these takes the WORLD rect (Game::gameView size) and a WORLD-space cursor, never a window,
    // so they run headless. The getHovered*Index(Game&) entry points above are thin wrappers that read the world
    // from game.gameView and convert the cursor with mapPixelToCoords, then delegate here -- which is what makes
    // the drawn row and the hover target share one basis at every window size.
    int hoveredMainRowAt(const sf::Vector2f &worldCursor, const sf::Vector2f &worldSize) const;
    int hoveredPauseRowAt(const sf::Vector2f &worldCursor, const sf::Vector2f &worldSize) const;
    int hoveredRulesRowAt(const sf::Vector2f &worldCursor, const sf::Vector2f &worldSize) const;

    // Row centres the renderer draws at, for the given world size. Drawing and hit-testing both go through these.
    float mainMenuRowYForTest(int index, const sf::Vector2f &worldSize) const;
    float pauseMenuRowYForTest(int index, const sf::Vector2f &worldSize) const;
    float gameRulesRowYForTest(int index, const sf::Vector2f &worldSize) const;

    // Layout the draw code consumes, exposed so a test can assert the no-overlap invariant on the real values.
    static PresentationLayout::ListLayout achievementListLayoutForTest(int rowCount, const sf::Vector2f &worldSize);
    static PresentationLayout::ListLayout gameRulesLayoutForTest(int rowCount, const sf::Vector2f &worldSize);
    static PresentationLayout::PausePanelLayout pausePanelLayoutForTest(const sf::Vector2f &worldSize);
    static PresentationLayout::MenuRowVisual menuRowVisualForTest(bool hovered, bool selected);
    static float cardFanStepForTest();
    static float cardFanWidthForTest(int cardCount);

    // expose the presentation-only reveal-clock internals so headless tests can drive the deal
    // animation timeline deterministically without a window, mirroring Game's *ForTest() accessors. Pure logic only --
    // no window/audio/network -- so they run over the Graphics-only Blackjack2Tests binary like every other logic test.
    void advanceRevealClockForTest(Game &game) { advanceRevealClock(game); }
    float playerRevealAtForForTest(Game &game, int p, int h, int i) { return playerRevealAtFor(game, p, h, i, dealClock_); }
    float dealerRevealAtForForTest(Game &game, int i) { return dealerRevealAtFor(game, i, dealClock_); }
    // the running reveal-clock value so tests can compute per-card animation elapsed time and
    // assert a card actually finishes sliding into its slot (elapsed >= DEAL_SLIDE_SECONDS), not just that it is stable.
    float dealClockForTest() const { return dealClock_; }

private:
    // The WORLD rect this renderer draws into (Game::gameView, a fixed 1600x900 letterboxed into the window by
    // resizeView()). Every layout call site must use this instead of window.getSize(): window pixels and world
    // units coincide only at exactly 1600x900, and mixing them is what made rows drift from their hover targets
    // and lists collide with their footers after a resize (U3/U11).
    static sf::Vector2f worldSizeOf(const Game &game);

    // Single source of truth for draw()'s AppMode->{backdrop, render} dispatch: one table replaces the old
    // backdrop if/else-if chain + separate dispatch switch
    // once on first use; consumed by draw() and exposed via modeEntryForTest(). Render lambdas call non-const draw* methods
    // via implicit this, so it's a non-const member (matching that draw() itself mutates window state).
    const std::map<AppMode, ModeEntry>& modeTable();

    void drawDealer(Game &game);
    // Draws a numeric hand total in white (16pt). rightAlign anchors the number's right edge at x
    // (dealer, flush left of its cards) instead of centering on it (player hands above each fan).
    void drawTotal(Game &game, int value, float x, float y, bool rightAlign) const;
    // Draws a status label ("BUSTED"/"BLACKJACK!") centered horizontally AND vertically over a hand's
    // anchor point (18pt bold), so it sits squarely on the cards. Color is chosen by the caller: red for
    // busted, green for "Blackjack!". Shared by both labels for consistent placement and style.
    void drawStatusLabel(Game &game, const std::string &label, float cx, float cy, const sf::Color &color) const;
    // Vertical pixel-centre of pause-menu row `index`; shared with drawing + hover
    float pauseRowY(Game &game, int index) const;
    std::vector<PlayerHandLayout> allocatePlayerLanes(Game &game) const;
    void drawPlayers(Game &game, const std::vector<PlayerHandLayout>& lanes);
    void drawChips(Game &game, const std::vector<PlayerHandLayout>& lanes);
    // Shared table-draw (dealer + players + chips) used by both Playing and Paused.
    void drawGameTable(Game &game);
    // Felt table + HUD + betting/side-bet/summary/achievement readouts shared by Playing and Tournament, so a tournament renders
    // exactly like an ordinary game with no duplicate rendering. Both modes call it once; the Tournament entry adds overlays after.
    void drawPlaySession(Game &game);
    // Tournament-only overlays: the live "Stage X of N · min bet" banner during play, and the end-of-tournament ranked leaderboard
    // + winner drawn once the final stage has run out. Pure presentation; reads game.tournament through Game's friend link.
    void drawTournamentBanner(Game &game);
    // Daily Challenge banner: the date + streak while a Daily shoe runs, and the booked result once it
    // is complete. A no-op outside a Daily session, so it can sit unconditionally in the Playing path.
    void drawDailyBanner(Game &game);
    void drawTournamentLeaderboard(Game &game);
    void drawDeck(Game &game);
    // thin sound-status indicator (bottom-right corner)
    // panel. Pure presentation; reads game.sound_ through the Game friend link.
    void drawSoundStatus(Game &game) const;
    void drawInsurance(Game &game);
    void drawSideBet(Game &game);
    void drawMenuItem(Game &game, const std::string &label, bool selected, float y, bool hovered = false);
    void drawMainMenu(Game &game);
    void drawNameEntry(Game &game);
    void drawPauseMenu(Game &game);
    void drawSummaryOverlay(Game &game);
    // browse-only list of every achievement with its unlocked/locked state, and a brief
    // popup for the most recently unlocked one during play. Pure presentation reading game.achievementManager_.
    void drawAchievementList(Game &game);
    void drawPendingAchievement(Game &game);
    // render the configurable-table-rules panel: each editable rule on its own row with the
    // live value, the selected row highlighted. Pure presentation; reads game.gameRules_ + the shared gameRuleRows()/
    // gameRuleDescription() helpers through the Game friend link.
    void drawGameRules(Game &game);
    sf::Vector2f getPlayerPosition(Game &game, size_t playerIndex, size_t totalPlayers) const;

    // presentation-only reveal timing so cards slide in from the shoe rather than
    // popping. NOT game state -- just per-frame animation clocks keyed by a stable card slot, advanced with
    // game.deltaTime and cleared whenever a round resets (total dealt cards can only drop at a new-round boundary).
    float dealClock_ = 0.f;                 // seconds accumulated so far this "deal"
    int prevTotalCards_ = 0;                // last frame's total player+dealer card count, for reset detection
    std::map<std::tuple<int, int, int>, float> playerRevealAt_; // (player, hand, card) -> reveal clock value (seconds)
    std::map<int, float> dealerRevealAt_;                    // (card index) -> reveal clock value (seconds)
    bool dealerFlipStarted_ = false;       // whether the hole-card flip has been seeded for the current round
    float dealerFlipStart_ = -1.f;         // dealClock value at which the hole card began flipping (seconds)

    // advance the reveal clock by one frame and clear stale reveal state if a round reset is detected.
    void advanceRevealClock(Game &game);
    // return (assigning on first sighting) the reveal-clock value for a player card slot, staggering early
    // slots so they start sliding before later ones. Stable per (player,hand,card); new slots begin at `now`.
    float playerRevealAtFor(Game &game, int p, int h, int i, float now);
    // same for a dealer card slot (staggered to reveal after the player's cards). Takes `game`
    // count the player cards already dealt and place this slot last in the sweep.
    float dealerRevealAtFor(Game &game, int i, float now);

    // effective per-speed durations. The base Feature-8 constants are scaled by
    // GameTiming::dealDurationScale(game.gameSpeed)
    // instant snap (no animation) in Hyper/Gamer where animationDisabled(). Presentation-only; declared here, defined in
    // GameRenderer.cpp alongside advanceRevealClock where Constants.hpp is already included. Used at every deal-duration site
    // (up/hole/revealed-card slide + doubled-down rotate + hole flip + per-card reveal staggering)
    // proportional to one another for a given speed.
    float dealSlideSeconds(Game &game) const;
    float dealStaggerSeconds(Game &game) const;
    float holeFlipSeconds(Game &game) const;
};
