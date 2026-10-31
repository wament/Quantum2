#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include <StatsDashboard.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using Catch::Approx;

using namespace Constants;

namespace
{

    // A natural blackjack: an Ace (value 1) plus a ten-value card. Hand::isBlackjack() requires two cards summing to 21.
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

    // A busted hand: 7 + 8 + 8 = 23.
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

    // A plain decisive hand: set won/surrendered explicitly; cards are irrelevant to the win tally.
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
}

// ---------------------------------------------------------------------------
// Feature 4 (Statistics Dashboard): the pure-logic analytics layer is tested
// headless-first, exactly mirroring test_tournament_mode.cpp - drive the class
// directly with synthetic round history and assert computed metrics + exports,
// before any UI. No SFML in these cases; they run over the Graphics-only binary.
// ---------------------------------------------------------------------------

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
    // nets: +10, -30, +5, +50  -> cumulative +10, -20, -15, +35 ; peak +35 ; worst dip from +10 to -20 = 30.
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

    // Parse the JSON export back so the check is independent of nlohmann's formatting (indent / array wrapping).
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
    // Two rounds -> per-round detail lines carry both nets: the loss prints with its sign ("-10") and the win net is a
    // positive integer ("10", which prints without a leading '+'). Both values appear in the CSV detail rows.
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

    // Exports must never be empty even with no data (a dashboard view is still exported).
    REQUIRE(!dash.toJson().empty());
    REQUIRE(!dash.toCsv().empty());
}

// TEMP reproduction of the user-reported stats crash after play + return-to-menu.
#include "Game.hpp"
#include "RoundEngine.hpp"
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

    // One full round to completion (settles -> appends the round to profile.currentHistory AND currentShoeRecord.rounds).
    game.startTestRound();
    game.confirmBet();
    game.stand();
    game.deltaTime = 100.0f;
    RoundEngine engine;
    for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        engine.tick(100.0f, game);
    CHECK(game.roundStateForTest() == RoundState::Cleanup);

    // Inspect the LIVE settled round (before any storage copy) to localize whether corruption originates in settlement.
    {
        const RoundRecord& live = game.currentRoundRecordForTest();
        std::cerr << "[REPRO] LIVE round dealerCards=" << live.dealerCards.size()
                  << " hands=" << live.playerHands.size() << std::endl;
        for (std::size_t h = 0; h < live.playerHands.size(); ++h)
            std::cerr << "   LIVE hand[" << h << "] cards=" << live.playerHands[h].cards.size() << std::endl;
    }

    // Consolidate the in-progress shoe into longHistory exactly as QuitToMenu does (addShoeRecord moves
    // profile.currentHistory -> longHistory). Crucially it leaves game.currentShoeRecord.rounds untouched, so on the next
    // stats-open startStatsSession feeds that round from BOTH longHistory AND currentShoeRecord -> double-count.
    {
        PlayerProfile profile = game.playersForTest()[0].getProfile();
        profile.addShoeRecord(ShoeRecord{});
        game.playersForTest()[0].updateProfile(profile);
    }

    // Diagnose BEFORE opening stats: inspect what actually got stored in longHistory after the quit-to-menu mimic.
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

    // Now open the statistics dashboard exactly as the menu does, then read how many rounds it fed through the real path.
    game.startStatsSession();
    const int roundsFed = game.statsPerRoundCountForTest();
    std::cerr << "[REPRO] rounds fed to dashboard=" << roundsFed << " (expected 1)" << std::endl;
    // A single completed round should feed exactly once. Double-counting (history + current record) makes this 2.
    CHECK(roundsFed == 1);
}

