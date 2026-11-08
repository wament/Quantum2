#pragma once

#include <string>
#include <vector>

#include "Card.hpp"

// Side bets: optional wagers placed before the deal, paying out on specific card combinations,
// independent of the main hand's win/loss; two are implemented — see paytable below.
// - Perfect Pairs (player's two cards): Mixed Pair 10:1, Colored Pair 25:1, Perfect Pair 30:1
// - 21+3 (player's two cards + dealer up-card, i.e. three cards): Trips 30:1, Straight Flush 40:1,
// Flush 9:1, Straight 5:1, Pair 2:1
enum class SideBetKind
{
    None,
    PerfectPairs,
    TwentyOneThree
};

// Payout of evaluating a side bet's cards against its paytable. `multiplier` is the payout RATIO that was hit
// (30 for a 30:1 result); it is 0 when no tier qualified and therefore the wager is lost. `won()` mirrors that so
// callers never compare an int against a magic zero directly.
struct SideBetResult
{
    SideBetKind kind = SideBetKind::None;
    int multiplier = 0; // payout ratio hit (30 for 30:1); 0 when nothing paid

    [[nodiscard]] bool won() const { return multiplier != 0; }
};

// Pure, side-effect-free evaluation of the two side bets. Kept in its own module
// from game state: this is the highest-ROI logic for and is exercised directly by test_side_bets.cpp.
class SideBetEvaluator
{
  public:
    // Perfect Pairs uses only the player's two cards.
    static SideBetResult perfectPairs(const Card& a, const Card& b);

    // 21+3 uses the player's two cards plus the dealer's up-card (exactly three cards total).
    static SideBetResult twentyOneThree(const Card& a, const Card& b, const Card& c);

    // Paytable tiers for a side bet as a display string (one line per tier, highest first). Pure -- shared by GameRenderer and
    // headless tests so the odds offered are asserted without an SDL window; mirrors perfectPairs()/twentyOneThree().
    static std::string paytableText(SideBetKind kind);
};

// Rank of a card is its raw `Card::value` (Ace=1 .. King=13). Color: Hearts/Diamonds are red, Spades/Clubs black.
// These free helpers are used by the evaluator and shared
inline bool sameRank(const Card& a, const Card& b) { return a.value == b.value; }
inline bool isRed(const Card& c) { return c.suit == Suit::Hearts || c.suit == Suit::Diamonds; }
inline bool sameColor(const Card& a, const Card& b) { return isRed(a) == isRed(b); }
