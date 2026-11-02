// Analytics & statistics module tests: betting analytics, charts, shoe-boundary analytics, CSV/json export.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include "Card.hpp"
#include "Constants.hpp"
#include "Hand.hpp"
#include "test_support.hpp"
#include <BasicStrategy.hpp>
#include <Card.hpp>
#include <Constants.hpp>
#include <Deck.hpp>
#include <Game.hpp>
#include <GameRecord.hpp>
#include <HUD.hpp>
#include <Player.hpp>
#include <StatsDashboard.hpp>
#include <algorithm>
#include <catch2/catch_all.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>

using Catch::Approx;
using json = nlohmann::json;
using namespace Constants;

// Shared fixtures/helpers in one anonymous namespace (identical copies de-duped across files).
namespace {
void padWithEights(std::vector<Card>& shoe)
    {
        for (int i = 0; i < 200; ++i)
            shoe.push_back(test_support::make_card(8, Suit::Clubs));
    }

std::vector<Card> humanLosesShoe()
    {
        std::vector<Card> shoe = {
            test_support::make_card(5, Suit::Spades), test_support::make_card(4, Suit::Clubs), test_support::make_card(3, Suit::Diamonds), test_support::make_card(6, Suit::Hearts),
            test_support::make_card(2, Suit::Spades), test_support::make_card(3, Suit::Clubs), test_support::make_card(2, Suit::Diamonds), test_support::make_card(4, Suit::Hearts),
            test_support::make_card(6, Suit::Spades), test_support::make_card(6, Suit::Clubs), test_support::make_card(6, Suit::Diamonds), test_support::make_card(6, Suit::Hearts),
        };
        padWithEights(shoe);
        return shoe;
    }

RoundRecord bjRound(int wager)
    {
        RoundRecord r;
        HandOutcome h;
        h.cards = {test_support::make_card(1, Suit::Hearts), test_support::make_card(10, Suit::Spades)}; // Ace + Ten = natural 21
        h.wager = wager;
        h.payout = wager * 3; // 3:2 natural payout (stake returned + 1.5x)
        h.won = true;
        r.playerHands.push_back(h);
        return r;
    }

RoundRecord bustRound(int wager, int payout)
    {
        RoundRecord r;
        HandOutcome h;
        h.cards = {test_support::make_card(7, Suit::Hearts), test_support::make_card(8, Suit::Spades), test_support::make_card(8, Suit::Diamonds)};
        h.wager = wager;
        h.payout = payout;
        r.playerHands.push_back(h);
        return r;
    }

RoundRecord decidedRound(bool won, bool surrendered, int wager, int payout)
    {
        RoundRecord r;
        HandOutcome h;
        h.cards = {test_support::make_card(5, Suit::Clubs), test_support::make_card(6, Suit::Hearts)}; // hard 11
        h.won = won;
        h.surrendered = surrendered;
        h.wager = wager;
        h.payout = payout;
        r.playerHands.push_back(h);
        return r;
    }

} // namespace

TEST_CASE("Reko true count is the raw running count; Hi-Lo normalizes by decks", "[Feature5][Counting]")
{
    Game game(true);

    // Initial deal is four cards summing to 6/7/8/8; Reko sums to 2, Hi-Lo to 1. Filler keeps draw() from running dry.
    Card six;   six.value = 6; six.suit = Suit::Hearts;
    Card seven; seven.value = 7; seven.suit = Suit::Spades;
    Card eight; eight.value = 8; eight.suit = Suit::Diamonds;
    std::vector<Card> deck = {six, seven, eight};
    for (int i = 0; i < 200; ++i)
    {
        Card filler;
        filler.value = 8; // neutral in BOTH systems (Reko 0, Hi-Lo 0), so it never perturbs either count
        filler.suit = Suit::Clubs;
        deck.push_back(filler);
    }

    test_support::seed_deck(game,deck);
    game.startTestRound();

    // Default style is Reko; the raw running count after the deal is reko(6)+reko(7)=2 and activeTrueCount must return it undivided by decks.
    const float reko = game.activeTrueCount();
    CHECK(reko == Approx(2.0f));

    // Toggling to Hi-Lo uses the raw card sum (1); true count must then normalize by decks remaining, so it cannot equal the raw 1.
    game.toggleCountingStyle();
    const float hilo = game.activeTrueCount();
    CHECK(hilo != Approx(1.0f));
}

// ---------------------------------------------------------------------------
// Feature 3: confirmBet records each wager into the session.
// bet-distribution histogram that backs the bet-histogram dashboard chart.
// ---------------------------------------------------------------------------

