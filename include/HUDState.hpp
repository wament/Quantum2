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

    std::string statusMessage;

    int streak = 0;
    bool streakIsWin = false;

    int runningCount = 0;
    float trueCount = 0.f;

    // Left Panel
    const PlayerStats* playerStats = nullptr;

    // Right Panel
    const ShoeRecord* currentShoe = nullptr;

    // Internal
    int cardsRemaining = 0;
    int decksRemaining = 0;

    // Basic Strategy Advisor
    std::string basicStrategyHint;

    // End-of-round summary overlay
    std::vector<std::string> playerSummaries;
    float summaryTimer = 0.0f;

    // Auto-play timers (0 means no wait)
    float afterHandTimer = 0.0f;
    float afterShoeTimer = 0.0f;
};