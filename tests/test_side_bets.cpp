// Side-bet payout logic (Perfect Pairs / 21-3) plus live side-bet intent handling.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include "Constants.hpp"
#include "Game.hpp"
#include "SideBet.hpp"
#include "SideBetLabels.hpp"
#include "test_support.hpp"
#include <catch2/catch_all.hpp>

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives in this top-level namespace (declared in Constants.hpp)

// Shared test fixtures/helpers lifted from every member file into one anonymous namespace (identical copies de-duped).
namespace {
void startHuman(Game& game)
    {
        test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(9, Suit::Hearts),
                      test_support::make_card(1, Suit::Spades), test_support::make_card(2, Suit::Diamonds)});
    }

} // namespace

TEST_CASE("Perfect Pairs pays 30:1 for a same-rank same-suit pair", "[SideBets][PerfectPairs]")
{
    auto perfect = SideBetEvaluator::perfectPairs(test_support::make_card(1, Suit::Spades), test_support::make_card(1, Suit::Spades));
    CHECK(perfect.kind == SideBetKind::PerfectPairs);
    CHECK(perfect.multiplier == 30);
    CHECK(perfect.won());
}

TEST_CASE("Perfect Pairs pays 25:1 for a same-rank, same-color, different-suit pair", "[SideBets][PerfectPairs]")
{
    // Two red sevens (Hearts + Diamonds): same rank and color but not the same suit -> Colored Pair.
    auto colored = SideBetEvaluator::perfectPairs(test_support::make_card(7, Suit::Hearts), test_support::make_card(7, Suit::Diamonds));
    CHECK(colored.multiplier == 25);
    CHECK(colored.won());
}

TEST_CASE("Perfect Pairs pays 10:1 for a same-rank, different-color pair", "[SideBets][PerfectPairs]")
{
    // A black seven and a red seven: same rank but different color -> Mixed Pair.
    auto mixed = SideBetEvaluator::perfectPairs(test_support::make_card(7, Suit::Spades), test_support::make_card(7, Suit::Hearts));
    CHECK(mixed.multiplier == 10);
    CHECK(mixed.won());
}

TEST_CASE("Perfect Pairs pays nothing for two cards of different rank", "[SideBets][PerfectPairs]")
{
    auto none = SideBetEvaluator::perfectPairs(test_support::make_card(5, Suit::Spades), test_support::make_card(6, Suit::Hearts));
    CHECK(none.multiplier == 0);
    CHECK_FALSE(none.won());
}

// ---------------------------------------------------------------------------
// 21+3: the player's two cards plus the dealer's up-card (three total). Ranked
// highest-to-lowest; a hand counts toward its single best-paying tier.
// ---------------------------------------------------------------------------
TEST_CASE("21+3 pays 40:1 for a straight flush", "[SideBets][TwentyOneThree]")
{
    auto sf = SideBetEvaluator::twentyOneThree(test_support::make_card(10, Suit::Spades), test_support::make_card(11, Suit::Spades),
                                               test_support::make_card(12, Suit::Spades)); // 10-J-Q all spades
    CHECK(sf.multiplier == 40);
    CHECK(sf.won());
}

TEST_CASE("21+3 pays 30:1 for three of a kind", "[SideBets][TwentyOneThree]")
{
    // Three aces of different suits -> trips, not a flush.
    auto trips = SideBetEvaluator::twentyOneThree(test_support::make_card(1, Suit::Spades), test_support::make_card(1, Suit::Hearts),
                                                  test_support::make_card(1, Suit::Diamonds));
    CHECK(trips.multiplier == 30);
    CHECK(trips.won());
}

TEST_CASE("21+3 pays 9:1 for a flush (same suit, not a straight)", "[SideBets][TwentyOneThree]")
{
    auto flush = SideBetEvaluator::twentyOneThree(test_support::make_card(2, Suit::Spades), test_support::make_card(5, Suit::Spades),
                                                  test_support::make_card(9, Suit::Spades));
    CHECK(flush.multiplier == 9);
    CHECK(flush.won());
}

TEST_CASE("21+3 pays 5:1 for a straight (consecutive, not all one suit)", "[SideBets][TwentyOneThree]")
{
    auto straight = SideBetEvaluator::twentyOneThree(test_support::make_card(4, Suit::Hearts), test_support::make_card(5, Suit::Diamonds),
                                                     test_support::make_card(6, Suit::Clubs));
    CHECK(straight.multiplier == 5);
    CHECK(straight.won());
}

TEST_CASE("21+3 treats Ace as high for the Q-K-A straight", "[SideBets][TwentyOneThree]")
{
    auto ah = SideBetEvaluator::twentyOneThree(test_support::make_card(12, Suit::Spades), test_support::make_card(13, Suit::Hearts),
                                               test_support::make_card(1, Suit::Diamonds)); // Q-K-A
    CHECK(ah.multiplier == 5);
    CHECK(ah.won());
}

