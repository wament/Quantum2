#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
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

void Player::setBankroll(int amount)
{
    bankroll = amount;
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

// --- Betting Strategy Implementation ---

void Player::setBettingStrategy(BettingStrategy strategy, int baseBet)
{
    bettingStrategy = strategy;
    this->baseBet = baseBet;
    currentProgressiveBet = baseBet;
    consecutiveWins = 0;
    fibIndex = 0;
}

int Player::calculateNextBet(bool lastRoundWon)
{
    // Use stored state from recordRoundResult(), not the parameter
    bool won = this->lastRoundWon;

    switch (bettingStrategy)
    {
    case BettingStrategy::Kelly: {
        // Kelly is calculated by Game using true count, not here
        return baseBet;
    }
    case BettingStrategy::Martingale: {
        // Double after loss, reset to base after win
        if (!won)
        {
            currentProgressiveBet = baseBet;
        }
        else
        {
            currentProgressiveBet *= 2;
            // Cap at reasonable multiple to avoid astronomical bets
            if (currentProgressiveBet > baseBet * 64)
                currentProgressiveBet = baseBet * 64;
        }
        return currentProgressiveBet;
    }
    case BettingStrategy::Fibonacci: {
        // Move up sequence after loss, down two after win
        static const int fibSeq[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144};
        static const int maxFibIndex = 11;

        if (won)
        {
            fibIndex = std::max(0, fibIndex - 2);
        }
        else
        {
            fibIndex = std::min(maxFibIndex, fibIndex + 1);
        }
        currentProgressiveBet = baseBet * fibSeq[fibIndex];
        return currentProgressiveBet;
    }
    case BettingStrategy::Paroli: {
        // Double after win (up to 3 wins), reset after loss
        if (won)
        {
            consecutiveWins++;
            if (consecutiveWins > 3)
            {
                // Reset after 3 consecutive wins
                consecutiveWins = 0;
                currentProgressiveBet = baseBet;
            }
            else
            {
                currentProgressiveBet = baseBet * (1 << consecutiveWins);
            }
        }
        else
        {
            consecutiveWins = 0;
            currentProgressiveBet = baseBet;
        }
        return currentProgressiveBet;
    }
    default: { // Flat and unknown strategies
        return baseBet;
    }
    }
}

void Player::recordRoundResult(bool won)
{
    lastRoundWon = won;
    if (!won)
    {
        consecutiveWins = 0;
    }
}