TEST_CASE("confirmBet records the wager into the session bet-distribution histogram", "[Feature3][Analytics]")
{
    Game game(true);

    // Any deck with enough cards for two deals; card values are irrelevant to this assertion.
    std::vector<Card> deck;
    for (int i = 0; i < 210; ++i)
    {
        Card c;
        c.value = 5;
        c.suit = Suit::Hearts;
        deck.push_back(c);
    }
    test_support::seed_deck(game,deck);
    game.startTestRound();

    // startTestRound sets a minimum bet but does not confirm/place it; that is confirmBet()'s job.
    CHECK(game.testPlayer(0).hands[0].getBet() == Constants::MINIMUM_BET);

    game.confirmBet();

    const PlayerStats& stats = game.testPlayer(0).getStats();
    CHECK(stats.betsPlaced == 1);
    CHECK(stats.betDistribution.size() == 1);
    CHECK(stats.betDistribution.at(Constants::MINIMUM_BET) == 1);
}


TEST_CASE("Every stat box except Bankroll restarts at the shoe boundary", "[Feature3][Analytics][Shoe]")
{
    Game game(true);
    test_support::seed_deck(game,humanLosesShoe());

    game.startTestRound();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);
    game.updateHUDStateForTest();

    // One settled hand: the readouts are populated.
    const ShoeAnalytics& first = game.shoeAnalyticsForTest();
    REQUIRE(first.handProfitCount == 1);
    REQUIRE(first.bankrollSeries.size() == 1u);
    REQUIRE(first.edgeSeries.size() == 1u);
    REQUIRE(first.betDistribution.size() == 1u);
    REQUIRE(first.strategyTotal == 1);
    CHECK(first.longestLosingStreak == 1);
    CHECK(first.currentLoseStreak == 1);
    CHECK(game.hudStateForTest().streak == 1);
    CHECK(game.hudStateForTest().longestLosingStreak == 1);

    const int bankrollBefore = game.testPlayer(0).getBankroll();
    REQUIRE(bankrollBefore != GameConfig::STARTING_BANKROLL); // the round really settled

    game.startNewShoeForTest();
    game.updateHUDStateForTest();

    // Per-shoe trackers: all the way back to zero.
    const ShoeAnalytics& after = game.shoeAnalyticsForTest();
    CHECK(after.handProfitCount == 0);
    CHECK(after.handProfitSum == 0);
    CHECK(after.handProfitSumSq == 0);
    CHECK(after.currentWinStreak == 0);
    CHECK(after.currentLoseStreak == 0);
    CHECK(after.longestWinningStreak == 0);
    CHECK(after.longestLosingStreak == 0);
    CHECK(after.roundStreak == 0);
    CHECK(after.strategyTotal == 0);
    CHECK(after.strategyHits == 0);
    CHECK(after.strategyDeviations == 0);
    CHECK(after.betDistribution.empty());
    CHECK(after.bankrollSeries.empty());
    CHECK(after.edgeSeries.empty());

    // These boxes read directly off HUDState: Std Dev, Long Win, Long Loss, Edge %, Streak and more.
    // Bets Placed histogram are all back at their baseline.
    REQUIRE(game.hudStateForTest().shoeAnalytics != nullptr);
    CHECK(game.hudStateForTest().analyticsStdDev == Approx(0.0));
    CHECK(game.hudStateForTest().longestWinningStreak == 0);
    CHECK(game.hudStateForTest().longestLosingStreak == 0);
    CHECK(game.hudStateForTest().edgePercent == Approx(Constants::BASE_PLAYER_EDGE_PERCENT));
    CHECK(game.hudStateForTest().streak == 0);
    CHECK(game.hudStateForTest().bankrollSeries.empty());
    CHECK(game.hudStateForTest().edgeSeries.empty());
    CHECK(game.hudStateForTest().shoeAnalytics->betDistribution.empty());

    // Bankroll is the one value that persists through the boundary.
    CHECK(game.testPlayer(0).getBankroll() == bankrollBefore);
    CHECK(game.hudStateForTest().bankroll == bankrollBefore);
}

// A streak must not cross a shoe boundary: losses split across it read as 1, not 3.
TEST_CASE("Streaks and std dev do not leak from one shoe into the next", "[Feature3][Analytics][Shoe]")
{
    Game game(true);
    test_support::seed_deck(game,humanLosesShoe());

    game.startTestRound();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    CHECK(game.shoeAnalyticsForTest().handProfitCount == 2);
    CHECK(game.shoeAnalyticsForTest().longestLosingStreak == 2);
    CHECK(game.shoeAnalyticsForTest().roundStreak == 2);

    // Seed the career record with numbers no shoe could produce; if any box still reads PlayerStats this fails.
    // it would show these instead of the shoe's own values.
    PlayerStats& career = game.testPlayer(0).getStats();
    career.handProfitCount = 100;
    career.handProfitSum = 1000;
    career.handProfitSumSq = 20000;
    career.longestWinningStreak = 42;
    career.longestLosingStreak = 17;
    career.betDistribution[995] = 5;

    game.updateHUDStateForTest();
    CHECK(game.hudStateForTest().longestLosingStreak == 2);  // not the career 17
    CHECK(game.hudStateForTest().longestWinningStreak == 0); // not the career 42
    // Two hands of -10 each: zero spread regardless of career accumulators.
    CHECK(game.hudStateForTest().analyticsStdDev == Approx(0.0));
    REQUIRE(game.hudStateForTest().shoeAnalytics != nullptr);
    CHECK(game.hudStateForTest().shoeAnalytics->betDistribution.count(995) == 0);

    // Cross the boundary, then lose once more in the fresh shoe.
    game.startNewShoeForTest();
    test_support::seed_deck(game,humanLosesShoe());
    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);
    game.updateHUDStateForTest();

    CHECK(game.shoeAnalyticsForTest().handProfitCount == 1);
    CHECK(game.shoeAnalyticsForTest().longestLosingStreak == 1); // the run restarted, it did not continue to 3
    CHECK(game.shoeAnalyticsForTest().roundStreak == 1);
    CHECK(game.hudStateForTest().longestLosingStreak == 1);
    CHECK(game.hudStateForTest().streak == 1);
    CHECK(game.hudStateForTest().streakIsWin == false);
}

