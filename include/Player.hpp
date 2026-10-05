#pragma once

#include "Hand.hpp"
#include "Deck.hpp"
#include "PlayerProfile.hpp"

#include <cstddef>

enum class BettingStrategy
{
    Kelly,
    Martingale,
    Fibonacci,
    Paroli,
    Flat
};

// How a single hand settles against the dealer, reported by Player::settle.
enum class SettlementResult { Surrender, Lose, Win, Push };

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

    // Per-seat bet sizing for a new round. Kelly sizes off the true count and bankroll; progressive
    // strategies size off their own internal state (see calculateNextBet) rather than the count.
    int determineRoundBet(float trueCount);

    // Adjust a hand's standing bet by delta, clamped to table limits. Target defaults to the active hand.
    void adjustHandBet(int delta, std::size_t handIndex = 0);

    // Insurance — the decision (a Hi-Lo rule) lives here; placement and the seat loop stay with Game.
    bool shouldTakeInsurance(int bet, float trueCount) const;

    // Take insurance for the active hand: place a wager of `amount` and record it on the hand.
    void takeInsurance(int amount);

    // Raise/lower the active hand's insurance bet by 1, clamped to [0, half the main bet].
    void raiseInsurance();
    void lowerInsurance();

    // Settle one hand against the dealer and apply the result to the bankroll. Reports how it settled;
    // writes the payout. A natural (2-card) unsplit blackjack pays 3:2 — any other hand totaling 21
    // settles at even money, so the !hand.isSplit() guard drives every branch below.
    SettlementResult settle(const Hand& hand, int dealerValue, bool dealerBust, int& payout);

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