TEST_CASE("21+3 pays 2:1 for a one-pair hand", "[SideBets][TwentyOneThree]")
{
    auto pair = SideBetEvaluator::twentyOneThree(test_support::make_card(8, Suit::Hearts), test_support::make_card(8, Suit::Clubs),
                                                 test_support::make_card(3, Suit::Spades));
    CHECK(pair.multiplier == 2);
    CHECK(pair.won());
}

TEST_CASE("21+3 pays nothing for an unqualifying three-card combination", "[SideBets][TwentyOneThree]")
{
    auto none = SideBetEvaluator::twentyOneThree(test_support::make_card(2, Suit::Spades), test_support::make_card(5, Suit::Diamonds),
                                                 test_support::make_card(9, Suit::Clubs));
    CHECK(none.multiplier == 0);
    CHECK_FALSE(none.won());
}

TEST_CASE("Three of a kind outranks the pair it would otherwise form", "[SideBets][TwentyOneThree]")
{
    // All-equal ranks must resolve to Trips (30), never fall back to Pair (2).
    auto trips = SideBetEvaluator::twentyOneThree(test_support::make_card(10, Suit::Spades), test_support::make_card(10, Suit::Hearts),
                                                  test_support::make_card(10, Suit::Diamonds));
    CHECK(trips.multiplier == 30);
}

TEST_CASE("Straight flush outranks three of a kind", "[SideBets][TwentyOneThree]")
{
    // A hand that is both suited and (if ranks allowed) consecutive takes the highest tier.
    auto sf = SideBetEvaluator::twentyOneThree(test_support::make_card(6, Suit::Clubs), test_support::make_card(7, Suit::Clubs),
                                               test_support::make_card(8, Suit::Clubs)); // 6-7-8 clubs -> straight flush
    CHECK(sf.multiplier == 40);
}


TEST_CASE("Round: Perfect Pairs pays 30:1 for a same-rank, same-suit pair", "[SideBets][PerfectPairs][Round]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::seed_deck(game,{test_support::make_card(1, Suit::Spades),   // player card 0 -> Ace
                  test_support::make_card(9, Suit::Hearts),   // dealer up-card (irrelevant to Perfect Pairs)
                  test_support::make_card(1, Suit::Spades),   // player card 1 -> second Ace of Spades (Perfect Pair)
                  test_support::make_card(2, Suit::Diamonds)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound();

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 10);
    CHECK(hand.getSideBetPayout() == 10 * (1 + 30)); // gross = stake + 30*stake

    // Fresh profile: exactly one side-bet win, profit = wager*ratio, loss counter untouched.
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetLosses == 0);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 30);

    // Bankroll grew by the pure side-bet net (main hand not settled yet): +300.
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 300);
}

TEST_CASE("Round: Perfect Pairs pays 25:1 for a same-rank, same-color pair", "[SideBets][PerfectPairs][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(7, Suit::Hearts),   // both red sevens -> Colored Pair 25:1
                  test_support::make_card(2, Suit::Clubs),
                  test_support::make_card(7, Suit::Diamonds),
                  test_support::make_card(3, Suit::Clubs)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 25));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 25);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 250);
}

TEST_CASE("Round: Perfect Pairs pays 10:1 for a same-rank, different-color pair", "[SideBets][PerfectPairs][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(7, Suit::Spades),   // black seven + red seven -> Mixed Pair 10:1
                  test_support::make_card(2, Suit::Clubs),
                  test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(3, Suit::Diamonds)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 10));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 10);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 100);
}

TEST_CASE("Round: Perfect Pairs loses the wager for two cards of different rank", "[SideBets][PerfectPairs][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(5, Suit::Spades),   // different ranks -> nothing pays
                  test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(3, Suit::Clubs)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound();

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetPayout() == 0); // gross returned on a loss is 0
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 0);
    CHECK(s.sideBetLosses == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(-10));
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL - 10);
}

// >>> MARK-A >>>
// ---------------------------------------------------------------------------
// 21+3: the player's two cards plus the dealer up-card (indices 0, 1, 2). Ranked
// highest-to-lowest; a hand resolves to its single best-paying tier.
// ---------------------------------------------------------------------------

TEST_CASE("Round: 21+3 pays 40:1 for a straight flush", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    // 4-5-6 all Hearts across indices 0,1(dealer up),2 -> Straight Flush 40:1.
    test_support::seed_deck(game,{test_support::make_card(4, Suit::Hearts), test_support::make_card(5, Suit::Hearts), test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(2, Suit::Clubs)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 40));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 40);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 400);
}