// ---------------------------------------------------------------------------
// The Count / True Count boxes. resetShoeAnalytics() used to re-seed only the
// active counting system and leave `backgroundRunningCount` holding the
// previous shoe's total, so the first style toggle after a reshuffle swapped a
// stale count into the box.
// ---------------------------------------------------------------------------
TEST_CASE("Both counting systems restart at the shoe boundary", "[Counting][Shoe]")
{
    Game game(true); // headless default style is Reko

    std::vector<Card> shoe = {
        test_support::make_card(2, Suit::Spades), test_support::make_card(3, Suit::Clubs), test_support::make_card(4, Suit::Diamonds), test_support::make_card(5, Suit::Hearts),
    };
    padWithEights(shoe);
    test_support::seed_deck(game,shoe);

    game.startTestRound();
    game.confirmBet();

    const int rekoSeed = BasicStrategy::rekoStartingOffset(Constants::NUM_DECKS);
    // Four low cards dealt: both buffers have moved off their seeds.
    CHECK(game.runningCountForTest() != rekoSeed);
    CHECK(game.backgroundCountForTest() != 0);

    game.startNewShoeForTest();

    CHECK(game.runningCountForTest() == rekoSeed); // active system re-seeded
    CHECK(game.backgroundCountForTest() == 0);     // the other system too, not left at last shoe's total

    game.updateHUDStateForTest();
    CHECK(game.hudStateForTest().runningCount == rekoSeed);
    // Reko is unbalanced: the True Count box shows the raw running count.
    CHECK(game.hudStateForTest().trueCount == Approx(static_cast<float>(rekoSeed)));

    // Toggling must swap in the fresh buffer, never the stale one.
    game.toggleCountingStyle();
    CHECK(game.runningCountForTest() == 0);
    CHECK(game.backgroundCountForTest() == rekoSeed);
}

// ---------------------------------------------------------------------------
// NPC isolation. The table's other seats must not move the human's readouts:
// the bankroll curve, the Streak box, the Strategy % box and the career
// win/loss record all follow seat 0 alone.
//
// Two seats. Deal order: p0, p1, dealer-up, p0, p1, dealer-hole | p0, p1,
// dealer, p0, p1, dealer. The human stands on 16 and loses; Alice plays basic
// strategy, stands on 19 and beats the dealer's 17.
// ---------------------------------------------------------------------------
TEST_CASE("Bankroll curve, Streak and Strategy boxes ignore the NPCs", "[Feature3][Analytics][NPC]")
{
    Game game(true);

    std::vector<Card> shoe = {
        test_support::make_card(5, Suit::Spades),    // p0 card 1
        test_support::make_card(8, Suit::Hearts),    // p1 card 1
        test_support::make_card(10, Suit::Diamonds), // dealer up card
        test_support::make_card(4, Suit::Clubs),     // p0 card 2 -> 9
        test_support::make_card(5, Suit::Spades),    // p1 card 2 -> 13
        test_support::make_card(2, Suit::Hearts),    // dealer hole  -> 12
        test_support::make_card(2, Suit::Clubs),     // p0 card 3 -> 11
        test_support::make_card(2, Suit::Spades),    // p1 card 3 -> 15
        test_support::make_card(3, Suit::Clubs),     // dealer      -> 15
        test_support::make_card(5, Suit::Diamonds),  // p0 card 4 -> 16
        test_support::make_card(4, Suit::Hearts),    // p1 card 4 -> 19
        test_support::make_card(2, Suit::Clubs),     // dealer      -> 17
    };
    padWithEights(shoe);
    test_support::seed_deck(game,shoe);

    game.startTestRound(2);
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    const int humanNet = game.testPlayer(0).getBankroll() - GameConfig::STARTING_BANKROLL;
    const int npcNet = game.testPlayer(1).getBankroll() - GameConfig::NPC_CONFIGS[0].bankroll;

    // Setup is honest but the table net differs from the human's, so the assertions below are meaningful.
    // would fail against the old "sum every seat" curve.
    REQUIRE(humanNet == -Constants::MINIMUM_BET);
    REQUIRE(npcNet == Constants::MINIMUM_BET);

    REQUIRE(game.bankrollSeriesForTest().size() == 1u);
    CHECK(game.bankrollSeriesForTest().back() == Approx(static_cast<float>(humanNet)));

    game.updateHUDStateForTest();
    // Human lost while Alice won: streak reads 1L, not the 0 a mixed table would give.
    CHECK(game.hudStateForTest().streak == 1);
    CHECK(game.hudStateForTest().streakIsWin == false);

    // Strategy % counts the human's single stand, not Alice's play as well.
    CHECK(game.shoeAnalyticsForTest().strategyTotal == 1);

    // Career attribution is per-seat now: Alice's win is hers, not the player's.
    CHECK(game.testPlayer(0).getStats().wins == 0);
    CHECK(game.testPlayer(0).getStats().losses == 1);
    CHECK(game.testPlayer(1).getStats().wins == 1);
    CHECK(game.testPlayer(1).getStats().losses == 0);
}



