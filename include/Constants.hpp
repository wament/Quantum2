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
// NPC reaction delay per speed (seconds); selected by npcActionDelay().
constexpr float NPC_FAST = 0.3f;
constexpr float NPC_NORMAL = 1.0f;
constexpr float NPC_SLOW = 2.0f;
constexpr float NPC_AUTOPLAY = 0.15f;
constexpr float NPC_HYPER = 0.05f; // Hyper: near-instant reactions, fastest the hardware can render

constexpr float DEALER_START_DELAY = 0.5f;
constexpr float DEALER_ACTION_DELAY = 0.5f;

// End-of-round overlay durations (seconds): how long a per-hand result stays on screen.
constexpr float SUMMARY_FAST_AUTOPLAY = 2.0f;
constexpr float SUMMARY_NORMAL_SLOW = 5.0f;
constexpr float SUMMARY_HYPER = 0.3f; // Hyper: just long enough to read

// How long a just-unlocked achievement toast stays on screen before it clears (seconds).
constexpr float ACHIEVEMENT_TOAST_SECONDS = 4.0f;

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

// Default monitor refresh rate (Hz) when Gamer mode can't detect hardware (no display/XRandr). Set to this user's real
// rate (240Hz) so headless-safe logic stays fast out of the box.
constexpr float DEFAULT_REFRESH_HZ = 240.0f;

// Refresh-rate ("Gamer") timing: one gameplay step per frame, scaling with hardware (144Hz -> ~7ms/step). Pure/headless-
// safe: takes an explicit Hz (the monitor rate is a runtime graphics property detected by Game at startup, see
// setRefreshRateForTest) rather than an SFML call in the logic layer. <=1Hz collapses to DEFAULT_REFRESH_HZ, not infinity.
inline float gamerStepInterval(float refreshRateHz)
{
    return (refreshRateHz > 1.0f) ? (1.0f / refreshRateHz) : DEFAULT_REFRESH_HZ;
}

// Dealer reveal pacing per speed. Automated-fast tiers (Gamer refresh-rate-derived, Hyper fixed NPC pace) reveal at the
// same speed players act, so a fast auto-play reads as one continuous deal; every other speed keeps the watchable
// DEALER_ACTION_DELAY beat for a casual player. Reuses npcActionDelay/gamerStepInterval
// drift apart.
inline float dealerRevealInterval(GameSpeed speed, float refreshRateHz)
{
    if (speed == GameSpeed::Gamer)
        return gamerStepInterval(refreshRateHz);
    if (speed == GameSpeed::Hyper)
        return npcActionDelay(speed);
    return DEALER_ACTION_DELAY;
}

// a per-speed scale factor multiplies every base Feature-8 deal duration
// (DEAL_SLIDE / DEAL_STAGGER / DEALER_HOLE_FLIP)
// pace. Fixes the Fast desync where seat highlights + sound raced ahead of cards sliding at a constant 0.32s beat: faster
// speeds compress, slower stretch for visibility. Hyper/Gamer deliver no dealing animation (factor 0 -> tween settles
// instantly). Mirrors npcActionDelay/summaryDisplayTime's "one timing table" pattern.
inline float dealDurationScale(GameSpeed speed)
{
    switch (speed)
    {
    case GameSpeed::Slow:     return 2.0f;   // stretch for a watchable, cinematic sweep
    case GameSpeed::Normal:   return 1.0f;   // unchanged base feel
    case GameSpeed::Fast:     return 0.4f;   // compress so cards land near the sound/action
    case GameSpeed::AutoPlay: return 0.3f;   // continuous automation -> snappiest manual-or-auto watchable tier
    case GameSpeed::Hyper:    return 0.0f;   // animation disabled
    case GameSpeed::Gamer:    return 0.0f;   // animation disabled
    }
    return 1.0f;
}

// True only for the pure-speed tiers (Hyper/Gamer) where dealing animations are intentionally not delivered
// presentation callers can skip them outright: the single source of "no animation here" (mirrors isAutoPlaying's pattern;
// used by GameRenderer for animations and Game.cpp for sound effects).
inline bool animationDisabled(GameSpeed speed) { return speed == GameSpeed::Hyper || speed == GameSpeed::Gamer; }

// "Continuous autoplay": every seat acts on basic strategy without waiting for input: AutoPlay plus Hyper and refresh-
// rate Gamer tiers. Centralized so all state-machine autoplay gates stay consistent (Hyper/Gamer differ from AutoPlay only
// in pacing, not automation).
inline bool isAutoPlaying(GameSpeed speed) { return speed == GameSpeed::AutoPlay || speed == GameSpeed::Gamer || speed == GameSpeed::Hyper; }
} // namespace GameTiming

