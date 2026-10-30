#include <catch2/catch_all.hpp>

#include "Game.hpp"
#include "InputHandler.hpp"
#include <array>
#include <functional>

// The five per-mode process* handlers (Trainer, Tournament, StatsDashboard, Achievement, Rules) used to each carry their own
// three-line "Escape -> main menu" block. Phase 0 of the refactor collapsed them into one InputHandler::returnToMainMenu()
// helper, so this drives an Escape key through every handler headlessly and asserts the shared path still lands on the main
// menu. Pure input logic -- no window, deck or deal needed (InputHandler is a friend of Game, so these read appMode/menuSelection).
namespace
{
    // A synthetic KeyPressed(Escape) event the process* handlers consume; built from an SFML 3 tagged subtype (the default
    // constructor and public `type` were removed in SFML 3), then read via getIf<>, matching how InputHandler reads events.
    sf::Event escapeEvent()
    {
        sf::Event::KeyPressed kp; // KeyPressed is nested in sf::Event (SFML 3), not sf::Keyboard
        kp.code = sf::Keyboard::Key::Escape;
        return sf::Event(kp);
    }
}

TEST_CASE("Escape returns every per-mode handler to the main menu", "[InputHandler][MainMenu]")
{
    Game game(true); // headless
    InputHandler input(game);

    // Trainer, Tournament, StatsDashboard, Achievement, Rules -- all funnel Escape through returnToMainMenu(). The Rules
    // handler additionally stamps table settings via rememberTableSettings() before leaving, but in an unstarted headless
    // Game players is empty so that is a no-op; it still exercises the shared path exactly like the other four modes.
    std::array<std::function<void(InputHandler &)>, 5> handlers = {
        [](InputHandler &h) { h.processTrainerEvents(escapeEvent()); },
        [](InputHandler &h) { h.processTournamentEvents(escapeEvent()); },
        [](InputHandler &h) { h.processStatsDashboardEvents(escapeEvent()); },
        [](InputHandler &h) { h.processAchievementEvents(escapeEvent()); },
        [](InputHandler &h) { h.processRulesEvents(escapeEvent()); }};

    // Each mode: Escape must reset appMode to MainMenu and menuSelection to PlayGame.
    auto assertAtMainMenu = [&]() {
        REQUIRE(game.getAppModeForTest() == AppMode::MainMenu);
        REQUIRE(game.menuSelectionForTest() == MenuOption::PlayGame);
    };

    for (auto &run : handlers)
    {
        run(input);
        assertAtMainMenu();
    }
}
