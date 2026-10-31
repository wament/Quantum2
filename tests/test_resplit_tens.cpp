#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>
#include <Hand.hpp>

// ---------------------------------------------------------------------------
// Feature 2 (Advanced Splitting): the configurable "resplit tens" rule. A fresh
// pair of tens is always splittable at the deal; this flag only gates re-splitting
// a ten-value hand that already came from a split. Non-ten pairs are unaffected.
// ---------------------------------------------------------------------------


// An ordinary pair of tens splits into two hands regardless of the flag (initial split is always allowed).
TEST_CASE("Initial split of ten-value cards is always allowed", "[SplitTens][Feature2]")
{
    const bool saved = Constants::RESPLIT_TENS_ALLOWED;
    Constants::RESPLIT_TENS_ALLOWED = false; // even with the rule off, an initial split must work

    Game game(true);
    // p:{10,10}, dealer hole 7, up card 6 (non-ace -> PlayerTurn).
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(10, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds)});

    CHECK(game.split());
    CHECK(game.testPlayer(0).hands.size() == 2);

    Constants::RESPLIT_TENS_ALLOWED = saved;
}

// With the rule on, re-splitting a ten-value hand that already came from a split produces another hand.
TEST_CASE("Resplitting tens is allowed when RESPLIT_TENS_ALLOWED is true", "[SplitTens][Feature2]")
{
    Constants::RESPLIT_TENS_ALLOWED = true;

    Game game(true);
    // p:{10,10}, dHole 7, dUp 6; then the next draw is another ten, making hand[0] a pair of tens again.
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(10, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(10, Suit::Clubs),   test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(3, Suit::Spades),   test_support::make_card(4, Suit::Hearts)});

    CHECK(game.split());                       // initial pair of tens -> two hands
    test_support::tickFrame(game);                            // first hand draws a ten -> {10,10} again
    auto& p = game.testPlayer(0);
    CHECK(p.hands[0].size() == 2);
    CHECK(p.hands[0].isSplit());               // already a split hand, so this is a re-split

    CHECK(game.split());                       // re-split tens -> three hands
    CHECK(p.hands.size() == 3);
}

// With the rule off, re-splitting a ten-value pair is refused (no new hand), while an ordinary initial split still works.
TEST_CASE("Resplitting tens is refused when RESPLIT_TENS_ALLOWED is false", "[SplitTens][Feature2]")
{
    Constants::RESPLIT_TENS_ALLOWED = false;

    Game game(true);
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(10, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(10, Suit::Clubs),   test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(3, Suit::Spades),   test_support::make_card(4, Suit::Hearts)});

    CHECK(game.split());                       // initial split of tens still allowed
    test_support::tickFrame(game);                            // first hand draws a ten -> {10,10}
    auto& p = game.testPlayer(0);
    int handsAfterInitialSplit = static_cast<int>(p.hands.size());

    CHECK_FALSE(game.split());                 // re-split of tens is blocked by the flag
    CHECK(static_cast<int>(p.hands.size()) == handsAfterInitialSplit); // no new hand created

    // The blocked re-split left the hand intact and actable, not finished.
    CHECK(p.hands[0].size() == 2);
    CHECK_FALSE(p.hands[0].isFinished());
}

// With tens re-splitting off, a non-ten pair (eights) can still be re-split — the flag only affects tens.
TEST_CASE("Non-ten pairs can still be re-split when RESPLIT_TENS_ALLOWED is false", "[SplitTens][Feature2]")
{
    Constants::RESPLIT_TENS_ALLOWED = false;

    Game game(true);
    // p:{8,8}, dHole 7, dUp 6; next draw another eight -> hand[0] a pair of eights again.
    test_support::start_round(game,{test_support::make_card(8, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(8, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(8, Suit::Clubs),   test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(3, Suit::Spades),   test_support::make_card(4, Suit::Hearts)});

    CHECK(game.split());                       // initial pair of eights -> two hands
    test_support::tickFrame(game);                            // first hand draws an eight -> {8,8}
    auto& p = game.testPlayer(0);

    CHECK(game.split());                       // re-split of eights still allowed despite the flag off
    CHECK(p.hands.size() == 3);
}