namespace Constants
{
// Window
constexpr unsigned WINDOW_WIDTH = 1600;
constexpr unsigned WINDOW_HEIGHT = 900;
// Max characters a player may enter on the name-entry screen (U7). Kept as a named constant so InputHandler's clamp and the
// renderer's "n chars" hint read from one value instead of two magic numbers.
constexpr unsigned MAX_PLAYER_NAME_LENGTH = 24;

// The neutral display name for a human seat that has not been named yet. It is a PLACEHOLDER only: the name-entry
// screen (AppMode::NameEntry) replaces it on first startup, and Player's loading constructor adopts the persisted
// profile name on every later launch. No production site may hard-code a personal name -- the seat label at
// GameRenderer.cpp:564 draws Player::getName(), so a hard-coded name reappears on screen on every launch.
constexpr const char* DEFAULT_PLAYER_NAME = "Player";
constexpr const char* WINDOW_TITLE = "Blackjack";

// Panel layout
constexpr float PANEL_MARGIN = 18.f;

// HUD text font size
constexpr int HUD_FONT_SIZE = 24;

// HUD panel geometry. Both side panels read SIDE_WIDTH
// deriving every HUD position from these (rather than hand-placement) is what stopped the left/right panels drifting apart
// when only one was resized.
namespace Hud
{
constexpr float SIDE_WIDTH = 240.f;   // width of BOTH the left and the right panel
constexpr float BOTTOM_HEIGHT = 96.f; // height of the bottom bar (spans the full window width)
constexpr float SIDE_HEIGHT = static_cast<float>(WINDOW_HEIGHT) - BOTTOM_HEIGHT;

// Left-panel chart internals, scaled with the panels
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
// Sentinel for an unlimited split-hand cap (the rules UI exposes "Unlimited" as this value). Guarded explicitly at
// every enforcement site so a size_t-vs-int comparison can't accidentally read it as a large finite cap.
constexpr int MAX_SPLIT_HANDS_UNLIMITED = -1;
// T2: cap completed-shoe history (PlayerProfile::longHistory) at this many shoes; older shoes roll into an aggregate carry so
// career totals survive exactly. Bounded, additive, and a no-op for profiles with fewer completed shoes than the cap.
constexpr std::size_t MAX_LONG_HISTORY_SHOES = 100;
// T1: rewrite the profile to disk at most once per this many human hands played (auto-save cadence) instead of every round, reducing
// save amplification. Forced writes at shoe boundary / pause-quit / exit are unaffected by this cadence.
constexpr unsigned AUTO_SAVE_EVERY_HANDS = 5;
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

// Bets Placed chart: wagers are folded into BET_BUCKET_COUNT evenly spaced ranges. The axis spans [MINIMUM_BET, 100] by
// default and grows to the largest wager placed in the active view window once that exceeds 100, hard-capped at
// MAXIMUM_BET (see HUD::computeAxisMax, §10 decision #8). The histogram always shows the same number of evenly spaced
// bars instead of one bar per distinct bet size.
constexpr int BET_BUCKET_COUNT = 10;
// BET_BUCKET_WIDTH was removed 2026-11-12: it was MAXIMUM_BET / BET_BUCKET_COUNT, i.e. a FIXED axis width, and U12
// replaced that model with an axis that grows with the largest wager in the view window. The live bucket width is
// therefore computed per view (HUD.cpp: (axisHi - axisLo) / BET_BUCKET_COUNT), so a compile-time constant cannot
// describe it. It had zero users when deleted.

// Kelly betting constants. f* = edge / variance, with BLACKJACK_VARIANCE ~1.37 (std-dev ~1.14 per bet).
constexpr float BLACKJACK_VARIANCE = 1.37f;

// Edge Over Time model: the house edge is a LEVEL re-estimated each round from the count as it stands, not a running
// total: it moves with the shoe, it does not accumulate hand over hand. Baseline sits ~0.5% behind on a fresh shoe and
// each true-count point is ~0.5% in the player's favour
// Accumulating per-round edge (vs re-reading) is what used to drive the curve to -13% after ten rounds. Calibration is
// Hi-Lo's: feed a Hi-Lo true count even if Reko is on screen (Reko is unbalanced and not on this scale).
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

// the shoe/deck stack sits upper-right near the dealer
// dealer cards alike -- slides out from one visible stacked deck instead of popping in from a corner. Fractional of
// window size keeps it inside the usable band [HANDS_LEFT_MARGIN, HANDS_RIGHT_MARGIN] when the window is resized.
constexpr float SHOE_X = static_cast<float>(WINDOW_WIDTH) * 0.76f;
constexpr float SHOE_Y = static_cast<float>(WINDOW_HEIGHT) * 0.04f;

constexpr float CHIP_RADIUS = 18.f;

constexpr float CHIP_BET_X_OFFSET = 24.f;

// Split-hand clamp band, tied to side-panel width (not fixed pixels): the HUD draws *after* the table
// clamped inside the old 120px margin would land under a panel and be hidden.
constexpr float HANDS_LEFT_MARGIN = Hud::SIDE_WIDTH + 20.f;
constexpr float HANDS_RIGHT_MARGIN = static_cast<float>(WINDOW_WIDTH) - Hud::SIDE_WIDTH - 20.f;

constexpr float BOX_PAD = 16.f;

constexpr float SPLIT_SPREAD_X = 80.f;
constexpr float SPLIT_SPREAD_Y = 80.f;

// presentation-only dealing-animation timing (seconds), matching GameTiming /
// dealer-reveal pacing. Cards ease from shoe to slot over this duration and are staggered
// popping. Pure polish: no effect on logic, settlement or headless determinism; used only in GameRenderer::draw, never
// the logic/tick path.
constexpr float DEAL_SLIDE_SECONDS = 0.32f;   // per-card slide-in duration from shoe to slot
constexpr float DEAL_STAGGER_SECONDS = 0.07f; // offset between successive cards' reveal start (the deal sweep)
constexpr float DEALER_HOLE_FLIP_SECONDS = 0.40f; // hole-card flip-to-face-up duration

// Persistence (fallback path if HOME is not set)
constexpr const char* PLAYER_FILE_FALLBACK = "saves/player_profile.json";

// Sound & music asset locations + defaults. Volumes use SFML's 0..100 float range
// directly onto sf::Sound::volume / sf::Music::setVolume with no conversion. Persisted as plain ints in the
// profile so the schema stays free of any SFML dependency (mirrors preferredSpeed/preferredCountingStyle).
namespace Sound
{
    constexpr const char* SOUND_DIR = "assets/sounds/";
    constexpr const char* MUSIC_DIR = "assets/music/";

    // Named effect clips, keyed by the event name Game plays. Kept in one place
    // known-events list (SoundManager) and every playSound() call site all agree: edit here, don't duplicate.
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
