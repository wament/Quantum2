#include "Player.hpp"
#include "Constants.hpp"
#include <iostream>



Player::Player(const std::string& playerName)
    : name(playerName)
{
    //load player profile
}

PlayerProfile Player::getProfile(){
    return profile;
}

PlayerProfile Player::updateProfile(PlayerProfile profile){
    this->profile = profile;
    return this->profile;
}

void Player::saveProfile(){
    //write to a file
}


void Player::resetRound(){
    hands.clear();
    hands.resize(1);
    currentHand = 0;
} 

int Player::getBankroll() const {
    return bankroll;
}

bool Player::placeBet(int amount)
{
    if(amount <= bankroll){
        bankroll -= amount;
        return true;
    }
    return false;
}

void Player::winBet(int amount)
{
    bankroll += amount;
}

void Player::loseBet()
{
    return;
}

void Player::pushBet(int amount)
{
    bankroll += amount; //this amount should be the inital bet + any splits/doubles
}