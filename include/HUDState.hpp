#pragma once
#include "GameRecord.hpp"
struct HUDState
{
    // Bottom Bar
    int bankroll = 0;
    int currentBet = 0;

    std::string statusMessage;

    int streak = 0;
    bool streakIsWin = false;

    int runningCount = 0;
    float trueCount = 0.f;

    // Risk-of-Ruin slider (Kelly fraction)
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
};