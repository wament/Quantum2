#pragma once

#include <SFML/Graphics.hpp>

class Game;
enum class MenuOption;
enum class PauseOption;

class InputHandler
{
public:
    InputHandler(Game& game);
    void processEvents();

private:
    Game& game;
    void processMenuEvents(const sf::Event& event);
    void activateMainMenuOption(Game &game, MenuOption option);
    // Same as above but for the pause menu (Resume/Restart/Quit). Shared by keyboard
    // Enter and the mouse click so selecting works identically.
    void activatePauseOption(Game &game, PauseOption option);

    // Shared by keyboard arrows AND the mouse wheel. `forward` = "advance" (like
    // Down/Right): up one step in the main-menu cycle, or up a value option's speed.
    void navigateMenu(bool forward);
    // Advances a value option (player count or game speed) in the given direction. `option` is
    // whichever row is under the cursor when called from scroll handling, so scrolling changes a
    // value without that row having to be keyboard-selected first (game.menuSelection).
    void adjustValue(bool forward, MenuOption option);

    void processGameEvents(const sf::Event& event);
    void processPauseEvents(const sf::Event& event);
};