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
    constexpr int NUM_DECKS = 6;
    constexpr int BLACKJACK = 21;
    constexpr int DEALER_STAND = 17;
    constexpr int MAX_SPLIT_HANDS = 4;
    constexpr float BLACKJACK_PAYOUT = 1.5; // 1.5 = 3:2, 1.2 = 6:5

    // Betting Constants
    constexpr int MINIMUM_BET = 10;
    constexpr int MAXIMUM_BET = 1000;

    // Kelly betting constants
    // Blackjack variance per bet (standard deviation ~1.14, variance ~1.3)
    // Used in Kelly formula: f* = edge / variance
    constexpr float BLACKJACK_VARIANCE = 1.37f;

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

    // Auto-play timing (seconds)
    constexpr float AUTOPLAY_AFTER_HAND_DELAY = 1.0f;
    constexpr float AUTOPLAY_AFTER_SHOE_DELAY = 3.0f;
}

// Gameplay configuration
namespace GameConfig
{
    // Starting bankroll for all players (human and NPCs)
    constexpr int STARTING_BANKROLL = 10000;

    // Default Kelly fraction (1/3 Kelly ≈ 3% Risk of Ruin)
    constexpr float KELLY_FRACTION = 0.33f;

    // Default player count (1 = human only, up to 6)
    constexpr int DEFAULT_PLAYER_COUNT = 1;

    // NPC configuration
    // strategy values: 0=Kelly, 1=Flat, 2=Fibonacci, 3=Paroli, 4=Martingale
    struct NpcConfig
    {
        const char* name;
        int bankroll;
        int baseBet;
        int strategy;
    };

    constexpr int NPC_COUNT = 5;
    constexpr NpcConfig NPC_CONFIGS[] = {
        {"Alice", 10000, 10, 1},  // Flat
        {"Bob",   10000, 10, 2},  // Fibonacci
        {"Carol", 10000, 10, 3},  // Paroli
        {"Dave",  10000, 50, 4},  // Martingale
        {"Eve",   10000, 50, 0}   // Kelly
    };
}

// Summary overlay display time (seconds)
namespace SummaryTiming
{
    constexpr float FAST_AUTOPLAY = 2.0f;
    constexpr float NORMAL_SLOW = 5.0f;
}
