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

    // Internal
    int cardsRemaining = 0;
    int decksRemaining = 0;

    // Basic Strategy Advisor
    std::string basicStrategyHint;

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