#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <BasicStrategy.hpp>
#include <Game.hpp>
#include <HUD.hpp>
#include <RoundEngine.hpp>
#include <Card.hpp>
#include <Deck.hpp>
#include <algorithm>
#include <map>
#include <vector>

using Catch::Approx;

// ---------------------------------------------------------------------------
// Feature 3 (Advanced Analytics): the three things the left-panel charts
// promise but historically broke --
//   1. the Edge Over Time curve + the Edge % readout must return to their
//      baseline at every shoe boundary,
//   2. the Bets Placed histogram must fold wagers into a fixed number of
//      evenly spaced ranges instead of one bar per distinct bet size, and
//   3. the edge must be plotted as a LEVEL re-read from the count each round,
//      not integrated round over round (which made an ordinary shoe read -13%).
// All of it is driven through the real round-state machine (startTestRound ->
// confirmBet/stand -> RoundEngine::tick) rather than by poking internals, so a
// regression in the shoe-boundary path itself is caught here.
// ---------------------------------------------------------------------------

namespace
{
    Card card(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    // Drive frames until the round fully settles (dealer finishes and evaluateHands runs, which is what
    // appends the bankroll/edge points). dealerTurn() paces itself off Game::deltaTime rather than the tick
    // argument, so seed that member large; bounded so a stuck state cannot spin forever.
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
} // namespace

// The shoe boundary here is the deck running out mid-deal: Game::handleEmptyDeck() reshuffles and must run the
// shared resetShoeAnalytics(). This is the reported bug -- the edge kept accumulating across shoes, so the Edge
// Over Time curve drifted upward forever instead of restarting at the baseline with the new shoe.
TEST_CASE("Edge series, bankroll series and Edge % all reset at the shoe boundary", "[Feature3][Analytics][Shoe]")
{
    Game game(true);

    // 11 cards. A test round burns 8 before the dealer even plays (4 dealt by startTestRound, 4 more by
    // confirmBet) plus one dealer hit here, so round 1 takes 9 and leaves 2. The next round's deal therefore runs
    // the shoe dry on its third card, which is what fires handleEmptyDeck().
    std::vector<Card> shoe = {
        card(5, Suit::Spades), card(3, Suit::Clubs), card(4, Suit::Diamonds), card(6, Suit::Hearts),
        card(2, Suit::Spades), card(2, Suit::Clubs), card(2, Suit::Diamonds), card(2, Suit::Hearts),
        card(7, Suit::Spades), card(7, Suit::Clubs), card(7, Suit::Diamonds),
    };
    game.setDeck(shoe);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

    // Round 1 settled: both curves have a point, and nothing mid-shoe has touched the recalibration counter.
    const int generationBefore = game.shoeGenerationForTest();
    REQUIRE(game.edgeSeriesForTest().size() == 1u);
    REQUIRE(game.bankrollSeriesForTest().size() == 1u);

    // Dealing the next round runs the shoe dry -> handleEmptyDeck() -> resetShoeAnalytics().
    game.startTestRound();

    CHECK(game.shoeGenerationForTest() == generationBefore + 1);   // the HUD's recalibration signal moved
    CHECK(game.edgeSeriesForTest().empty());                       // Edge Over Time back to baseline
    CHECK(game.bankrollSeriesForTest().empty());                   // Bankroll Curve back to baseline

    // The Edge % box is the live estimate read off the current count, so past the boundary it follows the new
    // shoe's count (a few cards have already been dealt out of it) instead of keeping the old shoe's figure.
    game.updateHUDStateForTest();
    CHECK(game.edgePercentForTest() == Approx(BasicStrategy::estimateEdgePercent(game.hiLoTrueCountForTest())));
    CHECK(game.edgePercentForTest() > -5.0);
    CHECK(game.edgePercentForTest() < 5.0);
}

// The reset must not fire on ordinary rounds: only a real shoe boundary clears the curves. The shoe is deep enough
// that two full rounds (each burns ~10 cards: 4 dealt by startTestRound, 4 more by confirmBet, plus dealer hits)
// never run it dry.
TEST_CASE("Ordinary rounds append to the edge series; they do not reset it", "[Feature3][Analytics][Shoe]")
{
    Game game(true);

    std::vector<Card> shoe;
    for (int i = 0; i < 40; ++i)
        shoe.push_back(card(2, Suit::Clubs));   // low + count-neutral: rounds settle fast, the shoe stays deep

    game.setDeck(shoe);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

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

    // Eight low cards, then neutral 8s. The human stands on 12 and the dealer stands on 17, so round 1 takes
    // exactly those eight cards - every one of them +1 to Hi-Lo.
    std::vector<Card> shoe = {
        card(5, Suit::Spades), card(4, Suit::Clubs), card(3, Suit::Diamonds), card(6, Suit::Hearts),
        card(2, Suit::Spades), card(3, Suit::Clubs), card(2, Suit::Diamonds), card(4, Suit::Hearts),
    };
    for (int i = 0; i < 204; ++i)
        shoe.push_back(card(8, Suit::Clubs));
    game.setDeck(shoe);

    game.startTestRound();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

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
    tickUntilSettled(game);

    game.nextRoundForTest();
    game.confirmBet();
    game.stand();
    tickUntilSettled(game);

    const std::vector<double>& series = game.edgeSeriesForTest();
    REQUIRE(series.size() == 3u);

    // Every sample is the edge AT that moment, so it stays inside the expected band...
    for (double v : series)
    {
        CHECK(v > -5.0);
        CHECK(v < 5.0);
    }
    // ...and the newest point is the live estimate off the current count, not a sum of the rounds before it.
    CHECK(series.back() == Approx(BasicStrategy::estimateEdgePercent(game.hiLoTrueCountForTest())));

    // Three rounds of a rising count moved the edge itself by well under a point; a running total would have
    // moved it by several, which is exactly the drift that made a normal shoe look like a -13% disaster.
    CHECK(series.back() - series.front() < 2.0);
}

// The expectation the readout has to satisfy in play: over a whole shuffled shoe of ordinary rounds, the edge
// wobbles inside a few percent of the fresh-shoe baseline. It never ramps to -13% the way the integrated
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
        tickUntilSettled(game);
        ++rounds;
        if (game.gameStateForTest() != GameState::Started)
            break;
        game.nextRoundForTest();
    }

    const std::vector<double>& series = game.edgeSeriesForTest();
    // A 6-deck shoe cut 25 cards short deals plenty of rounds; if this ever comes back tiny the shoe setup broke,
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
    // Deliberately scattered sizes: one bar per unique value would render 7 bars of wildly different widths.
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

// Wagers at or above the table maximum must clamp into the last range rather than growing the axis, and
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

// An empty session must still produce the full set of (empty) bars, so the chart keeps its evenly spaced scale
// instead of collapsing or blanking out between shoes.
TEST_CASE("Empty bet distribution still yields the full set of bars", "[Feature3][Analytics][Histogram]")
{
    const std::vector<int> buckets =
        HUD::bucketBetDistribution({}, Constants::BET_BUCKET_COUNT, Constants::MAXIMUM_BET);

    REQUIRE(buckets.size() == static_cast<std::size_t>(Constants::BET_BUCKET_COUNT));
    for (int b : buckets)
        CHECK(b == 0);
}
