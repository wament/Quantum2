#pragma once
enum class GameSpeed
{
    Fast,
    Normal,
    Slow,
    AutoPlay,
    Hyper,
    Gamer
};

namespace GameTiming
{
// NPC reaction delay (seconds): how long each NPC waits before taking its next action.
constexpr float NPC_FAST = 0.3f;
constexpr float NPC_NORMAL = 1.0f;
constexpr float NPC_SLOW = 2.0f;
constexpr float NPC_AUTOPLAY = 0.15f;
constexpr float NPC_HYPER = 0.05f; // Hyper mode - near-instant NPC reactions, closest to the fastest the hardware can render

constexpr float DEALER_START_DELAY = 0.5f;
constexpr float DEALER_ACTION_DELAY = 0.5f;

// End-of-round feedback / overlay durations (seconds): how long per-hand results stay on screen.
constexpr float SUMMARY_FAST_AUTOPLAY = 2.0f;
constexpr float SUMMARY_NORMAL_SLOW = 5.0f;
constexpr float SUMMARY_HYPER = 0.3f; // Hyper mode - result overlay flashes just long enough to read

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
    case GameSpeed::Hyper:
        return NPC_HYPER;
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
    case GameSpeed::Hyper:
        return SUMMARY_HYPER;
    default:
        return SUMMARY_NORMAL_SLOW;
    }
}
inline float afterHandWait(GameSpeed speed) { return (speed == GameSpeed::AutoPlay) ? AUTOPLAY_AFTER_HAND_DELAY : 0.0f; }
inline float afterShoeWait(GameSpeed speed) { return (speed == GameSpeed::AutoPlay) ? AUTOPLAY_AFTER_SHOE_DELAY : 0.0f; }

// Default monitor refresh rate (Hz) used when Gamer mode can't detect the real hardware value (no display or XRandr
// unavailable). Set to this user's actual monitor rate (240Hz) so headless-safe logic stays fast out of the box.
constexpr float DEFAULT_REFRESH_HZ = 240.0f;

// Refresh-rate ("Gamer") mode derives its per-step timing from the monitor's refresh rate in Hz, so one gameplay step
// advances per frame and speed scales with hardware (e.g. 144Hz -> ~7ms/step). Pure/headless-safe: it takes an explicit
// Hz value because the monitor rate is a runtime graphics property detected by Game at startup (see setRefreshRateForTest)
// rather than an SFML call in the logic layer. Rates of <=1Hz collapse to DEFAULT_REFRESH_HZ instead of exploding.
inline float gamerStepInterval(float refreshRateHz)
{
    return (refreshRateHz > 1.0f) ? (1.0f / refreshRateHz) : DEFAULT_REFRESH_HZ;
}

// Per-card reveal pacing for the dealer turn. In the automated-fast tiers (Gamer refresh-rate-derived, Hyper fixed NPC
// pace) the dealer reveals cards at the SAME speed players act, so a fast auto-play reads as one continuous deal rather
// than players flashing by while the dealer drags along on a 0.5s beat. Every other speed keeps the fixed, watchable
// DEALER_ACTION_DELAY beat so a casual player can follow each card. Reuses npcActionDelay/gamerStepInterval (the same
// helpers the player path uses) so dealer and player pacing never drift apart.
inline float dealerRevealInterval(GameSpeed speed, float refreshRateHz)
{
    if (speed == GameSpeed::Gamer)
        return gamerStepInterval(refreshRateHz);
    if (speed == GameSpeed::Hyper)
        return npcActionDelay(speed);
    return DEALER_ACTION_DELAY;
}

// A speed is "continuous autoplay": every seat acts on basic strategy without waiting for input - AutoPlay plus the
// Hyper and refresh-rate Gamer tiers. Centralizing this means all state-machine autoplay gates stay consistent,
// mirroring the single timing table that keys all pacing. (Hyper/Gamer differ from AutoPlay only in pacing, not in
// automation.)
inline bool isAutoPlaying(GameSpeed speed) { return speed == GameSpeed::AutoPlay || speed == GameSpeed::Gamer || speed == GameSpeed::Hyper; }
} // namespace GameTiming

