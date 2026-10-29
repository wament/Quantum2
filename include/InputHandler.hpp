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
    // Pause-menu activation for Enter/click — see activateMainMenuOption(). Runs only while paused;
    // Restart/QuitToMenu persist the shoe record before changing mode.
    void activatePauseOption(Game &game, PauseOption option);

    // Shared by keyboard arrows AND the mouse wheel. `forward` = "advance" (like
    // Down/Right): up one step in the main-menu cycle, or up a value option's speed.
    void navigateMenu(bool forward);
    // Advances a value option (player count or game speed) in the given direction. `option` is
    // whichever row is under the cursor when called from scroll handling, so scrolling changes a
    // value without that row having to be keyboard-selected first (game.menuSelection).
    void adjustValue(bool forward, MenuOption option);

    // Cycles the Speed row (Fast -> Normal -> Slow -> AutoPlay -> Fast) with wraparound — shared by arrow
    // keys, the mouse wheel, and Left/Right so every input advances/steps back identically instead of
    // stopping at either end. A static member so it inherits InputHandler's friendship access to Game.
    static void cycleGameSpeed(Game &game, int direction);

    void processTrainerEvents(const sf::Event& event);
    void processTournamentEvents(const sf::Event& event);
    // Feature 4 (Statistics Dashboard): browse-only tabbed view. Left/Right cycle the tabs; Escape returns to the main menu.
    void processStatsDashboardEvents(const sf::Event& event);
    // Feature 5 (Achievement System): browse-only list view. Escape returns to the main menu; nothing else is driven here.
    void processAchievementEvents(const sf::Event& event);
    void processGameEvents(const sf::Event& event);
    void processPauseEvents(const sf::Event& event);
};