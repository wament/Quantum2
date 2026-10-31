#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <BasicStrategy.hpp>
#include <Game.hpp>
#include <GameRecord.hpp>
#include <RoundEngine.hpp>
#include <Card.hpp>
#include <Deck.hpp>
#include <vector>

using Catch::Approx;

// ---------------------------------------------------------------------------
// Stat-box accuracy. Every readout on the HUD except Bankroll is supposed to
// describe the CURRENT shoe, and the bankroll curve is supposed to be the
// human seat's own profit and loss. Historically both broke:
//   1. Std Dev / Long Win / Long Loss / Strategy % / the Bets Placed histogram
//      were read straight out of PlayerStats, which is the career record saved
//      to the profile - so they survived every shoe (and every session) and
//      climbed forever.
//   2. The Count box re-seeded only the active counting system, so toggling
//      styles after a reshuffle swapped in the previous shoe's count.
//   3. evaluateHands() took players[0]'s stats once, outside the loop over all
//      seats, so NPC wins/losses were booked as the human's - into the streak,
//      the accuracy % and the bankroll curve.
// Everything is driven through the real round-state machine, so a regression in
// the shoe-boundary path itself is caught here.
// ---------------------------------------------------------------------------

namespace
{

    // Neutral filler: 8s score 0 in Hi-Lo AND in Reko, so the shoe never runs dry mid-round and the
    // counting systems stay exactly where the explicit cards above left them.
    void padWithEights(std::vector<Card>& shoe)
    {
        for (int i = 0; i < 200; ++i)
            shoe.push_back(test_support::make_card(8, Suit::Clubs));
    }

    // Drive frames until the round fully settles (dealer finishes and evaluateHands runs, which is what
    // writes the streaks, the variance accumulators and the two graph points). dealerTurn() paces itself
    // off Game::deltaTime rather than the tick argument, so seed that member large.
    void tickUntilSettled(Game& game)
    {
        RoundEngine engine;
        game.deltaTime = 100.0f;
        for (int i = 0; i < 50; ++i)
        {
            RoundState rs = game.roundStateForTest();
            if (rs == RoundState::PlayerTurn || rs == RoundState::DealerTurn)
                engine.tick(100.0f, game);
            else
                break;
        }
    }

    // One seat. Deal order: player, dealer-up, player, dealer-hole, player, dealer, player, dealer.
    // Round 1 (startTestRound + confirmBet deal twice): the human stands on 12, the dealer stands on 17.
    // The next four cards are the single deal a plain next-round takes: the human stands on 12 again while the
    // dealer hits its own 12 and lands on 20. Both rounds are losses.
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
} // namespace

// ---------------------------------------------------------------------------
// The shoe boundary is Game::startNewShoe() - the single path the R key at end
// of shoe and auto-play's advance both take. Everything the HUD reports except
// the bankroll must be back at its baseline on the other side of it.
// ---------------------------------------------------------------------------
TEST_CASE("Every stat box except Bankroll restarts at the shoe boundary", "[Feature3][Analytics][Shoe]")
{
    Game game(true);
    test_support::seed_deck(game,humanLosesShoe());

    game.startTestRound();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);
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

    // What the boxes render from, straight off HUDState: Std Dev, Long Win, Long Loss, Edge %, Streak and the
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

// A streak must not be carried into the next shoe: losses split across a boundary read as 1, not 3.
TEST_CASE("Streaks and std dev do not leak from one shoe into the next", "[Feature3][Analytics][Shoe]")
{
    Game game(true);
    test_support::seed_deck(game,humanLosesShoe());

    game.startTestRound();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

    CHECK(game.shoeAnalyticsForTest().handProfitCount == 2);
    CHECK(game.shoeAnalyticsForTest().longestLosingStreak == 2);
    CHECK(game.shoeAnalyticsForTest().roundStreak == 2);

    // Seed the persisted career record with numbers no shoe could produce. If any box still read PlayerStats,
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
    // Two hands of exactly -10: no spread at all, regardless of the career accumulators.
    CHECK(game.hudStateForTest().analyticsStdDev == Approx(0.0));
    REQUIRE(game.hudStateForTest().shoeAnalytics != nullptr);
    CHECK(game.hudStateForTest().shoeAnalytics->betDistribution.count(995) == 0);

    // Cross the boundary, then lose once more in the fresh shoe.
    game.startNewShoeForTest();
    test_support::seed_deck(game,humanLosesShoe());
    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);
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
    tickUntilSettled(game);

    const int humanNet = game.testPlayer(0).getBankroll() - GameConfig::STARTING_BANKROLL;
    const int npcNet = game.testPlayer(1).getBankroll() - GameConfig::NPC_CONFIGS[0].bankroll;

    // The setup does what it claims, and the table's net differs from the human's - so the assertions below
    // would fail against the old "sum every seat" curve.
    REQUIRE(humanNet == -Constants::MINIMUM_BET);
    REQUIRE(npcNet == Constants::MINIMUM_BET);

    REQUIRE(game.bankrollSeriesForTest().size() == 1u);
    CHECK(game.bankrollSeriesForTest().back() == Approx(static_cast<float>(humanNet)));

    game.updateHUDStateForTest();
    // The human lost the round while Alice won hers: the streak reads 1L, not the 0 a mixed table would give.
    CHECK(game.hudStateForTest().streak == 1);
    CHECK(game.hudStateForTest().streakIsWin == false);

    // Strategy % counts the human's single stand, not Alice's play as well.
    CHECK(game.shoeAnalyticsForTest().strategyTotal == 1);

    // The career record is attributed per seat now: Alice's win is hers, not the player's.
    CHECK(game.testPlayer(0).getStats().wins == 0);
    CHECK(game.testPlayer(0).getStats().losses == 1);
    CHECK(game.testPlayer(1).getStats().wins == 1);
    CHECK(game.testPlayer(1).getStats().losses == 0);
}