TEST_CASE("StatsDashboard: win-rate excludes pushes and tallies per hand", "[Feature4][Stats]")
{
    PlayerStats career;
    std::vector<RoundRecord> rounds;
    // 3 decisive wins, 2 decisive losses, 1 surrender (a neutral push here).
    rounds.push_back(decidedRound(true, false, 10, 20));   // win
    rounds.push_back(decidedRound(false, false, 10, 0));   // loss
    rounds.push_back(decidedRound(true, false, 10, 20));   // win
    rounds.push_back(decidedRound(false, false, 10, 0));   // loss
    rounds.push_back(decidedRound(true, false, 10, 20));   // win
    rounds.push_back(decidedRound(true, true, 10, 10));    // surrendered -> push (won flag is ignored)

    StatsDashboard dash;
    dash.feed(career, rounds);

    CHECK(dash.handsFinishedForTest() == 6);   // wins + losses + pushes
    CHECK(dash.computed().wins == 3);
    CHECK(dash.computed().losses == 2);
    CHECK(dash.computed().pushes == 1);
    CHECK(dash.winRateForTest() == Approx(0.6));   // 3 / (3 + 2) decisive, pushes excluded
}

TEST_CASE("StatsDashboard: detects naturals and busts per hand", "[Feature4][Stats]")
{
    PlayerStats career;
    std::vector<RoundRecord> rounds;
    rounds.push_back(bjRound(10));                 // 1 natural
    rounds.push_back(decidedRound(true, false, 10, 20)); // not a BJ (hard 11 -> needs more cards)
    rounds.push_back(bustRound(10, 0));            // 1 bust

    StatsDashboard dash;
    dash.feed(career, rounds);

    CHECK(dash.blackjacksForTest() == 1);
    CHECK(dash.bustsForTest() == 1);
    CHECK(dash.handsFinishedForTest() == 3);
    // Frequencies are per-hand: one BJ and one bust over three hands.
    CHECK(dash.bjFrequencyForTest() == Approx(1.0 / 3.0));
    CHECK(dash.bustFrequencyForTest() == Approx(1.0 / 3.0));
}

TEST_CASE("StatsDashboard: profit series, bankroll curve and drawdown", "[Feature4][Stats]")
{
    PlayerStats career;
    std::vector<RoundRecord> rounds;
    // Cumulative nets +10/-30/+5/+35 give peak +35 and worst dip of 30 (from +10 to -20).
    rounds.push_back(decidedRound(true, false, 10, 20));   // +10
    rounds.push_back(decidedRound(false, false, 30, 0));   // -30
    rounds.push_back(decidedRound(true, false, 10, 15));   // +5
    rounds.push_back(decidedRound(true, false, 10, 60));   // +50

    StatsDashboard dash;
    dash.feed(career, rounds);

    const auto& profit = dash.perRoundProfitForTest();
    CHECK(profit.size() == 4u);
    CHECK(profit[0] == 10);
    CHECK(profit[1] == -30);
    CHECK(profit[2] == 5);
    CHECK(profit[3] == 50);

    const auto& series = dash.bankrollSeriesForTest();
    CHECK(series.size() == 4u);
    CHECK(series[0] == 10);
    CHECK(series[1] == -20);
    CHECK(series[2] == -15);
    CHECK(series[3] == 35);

    CHECK(dash.peakBankrollForTest() == 35);
    CHECK(dash.maxDrawdownForTest() == 30);   // deepest peak-to-current decline, magnitude >= 0
}

TEST_CASE("StatsDashboard: strategy accuracy comes from career stats only", "[Feature4][Stats]")
{
    PlayerStats career;
    career.strategyHits = 80;
    career.strategyTotal = 100;
    StatsDashboard dash;
    dash.feed(career, {}); // no round history -> per-hand metrics stay zero

    CHECK(dash.handsFinishedForTest() == 0);
    CHECK(dash.winRateForTest() == Approx(0.0));
    CHECK(dash.strategyAccuracyForTest() == Approx(0.8));
}

