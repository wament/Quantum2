#pragma once

namespace BasicStrategy
{
enum class CountingStyle
{
    HI_LO,
    REKO
};

const char* countingStyleName(CountingStyle style);

char getRecommendedAction(int playerValue, int dealerUpValue, bool hasAce, bool isPair, int pairValue);

int calculateKellyBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet);

int calculateHiLoValue(const int bjValue);

int updateHiLo(int currentCount, const int bjValue);

// Reko: the user's two-level variant (unbalanced, compensated by a -4/deck starting offset). Card
// values -> +1 for 2-7, 0 for 8/9, and -1 for Aces and ten-value cards.
int calculateRekoValue(const int bjValue);

// Generic per-card updater that dispatches on the active counting style.
int updateCount(int currentCount, const int bjValue, CountingStyle style);

float computeTrueCount(int runningCount, int cardsRemaining); // hi-lo

    // For Reko the effective true count is simply the running count (no decks-remaining division).
    inline float rekoTrueCount(int runningCount)
    {
        return static_cast<float>(runningCount);
    }

    // Starting running-count offset for a fresh shoe of the given number of decks (unbalanced Reko system).
    // Roadmap values: -1 single, -5 two, -12 four, -20 six, -27 eight deck; interpolated in between.
    inline int rekoStartingOffset(int numDecks)
    {
        static const int offsets[] = {-1, -1, -5, -8, -12, -16, -20, -23, -27}; // index 0..8 decks
        if (numDecks < 0 || numDecks > 8)
            return -20; // sensible default for an unrecognised shoe size
        return offsets[numDecks];
    }

    // Reko bet sizing from the effective true count: TC<=0 -> 1 unit, 0<TC<2 -> 2 units, TC>2 -> (1+TC) units.
    inline int calculateRekoUnits(int trueCount)
    {
        if (trueCount <= 0)
            return 1;
        if (trueCount < 2)
            return 2;
        return 1 + static_cast<int>(trueCount);
    }

    inline int calculateRekoBet(int trueCount, int baseUnit, int minBet, int maxBet)
    {
        int bet = calculateRekoUnits(trueCount) * baseUnit;
        if (bet < minBet)
            return minBet;
        if (bet > maxBet)
            return maxBet;
        return bet;
    }
} // namespace BasicStrategy
