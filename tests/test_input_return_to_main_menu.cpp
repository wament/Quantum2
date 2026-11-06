#include <catch2/catch_all.hpp>

#include "Game.hpp"
#include "InputHandler.hpp"
#include "PlayerProfile.hpp"
#include <array>
#include <functional>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

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

// B7 — returning to the main menu via Escape must persist the profile. The window-close (Event::Closed) route saves on
// exit; the Escape path used to funnel straight through returnToMainMenu() without saving, so a session's bankroll (and
// its shoe-record / table-settings bookkeeping) was silently dropped. This drives an Escape through one of the per-mode
// handlers headlessly and reads the profile back from disk: it fails before the fix because nothing is written on that
// path, and passes once the exit persists. HOME is redirected to an isolated temp dir so Player::getSavePath() writes
// there (it resolves $HOME/.local/share/blackjack/saves/player_profile.json); original HOME is restored afterward.
TEST_CASE("B7: returning to the main menu via Escape persists the profile", "[InputHandler][Persistence]")
{
    const fs::path home = std::filesystem::temp_directory_path() / "bj_b7_test";
    std::error_code ec;
    fs::remove_all(home, ec);
    fs::create_directories(home);

    const char* originalHome = std::getenv("HOME");
    setenv("HOME", home.string().c_str(), 1);

    Game game(true);                          // headless: no window / assets / replay / HUD
    InputHandler input(game);

    game.startNewGame();                      // creates players[0] with the default profile (bankroll 1000)
    const int distinctiveBankroll = 98765;    // cannot arise from any default path
    game.testPlayer(0).setBankroll(distinctiveBankroll);

    // Escape through a per-mode handler reaches returnToMainMenu() exactly like it does in the windowed run loop. The
    // trainer branch only derefs trainer for H/S/D/P/Y/R keys; Escape is handled without touching trainer, so this is
    // safe with game.trainer == nullptr and appMode still Playing (startNewGame sets that).
    input.processTrainerEvents(escapeEvent());

    const fs::path saved = home / ".local" / "share" / "blackjack" / "saves" / "player_profile.json";
    CHECK(fs::exists(saved.string(), ec));                       // before fix: nothing was written on this exit path
    const PlayerProfile loaded = PlayerProfile::load(saved.string());
    CHECK(loaded.getBankroll() == distinctiveBankroll);          // before fix: still the default 1000

    if (originalHome)
        setenv("HOME", originalHome, 1);
    else
        unsetenv("HOME");
    fs::remove_all(home, ec);
}