TEST_CASE("StatsDashboard: best/worst single-hand net profit", "[Feature4][Stats]")
{
    PlayerStats career;
    std::vector<RoundRecord> rounds;
    // nets vs stake: -5, +40, -12, +3 -> worst -12, best +40.
    rounds.push_back(decidedRound(false, false, 5, 0));   // -5
    rounds.push_back(decidedRound(true, false, 20, 60));  // +40
    rounds.push_back(decidedRound(false, false, 12, 0));  // -12
    rounds.push_back(decidedRound(true, false, 3, 6));    // +3

    StatsDashboard dash;
    dash.feed(career, rounds);

    CHECK(dash.greatestHandProfitForTest() == 40);
    CHECK(dash.worstHandLossForTest() == -12);
}

TEST_CASE("StatsDashboard: aggregates across multiple shoes (history flattening)", "[Feature4][Stats]")
{
    // Mirrors Game::startStatsSession(): flatten every ShoeRecord's rounds into one history list, then feed.
    PlayerStats career;
    std::vector<ShoeRecord> shoes;

    ShoeRecord shoe0;
    shoe0.rounds.push_back(decidedRound(true, false, 10, 20));   // win
    shoe0.rounds.push_back(bustRound(10, 0));                    // bust

    ShoeRecord shoe1;
    shoe1.rounds.push_back(decidedRound(false, false, 10, 0));   // loss
    shoe1.rounds.push_back(bjRound(10));                          // natural
    shoes.push_back(shoe0);
    shoes.push_back(shoe1);

    std::vector<RoundRecord> rounds;
    for (const auto& shoe : shoes)
        for (const auto& r : shoe.rounds)
            rounds.push_back(r);

    StatsDashboard dash;
    dash.feed(career, rounds);

    CHECK(dash.handsFinishedForTest() == 4);   // two per shoe across both
    CHECK(dash.computed().wins == 2);          // decided win + natural BJ
    CHECK(dash.computed().losses == 2);        // bust hand + decided loss
    CHECK(dash.blackjacksForTest() == 1);
    CHECK(dash.bustsForTest() == 1);
}

TEST_CASE("StatsDashboard: JSON and CSV export stay populated for any history", "[Feature4][Export]")
{
    PlayerStats career;
    std::vector<RoundRecord> rounds;
    rounds.push_back(decidedRound(true, false, 10, 20));   // +10
    rounds.push_back(decidedRound(false, false, 10, 0));   // -10

    StatsDashboard dash;
    dash.feed(career, rounds);

    // Parse the JSON export back so the check is independent of nlohmann's formatting.
    const json parsed = json::parse(dash.toJson());
    REQUIRE(parsed.contains("wins"));
    REQUIRE(parsed.contains("winRate"));
    REQUIRE(parsed.contains("perRoundProfit"));
    CHECK(parsed.at("perRoundProfit").get<std::vector<int>>() == std::vector<int>{10, -10});
    CHECK(parsed.at("bankrollSeries").get<std::vector<long long>>() == std::vector<long long>{10, 0});

    const std::string csv = dash.toCsv();
    REQUIRE(!csv.empty());
    CHECK(csv.rfind("metric,value\n", 0) == 0);                // exact CSV header as the first line
    CHECK(csv.find("metric,value") != std::string::npos);      // CSV header
    CHECK(csv.find("hands_finished") != std::string::npos);     // metric rows present
    CHECK(csv.find("round_index,net_profit,cumulative_bankroll") != std::string::npos);  // per-round detail header
    // Two rounds -> CSV detail lines print both nets with correct signs ('-10' loss, '10' win).
    CHECK(csv.find("-10") != std::string::npos);
    CHECK(csv.find("10") != std::string::npos);
}

TEST_CASE("StatsDashboard: tab cycling wraps Overview->Hands->Profits->Overview", "[Feature4][Tab]")
{
    StatsDashboard dash;
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Overview);

    dash.nextTabForTest();   // Overview -> Hands
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Hands);
    dash.prevTabForTest();   // Hands -> Overview
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Overview);

    dash.prevTabForTest();   // wrap backward: Overview -> Profits
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Profits);
    dash.nextTabForTest();   // forward wrap: Profits -> Overview
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Overview);
}

TEST_CASE("StatsDashboard: empty history yields safe zeros and non-empty exports", "[Feature4][Edge]")
{
    PlayerStats career;
    StatsDashboard dash;
    dash.feed(career, {});

    CHECK(dash.handsFinishedForTest() == 0);
    CHECK(dash.computed().wins == 0);
    CHECK(dash.winRateForTest() == Approx(0.0));
    CHECK(dash.bjFrequencyForTest() == Approx(0.0));
    CHECK(dash.perRoundProfitForTest().empty());
    CHECK(dash.bankrollSeriesForTest().empty());

    // Exports must never be empty even with no data.
    REQUIRE(!dash.toJson().empty());
    REQUIRE(!dash.toCsv().empty());
}

// Reproduction of the user-reported stats crash after play + return-to-menu.
#include "Game.hpp"
#include "PlayerProfile.hpp"
#include <iostream>
namespace
{
    Card reproCard(int value, Suit suit) { Card c; c.value = value; c.suit = suit; return c; }
}

