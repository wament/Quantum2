#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Constants.hpp"
#include "Game.hpp"
#include "SideBet.hpp"

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives in this top-level namespace (declared in Constants.hpp)

// ---------------------------------------------------------------------------
// Feature 1 (Side Bets): placing-phase semantics. test_side_bet_round.cpp proves the paytables survive the full
// placeSideBet -> deal() -> resolveSideBets path; this file proves how placeSideBet itself behaves while the player is
// still choosing -- the interaction bugs reported for the UI: you must be able to pick a kind before funding it,
// incrementing from empty must make the selection appear on screen, cycling kinds keeps an existing amount, cycling to
// off erases it, and decrementing to $0 must strand nothing. Each case drives Game(true) headlessly: startTestRound()
// builds the human seat with a live main hand, then placeSideBet mutates that hand's side-bet state exactly as during
// real play (drawSideBet / updateHUDState read it back every frame). No window/audio/network is exercised.
// ---------------------------------------------------------------------------

namespace
{

    // Build a headless game with the human seat dealt (so hands[0] exists and placeSideBet can mutate it live). Game is
    // non-copyable (SoundManager pImpl), so take it by reference instead of returning one by value.
    void startHuman(Game& game)
    {
        test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(9, Suit::Hearts),
                      test_support::make_card(1, Suit::Spades), test_support::make_card(2, Suit::Diamonds)});
    }
}

TEST_CASE("placeSideBet keeps a selected kind at $0 so it shows up before any amount is added", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    // Cycle to a kind with no money placed yet (the "cycle before adding" path). It must remain selectable/visible.
    game.placeSideBet(SideBetKind::PerfectPairs, 0);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 0);
}

TEST_CASE("incrementing from empty defaults to the first side bet so the selection appears on screen", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    // Pressing X/V with no kind chosen yet must surface a visible selection instead of staying invisible at None.
    game.placeSideBet(SideBetKind::None, 10);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 10);
}

TEST_CASE("cycling the kind keeps an existing funded wager", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    game.placeSideBet(SideBetKind::PerfectPairs, 20);
    // Simulate Z: switch to the other side bet while keeping whatever amount is already placed.
    game.placeSideBet(SideBetKind::TwentyOneThree, 20);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::TwentyOneThree);
    CHECK(hand.getSideBetWager() == 20);
}

TEST_CASE("cycling to off opts out and erases the selection", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    game.placeSideBet(SideBetKind::PerfectPairs, 30);
    // Simulate Z again: None is an explicit opt-out; the amount must be erased (the correct behavior).
    game.placeSideBet(SideBetKind::None, 0);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::None);
    CHECK(hand.getSideBetWager() == 0);
}

TEST_CASE("decrementing to $0 keeps the kind selectable instead of stranding it", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    // Simulate V down to zero: the selected kind stays put (so you can re-add or cycle) rather than being lost.
    game.placeSideBet(SideBetKind::PerfectPairs, 0);
    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 0);

    // And incrementing again rebuilds the amount from that kept selection.
    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    CHECK(game.testHand(0, 0).getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(game.testHand(0, 0).getSideBetWager() == 10);
}

TEST_CASE("canPlaceBets is true only in an interactive PlaceBets phase for the human seat", "[SideBets][Placing]")
{
    Game game(true); // headless; the predicate reads plain members, so no seats are needed.

    game.setRoundStateForTest(RoundState::PlaceBets);
    game.setGameSpeedForTest(GameSpeed::Normal);
    CHECK(game.canPlaceBets());

    // Auto-advance means the human cannot actually place bets, so the side-bet control line stays hidden.
    game.setGameSpeedForTest(GameSpeed::AutoPlay);
    CHECK(!game.canPlaceBets());

    // Outside the PlaceBets phase there is nothing to place.
    game.setRoundStateForTest(RoundState::PlayerTurn);
    CHECK(!game.canPlaceBets());
}
