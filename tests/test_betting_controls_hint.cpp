// U4 -- betting-phase control hints used two competing formats ("Up/Down: set bet   Left: optimal" vs "[Up/Down | Adjust Bet]") and
// could show actions irrelevant to the current round state. Game::bettingControlsHint() / sideBetControlsHint() are the single source
// of truth for those two lines: pure, so a headless Game(true) asserts one consistent format with only relevant keys (CONTEXT §... /
// NEXT_STEPS). Each key is a real PlaceBets binding from InputHandler.cpp.

#include <catch2/catch_all.hpp>

#include "Game.hpp"

TEST_CASE("U4: betting hint uses one consistent format with clear, round-relevant keys", "[HUD][U4]")
{
    Game game(true); // headless: the hints are pure, no window or assets needed
    const std::string h = game.bettingControlsHint();

    REQUIRE(!h.empty());
    // Clear labels, not the old "Left: optimal" vagueness; real PlaceBets bindings (InputHandler.cpp).
    REQUIRE(h.find("Auto Kelly") != std::string::npos);
    REQUIRE(h.find("Increase Bet") != std::string::npos);
    REQUIRE(h.find("Confirm") != std::string::npos);

    // Only betting-phase controls here -- no player-turn actions (H/S/D/P/Y) that are irrelevant during PlaceBets.
    for (const char* action : {"Hit", "Stand", "Split", "Surrender"})
        REQUIRE(h.find(action) == std::string::npos);
}

TEST_CASE("U4: side-bet hint shares one consistent format and names the real Z/X/V keys", "[HUD][U4]")
{
    Game game(true);
    const std::string h = game.sideBetControlsHint();

    REQUIRE(h.find("Z") != std::string::npos); // cycle side-bet type
    REQUIRE(h.find("X") != std::string::npos); // raise wager by a chip
    REQUIRE(h.find("V") != std::string::npos); // lower wager by a chip
}