TEST_CASE("REPRO stats flow: play -> quit-to-menu -> open dashboard", "[StatsRepro][TEMP]")
{
    Game game(true); // headless
    std::vector<Card> shoe = {
        reproCard(2, Suit::Spades), reproCard(3, Suit::Hearts), reproCard(4, Suit::Diamonds), reproCard(5, Suit::Clubs),
        reproCard(6, Suit::Spades), reproCard(7, Suit::Hearts), reproCard(8, Suit::Diamonds), reproCard(9, Suit::Clubs),
        reproCard(10, Suit::Spades), reproCard(2, Suit::Hearts), reproCard(3, Suit::Diamonds), reproCard(4, Suit::Clubs)
    };
    test_support::seed_deck(game,shoe);

    // One full round settles and appends to both profile.currentHistory and currentShoeRecord.rounds.
    game.startTestRound();
    game.confirmBet();
    game.stand();
    game.deltaTime = 100.0f;
    for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        game.advanceTurnState(100.0f);
    CHECK(game.roundStateForTest() == RoundState::Cleanup);

    // Inspect the live settled round before any storage copy to localize corruption in settlement.
    {
        const RoundRecord& live = game.currentRoundRecordForTest();
        std::cerr << "[REPRO] LIVE round dealerCards=" << live.dealerCards.size()
                  << " hands=" << live.playerHands.size() << std::endl;
        for (std::size_t h = 0; h < live.playerHands.size(); ++h)
            std::cerr << "   LIVE hand[" << h << "] cards=" << live.playerHands[h].cards.size() << std::endl;
    }

    // Consolidate the in-progress shoe into longHistory like QuitToMenu while leaving currentShoeRecord untouched; opening stats then feeds that round from BOTH paths -> double-count if unguarded.
    {
        PlayerProfile profile = game.playersForTest()[0].getProfile();
        profile.addShoeRecord(ShoeRecord{});
        game.playersForTest()[0].updateProfile(profile);
    }

    // Before opening stats, inspect what actually landed in longHistory after the quit-to-menu mimic.
    {
        PlayerProfile diag = game.playersForTest()[0].getProfile();
        const auto& hist = diag.getLongHistory();
        std::cerr << "[REPRO] longHistory shoes=" << hist.size() << std::endl;
        for (std::size_t s = 0; s < hist.size(); ++s)
        {
            std::cerr << "  shoe[" << s << "] rounds=" << hist[s].rounds.size() << std::endl;
            for (std::size_t r = 0; r < hist[s].rounds.size(); ++r)
            {
                std::cerr << "    round[" << r << "] dealerCards=" << hist[s].rounds[r].dealerCards.size()
                          << " hands=" << hist[s].rounds[r].playerHands.size() << std::endl;
                for (std::size_t h = 0; h < hist[s].rounds[r].playerHands.size(); ++h)
                    std::cerr << "      hand[" << h << "] cards=" << hist[s].rounds[r].playerHands[h].cards.size()
                              << " wager=" << hist[s].rounds[r].playerHands[h].wager << std::endl;
            }
        }
    }

    // Open statistics exactly as the menu does, then count rounds fed through the real path.
    game.startStatsSession();
    const int roundsFed = game.statsPerRoundCountForTest();
    std::cerr << "[REPRO] rounds fed to dashboard=" << roundsFed << " (expected 1)" << std::endl;
    // A completed round feeds exactly once; history + current record makes it two.
    CHECK(roundsFed == 1);
}



TEST_CASE("Edge series, bankroll series and Edge % all reset at the shoe boundary", "[Feature3][Analytics][Shoe]")
{
    Game game(true);

    // An 11-card test round burns 8 before the dealer plays (4 deal + 4 confirmBet) plus one hit, so round 1 takes 9 and leaves 2.
    // the shoe dry on its third card, which is what fires handleEmptyDeck().
    std::vector<Card> shoe = {
        test_support::make_card(5, Suit::Spades), test_support::make_card(3, Suit::Clubs), test_support::make_card(4, Suit::Diamonds), test_support::make_card(6, Suit::Hearts),
        test_support::make_card(2, Suit::Spades), test_support::make_card(2, Suit::Clubs), test_support::make_card(2, Suit::Diamonds), test_support::make_card(2, Suit::Hearts),
        test_support::make_card(7, Suit::Spades), test_support::make_card(7, Suit::Clubs), test_support::make_card(7, Suit::Diamonds),
    };
    test_support::seed_deck(game,shoe);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    // Round 1 settled: both curves have a point; nothing mid-shoe touched the recalibration counter.
    const int generationBefore = game.shoeGenerationForTest();
    REQUIRE(game.edgeSeriesForTest().size() == 1u);
    REQUIRE(game.bankrollSeriesForTest().size() == 1u);

    // Dealing the next round runs the shoe dry -> handleEmptyDeck() -> resetShoeAnalytics().
    game.startTestRound();

    CHECK(game.shoeGenerationForTest() == generationBefore + 1);   // the HUD's recalibration signal moved
    CHECK(game.edgeSeriesForTest().empty());                       // Edge Over Time back to baseline
    CHECK(game.bankrollSeriesForTest().empty());                   // Bankroll Curve back to baseline

    // Edge % is the live estimate off the current count, so post-boundary it follows the new shoe's count instead of the old.
    game.updateHUDStateForTest();
    CHECK(game.edgePercentForTest() == Approx(BasicStrategy::estimateEdgePercent(game.hiLoTrueCountForTest())));
    CHECK(game.edgePercentForTest() > -5.0);
    CHECK(game.edgePercentForTest() < 5.0);
}

