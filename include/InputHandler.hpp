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
    // Main-menu Up/Down navigation over the full option cycle (PlayGame -> PlayerCount -> ... -> Statistics -> wrap). Exposed
    // public so headless tests can drive it directly; InputHandler is a friend of Game, so it reads/writes the private menuSelection.
    void navigateMenu(bool forward);

private:
    Game& game;
    // Debounce for the rules-config mouse-wheel rate limit (see processRulesEvents and processEvents). Seconds remaining
    // before the next scroll is allowed through; decays by real elapsed time once per frame in processEvents().
    float scrollCooldown_ = 0.f;
    void processMenuEvents(const sf::Event& event);
    void activateMainMenuOption(Game &game, MenuOption option);
    // Pause-menu activation for Enter/click — see activateMainMenuOption(). Runs only while paused;
    // Restart/QuitToMenu persist the shoe record before changing mode.
    void activatePauseOption(Game &game, PauseOption option);

    // Advances a value option (player count or game speed) in the given direction. `option` is
    // whichever row is under the cursor when called from scroll handling, so scrolling changes a
    // value without that row having to be keyboard-selected first (game.menuSelection).
    void adjustValue(bool forward, MenuOption option);

    // Cycles the Speed row (Fast -> Normal -> Slow -> AutoPlay -> Fast) with wraparound — shared by arrow
    // keys, the mouse wheel, and Left/Right so every input advances/steps back identically instead of
    // stopping at either end. A static member so it inherits InputHandler's friendship access to Game.
    static void cycleGameSpeed(Game &game, int direction);

    // Baseline hover-selection shared by every list-style menu (main menu + Game Rules), generic over an int- or enum-based
    // selection field so any future menu can reuse it. Updates `selection` to the row under the cursor only when it actually
    // changes, so pointing at an item selects it exactly like Up/Down would. `hoveredIndex` < 0 means off-list, leaving the
    // current selection untouched (it is never reset to a null row). Defined inline in the header because it is templated.
    template <typename Selection>
    void applyMenuRowSelection(int hoveredIndex, Selection& selection)
    {
        if (hoveredIndex >= 0 && static_cast<int>(selection) != hoveredIndex)
            selection = static_cast<Selection>(hoveredIndex);
    }

    void processTrainerEvents(const sf::Event& event);
    void processTournamentEvents(const sf::Event& event);
    // Feature 4 (Statistics Dashboard): browse-only tabbed view. Left/Right cycle the tabs; Escape returns to the main menu.
    void processStatsDashboardEvents(const sf::Event& event);
    // Feature 5 (Achievement System): browse-only list view. Escape returns to the main menu; nothing else is driven here.
    void processAchievementEvents(const sf::Event& event);
    void processGameEvents(const sf::Event& event);
    void processPauseEvents(const sf::Event& event);
    // Feature 3 (Custom Game Rules): drives the in-game rules-config panel (mouse-wheel edits the selected rule, Escape exits).
    void processRulesEvents(const sf::Event& event);
};