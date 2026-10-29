#pragma once

#include "GameRules.hpp"

namespace BasicStrategy
{
    enum class CountingStyle
    {
        HI_LO,
        REKO
    };

    const char* countingStyleName(CountingStyle style);

    char getRecommendedAction(int playerValue,int dealerUpValue,bool hasAce,bool isPair,int pairValue);

    // Feature 3 (Custom Game Rules): rules-aware advisor. Identical decision tables to the plain overload, but the
    // double-after-split ('M' pairs) and surrender gates read from `rules` instead of compile-time Constants. The plain
    // overload above delegates here with default rules so every existing advisor test keeps passing unchanged.
    char getRecommendedAction(const GameRules& rules,int playerValue,int dealerUpValue,bool hasAce,bool isPair,int pairValue);

    int calculateKellyBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet);

    int calculateHiLoValue(const int bjValue);

    int updateHiLo(int currentCount, const int bjValue);

    // Reko: the user's two-level variant (unbalanced, compensated by a -4/deck starting offset). Card
    // values -> +1 for 2-7, 0 for 8/9, and -1 for Aces and ten-value cards.
    int calculateRekoValue(const int bjValue);

    // Reko unbalanced starting running-count offset for a shoe of `decks` decks, per the roadmap table:
    // 1 deck -1, 2 decks -5, 4 decks -12, 6 decks -20, 8 decks -27. Deck counts outside that set fall back to
    // the nearest lower bracket (so 5 decks behaves like 4, 9+ decks stays capped at the 8-deck value).
    int rekoStartingOffset(int decks);

    // Reko betting pattern: TC <= 0 -> 1 unit, 0 < TC < 2 -> 2 units, TC > 2 -> floor(1 + TC) units. Each unit is
    // `baseBet` (the table minimum), and the result is clamped to [minBet, maxBet].
    int calculateRekoBet(float trueCount, int baseBet, int minBet, int maxBet);

    // Generic per-card updater that dispatches on the active counting style.
    int updateCount(int currentCount, const int bjValue, CountingStyle style);

    float computeTrueCount(int runningCount, int cardsRemaining); //hi-lo

    // The player's estimated edge, as a percentage, at a given true count. This is a LEVEL, not a total: the
    // Edge Over Time chart re-estimates it every round from the count as it stands, so the curve wobbles around
    // the baseline instead of ramping up round after round. At true count 0 it is the fresh-shoe figure
    // (Constants::BASE_PLAYER_EDGE_PERCENT, about -0.5% for basic strategy), moving by
    // Constants::EDGE_PERCENT_PER_TRUE_COUNT per point of count.
    // `trueCount` must be a Hi-Lo true count - that is what the calibration is measured against. Reko is
    // unbalanced and its raw running count is not on the same scale.
    float estimateEdgePercent(float trueCount);
}
