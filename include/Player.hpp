#pragma once

#include "Hand.hpp"
#include "Deck.hpp"
#include "PlayerProfile.hpp"
#include "BasicStrategy.hpp"

#include <cstddef>

enum class BettingStrategy
{
    Flat,
    Fibonacci,
    Paroli,
    Martingale,
    Kelly
};

// How a single hand settles against the dealer, reported by Player::settle.
enum class SettlementResult
{
    Surrender,
    Lose,
    Win,
    Push
};

class Player
{
  public:
    Player(const std::string& playerName);

    // NPC seat
    Player(const std::string& playerName, int startingBankroll);

    void resetRound(int previousBet);

    int currentHand = 0;
    std::vector<Hand> hands;

    // Betting
    bool placeBet(int amount);
    void winBet(int amount);
    void loseBet(int amount);

    // Betting strategy
    void setBettingStrategy(BettingStrategy strategy, int baseBet = 10);
    BettingStrategy getBettingStrategy() const { return bettingStrategy; }
    int calculateNextBet(bool lastRoundWon);
    void recordRoundResult(bool won);

    int determineRoundBet(float trueCount);

    void adjustHandBet(int delta, std::size_t handIndex = 0);

    bool shouldTakeInsurance(int bet, float trueCount, BasicStrategy::CountingStyle style = BasicStrategy::CountingStyle::HI_LO) const;

    void takeInsurance(int amount);

    // Raise/lower the active hand's insurance bet by 1, clamped to [0, half the main bet].
    void raiseInsurance();
    void lowerInsurance();

    // Settle one hand against the dealer and apply the result to the bankroll. Reports how it settled;
    // writes the payout. A natural (2-card) unsplit blackjack pays 3:2 — any other hand totaling 21
    // settles at even money, so the !hand.isSplit() guard drives every branch below.
    // dealerHasBlackjack must reflect whether the DEALER's 21 is also a natural (exactly two cards):
    // a player natural only pushes against a dealer natural, never against a dealer 21 built with
    // three or more cards.
    SettlementResult settle(const Hand& hand, int dealerValue, bool dealerBust, bool dealerHasBlackjack, int& payout);

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

    BettingStrategy bettingStrategy = BettingStrategy::Flat;
    int baseBet = 10;
    int currentProgressiveBet = 0;
    int consecutiveWins = 0;
    int fibIndex = 0;
    bool lastRoundWon = false;
};