namespace Constants
{
// Window
constexpr unsigned WINDOW_WIDTH = 1600;
constexpr unsigned WINDOW_HEIGHT = 900;
constexpr const char* WINDOW_TITLE = "Blackjack";

// Panel layout
constexpr float PANEL_MARGIN = 18.f;

// HUD text font size
constexpr int HUD_FONT_SIZE = 24;

// HUD panel geometry. Both side panels read SIDE_WIDTH, so they are always the same width and the table stays
// centered between them. Every HUD position is derived from these values rather than placed by hand, which is
// what previously let the left/right panels drift apart when only one of them was resized.
namespace Hud
{
constexpr float SIDE_WIDTH = 240.f;   // width of BOTH the left and the right panel
constexpr float BOTTOM_HEIGHT = 96.f; // height of the bottom bar (spans the full window width)
constexpr float SIDE_HEIGHT = static_cast<float>(WINDOW_HEIGHT) - BOTTOM_HEIGHT;

// Left-panel chart internals, scaled with the panels so the plots stay proportional at any panel size.
constexpr float CHART_MARGIN = 10.f;         // inset of the stacked charts from the panel edge
constexpr float CHART_GAP = 18.f;            // vertical gap between stacked charts
constexpr float CHART_TITLE = 20.f;          // strip reserved at a chart's top for its title
constexpr float CHART_XAXIS = 20.f;          // strip reserved at a chart's bottom for x-axis ticks
constexpr float CHART_GUTTER = 38.f;         // left inset holding right-aligned y-axis value labels
constexpr float CHART_TICK = 5.f;            // x-axis tick length
constexpr int CHART_LABEL_FONT = 12;         // axis / legend / counter label size
constexpr int CHART_TITLE_FONT = 14;         // chart title + live value readout size
constexpr int CHART_Y_GRIDLINES = 3;         // horizontal gridlines per plot before it stops being readable
constexpr int CONTROLS_FONT_SIZE = 16;       // keyboard-controls line above the bottom bar
constexpr float CHART_BG_INSET = 8.f;        // chart background inset from its pane rect
} // namespace Hud

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

// Bets Placed chart: wagers are folded into BET_BUCKET_COUNT evenly spaced ranges spanning [0, MAXIMUM_BET], so the
// histogram always shows the same number of evenly spaced bars instead of one bar per distinct bet size.
constexpr int BET_BUCKET_COUNT = 10;
constexpr int BET_BUCKET_WIDTH = MAXIMUM_BET / BET_BUCKET_COUNT;

// Kelly betting constants
// Blackjack variance per bet (standard deviation ~1.14, variance ~1.3)
// Used in Kelly formula: f* = edge / variance
constexpr float BLACKJACK_VARIANCE = 1.37f;

// Edge Over Time model. The edge is a LEVEL that is re-estimated every round from the count as it stands, not a
// running total: the house edge does not accumulate hand over hand, it moves with the shoe. Basic strategy sits
// about half a percent behind the house on a fresh shoe, and each point of true count is worth roughly half a
// percent in the player's favour - so a +4 true count reads as +1.50%, and a shoe's worth of rounds stays inside
// roughly +-5%. (Accumulating the per-round edge instead of re-reading it is what used to drive the curve to -13%
// after ten rounds of an ordinary shoe.)
// The calibration is Hi-Lo's, so the true count fed in must be a Hi-Lo true count regardless of the style on
// screen - Reko is unbalanced and its raw count is not on the same scale.
constexpr double BASE_PLAYER_EDGE_PERCENT = -0.5;
constexpr double EDGE_PERCENT_PER_TRUE_COUNT = 0.5;

// Drawing positions
// DEALER_X sits 20px left of the center band's midpoint (the band between the two side panels), matching the
// offset it had when the band spanned 200..1200.
constexpr float DEALER_X = 780.f;
constexpr float DEALER_Y = 10.f;

constexpr float PLAYER_X = 620.f;
constexpr float PLAYER_Y = 470.f;

constexpr float DECK_X = 10.f;
constexpr float DECK_Y = 640.f;

constexpr float CHIP_RADIUS = 18.f;

constexpr float CHIP_BET_X_OFFSET = 24.f;

// Split-hand clamp band. Tied to the side-panel width (not fixed pixels) because the HUD draws *after* the
// table: a wide split clamped inside the old 120px margin would land underneath a panel and be hidden.
constexpr float HANDS_LEFT_MARGIN = Hud::SIDE_WIDTH + 20.f;
constexpr float HANDS_RIGHT_MARGIN = static_cast<float>(WINDOW_WIDTH) - Hud::SIDE_WIDTH - 20.f;

constexpr float BOX_PAD = 16.f;

constexpr float SPLIT_SPREAD_X = 80.f;
constexpr float SPLIT_SPREAD_Y = 80.f;

// Persistence (fallback path if HOME is not set)
constexpr const char* PLAYER_FILE_FALLBACK = "saves/player_profile.json";

// Sound & music asset locations + defaults (Feature 7). Volumes use SFML's 0..100 float range so they map
// directly onto sf::Sound::volume / sf::Music::setVolume with no conversion. Persisted as plain ints in the
// profile so the schema stays free of any SFML dependency (mirrors preferredSpeed/preferredCountingStyle).
namespace Sound
{
    constexpr const char* SOUND_DIR = "assets/sounds/";
    constexpr const char* MUSIC_DIR = "assets/music/";

    // Named effect clips, keyed by the event name Game plays. Kept in one place so the asset filenames, the
    // known-events list (SoundManager) and every playSound() call site all agree — edit here, don't duplicate.
    constexpr const char* EVENT_CARD = "card";
    constexpr const char* EVENT_BET = "bet";
    constexpr const char* EVENT_CHIP = "chip";
    constexpr const char* EVENT_BLACKJACK = "blackjack";
    constexpr const char* EVENT_WIN = "win";
    constexpr const char* EVENT_LOSE = "lose";

    // Music track played at the table and on menus.
    constexpr const char* MUSIC_TRACK = "theme";

    constexpr float DEFAULT_SOUND_VOLUME = 100.f;
    constexpr float DEFAULT_MUSIC_VOLUME = 60.f;
    constexpr bool DEFAULT_MUTED = false;
} // namespace Sound
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
