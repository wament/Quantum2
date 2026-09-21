#pragma once

namespace Constants
{
    // Window
    constexpr unsigned WINDOW_WIDTH = 1400;
    constexpr unsigned WINDOW_HEIGHT = 800;
    constexpr const char *WINDOW_TITLE = "Blackjack";
    constexpr const float FONT_SIZE = 14.f;

    // Panel layout
    constexpr float PANEL_MARGIN = 15.f;

    // HUD text font size
    constexpr int HUD_FONT_SIZE = 20;

    // Card sprite sheet
    constexpr int CARD_WIDTH = 64;
    constexpr int CARD_HEIGHT = 64;

    constexpr int CARD_X_PADDING = 11;
    constexpr int CARD_Y_PADDING = 2;

    // Special card value used as the cut card
    constexpr int CUT_CARD_VALUE = 14;

    // Game rules
    constexpr int NUM_DECKS = 2;
    constexpr int BLACKJACK = 21;
    constexpr int DEALER_STAND = 17;
    constexpr int MAX_SPLIT_HANDS = 4;
    constexpr float BLACKJACK_PAYOUT = 1.5; // 1.5 = 3:2, 1.2 = 6:5

    // Betting Constants
    constexpr int MINIMUM_BET = 10;
    constexpr int MAXIMUM_BET = 500;

    // Drawing positions
    constexpr float DEALER_X = 680.f;
    constexpr float DEALER_Y = 10.f;

    constexpr float PLAYER_X = 620.f;
    constexpr float PLAYER_Y = 470.f;

    constexpr float DECK_X = 10.f;
    constexpr float DECK_Y = 640.f;

    constexpr float CHIP_RADIUS = 18.f;
    constexpr float CHIP_SPACING = 6.f;

    // Persistence (fallback path if HOME is not set)
    constexpr const char *PLAYER_FILE_FALLBACK = "saves/player_profile.json";

    // Rules

    constexpr bool DOUBLE_AFTER_SPLIT_ALLOWED = true;
    constexpr bool RESPLIT_ACES_ALLOWED = true;
}

// Basic Strategy
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

    // include/Constants.hpp — add to the BasicStrategy namespace

    // Shared basic strategy lookup used by both Game and Replay.
    // Parameters:
    //   playerValue     = total hand value (8-21)
    //   dealerUpValue   = dealer upcard blackjack value (2-11)
    //   hasAce          = true if hand contains an Ace
    //   isPair          = true if hand is a pair (two equal-value cards)
    //   pairValue       = value of the pair (only meaningful if isPair=true)
    // Returns: 'H', 'S', 'D', 'P', 'R', or ' ' (unknown)
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

// inline implementation

inline char BasicStrategy::getRecommendedAction(
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
            if (result == 'N')
                return 'S'; // don't split, stand on 10s
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

inline int BasicStrategy::calculateKellyBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet)
{
    if (trueCount < 2.0f)
        return minBet;

    float playerEdge = 0.005f * (trueCount - 1.0f) - 0.0005f;
    if (playerEdge <= 0.0f)
        return minBet;

    float rawBet = kellyFraction * static_cast<float>(bankroll) * playerEdge;
    int wager = static_cast<int>(rawBet);

    return std::max(minBet, std::min(maxBet, wager));
}

inline int BasicStrategy::calculateHiLoValue(const int bjValue)
{
    if (bjValue > 1 && bjValue < 7)
        return 1;
    else if (bjValue > 9 || bjValue == 1)
        return -1;
    return 0;
}

inline int BasicStrategy::updateHiLo(int currentCount, const int bjValue)
{
    return currentCount + calculateHiLoValue(bjValue);
}

inline float BasicStrategy::computeTrueCount(int runningCount, int cardsRemaining)
{
    float decksRemaining = cardsRemaining / 52.0f;
    if (decksRemaining < 0.5f)
        decksRemaining = 0.5f;
    return static_cast<float>(runningCount) / decksRemaining;
}