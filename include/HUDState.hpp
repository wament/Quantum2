#pragma once

#include <string>

#include "GameRecord.hpp"      // PlayerStats / ShoeRecord

struct HUDState
{
    // Bottom Bar
    int bankroll = 0;
    int currentBet = 0;

    int runningCount = 0;
    float trueCount = 0.f;

    int cardsRemaining = 0;
    int decksRemaining = 0;

    std::string statusMessage;

    // Statistics
    const PlayerStats* playerStats = nullptr;
    const ShoeRecord* currentShoe = nullptr;
};