// U9 -- no side-bet paytable is shown anywhere, so Perfect Pairs / 21+3 are offered with no odds visible (CONTEXT §... /
// NEXT_STEPS). SideBetEvaluator::paytableText() is the single source of truth for those odds; it is pure (no SFML), so headless
// tests assert the tiers without an SDL window, mirroring test_side_bets.cpp. Game::activeSideBetKind() gates whether that
// paytable is drawn during PlaceBets.

#include <catch2/catch_all.hpp>

#include "Card.hpp"
#include "Constants.hpp"
#include "Game.hpp"
#include "SideBet.hpp"
#include "test_support.hpp"

using namespace Constants;

TEST_CASE("U9: Perfect Pairs paytable lists all three tiers with correct odds", "[SideBets][U9]")
{
    const std::string t = SideBetEvaluator::paytableText(SideBetKind::PerfectPairs);

    REQUIRE(t.find("Mixed Pair 10:1") != std::string::npos);
    REQUIRE(t.find("Colored Pair 25:1") != std::string::npos);
    REQUIRE(t.find("Perfect Pair 30:1") != std::string::npos);
}

TEST_CASE("U9: 21+3 paytable lists every tier from Straight Flush 40:1 down to Pair 2:1", "[SideBets][U9]")
{
    const std::string t = SideBetEvaluator::paytableText(SideBetKind::TwentyOneThree);

    REQUIRE(t.find("Trips 30:1") != std::string::npos);
    REQUIRE(t.find("Straight Flush 40:1") != std::string::npos);
    REQUIRE(t.find("Flush 9:1") != std::string::npos);
    REQUIRE(t.find("Straight 5:1") != std::string::npos);
    REQUIRE(t.find("Pair 2:1") != std::string::npos);
}

TEST_CASE("U9: no side bet selected -> empty paytable so nothing is drawn", "[SideBets][U9]")
{
    REQUIRE(SideBetEvaluator::paytableText(SideBetKind::None).empty());
}

TEST_CASE("U9: a selected kind surfaces through Game::activeSideBetKind() so its paytable can be rendered", "[SideBets][U9]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::start_round(game, {test_support::make_card(1, Suit::Spades),
                                      test_support::make_card(9, Suit::Hearts),
                                      test_support::make_card(1, Suit::Spades),
                                      test_support::make_card(2, Suit::Diamonds)});

    game.placeSideBet(SideBetKind::PerfectPairs, 0); // selects kind; shows its paytable during PlaceBets even pre-deal
    REQUIRE(game.activeSideBetKind() == SideBetKind::PerfectPairs);
    REQUIRE(SideBetEvaluator::paytableText(game.activeSideBetKind()).find("30:1") != std::string::npos);

    // Cycling to the other side bet swaps which paytable is shown.
    game.placeSideBet(SideBetKind::TwentyOneThree, 0);
    REQUIRE(game.activeSideBetKind() == SideBetKind::TwentyOneThree);
}
