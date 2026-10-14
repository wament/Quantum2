#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

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

class GameRenderer
{
public:
    void draw(Game &game);
    void resizeView(sf::RenderWindow &window, sf::View &view);

    // Menu-item index under the cursor, or -1; shared source of truth with input selection — see mainMenuItems().
    int getHoveredMenuItemIndex(Game &game) const;
    // Same table pattern as the main menu.
    int getHoveredPauseMenuItemIndex(Game &game) const;

private:
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
    void drawInsurance(Game &game);
    void drawMenuItem(Game &game, const std::string &label, bool selected, float y, bool hovered = false);
    void drawMainMenu(Game &game);
    void drawPauseMenu(Game &game);
    void drawSummaryOverlay(Game &game);
    sf::Vector2f getPlayerPosition(Game &game, size_t playerIndex, size_t totalPlayers) const;
};
