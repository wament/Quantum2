#pragma once
#include "GameRecord.hpp"
#include <string>
#include <vector>

struct HUDState
{
    // Bottom Bar
    int bankroll = 0;
    int currentBet = 0;
    std::vector<int> playerBankrolls;

    int streak = 0;
    bool streakIsWin = false;

    int runningCount = 0;
    // True count of the HI_LO system regardless of which style is currently active (see Game::updateHUDState).
    float trueCount = 0.f;

    // Yellow when Reko is active and we surface the Hi-Lo true count as a cross-check; white otherwise.
    bool trueCountHighlighted = false;
 
    float kellyFraction = 0.33f;
    bool riskSliderDragging = false;

    // Left Panel
    const PlayerStats* playerStats = nullptr;

    // Right Panel
    const ShoeRecord* currentShoe = nullptr;

    // Bumped by the game whenever a new shoe begins (fresh deal or a deck reshuffle). The HUD uses it as the
    // graph-recalibration signal: when it changes every accumulated chart series is cleared so graphs show only
    // this shoe's data instead of drifting persistently upward across shoes/sessions.
    int currentShoeId = 0;

    // Internal
    int cardsRemaining = 0;
    int decksRemaining = 0;

    // Basic Strategy Advisor
    std::string basicStrategyHint;
    // Verdict on the Book box after a completed action: Correct -> green, Deviated -> red. None while advising live.
    AdvisorFeedback advisorFeedback = AdvisorFeedback::None;

    // Advanced Analytics dashboard values (Feature 3). stdDev and edge are derived live; streaks and series come
    // from the player's incremental trackers and feed the HUD chart + stat boxes.
    double analyticsStdDev = 0.0;   // standard deviation of per-hand net profits
    int longestWinningStreak = 0;
    int longestLosingStreak = 0;
    double edgePercent = 0.0;       // estimated player edge over the current session, as a percentage
    std::vector<float> bankrollSeries; // cumulative net profit per round (bankroll curve for the chart)
    std::vector<double> edgeSeries;    // cumulative estimated edge per round (edge-tracking chart)
    std::vector<float> trueCountPerRound_; // raw true count per round (backs the Edge chart secondary view)

    // Human-readable name of the active counting style (e.g. "Hi-Lo"/"Reko"), used to title the Count box.
    std::string countingStyleName;

    // True while it's the dealer's turn. The book advisor hides its action during this window.
    bool isDealerTurn = false;

    // End-of-round summary overlay
    std::vector<std::string> playerSummaries;
    float summaryTimer = 0.0f;

    // Auto-play timers (0 means no wait)
    float afterHandTimer = 0.0f;
    float afterShoeTimer = 0.0f;
};