TEST_CASE("Round: 21+3 pays 30:1 for three of a kind", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    // Three aces (different suits) -> Trips 30:1.
    test_support::seed_deck(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(1, Suit::Hearts), test_support::make_card(1, Suit::Diamonds),
                  test_support::make_card(2, Suit::Clubs)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 30));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 30);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 300);
}

TEST_CASE("Round: 21+3 pays 9:1 for a flush (same suit, not a straight)", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(2, Suit::Spades), test_support::make_card(5, Suit::Spades), test_support::make_card(9, Suit::Spades),
                  test_support::make_card(3, Suit::Clubs)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 9));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 9);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 90);
}

TEST_CASE("Round: 21+3 pays 5:1 for a straight (consecutive, not one suit)", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(4, Suit::Hearts), test_support::make_card(5, Suit::Diamonds), test_support::make_card(6, Suit::Clubs),
                  test_support::make_card(2, Suit::Spades)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 5));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 5);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 50);
}

TEST_CASE("Round: 21+3 treats Ace as high for the Q-K-A straight", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    // Q-K-A across indices 0,1,2 -> Straight (Ace high), not a pair.
    test_support::seed_deck(game,{test_support::make_card(12, Suit::Spades), test_support::make_card(13, Suit::Hearts), test_support::make_card(1, Suit::Diamonds),
                  test_support::make_card(2, Suit::Clubs)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 5));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 5);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 50);
}

TEST_CASE("Round: 21+3 pays 2:1 for a one-pair hand", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(8, Suit::Hearts), test_support::make_card(8, Suit::Clubs), test_support::make_card(3, Suit::Spades),
                  test_support::make_card(2, Suit::Diamonds)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 10 * (1 + 2));
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 2);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 20);
}

TEST_CASE("Round: 21+3 loses the wager for an unqualifying three-card combination", "[SideBets][TwentyOneThree][Round]")
{
    Game game(true);
    test_support::seed_deck(game,{test_support::make_card(2, Suit::Spades), test_support::make_card(5, Suit::Diamonds), test_support::make_card(9, Suit::Clubs),
                  test_support::make_card(3, Suit::Hearts)});

    game.placeSideBet(SideBetKind::TwentyOneThree, 10);
    game.startTestRound();

    CHECK(game.testHand(0, 0).getSideBetPayout() == 0);
    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 0);
    CHECK(s.sideBetLosses == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(-10));
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL - 10);
}

// >>> MARK-B >>>
// ---------------------------------------------------------------------------
// Ordering / non-conflation: the side bet must resolve BEFORE main-hand settlement
// and its counters must be independent of the main hand's win/loss. Set up a round
// where the perfect-pair side bet wins while the main hand is still open, then drive
// it to completion; the side-bet tallies set at deal time must not change again when
// evaluateHands() settles the main hand (no double-counting, no conflation).
// ---------------------------------------------------------------------------

TEST_CASE("Round: side bet resolves before main-hand settlement and stays independent", "[SideBets][Ordering]")
{
    Game game(true);
    // Pair of eights -> Mixed Pair 10:1 wins (index0=index2 = rank 8, different color).
    // Dealer up-card is a high card so the main hand's eventual settlement is separate.
    test_support::seed_deck(game,{test_support::make_card(8, Suit::Spades),   // player card 0
                  test_support::make_card(13, Suit::Hearts),  // dealer up-card (King)
                  test_support::make_card(8, Suit::Hearts),   // player card 1 -> pair of eights
                  test_support::make_card(2, Suit::Diamonds),  // dealer hole + fillers below
                  test_support::make_card(3, Suit::Clubs),
                  test_support::make_card(4, Suit::Diamonds),
                  test_support::make_card(5, Suit::Hearts)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound();

    // Immediately after the deal: the side bet is already banked (+100 net) but the round
    // is still at PlayerTurn, i.e. before evaluateHands() has touched the main hand.
    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetPayout() == 10 * (1 + 10));
    CHECK(game.roundStateForTest() == RoundState::PlayerTurn);
    CHECK(hand.size() == 2); // main hand not yet settled

    const long long sbWinsBefore = game.testPlayer(0).getStats().sideBetWins;
    const long long sbLossesBefore = game.testPlayer(0).getStats().sideBetLosses;
    const long long sbProfitBefore = game.testPlayer(0).getStats().sideBetProfitSum;

    // Finish the human's hand so the engine can advance PlayerTurn -> DealerTurn -> settle. Standing on 16 lets the
    // dealer draw to its own rules; evaluateHands() then settles both hands. The side-bet tallies were already
    // banked at deal time (checks above) and must remain frozen through this main-hand settlement.
    game.stand();

    // dealerTurn() advances its reveal pacing timers using Game::deltaTime (a member), not the RoundEngine tick arg,
    // so drive it with a large delta like the settlement tests do; then the dealer draws/stands and evaluateHands()
    // settles the round in finite ticks.
    game.deltaTime = 100.0f;

    test_support::advanceToDealerTurn(game);          // all hands finished -> DealerTurn
    for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        test_support::tickFrame(game, 100.0f);                // elapsed timers let the dealer draw/stand then evaluateHands settles round -> Cleanup


    PlayerStats& s = game.testPlayer(0).getStats();
    // Side-bet tallies are frozen at their deal-time values: counted exactly once, and not
    // disturbed by the main-hand settlement below.
    CHECK(s.sideBetWins == sbWinsBefore);
    CHECK(s.sideBetLosses == sbLossesBefore);
    CHECK(s.sideBetProfitSum == sbProfitBefore);

    // The main hand's win/loss/push counter is now exactly one of (the round has finished),
    // proving the two result streams are kept separate.
    const int terminalWins = s.wins;
    const int terminalLosses = s.losses;
    const int terminalPushes = s.pushes;
    CHECK((terminalWins + terminalLosses + terminalPushes) == 1);

    // The gross side-bet payout recorded on the hand is preserved for replay/analytics.
    CHECK(hand.getSideBetPayout() == 10 * (1 + 10));
}


TEST_CASE("placeSideBet keeps a selected kind at $0 so it shows up before any amount is added", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    // Cycle to a kind with no money placed yet (the "cycle before adding" path). It must remain selectable/visible.
    game.placeSideBet(SideBetKind::PerfectPairs, 0);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 0);
}

