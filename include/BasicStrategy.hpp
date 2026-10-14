#pragma once

namespace BasicStrategy
{
    enum class CountingStyle
    {
        HI_LO,
        REKO
    };

    const char* countingStyleName(CountingStyle style);

    char getRecommendedAction(int playerValue,int dealerUpValue,bool hasAce,bool isPair,int pairValue);

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
}
