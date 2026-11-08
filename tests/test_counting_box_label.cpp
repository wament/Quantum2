// U5 -- the right-panel "True Count" box mislabels Reko's running count as a "True Count", which reads as a broken negative on a
// fresh 6-deck shoe (-20.0) with no way for a learner to know it is intended (CONTEXT §... / NEXT_STEPS). Game::trueCountStatusLabel()
// is the single source of truth for that box title: pure, so a headless Game(true) sets the counting style and asserts the label.
// Hi-Lo keeps "True Count" (a genuine true count); Reko becomes "Betting Count (Reko)" because under Reko the number shown IS the
// running/betting count ("the true count is the running count" per the roadmap). HUDState.countingBoxLabel carries the same value so
// HUD.cpp applies it via StatBox::setTitle().

#include <catch2/catch_all.hpp>

#include "BasicStrategy.hpp"
#include "Card.hpp"
#include "Constants.hpp"
#include "Game.hpp"
#include "test_support.hpp"

using namespace Constants;

TEST_CASE("U5: hi-lo true-count box keeps a genuine 'True Count' label", "[HUD][U5]")
{
    Game game(true); // headless: the label is pure, no window or assets needed
    game.setCountingStyleForTest(BasicStrategy::CountingStyle::HI_LO);

    REQUIRE(game.trueCountStatusLabel() == "True Count");
}

TEST_CASE("U5: reko true-count box is labelled 'Betting Count (Reko)', not a misleading True Count", "[HUD][U5]")
{
    Game game(true);
    game.setCountingStyleForTest(BasicStrategy::CountingStyle::REKO);

    // Before U5 this box read "True Count" for a running count like -20 -> reads as broken. Now it is honest about what it shows.
    REQUIRE(game.trueCountStatusLabel() == "Betting Count (Reko)");
}

TEST_CASE("U5: HUDState carries the same live label so HUD.cpp applies it via setTitle", "[HUD][U5]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::start_round(game, {test_support::make_card(1, Suit::Spades),
                                      test_support::make_card(9, Suit::Hearts),
                                      test_support::make_card(1, Suit::Spades),
                                      test_support::make_card(2, Suit::Diamonds)});

    game.setCountingStyleForTest(BasicStrategy::CountingStyle::REKO);
    game.updateHUDState(); // syncCounting() populates hudState.countingBoxLabel from trueCountStatusLabel()
    REQUIRE(game.hudStateForTest().countingBoxLabel == "Betting Count (Reko)");

    game.setCountingStyleForTest(BasicStrategy::CountingStyle::HI_LO);
    game.updateHUDState();
    REQUIRE(game.hudStateForTest().countingBoxLabel == "True Count");
}
