#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>
#include <Hand.hpp>
#include <Deck.hpp>
#include <Player.hpp>

// ---------------------------------------------------------------------------
// Feature 1 (Basic Strategy Advisor UI): the verdict logic behind the Book box.
// The SFML color itself is driven purely from Game::advisorFeedback, which these
// tests exercise directly through the real round-state machine (startTestRound
// -> hit/stand and RoundEngine::tick) so a regression in deviation tracking,
// win attribution or winsAfterDeviation would be caught here.
// ---------------------------------------------------------------------------

namespace
{

    // Drive frames until the round fully settles to Cleanup (dealer finishes and evaluateHands runs). dealerTurn()
    // advances its own pacing off Game::deltaTime rather than the tick argument, so seed that member large to let the
    // start/action timers drain within a frame; bounded so a stuck state cannot spin forever.
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
}

// A correct action (matches basic strategy) records a hit and leaves the Book box green: advisorFeedback is Correct.
TEST_CASE("Correct action matches basic strategy -> Correct feedback", "[Advisor][Feature1]")
{
    Game game(true);
    // Hard 8 (5+3): basic strategy says Hit against any up-card. c0=5(P1), c1=7(dealer shown), c2=3(P2); hit card c4=2.
    test_support::start_round(game,{test_support::make_card(5, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(3, Suit::Diamonds),  test_support::make_card(6, Suit::Clubs),
                  test_support::make_card(2, Suit::Spades),    test_support::make_card(9, Suit::Hearts)});

    CHECK(game.testPlayer(0).getStats().strategyHits == 0);

    game.hit(); // book says Hit -> this is the correct play

    const PlayerStats& stats = game.testPlayer(0).getStats();
    CHECK(stats.strategyHits == 1);       // matched the book
    CHECK(stats.strategyDeviations == 0); // and did not deviate
    CHECK(game.advisorFeedbackForTest() == AdvisorFeedback::Correct);
}

// A deviation (player stands when the book says Hit) records a deviation, turns the Book box red, and pins what the
// book actually said. At this point (before evaluation) the per-hand win flag is still false.
TEST_CASE("Deviating from basic strategy -> Deviated feedback with a recorded deviation", "[Advisor][Feature1]")
{
    Game game(true);
    // Hard 8 (5+3); basic strategy says Hit, so Standing is the deviation. Same deck as the Correct case above.
    test_support::start_round(game,{test_support::make_card(5, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(3, Suit::Diamonds),  test_support::make_card(6, Suit::Clubs),
                  test_support::make_card(2, Suit::Spades),    test_support::make_card(9, Suit::Hearts)});

    game.stand(); // book says Hit -> deviation

    const PlayerStats& stats = game.testPlayer(0).getStats();
    CHECK(stats.strategyDeviations == 1);
    CHECK(stats.strategyHits == 0);
    CHECK(game.advisorFeedbackForTest() == AdvisorFeedback::Deviated);

    REQUIRE(!stats.recentDeviations.empty());
    const DeviationRecord& rec = stats.recentDeviations.back();
    CHECK(rec.actionTaken == 'S');      // the player stood
    CHECK(rec.correctAction == 'H');    // but the book said hit
    CHECK(rec.won == false);            // not evaluated yet
}

// The headline learning metric: winning a hand you deviated on still counts as a "win after deviation". Deck forces it:
// P1=ten, P2=six => hard 16; dealer shown = five. Basic strategy says Stand vs 5, so hitting is a deviation. The hit
// card (five) makes 21 and auto-finishes; the dealer hole is a ten (dealer total 15), and its next draw is a seven ->
// 22, busting. Player 21 beats a busted dealer, so this deviating hand wins.
TEST_CASE("Winning after a deviation increments winsAfterDeviation and stamps DeviationRecord.won", "[Advisor][Feature1]")
{
    Game game(true);
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(5, Suit::Hearts),
                  test_support::make_card(6, Suit::Diamonds),  test_support::make_card(10, Suit::Clubs),
                  test_support::make_card(5, Suit::Spades),    test_support::make_card(7, Suit::Hearts)});

    // Hard 16 (ten+six) vs dealer's five: basic strategy says Stand. Player hits the five -> 21, auto-finishes; a deviation.
    game.hit();

    tickUntilSettled(game); // resolve through the dealer turn and evaluation

    const PlayerStats& stats = game.testPlayer(0).getStats();
    CHECK(stats.strategyDeviations == 1); // one deviation occurred this round
    CHECK(stats.winsAfterDeviation == 1); // and that deviating hand won

    REQUIRE(!stats.recentDeviations.empty());
    CHECK(stats.recentDeviations.back().won == true); // evaluation stamped the win on the record

    // A single winning hand yields a longest-win streak of exactly one.
    CHECK(stats.longestWinningStreak == 1);
}

// The variance/standard-deviation path feeds from per-hand net-profit accumulators (Feature 3). After any fully
// resolved round these counters must be populated for the settled hands, which is what analyticsStdDev derives from.
// P1=nine, P2=four => 13; dealer shown = five with a ten hole (15); hit card eight -> 21 (auto-finishes); the dealer's
// next draw is a seven -> 22, busting. The player wins and logs one positive settled hand.
TEST_CASE("Settled hands populate the variance accumulators feeding standard deviation", "[Advisor][Feature1][Analytics]")
{
    Game game(true);
    test_support::start_round(game,{test_support::make_card(9, Suit::Spades), test_support::make_card(5, Suit::Hearts),
                  test_support::make_card(4, Suit::Diamonds),  test_support::make_card(10, Suit::Clubs),
                  test_support::make_card(8, Suit::Spades),    test_support::make_card(7, Suit::Hearts)});

    game.hit(); // 13 + eight -> 21 (auto-finish)
    tickUntilSettled(game);

    const PlayerStats& stats = game.testPlayer(0).getStats();
    CHECK(stats.handProfitCount == 1);   // one hand settled for variance
    CHECK(stats.handProfitSum > 0);      // the winning hand is a positive net profit
    CHECK(stats.handProfitSumSq > 0);    // squared term accumulates too
}
