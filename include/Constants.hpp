#pragma once
enum class GameSpeed
{
    Fast,
    Normal,
    Slow,
    AutoPlay
};

namespace GameTiming
{
// NPC reaction delay (seconds): how long each NPC waits before taking its next action.
constexpr float NPC_FAST = 0.3f;
constexpr float NPC_NORMAL = 1.0f;
constexpr float NPC_SLOW = 2.0f;
constexpr float NPC_AUTOPLAY = 0.15f;

constexpr float DEALER_START_DELAY = 0.5f;
constexpr float DEALER_ACTION_DELAY = 0.5f;

// End-of-round feedback / overlay durations (seconds): how long per-hand results stay on screen.
constexpr float SUMMARY_FAST_AUTOPLAY = 2.0f;
constexpr float SUMMARY_NORMAL_SLOW = 5.0f;

constexpr float AUTOPLAY_AFTER_HAND_DELAY = 1.0f; // hold between hands in AutoPlay
constexpr float AUTOPLAY_AFTER_SHOE_DELAY = 3.0f; // hold before dealing the next shoe

inline float npcActionDelay(GameSpeed speed)
{
    switch (speed)
    {
    case GameSpeed::Fast:
        return NPC_FAST;
    case GameSpeed::AutoPlay:
        return NPC_AUTOPLAY;
    case GameSpeed::Normal:
        return NPC_NORMAL;
    case GameSpeed::Slow:
        return NPC_SLOW;
    }
    return NPC_NORMAL;
}
inline float summaryDisplayTime(GameSpeed speed)
{
    switch (speed)
    {
    case GameSpeed::Fast:
    case GameSpeed::AutoPlay:
        return SUMMARY_FAST_AUTOPLAY;
    default:
        return SUMMARY_NORMAL_SLOW;
    }
}
inline float afterHandWait(GameSpeed speed) { return (speed == GameSpeed::AutoPlay) ? AUTOPLAY_AFTER_HAND_DELAY : 0.0f; }
inline float afterShoeWait(GameSpeed speed) { return (speed == GameSpeed::AutoPlay) ? AUTOPLAY_AFTER_SHOE_DELAY : 0.0f; }
} // namespace GameTiming

namespace Constants
{
// Window
constexpr unsigned WINDOW_WIDTH = 1400;
constexpr unsigned WINDOW_HEIGHT = 800;
constexpr const char* WINDOW_TITLE = "Blackjack";

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
constexpr int DEALER_STAND = 17;
constexpr int MAX_SPLIT_HANDS = 4;
constexpr float BLACKJACK_PAYOUT = 1.5; // 1.5 = 3:2, 1.2 = 6:5
constexpr bool DOUBLE_AFTER_SPLIT_ALLOWED = true;
constexpr bool RESPLIT_ACES_ALLOWED = true;
// Resplitting ten-value cards (10/J/Q/K) into a second/third/etc. hand after one has been drawn to it.
// A fresh pair of tens is always splittable at the deal regardless of this flag; it only gates *re*-splits.
// inline so tests can flip the rule at runtime (constexpr cannot be reassigned).
inline bool RESPLIT_TENS_ALLOWED = true;

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

constexpr float CHIP_BET_X_OFFSET = 24.f;

constexpr float HANDS_LEFT_MARGIN = 120.f;
constexpr float HANDS_RIGHT_MARGIN = WINDOW_WIDTH - 160.f;

constexpr float BOX_PAD = 16.f;

constexpr float SPLIT_SPREAD_X = 80.f;
constexpr float SPLIT_SPREAD_Y = 80.f;

// Persistence (fallback path if HOME is not set)
constexpr const char* PLAYER_FILE_FALLBACK = "saves/player_profile.json";
} // namespace Constants

// Gameplay configuration
namespace GameConfig
{
constexpr int STARTING_BANKROLL = 10000;
constexpr float KELLY_FRACTION = 0.33f;
constexpr int DEFAULT_PLAYER_COUNT = 1;

struct NpcConfig
{
    const char* name;
    int bankroll;
    int baseBet;
    int strategy;
};

constexpr int NPC_COUNT = 5;
constexpr NpcConfig NPC_CONFIGS[] = {
    {"Alice", 10000, 10, 0}, // Flat
    {"Bob", 10000, 10, 1},   // Fibonacci
    {"Carol", 10000, 10, 2}, // Paroli
    {"Dave", 10000, 50, 3},  // Martingale
    {"Eve", 10000, 50, 4}    // Kelly
};
} // namespace GameConfig
