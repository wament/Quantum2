#include "Player.hpp"
#include "Constants.hpp"
#include <iostream>

Player::Player(const std::string& playerName)
    : name(playerName)
{
    // Try to load existing profile; otherwise create new
    profile = PlayerProfile::load(Constants::PLAYER_FILE);
    if (profile.getBankroll() == 0)
    {
        profile = PlayerProfile(playerName);
    }
    bankroll = profile.getBankroll();
}

PlayerProfile Player::getProfile()
{
    return profile;
}

PlayerStats& Player::getStats()
{
    return profile.stats;
}

PlayerProfile Player::updateProfile(PlayerProfile p)
{
    this->profile = p;
    return this->profile;
}

void Player::saveProfile()
{
    // Sync current bankroll into profile before saving
    profile.setBankroll(bankroll);
    profile.save(Constants::PLAYER_FILE);
    std::cout << "Profile saved." << std::endl;
}

void Player::resetRound(int previousBet)
{
    hands.clear();
    hands.resize(1);
    hands[0].setBet(previousBet);
    currentHand = 0;
} 

int Player::getBankroll() const
{
    return bankroll;
}

const std::string& Player::getName() const
{
    return name;
}

bool Player::placeBet(int amount)
{
    if (amount <= bankroll)
    {
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
    // Bankroll already reduced on placeBet; nothing to do.
}

void Player::pushBet(int amount)
{
    bankroll += amount;
}