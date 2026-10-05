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

    char getRecommendedAction(
        int playerValue,
        int dealerUpValue,
        bool hasAce,
        bool isPair,
        int pairValue);

    int calculateKellyBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet);

    int calculateHiLoValue(const int bjValue);

    int updateHiLo(int currentCount, const int bjValue);

    float computeTrueCount(int runningCount, int cardsRemaining);
}
