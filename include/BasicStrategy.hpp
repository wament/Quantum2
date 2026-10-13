#pragma once

// Basic strategy domain logic: decision tables, card counting and Kelly betting, kept in its
// own module rather than bundled with unrelated window/betting config constants. The declarations
// below are defined exactly once in src/BasicStrategy.cpp.

namespace BasicStrategy
{
    enum class Action
    {
        HIT,
        STAND,
        DOUBLE,
        DOUBLE_STAND,
        SPLIT,
        SURRENDER
    };

    enum class PairAction
    {
        SPLIT,
        NO_SPLIT,
        SPLIT_IF_DAS
    };

    enum class HandType
    {
        HARD,
        SOFT,
        PAIR
    };

    enum class CountingStyle
    {
        HI_LO,
        REKO
    };

    const char* countingStyleName(CountingStyle style);

    char getRecommendedAction(
        int playerValue,
        int dealerUpValue,
        bool hasAce,
        bool isPair,
        int pairValue);

    int calculateKellyBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet);

    int calculateHiLoValue(const int bjValue);

    int updateHiLo(int currentCount, const int bjValue);

    // Reko: the user's two-level variant (unbalanced, compensated by a -4/deck starting offset). Card
    // values -> +1 for 2-7, 0 for 8/9, and -1 for Aces and ten-value cards.
    int calculateRekoValue(const int bjValue);

    // Generic per-card updater that dispatches on the active counting style.
    int updateCount(int currentCount, const int bjValue, CountingStyle style);

    float computeTrueCount(int runningCount, int cardsRemaining);
}
