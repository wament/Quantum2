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

    // Feature 3: per-hand net-profit standard deviation, precomputed in Game::updateHUDState so the HUD can
    // show it without recomputing from running sums each frame.
    double handStandardDeviation = 0.0;

    // Right Panel
    const ShoeRecord* currentShoe = nullptr;

    // Internal
    int cardsRemaining = 0;
    int decksRemaining = 0;

    // Feature 1: Green/red feedback shown after the player acts. advisorFeedbackText is the transient
    // symbol (✓/✗) tinted by color; basicStrategyHint holds the standing recommendation during the turn.
    std::string basicStrategyHint;   // standing recommendation letter(s) or manual peek text
    std::string advisorFeedbackText; // transient result shown briefly after an action ('✓' / '✗')

    // Green/red feedback shown after the player acts: 'C' matched strategy, 'D' deviated. While
    // advisorFeedbackTtl > 0 the HUD renders this tinted; advisorPeekActive keeps a manual peek ('I')
    // visible instead of the live advice during the player's turn.
    int advisorFeedbackTtl = 0;
    char advisorResult = 0;          // 'C' correct match, 'D' deviation (active while ttl > 0)
    bool advisorPeekActive = false;

    // Feature 5: which counting system the true-count box describes ("Hi-Lo" / "Reko")
    std::string countingStyleNameStr = "Hi-Lo";

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