TEST_CASE("incrementing from empty defaults to the first side bet so the selection appears on screen", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    // Pressing X/V with no kind chosen yet must surface a visible selection instead of staying invisible at None.
    game.placeSideBet(SideBetKind::None, 10);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 10);
}

TEST_CASE("cycling the kind keeps an existing funded wager", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    game.placeSideBet(SideBetKind::PerfectPairs, 20);
    // Simulate Z: switch to the other side bet while keeping whatever amount is already placed.
    game.placeSideBet(SideBetKind::TwentyOneThree, 20);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::TwentyOneThree);
    CHECK(hand.getSideBetWager() == 20);
}

TEST_CASE("cycling to off opts out and erases the selection", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    game.placeSideBet(SideBetKind::PerfectPairs, 30);
    // Simulate Z again: None is an explicit opt-out; the amount must be erased (the correct behavior).
    game.placeSideBet(SideBetKind::None, 0);

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::None);
    CHECK(hand.getSideBetWager() == 0);
}

TEST_CASE("decrementing to $0 keeps the kind selectable instead of stranding it", "[SideBets][Place]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    startHuman(game);

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    // Simulate V down to zero: the selected kind stays put (so you can re-add or cycle) rather than being lost.
    game.placeSideBet(SideBetKind::PerfectPairs, 0);
    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetWager() == 0);

    // And incrementing again rebuilds the amount from that kept selection.
    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    CHECK(game.testHand(0, 0).getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(game.testHand(0, 0).getSideBetWager() == 10);
}

TEST_CASE("canPlaceBets is true only in an interactive PlaceBets phase for the human seat", "[SideBets][Placing]")
{
    Game game(true); // headless; the predicate reads plain members, so no seats are needed.

    game.setRoundStateForTest(RoundState::PlaceBets);
    game.setGameSpeedForTest(GameSpeed::Normal);
    CHECK(game.canPlaceBets());

    // Auto-advance means the human cannot actually place bets, so the side-bet control line stays hidden.
    game.setGameSpeedForTest(GameSpeed::AutoPlay);
    CHECK(!game.canPlaceBets());

    // Outside the PlaceBets phase there is nothing to place.
    game.setRoundStateForTest(RoundState::PlayerTurn);
    CHECK(!game.canPlaceBets());
}

// B3 regression: the side-bet readout must report WON / LOST / PUSH by net result, not always WON. The buggy renderer
// took abs() before testing the sign, so a lost stake displayed "WON +20". This asserts all three outcomes against the
// pure label builder; it fails today (loss and push both print WON) and passes once the sign is preserved.
TEST_CASE("B3: side-bet label reports WON / LOST / PUSH by net result", "[SideBets][Label]")
{
    using namespace sidebet;

    // Won: payout exceeds the stake -> net gain, printed with a plus.
    CHECK(resultLabel(20, 40) == "  WON +20");

    // Lost: resolveSideBets pays nothing on a loss (payout 0), so the full stake is gone -> net loss, minus sign.
    CHECK(resultLabel(20, 0) == "  LOST -20");

    // Push: payout equals the stake -> zero net, which is a push, not a win.
    CHECK(resultLabel(20, 20) == "  PUSH  0");
}

