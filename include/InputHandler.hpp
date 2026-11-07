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

    // Per-mode event handlers exposed
    // Like the public navigateMenu(), these let a test call a mode handler without an SFML window; InputHandler is a friend of Game,
    // so they read/write the private appMode/menuSelection exactly as processEvents() does at runtime.
    void processTrainerEvents(const sf::Event& event);
    void processTournamentEvents(const sf::Event& event);
    void processStatsDashboardEvents(const sf::Event& event);
    void processNameEntryEvents(const sf::Event& event);
    void processAchievementEvents(const sf::Event& event);
    void processRulesEvents(const sf::Event& event);

private:
    Game& game;
    // Debounce for the rules-config mouse-wheel rate limit (see processRulesEvents and processEvents). Seconds remaining
    // before the next scroll is allowed through; decays by real elapsed time once per frame in processEvents().
    float scrollCooldown_ = 0.f;
    void processMenuEvents(const sf::Event& event);
    void activateMainMenuOption(Game &game, MenuOption option);
    // Pause-menu activation for Enter/click: see activateMainMenuOption(). Runs only while paused;
    // Restart/QuitToMenu persist the shoe record before changing mode.
    void activatePauseOption(Game &game, PauseOption option);

    // Advances a value option (player count or game speed) in the given direction. `option` is
    // whichever row is under the cursor when called from scroll handling, so scrolling changes a
    // value without that row having to be keyboard-selected first (game.menuSelection).
    void adjustValue(bool forward, MenuOption option);

    // Cycles the Speed row (Fast -> Normal -> Slow -> AutoPlay -> Fast) with wraparound: shared by arrow
    // keys, the mouse wheel, and Left/Right
    // stopping at either end. A static member
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

    // Browse-only modes not exercised by this refactor's tests; kept private.
    void processGameEvents(const sf::Event& event);
    void processPauseEvents(const sf::Event& event);
    // Shared "Escape returns to main menu" for every per-mode process* method: one log line naming the source mode, then a
    // single AppMode/selection reset. Each caller passes its own mode string
    void returnToMainMenu(Game& game, const char* fromMode);
};