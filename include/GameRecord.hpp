#pragma once

#include <vector>
#include <deque>
#include <string>
#include <chrono>
#include <map>
#include <algorithm>  // std::max for the longest-streak trackers in computeCareerStats below

#include "Card.hpp"
#include "Deck.hpp"
#include "SideBet.hpp"

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

    int betsPlaced = 0; // +1 for each hand played, and +1 anytime a hand is doubled, or split
    int betSum = 0; // total sum of bets places
    int greatestHand = 0; // greatest single hand win

    int greatestWin = 0; // cumulative win for the whole shoe
    int greatestLoss = 0; // cumulative loss for the whole shoe

    int greatestSessionWin = 0; // cumulative win for all shoes in program session
    int greatestSessionLoss = 0; // cumulative loss for all shoes in program session

    // Strategy deviation tracking
    int strategyHits = 0;       // plays matching basic strategy
    int strategyDeviations = 0; // plays not matching basic strategy
    int strategyTotal = 0;      // total plays evaluated
    std::vector<DeviationRecord> recentDeviations; // last 10 deviations

    // Times the player won a hand after deviating from basic strategy. "Winning" means the hand's
    // settlement was a win, not a push: a push neither wins nor loses the wager.
    int winsAfterDeviation = 0;

    // Advanced analytics live trackers, updated once per settled hand
    // These are the CAREER record written to the profile on disk; the HUD's Std Dev / Long Win / Long Loss boxes
    // read Game::ShoeAnalytics instead, which restarts at every shoe boundary.
    int greatestHandLoss = 0;          // worst single-hand net profit (<= 0)
    int longestWinningStreak = 0;      // longest run of winning hands, ever
    int longestLosingStreak = 0;       // longest run of losing hands, ever
    long long handProfitCount = 0;     // hands settled for variance
    long long handProfitSum = 0;       // sum of per-hand net profits (mean * count)
    long long handProfitSumSq = 0;     // sum of squared per-hand net profits (for variance/std dev)

    // Bet sizes placed, keyed by bet value -> frequency. This is the career copy kept in the saved profile; the
    // bet-distribution chart reads ShoeAnalytics::betDistribution, which restarts at every shoe.
    std::map<int, int> betDistribution;

    // Side bets: career tallies written to the saved profile. Additive: older profiles load with these at
    // their defaults (see Serialization.hpp from_json). "Win/Loss" counts a placed side bet that paid / was lost;
    // profit sum is the net (winnings minus wagers) across every resolved side bet.
    int sideBetWins = 0;
    int sideBetLosses = 0;
    long long sideBetProfitSum = 0;
};

// Per-shoe analytics: every number the HUD reports EXCEPT the bankroll
// across a shoe boundary. Game owns one of these and Game::resetShoeAnalytics() zeroes the whole struct at every
// boundary (new game, R at end of shoe, auto-play advance, mid-shoe reshuffle)
// Long Loss, Count, Streak, Strategy % and the Bets Placed histogram all restart with the shoe instead of
// drifting upward across shoes and sessions.
//
// This is deliberately separate from PlayerStats: that struct is the persisted career record written to the
// profile on disk (and read back at launch)
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

    // the side bet attached to this hand, resolved independently of the main-hand payout above.
    SideBetKind sideBetKind = SideBetKind::None;
    int sideBetWager = 0;          // side-bet wager placed (0 when no side bet)
    int sideBetPayout = 0;         // gross returned on a win (stake + winnings), 0 on a loss -> net is payout - wager
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
        sum += hand.wager + hand.sideBetWager; // include the side-bet stake so net profit reflects it too.
    return sum;
}

