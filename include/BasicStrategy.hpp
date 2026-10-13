#pragma once

namespace BasicStrategy
{
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

    float computeTrueCount(int runningCount, int cardsRemaining); //hi-lo
}
