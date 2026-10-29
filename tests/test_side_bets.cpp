#include <catch2/catch_all.hpp>

#include "SideBet.hpp"
#include "Constants.hpp"

using namespace Constants;

namespace
{
    Card makeCard(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }
}

// ---------------------------------------------------------------------------
// Perfect Pairs: the player's two cards. Same rank required; payout scales with
// how tightly the pair matches (Perfect 30 > Colored 25 > Mixed 10).
// ---------------------------------------------------------------------------
TEST_CASE("Perfect Pairs pays 30:1 for a same-rank same-suit pair", "[SideBets][PerfectPairs]")
{
    auto perfect = SideBetEvaluator::perfectPairs(makeCard(1, Suit::Spades), makeCard(1, Suit::Spades));
    CHECK(perfect.kind == SideBetKind::PerfectPairs);
    CHECK(perfect.multiplier == 30);
    CHECK(perfect.won());
}

TEST_CASE("Perfect Pairs pays 25:1 for a same-rank, same-color, different-suit pair", "[SideBets][PerfectPairs]")
{
    // Two red sevens (Hearts + Diamonds): same rank and color but not the same suit -> Colored Pair.
    auto colored = SideBetEvaluator::perfectPairs(makeCard(7, Suit::Hearts), makeCard(7, Suit::Diamonds));
    CHECK(colored.multiplier == 25);
    CHECK(colored.won());
}

TEST_CASE("Perfect Pairs pays 10:1 for a same-rank, different-color pair", "[SideBets][PerfectPairs]")
{
    // A black seven and a red seven: same rank but different color -> Mixed Pair.
    auto mixed = SideBetEvaluator::perfectPairs(makeCard(7, Suit::Spades), makeCard(7, Suit::Hearts));
    CHECK(mixed.multiplier == 10);
    CHECK(mixed.won());
}

TEST_CASE("Perfect Pairs pays nothing for two cards of different rank", "[SideBets][PerfectPairs]")
{
    auto none = SideBetEvaluator::perfectPairs(makeCard(5, Suit::Spades), makeCard(6, Suit::Hearts));
    CHECK(none.multiplier == 0);
    CHECK_FALSE(none.won());
}

// ---------------------------------------------------------------------------
// 21+3: the player's two cards plus the dealer's up-card (three total). Ranked
// highest-to-lowest; a hand counts toward its single best-paying tier.
// ---------------------------------------------------------------------------
TEST_CASE("21+3 pays 40:1 for a straight flush", "[SideBets][TwentyOneThree]")
{
    auto sf = SideBetEvaluator::twentyOneThree(makeCard(10, Suit::Spades), makeCard(11, Suit::Spades),
                                               makeCard(12, Suit::Spades)); // 10-J-Q all spades
    CHECK(sf.multiplier == 40);
    CHECK(sf.won());
}

TEST_CASE("21+3 pays 30:1 for three of a kind", "[SideBets][TwentyOneThree]")
{
    // Three aces of different suits -> trips, not a flush.
    auto trips = SideBetEvaluator::twentyOneThree(makeCard(1, Suit::Spades), makeCard(1, Suit::Hearts),
                                                  makeCard(1, Suit::Diamonds));
    CHECK(trips.multiplier == 30);
    CHECK(trips.won());
}

TEST_CASE("21+3 pays 9:1 for a flush (same suit, not a straight)", "[SideBets][TwentyOneThree]")
{
    auto flush = SideBetEvaluator::twentyOneThree(makeCard(2, Suit::Spades), makeCard(5, Suit::Spades),
                                                  makeCard(9, Suit::Spades));
    CHECK(flush.multiplier == 9);
    CHECK(flush.won());
}

TEST_CASE("21+3 pays 5:1 for a straight (consecutive, not all one suit)", "[SideBets][TwentyOneThree]")
{
    auto straight = SideBetEvaluator::twentyOneThree(makeCard(4, Suit::Hearts), makeCard(5, Suit::Diamonds),
                                                     makeCard(6, Suit::Clubs));
    CHECK(straight.multiplier == 5);
    CHECK(straight.won());
}

TEST_CASE("21+3 treats Ace as high for the Q-K-A straight", "[SideBets][TwentyOneThree]")
{
    auto ah = SideBetEvaluator::twentyOneThree(makeCard(12, Suit::Spades), makeCard(13, Suit::Hearts),
                                               makeCard(1, Suit::Diamonds)); // Q-K-A
    CHECK(ah.multiplier == 5);
    CHECK(ah.won());
}

TEST_CASE("21+3 pays 2:1 for a one-pair hand", "[SideBets][TwentyOneThree]")
{
    auto pair = SideBetEvaluator::twentyOneThree(makeCard(8, Suit::Hearts), makeCard(8, Suit::Clubs),
                                                 makeCard(3, Suit::Spades));
    CHECK(pair.multiplier == 2);
    CHECK(pair.won());
}

TEST_CASE("21+3 pays nothing for an unqualifying three-card combination", "[SideBets][TwentyOneThree]")
{
    auto none = SideBetEvaluator::twentyOneThree(makeCard(2, Suit::Spades), makeCard(5, Suit::Diamonds),
                                                 makeCard(9, Suit::Clubs));
    CHECK(none.multiplier == 0);
    CHECK_FALSE(none.won());
}

TEST_CASE("Three of a kind outranks the pair it would otherwise form", "[SideBets][TwentyOneThree]")
{
    // All-equal ranks must resolve to Trips (30), never fall back to Pair (2).
    auto trips = SideBetEvaluator::twentyOneThree(makeCard(10, Suit::Spades), makeCard(10, Suit::Hearts),
                                                  makeCard(10, Suit::Diamonds));
    CHECK(trips.multiplier == 30);
}

TEST_CASE("Straight flush outranks three of a kind", "[SideBets][TwentyOneThree]")
{
    // A hand that is both suited and (if ranks allowed) consecutive takes the highest tier.
    auto sf = SideBetEvaluator::twentyOneThree(makeCard(6, Suit::Clubs), makeCard(7, Suit::Clubs),
                                               makeCard(8, Suit::Clubs)); // 6-7-8 clubs -> straight flush
    CHECK(sf.multiplier == 40);
}
