#pragma once

namespace Constants
{
    // Window
    constexpr unsigned WINDOW_WIDTH  = 1400;
    constexpr unsigned WINDOW_HEIGHT = 800;
    constexpr const char* WINDOW_TITLE = "Blackjack";
    constexpr const float FONT_SIZE = 14.f;

    // Panel layout
    constexpr float PANEL_MARGIN = 15.f;

    // HUD text font size
    constexpr int HUD_FONT_SIZE = 20;

    // Card sprite sheet
    constexpr int CARD_WIDTH  = 64;
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

    // Persistence
    constexpr const char* PLAYER_FILE = "/saves/player_profile.json";
 
    
}


// Basic Strategy 
namespace BasicStrategy {
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

enum class HandType {
    HARD,
    SOFT,
    PAIR
}; 

const char HARD[10][10] =
{
/*       2   3   4   5   6   7   8   9   T   A */

 /*8*/ {'H','H','H','H','H','H','H','H','H','H'},

 /*9*/ {'H','D','D','D','D','H','H','H','H','H'},

/*10*/ {'D','D','D','D','D','D','D','D','H','H'},

/*11*/ {'D','D','D','D','D','D','D','D','D','D'},

/*12*/ {'H','H','S','S','S','H','H','H','H','H'},

/*13*/ {'S','S','S','S','S','H','H','H','H','H'},

/*14*/ {'S','S','S','S','S','H','H','H','H','H'},

/*15*/ {'S','S','S','S','S','H','H','H','H','H'},

/*16*/ {'S','S','S','S','S','H','H','H','H','H'},

/*17*/ {'S','S','S','S','S','S','S','S','S','S'}

};

const char SOFT[8][10] =
{
/*       2   3   4   5   6   7   8   9   T   A */

/*A2*/ {'H','H','H','D','D','H','H','H','H','H'},

/*A3*/ {'H','H','H','D','D','H','H','H','H','H'},

/*A4*/ {'H','H','D','D','D','H','H','H','H','H'},

/*A5*/ {'H','H','D','D','D','H','H','H','H','H'},

/*A6*/ {'H','D','D','D','D','H','H','H','H','H'},

/*A7*/ {'M','M','M','M','M','S','S','H','H','H'},

/*A8*/ {'S','S','S','S','M','S','S','S','S','S'},

/*A9*/ {'S','S','S','S','S','S','S','S','S','S'}

};

const char PAIRS[10][10] =
{
/*       2   3   4   5   6   7   8   9   T   A */

/*A,A*/ {'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y'},

/*10*/ {'N','N','N','N','N','N','N','N','N','N'},

/*9,9*/ {'Y','Y','Y','Y','Y','N','Y','Y','N','N'},

/*8,8*/ {'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y'},

/*7,7*/ {'Y','Y','Y','Y','Y','Y','N','N','N','N'},

/*6,6*/ {'M','Y','Y','Y','Y','N','N','N','N','N'},

/*5,5*/ {'N','N','N','N','N','N','N','N','N','N'},

/*4,4*/ {'N','N','N','M','M','N','N','N','N','N'},

/*3,3*/ {'M','M','Y','Y','Y','Y','N','N','N','N'},

/*2,2*/ {'M','M','Y','Y','Y','Y','N','N','N','N'}

};

const char SURRENDER[2][10] =
{
/*15*/
{' ',' ',' ',' ',' ',' ',' ',' ','R',' '},

/*16*/
{' ',' ',' ',' ',' ',' ',' ','R','R','R'}
};
}