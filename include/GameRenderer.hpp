#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <map>
#include <tuple>
#include <vector>
#include <functional>
#include <optional>

// AppMode drives GameRenderer::draw()'s unified mode-dispatch table (modeTable()/modeEntryForTest()), so the renderer
// header needs the enum even though it only forward-declares Game for its draw(Game&) methods. Game.hpp defines AppMode
// before including this header, so this self-reference resolves without a circular-include cycle.
#include "Game.hpp"

// One source of truth for menu contents and row positions, shared by the renderer (drawing + hover)
// and InputHandler (click selection) so they always agree. Edit here; don't duplicate it elsewhere.
struct MainMenuItem { MenuOption option; float y; };

inline const std::vector<MainMenuItem> &mainMenuItems()
{
    static const std::vector<MainMenuItem> items = {
        {MenuOption::PlayGame,      220.f},
        {MenuOption::PlayerCount,   260.f},
        {MenuOption::GameSpeed,     300.f},
        {MenuOption::ReviewHistory, 340.f},
        {MenuOption::StrategyTrainer, 380.f},
        {MenuOption::Tournament, 420.f},
        {MenuOption::Achievements, 460.f},
        {MenuOption::GameRules, 500.f},
        {MenuOption::Statistics, 540.f},
    };
    return items;
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

class Game;

// Per-player split-hand layout, allocated once per frame so cards, chips and the turn highlight all line up.
struct PlayerHandLayout {
    std::vector<float> leftX;   // screen-x left edge of each hand's first card (one entry per split hand)
    std::vector<float> offsetY; // screen-y drop for each hand relative to its seat (staggered split hands)
    float cx = 0.f;             // group centre x (used to draw the active-player highlight)
    float cw = 0.f;             // group width
};

// One row of the unified AppMode->{backdrop, render} table consumed by GameRenderer::draw() (Finding #4): one place per
// mode so a mode's clear color and its draw fn can't drift apart. `backdrop` is an explicit clear color, or nullopt for
// modes that default to a plain clear + backgroundSprite felt; `render` is the per-mode presentation invoked after clearing.
// Exposed alongside modeEntryForTest() so headless tests lock this table without opening an SFML window (Graphics-only).
struct ModeEntry {
    std::optional<sf::Color> backdrop;
    std::function<void(Game&)> render;
};

class GameRenderer
{
public:
    void draw(Game &game);
    // Feature 4-style (unified mode dispatch, Finding #4): return the AppMode->{backdrop, render} entry so headless tests can
    // assert each mode clears to its documented backdrop color and routes to a render fn without opening an SFML window.
    // Mirrors the *ForTest() pattern -- pure logic, Graphics-only, runs over Blackjack2Tests like every other logic test.
    ModeEntry modeEntryForTest(AppMode appMode);
    void resizeView(sf::RenderWindow &window, sf::View &view);

    // Menu-item index under the cursor, or -1; shared source of truth with input selection — see mainMenuItems().
    int getHoveredMenuItemIndex(Game &game) const;
    // Same table pattern as the main menu.
    int getHoveredPauseMenuItemIndex(Game &game) const;
    // Feature 3 (Custom Game Rules): rules-row index under the cursor, or -1 -- shared source of truth with input hover so
    // pointing at a rule selects it, exactly like getHoveredMenuItemIndex() does for the main menu.
    int getHoveredGameRulesRowIndex(Game &game) const;

    // Feature 8 (test helpers): expose the presentation-only reveal-clock internals so headless tests can drive the deal
    // animation timeline deterministically without a window, mirroring Game's *ForTest() accessors. Pure logic only --
    // no window/audio/network -- so they run over the Graphics-only Blackjack2Tests binary like every other logic test.
    void advanceRevealClockForTest(Game &game) { advanceRevealClock(game); }
    float playerRevealAtForForTest(Game &game, int p, int h, int i) { return playerRevealAtFor(game, p, h, i, dealClock_); }
    float dealerRevealAtForForTest(Game &game, int i) { return dealerRevealAtFor(game, i, dealClock_); }
    // Feature 8 (test helper): the running reveal-clock value so tests can compute per-card animation elapsed time and
    // assert a card actually finishes sliding into its slot (elapsed >= DEAL_SLIDE_SECONDS), not just that it is stable.
    float dealClockForTest() const { return dealClock_; }

private:
    // Single source of truth for draw()'s AppMode->{backdrop, render} dispatch (Finding #4): one table replaces the old
    // backdrop if/else-if chain + separate dispatch switch, so adding a mode is one row instead of editing both chains. Built
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
    // Vertical pixel-centre of pause-menu row `index`; shared with drawing + hover so placement agrees.
    float pauseRowY(Game &game, int index) const;
    std::vector<PlayerHandLayout> allocatePlayerLanes(Game &game) const;
    void drawPlayers(Game &game, const std::vector<PlayerHandLayout>& lanes);
    void drawChips(Game &game, const std::vector<PlayerHandLayout>& lanes);
    // Shared table-draw (dealer + players + chips) used by both Playing and Paused.
    void drawGameTable(Game &game);
    void drawDeck(Game &game);
    // Feature 7: thin sound-status indicator (bottom-right corner) so mute/volume state is visible without a settings
    // panel. Pure presentation; reads game.sound_ through the Game friend link.
    void drawSoundStatus(Game &game) const;
    void drawInsurance(Game &game);
    void drawSideBet(Game &game);
    void drawMenuItem(Game &game, const std::string &label, bool selected, float y, bool hovered = false);
    void drawMainMenu(Game &game);
    void drawPauseMenu(Game &game);
    void drawSummaryOverlay(Game &game);
    // Feature 5 (Achievement System): browse-only list of every achievement with its unlocked/locked state, and a brief
    // popup for the most recently unlocked one during play. Pure presentation reading game.achievementManager_.
    void drawAchievementList(Game &game);
    void drawPendingAchievement(Game &game);
    // Feature 3 (Custom Game Rules): render the configurable-table-rules panel — each editable rule on its own row with the
    // live value, the selected row highlighted. Pure presentation; reads game.gameRules_ + the shared gameRuleRows()/
    // gameRuleDescription() helpers through the Game friend link.
    void drawGameRules(Game &game);
    sf::Vector2f getPlayerPosition(Game &game, size_t playerIndex, size_t totalPlayers) const;

    // Feature 8 (Animated Card Dealing): presentation-only reveal timing so cards slide in from the shoe rather than
    // popping. NOT game state -- just per-frame animation clocks keyed by a stable card slot, advanced with
    // game.deltaTime and cleared whenever a round resets (total dealt cards can only drop at a new-round boundary).
    float dealClock_ = 0.f;                 // seconds accumulated so far this "deal"
    int prevTotalCards_ = 0;                // last frame's total player+dealer card count, for reset detection
    std::map<std::tuple<int, int, int>, float> playerRevealAt_; // (player, hand, card) -> reveal clock value (seconds)
    std::map<int, float> dealerRevealAt_;                    // (card index) -> reveal clock value (seconds)
    bool dealerFlipStarted_ = false;       // whether the hole-card flip has been seeded for the current round
    float dealerFlipStart_ = -1.f;         // dealClock value at which the hole card began flipping (seconds)

    // Feature 8: advance the reveal clock by one frame and clear stale reveal state if a round reset is detected.
    void advanceRevealClock(Game &game);
    // Feature 8: return (assigning on first sighting) the reveal-clock value for a player card slot, staggering early
    // slots so they start sliding before later ones. Stable per (player,hand,card); new slots begin at `now`.
    float playerRevealAtFor(Game &game, int p, int h, int i, float now);
    // Feature 8: same for a dealer card slot (staggered to reveal after the player's cards). Takes `game` so it can
    // count the player cards already dealt and place this slot last in the sweep.
    float dealerRevealAtFor(Game &game, int i, float now);

    // Feature 8 (speed-linked dealing animation): effective per-speed durations. The base Feature-8 constants are scaled by
    // GameTiming::dealDurationScale(game.gameSpeed) so the card-deal tempo matches the action cadence and collapses to an
    // instant snap (no animation) in Hyper/Gamer where animationDisabled(). Presentation-only; declared here, defined in
    // GameRenderer.cpp alongside advanceRevealClock where Constants.hpp is already included. Used at every deal-duration site
    // (up/hole/revealed-card slide + doubled-down rotate + hole flip + per-card reveal staggering) so all three durations stay
    // proportional to one another for a given speed.
    float dealSlideSeconds(Game &game) const;
    float dealStaggerSeconds(Game &game) const;
    float holeFlipSeconds(Game &game) const;
};
