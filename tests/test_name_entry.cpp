#include <catch2/catch_all.hpp>

#include "Game.hpp"
#include "Constants.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// U7 name-entry screen (resolved 2026-11-07). The real defect was that nothing ever SET persisted profile.name, so the
// identity round-tripped to "" on load. The testable core lives in Game: m_nameBuffer_ editing with a hard max-length
// clamp (nameEntryAppendChar / nameEntryBackspace / nameEntryText), the needsNameEntry() gate, and commitPlayerName()
// which writes seat 0's display + persisted profile.name and saves. Text-capture glue (sf::Event::TextInput) is visual and
// lives in InputHandler; persistence/gating/edit semantics here are driven headlessly with an isolated HOME dir.

namespace
{
    // Isolate the profile for one test invocation so gating + round-trip assertions are deterministic regardless of what
    // sits in the real ~/.local/share/blackjack. A per-invocation unique dir avoids races under ctest -j8.
    struct IsolatedHome
    {
        std::string home;          // temp dir we point HOME at while the test runs
        bool hadHome{};            // whether HOME was set before this test ran
        std::string original{};    // value to restore on teardown (or unset)
        IsolatedHome() : hadHome(std::getenv("HOME") != nullptr)
        {
            if (hadHome)
                original = std::getenv("HOME");
            static int counter = 0;
            const fs::path dir = fs::temp_directory_path() / ("bj_name_entry_" + std::to_string(counter++));
            fs::create_directories(dir);
            home = dir.string();
            setenv("HOME", home.c_str(), 1);
        }
        ~IsolatedHome()
        {
            if (hadHome)
                setenv("HOME", original.c_str(), 1);
            else
                unsetenv("HOME");
            std::error_code ec;
            fs::remove_all(home, ec);
        }
    };
}

TEST_CASE("NameEntry editing buffer appends, backspaces and clamps to the max length", "[NameEntry]")
{
    Game game(true); // headless

    CHECK(game.nameEntryText().empty());   // starts empty

    for (char c : {'A', 'd', 'a'})
        game.nameEntryAppendChar(c);
    CHECK(game.nameEntryText() == "Ada");

    game.nameEntryBackspace();             // remove the last character
    CHECK(game.nameEntryText() == "Ad");

    game.nameEntryBackspace();             // removing from an empty buffer must not crash or go negative
    game.nameEntryBackspace();
    CHECK(game.nameEntryText().empty());

    // Hard clamp: appending past MAX_PLAYER_NAME_LENGTH keeps the string exactly that long and never overflows.
    for (unsigned i = 0; i < Constants::MAX_PLAYER_NAME_LENGTH + 5; ++i)
        game.nameEntryAppendChar('x');
    CHECK(game.nameEntryText().size() == Constants::MAX_PLAYER_NAME_LENGTH);
    REQUIRE(std::all_of(game.nameEntryText().begin(), game.nameEntryText().end(), [](char c) { return c == 'x'; }));
}

TEST_CASE("needsNameEntry gates on whether the profile has a persisted name", "[NameEntry]")
{
    IsolatedHome home;                     // empty fresh profile -> no name yet
    Game game(true);
    game.startNewGame();                   // load the real persistable human seat from isolated HOME

    REQUIRE(game.needsNameEntry() == true); // first run: prompt before the main menu

    game.commitPlayerName("Ada");          // persist a name, then a fresh session must not prompt again
    Game reloaded(true);
    reloaded.startNewGame();
    REQUIRE(reloaded.needsNameEntry() == false);
}

TEST_CASE("commitPlayerName persists the name through save/load", "[NameEntry][Persistence]")
{
    IsolatedHome home;                     // isolated so the round-trip is deterministic

    Game game(true);                       // fresh profile, empty name
    game.startNewGame();                   // load the real persistable human seat from isolated HOME
    REQUIRE(game.needsNameEntry() == true);

    game.commitPlayerName("Ada");          // commit + save (this is what fixes U7)
    CHECK(game.testPlayer().getProfile().getName() == "Ada");

    // A brand-new Game in the same isolated HOME loads the persisted name and no longer needs prompting.
    Game reloaded(true);
    reloaded.startNewGame();
    REQUIRE(reloaded.needsNameEntry() == false);
    CHECK(reloaded.testPlayer().getProfile().getName() == "Ada");
}
