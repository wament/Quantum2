#include "SideBet.hpp"

#include <algorithm>
#include <string>

namespace
{
// Three ranks form a straight when consecutive, treating Ace as either low (A-2-3) or high (Q-K-A). Ranks are given
// sorted ascending; the only extra case beyond "consecutive" is the ace-high wheel {1, 12, 13}.
bool isStraight(int r0, int r1, int r2)
{
    if (r1 - r0 == 1 && r2 - r1 == 1)
        return true;
    return r0 == 1 && r1 == 12 && r2 == 13; // Ace high: Q-K-A
}
} // namespace

SideBetResult SideBetEvaluator::perfectPairs(const Card& a, const Card& b)
{
    if (!sameRank(a, b))
        return {SideBetKind::PerfectPairs, 0}; // different ranks -> nothing pays

    if (a.suit == b.suit)
        return {SideBetKind::PerfectPairs, 30};          // same rank AND same suit -> Perfect Pair 30:1
    if (sameColor(a, b))
        return {SideBetKind::PerfectPairs, 25};          // same rank, same color, different suit -> Colored Pair 25:1
    return {SideBetKind::PerfectPairs, 10};              // same rank, different color -> Mixed Pair 10:1
}

SideBetResult SideBetEvaluator::twentyOneThree(const Card& a, const Card& b, const Card& c)
{
    int ranks[3] = {a.value, b.value, c.value};
    std::sort(ranks, ranks + 3);
    const bool flush = (a.suit == b.suit) && (b.suit == c.suit);
    const bool trips = ranks[0] == ranks[2];                 // all three same rank
    const bool straight = isStraight(ranks[0], ranks[1], ranks[2]);
    // Exactly two equal: covered by "not trips but a pair exists": with only 3 cards, non-trips + any repeat is a pair.
    const bool hasPair = (ranks[0] == ranks[1]) || (ranks[1] == ranks[2]);

    // Highest tier first so overlapping hands resolve to their best payout.
    if (flush && straight)
        return {SideBetKind::TwentyOneThree, 40}; // Straight Flush 40:1
    if (trips)
        return {SideBetKind::TwentyOneThree, 30}; // Three of a Kind 30:1
    if (flush)
        return {SideBetKind::TwentyOneThree, 9};  // Flush 9:1
    if (straight)
        return {SideBetKind::TwentyOneThree, 5};  // Straight 5:1
    if (hasPair)
        return {SideBetKind::TwentyOneThree, 2};  // Pair 2:1
    return {SideBetKind::TwentyOneThree, 0};      // nothing qualified
}

std::string SideBetEvaluator::paytableText(SideBetKind kind)
{
    // Paytable tiers for a side bet as a display string (highest tier first), so the odds offered are visible in-game and can be
    // asserted headlessly without an SDL window (U9). Tier wording matches the documented paytables in include/SideBet.hpp.
    switch (kind)
    {
    case SideBetKind::PerfectPairs:
        return "Perfect Pairs\n"
               "Mixed Pair 10:1   Colored Pair 25:1   Perfect Pair 30:1";
    case SideBetKind::TwentyOneThree:
        return "21+3\n"
               "Trips 30:1   Straight Flush 40:1   Flush 9:1   Straight 5:1   Pair 2:1";
    default:
        return {};
    }
}
