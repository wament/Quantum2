#pragma once

#include <vector>
#include <deque>
#include <string>
#include <chrono>
#include <map>

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
    bool won = false; // set at evaluation time: whether the hand that contained this deviation ultimately won
};

// Verdict shown on the Book advisor box after a player completes an action.
enum class AdvisorFeedback { None, Correct, Deviated };

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

    // Times the player won a hand after deviating from basic strategy (Feature 1). "Winning" means the hand's
    // settlement was a win, not a push — a push neither wins nor loses the wager.
    int winsAfterDeviation = 0;

    // Advanced analytics live trackers (Feature 3), updated once per settled hand so they persist and stay O(1).
    // These are the CAREER record written to the profile on disk; the HUD's Std Dev / Long Win / Long Loss boxes
    // read Game::ShoeAnalytics instead, which restarts at every shoe boundary.
    int greatestHandLoss = 0;          // worst single-hand net profit (<= 0)
    int longestWinningStreak = 0;      // longest run of winning hands, ever
    int longestLosingStreak = 0;       // longest run of losing hands, ever
    long long handProfitCount = 0;     // hands settled for variance
    long long handProfitSum = 0;       // sum of per-hand net profits (mean * count)
    long long handProfitSumSq = 0;     // sum of squared per-hand net profits (for variance/std dev)

    // Bet sizes placed, keyed by bet value -> frequency. This is the career copy kept in the saved profile; the
    // Feature 3 bet-distribution chart reads ShoeAnalytics::betDistribution, which restarts at every shoe.
    std::map<int, int> betDistribution;
};

// Per-shoe analytics: every number the HUD reports EXCEPT the bankroll, which is the only value that carries
// across a shoe boundary. Game owns one of these and Game::resetShoeAnalytics() zeroes the whole struct at every
// boundary (new game, R at end of shoe, auto-play advance, mid-shoe reshuffle), so Edge %, Std Dev, Long Win,
// Long Loss, Count, Streak, Strategy % and the Bets Placed histogram all restart with the shoe instead of
// drifting upward across shoes and sessions.
//
// This is deliberately separate from PlayerStats: that struct is the persisted career record written to the
// profile on disk (and read back at launch), so it must never be zeroed mid-session. Nothing on the HUD reads it.
struct ShoeAnalytics
{
    // Std Dev box: variance of per-hand net profit is derived from these (variance = E[x^2] - (E[x])^2).
    long long handProfitCount = 0;
    long long handProfitSum = 0;
    long long handProfitSumSq = 0;

    // Long Win / Long Loss boxes: the running hand-to-hand streaks plus this shoe's longest runs.
    int currentWinStreak = 0;
    int currentLoseStreak = 0;
    int longestWinningStreak = 0;
    int longestLosingStreak = 0;

    // Streak box on the bottom bar: the round-to-round streak, human seat only.
    int roundStreak = 0;
    bool roundStreakIsWin = false;

    // Strategy box: basic-strategy accuracy over this shoe, human seat only.
    int strategyHits = 0;
    int strategyDeviations = 0;
    int strategyTotal = 0;

    // Bets Placed histogram: this shoe's wagers, bet value -> frequency.
    std::map<int, int> betDistribution;

    // Left-panel graph series: cumulative net profit per round (bankroll curve, human seat only) and the estimated
    // edge sampled once per round. The edge is a level re-read from the count each round, not a running total.
    std::vector<float> bankrollSeries;
    std::vector<double> edgeSeries;
};

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
    int bankrollAtShoeEnd = 0; // player's bankroll when the shoe record was finalized (backs the lifetime bankroll progression)
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

