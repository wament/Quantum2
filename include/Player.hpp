#pragma once

#include "Hand.hpp"
#include "Deck.hpp"
#include "PlayerProfile.hpp"

enum class BettingStrategy
{
    Kelly,
    Martingale,
    Fibonacci,
    Paroli,
    Flat
};

class Player
{
public: 
    Player(const std::string& playerName);

    void resetRound(int previousBet);  

    int currentHand = 0; 
    std::vector<Hand> hands;

    // Betting
    bool placeBet(int amount);
    void winBet(int amount);
    void loseBet(int amount);
    void pushBet(int amount);

    // Betting strategy
    void setBettingStrategy(BettingStrategy strategy, int baseBet = 10);
    BettingStrategy getBettingStrategy() const { return bettingStrategy; }
    int calculateNextBet(bool lastRoundWon);
    void recordRoundResult(bool won);

    // Bankroll
    void setBankroll(int amount);

    // Getters
    const std::string& getName() const;
    int getBankroll() const;
    PlayerProfile getProfile();
    PlayerStats& getStats();
    PlayerProfile updateProfile(PlayerProfile profile);
    void saveProfile();
private:
    std::string name;

    PlayerProfile profile;
    int bankroll = 1000;
 
    // Progressive betting state
    BettingStrategy bettingStrategy = BettingStrategy::Flat;
    int baseBet = 10;
    int currentProgressiveBet = 0;
    int consecutiveWins = 0;
    int fibIndex = 0;
    bool lastRoundWon = false;
};
