#pragma once

#include "Hand.hpp"
#include "Player.hpp"
#include "GameRecord.hpp"

#include <vector>

class Game;

class HandEvaluator
{
public:
    HandEvaluator(Game& game);
    
    // Evaluate all player hands against dealer hand
    void evaluateHands();
    
    // Check for natural blackjack (called after initial deal)
    void evaluateBlackjack();

private:
    Game& game;
};