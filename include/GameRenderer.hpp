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
    // Vertical pixel-centre of pause-menu row `index`; shared with drawing + hover so placement agrees.
    float pauseRowY(Game &game, int index) const;
    void drawPlayers(Game &game);
    void drawChips(Game &game);
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
