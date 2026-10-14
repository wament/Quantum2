#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>
#include <Hand.hpp>

// ---------------------------------------------------------------------------
// Feature 2 (Advanced Splitting): the configurable "resplit tens" rule. A fresh
// pair of tens is always splittable at the deal; this flag only gates re-splitting
// a ten-value hand that already came from a split. Non-ten pairs are unaffected.
// ---------------------------------------------------------------------------

namespace
{
    Card makeCard(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    void tickOnce(Game& game)
    {
        RoundEngine engine;
        engine.tick(0.0f, game);
    }
}

// An ordinary pair of tens splits into two hands regardless of the flag (initial split is always allowed).
TEST_CASE("Initial split of ten-value cards is always allowed", "[SplitTens][Feature2]")
{
    const bool saved = Constants::RESPLIT_TENS_ALLOWED;
    Constants::RESPLIT_TENS_ALLOWED = false; // even with the rule off, an initial split must work

    Game game(true);
    // p:{10,10}, dealer hole 7, up card 6 (non-ace -> PlayerTurn).
    game.setDeck({makeCard(10, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(10, Suit::Hearts),  makeCard(6, Suit::Diamonds)});
    game.startTestRound();

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
    game.setDeck({makeCard(10, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(10, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(10, Suit::Clubs),   makeCard(2, Suit::Hearts),
                  makeCard(3, Suit::Spades),   makeCard(4, Suit::Hearts)});
    game.startTestRound();

    CHECK(game.split());                       // initial pair of tens -> two hands
    tickOnce(game);                            // first hand draws a ten -> {10,10} again
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
    game.setDeck({makeCard(10, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(10, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(10, Suit::Clubs),   makeCard(2, Suit::Hearts),
                  makeCard(3, Suit::Spades),   makeCard(4, Suit::Hearts)});
    game.startTestRound();

    CHECK(game.split());                       // initial split of tens still allowed
    tickOnce(game);                            // first hand draws a ten -> {10,10}
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
    game.setDeck({makeCard(8, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(8, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(8, Suit::Clubs),   makeCard(2, Suit::Hearts),
                  makeCard(3, Suit::Spades),   makeCard(4, Suit::Hearts)});
    game.startTestRound();

    CHECK(game.split());                       // initial pair of eights -> two hands
    tickOnce(game);                            // first hand draws an eight -> {8,8}
    auto& p = game.testPlayer(0);

    CHECK(game.split());                       // re-split of eights still allowed despite the flag off
    CHECK(p.hands.size() == 3);
}
