#include <catch2/catch_all.hpp>

#include "Game.hpp"
#include "Constants.hpp"
#include "test_support.hpp"   // test_support::IsolatedHome (B17: hermetic, per-process-unique HOME)

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

// B17: the isolation guard used to live here as a local struct whose directory name came from a `static int counter`.
// That counter is per-PROCESS, and ctest runs every TEST_CASE in its own process, so it was always 0 -- every test in
// this file landed in /tmp/bj_name_entry_0. "commitPlayerName persists..." (#291) writes a profile named "Ada" there;
// "needsNameEntry gates..." (#290) then loaded it, saw a non-empty name and failed its REQUIRE. Reproduced by running
// the two side by side. The guard is now shared (test_support.hpp) and namespaced by PID.

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
    test_support::IsolatedHome home("name_entry");   // empty fresh profile -> no name yet
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
    test_support::IsolatedHome home("name_entry");   // isolated so the round-trip is deterministic

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

// The gap the tests above left open: they assert the PERSISTED name, and that part worked. The seat's DISPLAY name came
// from the constructor argument, and every construction site passed a hard-coded personal name, so the loading
// constructor never synced it -- the screen kept showing that name after a restart while the profile held "Ada".
// GameRenderer.cpp:564 draws getName() as the seat label, so this was visible on every launch.
TEST_CASE("U7: a reopened seat displays the persisted name, not a hard-coded placeholder", "[NameEntry][Persistence]")
{
    test_support::IsolatedHome home("name_display");
    {
        Game game(true);
        game.startNewGame();
        game.commitPlayerName("Ada");
    }

    Game reloaded(true);
    reloaded.startNewGame();
    CHECK(reloaded.testPlayer().getProfile().getName() == "Ada");   // persisted (was already true)
    CHECK(reloaded.testPlayer().getName() == "Ada");                // displayed (this is the fix)
}

TEST_CASE("U7: a fresh seat carries the neutral placeholder and still asks for a name", "[NameEntry]")
{
    test_support::IsolatedHome home("name_placeholder");
    Game game(true);
    game.startNewGame();

    CHECK(game.testPlayer().getName() == Constants::DEFAULT_PLAYER_NAME);
    CHECK(game.testPlayer().getProfile().getName().empty());
    CHECK(game.needsNameEntry() == true);
}

TEST_CASE("U7: an explicit commit overrides the placeholder in the live seat", "[NameEntry]")
{
    test_support::IsolatedHome home("name_commit");
    Game game(true);
    game.startNewGame();
    REQUIRE(game.testPlayer().getName() == Constants::DEFAULT_PLAYER_NAME);

    game.commitPlayerName("Grace O'Malley");
    CHECK(game.testPlayer().getName() == "Grace O'Malley");
    CHECK(game.testPlayer().getProfile().getName() == "Grace O'Malley");
}
