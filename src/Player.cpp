#include "Player.hpp"
#include "Constants.hpp"
#include "Logger.hpp"
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

// Get a reliable save path in the user's home directory
static std::string getSavePath()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr)
    {
        // Fallback to relative path if HOME is not set
        return std::string(Constants::PLAYER_FILE_FALLBACK);
    }
    
    fs::path saveDir = fs::path(home) / ".local" / "share" / "blackjack" / "saves";
    fs::create_directories(saveDir);
    return (saveDir / "player_profile.json").string();
}

Player::Player(const std::string& playerName)
    : name(playerName)
{
    std::string savePath = getSavePath();
    
    // Try to load existing profile; otherwise create new
    profile = PlayerProfile::load(savePath);
    if (profile.getBankroll() == 0)
    {
        profile = PlayerProfile(playerName);
        Logger::info("New player profile created for " + playerName + ".");
    }
    else
    {
        Logger::info() << "Loaded profile for " << playerName << " with bankroll " << bankroll << ".";
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
    std::string savePath = getSavePath();
    profile.save(savePath);
    Logger::debug("Profile saved to " + savePath + ".");
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
        return true;
    }
    return false;
}

void Player::winBet(int amount)
{
    bankroll += amount;
}

void Player::loseBet(int amount)
{
   bankroll -= amount;
}

void Player::pushBet(int amount)
{
    bankroll += amount;
}