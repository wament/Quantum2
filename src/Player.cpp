#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "Player.hpp"
#include "Constants.hpp"
#include "Logger.hpp"
#include "BasicStrategy.hpp"
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

// NPC seat: fresh in-memory profile only. NPCs are ephemeral: they keep no profile on disk.
// The human player persists via the load/save path above
Player::Player(const std::string& playerName, int startingBankroll)
    : name(playerName), bankroll(startingBankroll)
{
    profile = PlayerProfile(playerName);
    profile.setBankroll(startingBankroll);
}

PlayerProfile Player::getProfile() { return profile; }

PlayerStats& Player::getStats() { return profile.stats; }

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

int Player::getBankroll() const { return bankroll; }

void Player::setBankroll(int amount) { bankroll = amount; }

const std::string& Player::getName() const { return name; }

bool Player::placeBet(int amount) { return amount <= bankroll; }

void Player::winBet(int amount) { bankroll += amount; }

void Player::loseBet(int amount) { bankroll -= amount; }

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
    case BettingStrategy::Kelly:
    {
        // Kelly is calculated by Game using true count, not here
        return baseBet;
    }
    case BettingStrategy::Martingale:
    {
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
    case BettingStrategy::Fibonacci:
    {
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
    case BettingStrategy::Paroli:
    {
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
    default:
    { // Flat and unknown strategies
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

int Player::determineRoundBet(float trueCount)
{
    if (bettingStrategy == BettingStrategy::Kelly)
        return BasicStrategy::calculateKellyBet(trueCount, GameConfig::KELLY_FRACTION, bankroll, Constants::MINIMUM_BET, Constants::MAXIMUM_BET);

    // Progressive strategies size off their own internal state, not the true count.
    return calculateNextBet(false);
}

void Player::adjustHandBet(int delta, std::size_t handIndex)
{
    if (handIndex >= hands.size())
        handIndex = 0;

    int next = hands[handIndex].getBet() + delta;
    if (next < Constants::MINIMUM_BET)
        next = Constants::MINIMUM_BET;
    if (next > Constants::MAXIMUM_BET)
        next = Constants::MAXIMUM_BET;
    hands[handIndex].setBet(next);
}

bool Player::shouldTakeInsurance(int bet, float trueCount, BasicStrategy::CountingStyle style) const
{
    // Deviation thresholds are counting-system specific: Hi-Lo requires +3 to take insurance, Reko requires +2.
    float threshold = (style == BasicStrategy::CountingStyle::REKO) ? 2.0f : 3.0f;
    return trueCount >= threshold && bet > 0;
}

void Player::takeInsurance(int amount)
{
    if (amount <= 0)
        return;

    hands[currentHand].setInsuranceBet(amount);
    placeBet(amount);
}

void Player::raiseInsurance()
{
    int maxIns = hands[currentHand].getBet() / 2;
    if (hands[currentHand].getInsuranceBet() < maxIns)
        hands[currentHand].setInsuranceBet(hands[currentHand].getInsuranceBet() + 1);
}

void Player::lowerInsurance()
{
    int current = hands[currentHand].getInsuranceBet();
    if (current > 0)
        hands[currentHand].setInsuranceBet(current - 1);
}

SettlementResult Player::settle(const Hand& hand, int dealerValue, bool dealerBust, bool dealerHasBlackjack, int& payout)
{
    // Only a natural (2-card) unsplit blackjack pays 3:2. A split hand that totals 21 is not treated
    // as one and settles at even money: the !hand.isSplit() guard drives every branch below.
    const bool isNaturalBlackjack = hand.isBlackjack() && !hand.isSplit();

    if (hand.isSurrendered())
    {
        loseBet(hand.getBet() / 2);
        payout = hand.getBet() / 2;
        return SettlementResult::Surrender;
    }

    // A natural blackjack is ranked against the dealer's hand TYPE, not against the dealer's TOTAL: it
    // beats every non-natural hand, including a dealer 21 built from three or more cards, and only
    // pushes against the dealer's own natural. Falling through to the total comparison below used to
    // score "my blackjack vs a dealer that drew to 21" as a 21-vs-21 push.
    if (isNaturalBlackjack)
    {
        if (dealerHasBlackjack)
        {
            payout = hand.getBet();
            return SettlementResult::Push;
        }

        winBet(hand.getBet() * Constants::BLACKJACK_PAYOUT);
        payout = hand.getBet() * (1 + Constants::BLACKJACK_PAYOUT);
        return SettlementResult::Win;
    }

    if (hand.isBust() || (hand.getValue() < dealerValue && !dealerBust))
    {
        loseBet(hand.getBet());
        payout = 0;
        return SettlementResult::Lose;
    }
    if (hand.getValue() > dealerValue || dealerBust)
    {
        winBet(hand.getBet());
        payout = hand.getBet() * 2;
        return SettlementResult::Win;
    }

    // Push: the wager stays on the table and the bankroll is unchanged.
    payout = hand.getBet();
    return SettlementResult::Push;
}