// The reset fires only on a real shoe boundary, not ordinary rounds (the shoe is deep enough to distinguish).
// Two full rounds each burn ~10 cards (4 deal + 4 confirmBet + dealer hits), driving the recalibration check.
// never run it dry.
TEST_CASE("Ordinary rounds append to the edge series; they do not reset it", "[Feature3][Analytics][Shoe]")
{
    Game game(true);

    std::vector<Card> shoe;
    for (int i = 0; i < 40; ++i)
        shoe.push_back(test_support::make_card(2, Suit::Clubs));   // low + count-neutral: rounds settle fast, the shoe stays deep

    test_support::seed_deck(game,shoe);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    CHECK(game.shoeGenerationForTest() == 0);            // no boundary crossed
    CHECK(game.edgeSeriesForTest().size() == 2u);        // both rounds contributed, nothing was wiped
    CHECK(game.bankrollSeriesForTest().size() == 2u);
}

// ---------------------------------------------------------------------------
// 3. The edge is a LEVEL, not a running total. The house edge does not stack
//    up hand over hand - it moves with the count, so every round re-estimates
//    it from the count as it stands. Integrating it (the old behaviour) is what
//    drove the curve to -13% after ten rounds of an ordinary shoe.
// ---------------------------------------------------------------------------
TEST_CASE("estimateEdgePercent is calibrated to the count and stays inside a few percent", "[Feature3][Analytics][Edge]")
{
    // True count 0 is a fresh shoe: basic strategy's usual half percent behind the house.
    CHECK(BasicStrategy::estimateEdgePercent(0.0f) == Approx(Constants::BASE_PLAYER_EDGE_PERCENT));
    CHECK(BasicStrategy::estimateEdgePercent(4.0f) == Approx(-0.5 + 4 * 0.5));
    CHECK(BasicStrategy::estimateEdgePercent(-4.0f) == Approx(-0.5 - 4 * 0.5));

    // Any count a real shoe reaches keeps the estimate inside the expected +-5% band.
    for (int tc = -8; tc <= 8; ++tc)
    {
        const double edge = BasicStrategy::estimateEdgePercent(static_cast<float>(tc));
        CHECK(edge > -5.0);
        CHECK(edge < 5.0);
    }
}

TEST_CASE("Edge Over Time samples the current edge instead of adding it up each round", "[Feature3][Analytics][Edge]")
{
    Game game(true);

    // Eight low cards then neutral 8s; the human stands on 12 and the dealer on 17, so round 1 takes...
    // exactly those eight cards - every one of them +1 to Hi-Lo.
    std::vector<Card> shoe = {
        test_support::make_card(5, Suit::Spades), test_support::make_card(4, Suit::Clubs), test_support::make_card(3, Suit::Diamonds), test_support::make_card(6, Suit::Hearts),
        test_support::make_card(2, Suit::Spades), test_support::make_card(3, Suit::Clubs), test_support::make_card(2, Suit::Diamonds), test_support::make_card(4, Suit::Hearts),
    };
    for (int i = 0; i < 204; ++i)
        shoe.push_back(test_support::make_card(8, Suit::Clubs));
    test_support::seed_deck(game,shoe);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    REQUIRE(game.edgeSeriesForTest().size() == 1u);

    // Hi-Lo count +8 with 204 cards left -> 3.9231 decks -> TC 2.0392 -> edge -0.5 + 1.0196.
    const double round1Tc = 8.0 / (204.0 / 52.0);
    CHECK(game.edgeSeriesForTest().back() == Approx(-0.5 + 0.5 * round1Tc));
    // The old model plotted the edge as a per-round increment (0.5 * TC here) and kept adding to it.
    CHECK(game.edgeSeriesForTest().back() != Approx(0.5 * round1Tc));

    // Two more ordinary rounds: the plotted value drifts with the count, it does not ramp.
    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    test_support::settleRound(game);

    const std::vector<double>& series = game.edgeSeriesForTest();
    REQUIRE(series.size() == 3u);

    // Every sample is the edge AT that moment, so it stays inside the expected band...
    for (double v : series)
    {
        CHECK(v > -5.0);
        CHECK(v < 5.0);
    }
    // The newest point is the live estimate off the current count, not a running total of prior rounds.
    CHECK(series.back() == Approx(BasicStrategy::estimateEdgePercent(game.hiLoTrueCountForTest())));

    // Three rising-count rounds moved the edge under a point; a running total would have drifted several, turning a normal shoe into a spurious -13% disaster.
    CHECK(series.back() - series.front() < 2.0);
}

