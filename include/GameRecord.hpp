#pragma once

#include <vector>
#include <deque>
#include <string>
#include <chrono>
#include <cmath>

#include "Card.hpp"
#include "Deck.hpp"

struct DeviationRecord
{
    int playerValue = 0;
    int dealerUpValue = 0;
    bool wasSoft = false;
    bool wasPair = false;
    int pairValue = 0;
    char actionTaken = 0;
    char correctAction = 0;
    int bet = 0;
    bool won = false;
};

// One sample of the player-edge trajectory over a round: the effective true count together with the
// running cumulative profit at that point, used to estimate edge over time (Feature 3).
struct AnalyticsEdgeSample
{
    int roundIndex = 0;
    double trueCount = 0.0;
    double cumulativeProfit = 0.0;
};

struct PlayerStats
{
    int shoesFinished = 0;
    int handsFinished = 0;

    int wins = 0;
    int losses = 0;
    int pushes = 0;
    int blackjacks = 0;
    int splits = 0;
    int doubles = 0;

    int betsPlaced = 0; //+1 for each hand played, and +1 anytime a hand is doubled, or split
    int betSum = 0; //total sum of bets places
    int greatestHand = 0; //greatest single hand win

    int greatestWin = 0; //cumulative win for the whole shoe
    int greatestLoss = 0; //cumulative loss for the whole shoe

    int greatestSessionWin = 0; //cumulative win for all shoes in program session
    int greatestSessionLoss = 0; //cumulative loss for all shoes in program session

    // Strategy deviation tracking
    int strategyHits = 0;       // plays matching basic strategy
    int strategyDeviations = 0; // plays not matching basic strategy
    int strategyTotal = 0;      // total plays evaluated
    std::vector<DeviationRecord> recentDeviations; // last 10 deviations

    // --- Feature 1: win-after-deviation tracking ---
    int strategyDeviationWins = 0; // hands that deviated from basic strategy yet still won

    // --- Feature 3: advanced analytics ---
    int greatestSingleHandLoss = 0;   // most negative per-hand net profit (<= 0)
    int longestWinningStreak = 0;     // longest consecutive win streak across the whole session
    int longestLosingStreak = 0;      // longest consecutive loss streak across the whole session
    double handProfitSum = 0.0;       // running sum of per-hand net profit (for variance / stddev)
    double handProfitSqSum = 0.0;     // running sum of squared per-hand net profit

    std::vector<int> bankrollCurve;        // human bankroll sampled once per finished round
    std::vector<AnalyticsEdgeSample> edgeHistory; // true count vs cumulative profit over rounds

    // Feature 3: distribution of bet sizes placed this session (one count per bucket, sized at runtime)
    std::vector<int> betBuckets;
};

// Standard deviation of per-hand net profit for a session (Feature 3 - advanced analytics). Uses the running
// sums stored in PlayerStats so no history buffer is needed: variance = E[x^2] - E[x]^2. Guarded against
// empty sessions and tiny negative round-off so callers get a stable, always-non-negative result.
inline double computeHandStandardDeviation(const PlayerStats& s)
{
    long n = static_cast<long>(s.handsFinished);
    if (n <= 1)
        return 0.0; // zero or one sample -> nothing to vary
    double mean = s.handProfitSum / static_cast<double>(n);
    double variance = s.handProfitSqSum / static_cast<double>(n) - mean * mean;
    if (variance < 0.0)
        variance = 0.0; // guard against floating-point round-off near zero
    return std::sqrt(variance);
}

struct HandOutcome
{
    std::vector<Card> cards;
    std::vector<std::string> actions;
    int wager = 0;
    int payout = 0;
    bool won = false;
    bool surrendered = false;      // whether this hand was surrendered
    int insuranceBet = 0;          // insurance wager (only on first hand of round)
    bool insuranceTaken = false;   // whether insurance was taken
    int playerIndex = 0;           // which player this hand belongs to
    std::string playerName = "";   // human-readable seat name (William or an NPC)
};

struct RoundRecord
{
    std::vector<Card> dealerCards;
    std::vector<HandOutcome> playerHands;
    int runningCount = 0;
    float trueCount = 0;
    int cardsRemaining = 0;

    long long timestamp = 0;

    int totalWager() const;
    int totalPayout() const;
    int netProfit() const;
    bool overallWin() const;
};

struct ShoeRecord
{
    std::vector<Card> initialDeck;
    std::vector<RoundRecord> rounds;
    
    bool isContinuation = false; // true if this shoe was created due to deck exhaustion mid-hand
    bool completed = true; // true if cut card reached, false if player quit mid-shoe
    unsigned long long shuffleSeed = 0; // MT19937 seed used when shoe was created
};

// Inline implementations for RoundRecord computed methods
inline int RoundRecord::totalWager() const
{
    int sum = 0;
    for (const auto& hand : playerHands)
        sum += hand.wager;
    return sum;
}

inline int RoundRecord::totalPayout() const
{
    int sum = 0;
    for (const auto& hand : playerHands)
        sum += hand.payout;
    return sum;
}

inline int RoundRecord::netProfit() const
{
    return totalPayout() - totalWager();
}

inline bool RoundRecord::overallWin() const
{
    return netProfit() >= 0;
}

// Reconstructs the deck state at the start of a specific round
// by removing all cards dealt in rounds [0, roundIndex)
inline std::deque<Card> reconstructDeckAtRound(
    const ShoeRecord& shoe, 
    size_t roundIndex)
{
    std::deque<Card> deck(shoe.initialDeck.begin(), shoe.initialDeck.end());
    for (size_t r = 0; r < roundIndex; ++r)
    {
        const auto& round = shoe.rounds[r];

        // Remove dealer cards
        for (size_t i = 0; i < round.dealerCards.size(); ++i)
            deck.pop_front();

        // Remove each player hand's cards
        for (const auto& hand : round.playerHands)
        {
            for (size_t i = 0; i < hand.cards.size(); ++i)
                deck.pop_front();
        }
    }

    return deck;
}

