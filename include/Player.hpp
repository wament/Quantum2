#pragma once

#include "Hand.hpp"
#include "Deck.hpp"
#include "PlayerProfile.hpp"




class Player
{
public: 
    Player(const std::string& playerName);

    void resetRound();  

    int currentHand = 0; 
    std::vector<Hand> hands;

    // Betting
    bool placeBet(int amount);
    void winBet(int amount);
    void loseBet(int amount);
    void pushBet(int amount);


    // Getters
    const std::string& getName() const;
    int getMoney() const;
    PlayerProfile getProfile();
    PlayerProfile updateProfile(PlayerProfile profile);
    void saveProfile();
private:
    std::string name;

    PlayerProfile profile;
    int bankroll = 1000;
 
};