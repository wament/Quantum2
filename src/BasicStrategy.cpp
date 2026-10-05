#include "BasicStrategy.hpp"
#include "Constants.hpp"
#include <algorithm>

// Decision tables indexed [playerValue - base][dealerUpValue - 2]. These are the same
// tables that previously lived inside Constants.hpp; defining them once here (with the
// default internal linkage of a namespace-scope const) keeps every translation unit's
// copy independent while exposing only the functions declared in BasicStrategy.hpp.

namespace BasicStrategy
{
    const char HARD[10][10] =
        {
            /*       2   3   4   5   6   7   8   9   T   A */

            /*8*/ {'H', 'H', 'H', 'H', 'H', 'H', 'H', 'H', 'H', 'H'},

            /*9*/ {'H', 'D', 'D', 'D', 'D', 'H', 'H', 'H', 'H', 'H'},

            /*10*/ {'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'H', 'H'},

            /*11*/ {'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D'},

            /*12*/ {'H', 'H', 'S', 'S', 'S', 'H', 'H', 'H', 'H', 'H'},

            /*13*/ {'S', 'S', 'S', 'S', 'S', 'H', 'H', 'H', 'H', 'H'},

            /*14*/ {'S', 'S', 'S', 'S', 'S', 'H', 'H', 'H', 'H', 'H'},

            /*15*/ {'S', 'S', 'S', 'S', 'S', 'H', 'H', 'H', 'H', 'H'},

            /*16*/ {'S', 'S', 'S', 'S', 'S', 'H', 'H', 'H', 'H', 'H'},

            /*17*/ {'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S'}

    };

    const char SOFT[8][10] =
        {
            /*       2   3   4   5   6   7   8   9   T   A */

            /*A2*/ {'H', 'H', 'H', 'D', 'D', 'H', 'H', 'H', 'H', 'H'},

            /*A3*/ {'H', 'H', 'H', 'D', 'D', 'H', 'H', 'H', 'H', 'H'},

            /*A4*/ {'H', 'H', 'D', 'D', 'D', 'H', 'H', 'H', 'H', 'H'},

            /*A5*/ {'H', 'H', 'D', 'D', 'D', 'H', 'H', 'H', 'H', 'H'},

            /*A6*/ {'H', 'D', 'D', 'D', 'D', 'H', 'H', 'H', 'H', 'H'},

            /*A7*/ {'D', 'D', 'D', 'D', 'D', 'S', 'S', 'H', 'H', 'H'},

            /*A8*/ {'S', 'S', 'S', 'S', 'D', 'S', 'S', 'S', 'S', 'S'},

            /*A9*/ {'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S'}

    };

    // if DAS is not allowed, M = Hit. Otherwise M = Y (split)
    const char PAIRS[10][10] =
        {
            /*       2   3   4   5   6   7   8   9   T   A */

            /*A,A*/ {'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y'},

            /*10*/ {'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N'},

            /*9,9*/ {'Y', 'Y', 'Y', 'Y', 'Y', 'N', 'Y', 'Y', 'N', 'N'},

            /*8,8*/ {'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y'},

            /*7,7*/ {'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'N', 'N', 'N', 'N'},

            /*6,6*/ {'M', 'Y', 'Y', 'Y', 'Y', 'N', 'N', 'N', 'N', 'N'},

            /*5,5*/ {'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N'},

            /*4,4*/ {'N', 'N', 'N', 'M', 'M', 'N', 'N', 'N', 'N', 'N'},

            /*3,3*/ {'M', 'M', 'Y', 'Y', 'Y', 'Y', 'N', 'N', 'N', 'N'},

            /*2,2*/ {'M', 'M', 'Y', 'Y', 'Y', 'Y', 'N', 'N', 'N', 'N'}

    };

    const char SURRENDER[2][10] =
        {
            /*15*/
            {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'R', ' '},
            /*16*/
            {' ', ' ', ' ', ' ', ' ', ' ', ' ', 'R', 'R', 'R'}};

    char getRecommendedAction(
        int playerValue, int dealerUpValue, bool hasAce, bool isPair, int pairValue)
    {
        if (dealerUpValue < 2 || dealerUpValue > 11)
            return playerValue >= 17 ? 'S' : 'H';

        int dealerIdx = dealerUpValue - 2;

        // Soft hands (Ace counting as 11, value 13-20)
        if (hasAce && playerValue >= 13 && playerValue <= 20)
        {
            int row = playerValue - 13;
            if (row >= 0 && row < 8 && dealerIdx >= 0 && dealerIdx < 10)
            {
                char result = SOFT[row][dealerIdx];
                if (result == 'Y')
                    return 'H';
                if (result == 'M')
                    return 'S';
                return result; // H, D, S
            }
            return playerValue >= 17 ? 'S' : 'H';
        }

        // Pair splitting
        if (isPair)
        {
            int row = -1;
            if (pairValue == 11)
                row = 0;
            else if (pairValue == 10)
                row = 1;
            else
                row = 11 - pairValue; // 9->2, 8->3, ..., 2->9

            if (row >= 0 && row < 10 && dealerIdx >= 0 && dealerIdx < 10)
            {
                char result = PAIRS[row][dealerIdx];
                if (result == 'Y')
                    return 'P';
                if (result == 'M')
                    if (Constants::DOUBLE_AFTER_SPLIT_ALLOWED)
                        return 'P';
                    else
                        return 'H';
                // 'N' = don't split, fall through to hard hand logic
            }
        }

        // Surrender check for 15 and 16
        if (dealerIdx >= 0 && dealerIdx < 10)
        {
            if (playerValue == 15 && SURRENDER[0][dealerIdx] != ' ')
                return SURRENDER[0][dealerIdx];
            if (playerValue == 16 && SURRENDER[1][dealerIdx] != ' ')
                return SURRENDER[1][dealerIdx];
        }

        // Hard hands 8-17
        if (playerValue >= 8 && playerValue <= 17)
        {
            int row = playerValue - 8;
            if (row >= 0 && row < 10 && dealerIdx >= 0 && dealerIdx < 10)
                return HARD[row][dealerIdx];
        }

        // Defaults
        return playerValue >= 17 ? 'S' : 'H';
    }

    int calculateKellyBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet)
    {
        if (trueCount < 2.0f)
            return minBet;

        // Player edge based on true count (Hi-Lo system)
        // At TC=2, edge is about 0.45%, increasing by ~0.5% per TC point
        float playerEdge = 0.005f * (trueCount - 1.0f) - 0.0005f;
        if (playerEdge <= 0.0f)
            return minBet;

        // Kelly criterion for multi-outcome bets: f* = edge / variance
        // For blackjack, variance per bet is approximately 1.3
        float kellyFractionOptimal = playerEdge / Constants::BLACKJACK_VARIANCE;

        // Apply user's Kelly multiplier (0.0 to 1.0 = 0% to full Kelly)
        // Using less than full Kelly reduces volatility significantly
        // while sacrificing relatively little growth (half Kelly = 75% of growth)
        float rawBet = kellyFraction * kellyFractionOptimal * static_cast<float>(bankroll);
        int wager = static_cast<int>(rawBet);

        return std::max(minBet, std::min(maxBet, wager));
    }

    int calculateHiLoValue(const int bjValue)
    {
        if (bjValue > 1 && bjValue < 7)
            return 1;
        else if (bjValue > 9 || bjValue == 1)
            return -1;
        return 0;
    }

    int updateHiLo(int currentCount, const int bjValue)
    {
        return currentCount + calculateHiLoValue(bjValue);
    }

    float computeTrueCount(int runningCount, int cardsRemaining)
    {
        float decksRemaining = cardsRemaining / 52.0f;
        if (decksRemaining < 0.5f)
            decksRemaining = 0.5f;
        return static_cast<float>(runningCount) / decksRemaining;
    }
}

