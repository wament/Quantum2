#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

// A single selectable entry on the main menu. This is the one source of truth
// for what the main menu contains and where each option sits vertically, shared
// between the renderer (drawing + hover highlighting) and the input handler
// (click selection), so both always agree.
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

// A single selectable entry on the pause menu, using the SAME shared source of truth as
// MainMenuItem/mainMenuItems: one table drives drawing + hover highlight (renderer) and
// click selection (input), so they always agree. Fractional Y keeps each row's centre
// stable regardless of window height.
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

    // Returns the index of the main-menu item currently under the mouse cursor,
    // or -1 if none. Used by both hover highlighting (drawing) and click
    // selection (input), so they pick the same option.
    int getHoveredMenuItemIndex(Game &game) const;
    // Same as above but for the pause-menu rows so click + hover highlight agree.
    int getHoveredPauseMenuItemIndex(Game &game) const;

private:
    void drawDealer(Game &game);
    // Vertical pixel-centre of pause-menu row `index` at the current window height, shared
    // with drawPauseMenu and getHoveredPauseMenuItemIndex so all three agree on placement.
    float pauseRowY(Game &game, int index) const;
    void drawPlayers(Game &game);
    void drawChips(Game &game);
    void drawDeck(Game &game);
    void drawInsurance(Game &game);
    void drawMenuItem(Game &game, const std::string &label, bool selected, float y, bool hovered = false);
    void drawMainMenu(Game &game);
    void drawPauseMenu(Game &game);
    void drawSummaryOverlay(Game &game);
    sf::Vector2f getPlayerPosition(Game &game, size_t playerIndex, size_t totalPlayers) const;
};