inline int RoundRecord::totalPayout() const
{
    int sum = 0;
    for (const auto& hand : playerHands)
        sum += hand.payout + hand.sideBetPayout; // include the side-bet payout so net profit reflects it too.
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
// A natural 21: exactly two cards totalling 21 = one Ace (value 1) plus a ten-value card (>= 10). This is the single,
// record-derived source of truth for every blackjack tally in the game
// PlayerStats::blackjacks and AchievementFeeder derives event.isBlackjack from it
// counter can never drift from the persisted/dashboard career record. Records carry no split flag
// is counted here just as live play counts it; natural detection now lives in one place.
inline bool handIsNaturalBJ(const std::vector<Card>& cards)
{
    return cards.size() == 2 &&
        ((cards[0].value == 1 && cards[1].value >= 10) ||
         (cards[1].value == 1 && cards[0].value >= 10));
}

// Aggregates career PlayerStats across every ShoeRecord in a session
// computeCareerStats (one shoe), this stitches shoes together using the exact boundary rules live play uses: greatestWin/
// greatestLoss track each shoe's running cumulative separately but persist as a career-long max/min over those per-shoe peaks,
// while greatestSession* accumulate across all shoes. Streaks reset their running pair at every shoe boundary (a run never
// crosses one -- game.shoe_.currentWinStreak lives in the per-shoe ShoeAnalytics) yet the longest run stays career-long.
inline PlayerStats computeCareerStatsHistory(const std::vector<ShoeRecord>& history)
{
    PlayerStats out;  // zero defaults match a profile with no hands played.
    long long shoeCumulative = 0;     // running net profit within one shoe (greatestWin/greatestLoss); resets per shoe boundary.
    long long sessionCumulative = 0;  // session-wide running net profit (never resets -> greatestSession* fields).
    int winStreak = 0, loseStreak = 0;

    for (const auto& shoe : history)
    {
        shoeCumulative = 0;   // live resets game.shoeCumulativeWinLoss at every shoe boundary.
        for (const auto& round : shoe.rounds)
        {
            for (const auto& hand : round.playerHands)
            {
                if (hand.playerIndex != 0)
                    continue;   // human seat only, matching HandEvaluator::settleHand's pIdx == 0 gating.

                const long long net = static_cast<long long>(hand.payout) - hand.wager;
                const bool isWin = hand.won && net > 0;
                const bool isPush = hand.won && net == 0;   // a winning side bet leaves the MAIN result unchanged

                if (isWin) ++out.wins;
                else if (!hand.won) ++out.losses;   // Lose OR Surrender both count as a loss
                else ++out.pushes;                  // Push

                // Natural blackjacks read from the single record-derived predicate above, never a parallel definition.
                if (handIsNaturalBJ(hand.cards))
                    ++out.blackjacks;

                // Per-hand advanced analytics: variance accumulators, worst single-hand loss, best single-hand win. Career-long.
                out.handProfitCount++;
                out.handProfitSum += net;
                out.handProfitSumSq += net * net;
                if (net < out.greatestHandLoss) out.greatestHandLoss = static_cast<int>(net);   // most-negative <= 0
                if (net > out.greatestHand)     out.greatestHand = static_cast<int>(net);

                // Longest win/lose streaks: Win extends the run, Lose/Surrender the losing run, Push breaks both. The running
                // pair resets per shoe below; longest* is career-long, matching game.shoe_.longestWinningStreak bookkeeping.
                if (isWin) { ++winStreak; loseStreak = 0; }
                else if (!hand.won) { ++loseStreak; winStreak = 0; }
                else { winStreak = 0; loseStreak = 0; }   // Push breaks both streaks
                out.longestWinningStreak = std::max(out.longestWinningStreak, winStreak);
                out.longestLosingStreak = std::max(out.longestLosingStreak, loseStreak);

                shoeCumulative += net;
                sessionCumulative += net;
                if (shoeCumulative > out.greatestWin)      out.greatestWin = static_cast<int>(shoeCumulative);
                if (shoeCumulative < out.greatestLoss)     out.greatestLoss = static_cast<int>(shoeCumulative);
                if (sessionCumulative > out.greatestSessionWin)  out.greatestSessionWin = static_cast<int>(sessionCumulative);
                if (sessionCumulative < out.greatestSessionLoss) out.greatestSessionLoss = static_cast<int>(sessionCumulative);

                // Side bets: every placed side bet resolves to a win or a loss (no push). Net is payout - wager in both cases.
                if (hand.sideBetWager > 0)
                {
                    if (hand.sideBetPayout > 0)
                    {
                        ++out.sideBetWins;
                        out.sideBetProfitSum += static_cast<long long>(hand.sideBetPayout) - hand.sideBetWager;
                    }
                    else
                    {
                        ++out.sideBetLosses;
                        out.sideBetProfitSum -= hand.sideBetWager;
                    }
                }
            }
        }
        // A run never crosses a shoe boundary
        winStreak = 0;
        loseStreak = 0;
    }

    // Every settled human hand: decisive (win+loss) plus pushes
    const int decisive = out.wins + out.losses;
    out.handsFinished = decisive + out.pushes;
    return out;
}


// Aggregates a single shoe's recorded rounds into the career PlayerStats fields fully determined by the record
// target of the stats-batching refactor . Pure logic over GameRecord types
// tests/test_stats_batching.cpp proves its output equals the live per-hand increments HandEvaluator/Game/RoundDealer produce.
// Reconstruction mirrors feed()/HandEvaluator exactly:
// - net per hand = payout - wager (== netProfitOnHand, HandEvaluator line 103)
// - result = Win if won&&net>0, Push if won&&net==0, Lose/Surrender if !won (surrender has won=false in the record)
// - streaks = Win extends the win run, Lose/Surrender the lose run, Push breaks BOTH
// - cumulative = human seat (playerIndex==0) only; greatestWin/greatestLoss reset per shoe, session-wide otherwise
inline PlayerStats computeCareerStats(const ShoeRecord& shoe)
{
    // Delegating to computeCareerStatsHistory guarantees a one-shoe history reproduces this result exactly (single-shoe == history of one).
    return computeCareerStatsHistory({shoe});
}