// In play over a full shuffled shoe of ordinary rounds, edge wobbles within a few percent of the fresh-shoe baseline and never ramps to -13% like an integrated total would.
// version did, no matter how many rounds the shoe gets through.
TEST_CASE("A whole shoe of ordinary rounds keeps the edge inside the expected band", "[Feature3][Analytics][Edge]")
{
    Game game(true);
    game.startNewShoeForTest();  // a real shuffled 6-deck shoe, not an injected one
    game.startTestRound();

    int rounds = 0;
    for (int i = 0; i < 40 && game.gameStateForTest() == GameState::Started; ++i)
    {
        game.confirmBet();
        game.stand();
        test_support::settleRound(game);
        ++rounds;
        if (game.gameStateForTest() != GameState::Started)
            break;
        game.nextRoundForTest();
    }

    const std::vector<double>& series = game.edgeSeriesForTest();
    // A 6-deck shoe cut 25 cards short still deals plenty of rounds; a tiny count means the shoe setup broke.
    // not the edge model.
    REQUIRE(rounds >= 10);
    REQUIRE(series.size() >= 10u);

    double lo = series.front(), hi = series.front();
    for (double v : series)
    {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
        CHECK(v > -5.0);
        CHECK(v < 5.0);
    }

    // The whole curve spans a narrow band around the baseline rather than drifting across it.
    CHECK(hi - lo < 5.0);
    CHECK(game.edgePercentForTest() > -5.0);
    CHECK(game.edgePercentForTest() < 5.0);
}

// ---------------------------------------------------------------------------
// Bets Placed histogram bucketing. One bar per distinct wager is unreadable as
// soon as the player varies their bet, and the bar set shifts as new sizes come
// in. bucketBetDistribution() folds everything into a fixed number of evenly
// spaced ranges spanning [0, MAXIMUM_BET].
// ---------------------------------------------------------------------------

TEST_CASE("Bet distribution folds into a fixed count of evenly spaced ranges", "[Feature3][Analytics][Histogram]")
{
    // Deliberately scattered bet sizes: one bar per value would yield seven wildly-different-width bars.
    std::map<int, int> bets = {{10, 3}, {25, 1}, {50, 2}, {100, 4}, {500, 1}, {995, 2}, {1000, 5}};

    const std::vector<int> buckets =
        HUD::bucketBetDistribution(bets, Constants::BET_BUCKET_COUNT, Constants::MAXIMUM_BET);

    // Always exactly bucketCount bars, whatever was wagered -- the chart's scale never moves.
    REQUIRE(buckets.size() == static_cast<std::size_t>(Constants::BET_BUCKET_COUNT));
    CHECK(Constants::BET_BUCKET_WIDTH == Constants::MAXIMUM_BET / Constants::BET_BUCKET_COUNT);

    // Bucket i covers [i*width, (i+1)*width).
    CHECK(buckets[0] == 3 + 1 + 2);   // 10, 25 and 50 all land in [0, 100)
    CHECK(buckets[1] == 4);           // 100 lands in [100, 200)
    CHECK(buckets[5] == 1);           // 500 lands in [500, 600)
    CHECK(buckets[9] == 2 + 5);       // 995 and the table max 1000 share the top range

    // Nothing is dropped or double counted.
    int placed = 0;
    for (int b : buckets)
        placed += b;
    int wagered = 0;
    for (const auto& e : bets)
        wagered += e.second;
    CHECK(placed == wagered);
}

// Wagers at or above the table maximum clamp into the last range rather than growing the axis.
// non-positive entries must not create a phantom bar.
TEST_CASE("Oversized wagers clamp to the top range; zero/negative wagers are ignored", "[Feature3][Analytics][Histogram]")
{
    std::map<int, int> bets = {{0, 7}, {-50, 3}, {1000, 1}, {5000, 2}, {100000, 1}};

    const std::vector<int> buckets =
        HUD::bucketBetDistribution(bets, Constants::BET_BUCKET_COUNT, Constants::MAXIMUM_BET);

    REQUIRE(buckets.size() == static_cast<std::size_t>(Constants::BET_BUCKET_COUNT));
    CHECK(buckets[0] == 0);                       // 0 and -50 never reach the chart
    CHECK(buckets[9] == 1 + 2 + 1);               // everything at/above the max clamps into the last bar
}

// An empty session still produces the full set of empty bars, keeping the evenly spaced scale intact.
// instead of collapsing or blanking out between shoes.
TEST_CASE("Empty bet distribution still yields the full set of bars", "[Feature3][Analytics][Histogram]")
{
    const std::vector<int> buckets =
        HUD::bucketBetDistribution({}, Constants::BET_BUCKET_COUNT, Constants::MAXIMUM_BET);

    REQUIRE(buckets.size() == static_cast<std::size_t>(Constants::BET_BUCKET_COUNT));
    for (int b : buckets)
        CHECK(b == 0);
}
