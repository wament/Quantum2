#include <catch2/catch_all.hpp>

#include "Constants.hpp"
#include "Game.hpp"
#include "RoundEngine.hpp"
#include "SideBet.hpp"

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives in this top-level namespace (declared in Constants.hpp)

// ---------------------------------------------------------------------------
// Feature 1 (Side Bets): round-level integration tests. test_side_bets.cpp covers
// the pure SideBetEvaluator paytables in isolation; this file proves the same
// paytables survive the full Game path — placeSideBet -> deal() -> resolveSideBets
// before any main-hand settlement — by asserting exact gross payouts, bankroll
// movement and PlayerStats tallies through a headless Game(true).
//
// setDeck draw order equals the input index order (the reversal in setDeck cancels
// with the back-pull of Deck::draw): input[0]=player card 0, input[1]=dealer up-card
// (the third card for 21+3), input[2]=player card 1, input[3]=dealer hole card.
// Side bets are placed BEFORE startTestRound so the pending intent is applied inside
// deal(); at that point the headless seat does not exist yet, so only the intent is
// stored and resolveSideBets() runs once the two hand cards + dealer up-card are known.
// ---------------------------------------------------------------------------

namespace
{
    Card makeCard(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    // Drive frames of the shared round-state machine. deltaTime is ignored by the player/turn transitions, but the
    // dealer's reveal pacing timers only elapse with a large delta; the settle ticks use 100.0f (matching the
    // settlement tests) so the dealer draws, stands and runs evaluateHands() in finite time.
    void tickOnce(Game& game, float deltaTime = 0.0f)
    {
        RoundEngine engine;
        engine.tick(deltaTime, game);
    }

    // Frame-loop until the round leaves PlayerTurn (all hands resolved -> DealerTurn).
    // Bounded so a stuck state cannot spin forever. Never invokes a player action.
    void tickUntilLeavesPlayerTurn(Game& game)
    {
        RoundEngine engine;
        for (int i = 0; i < 25 && game.roundStateForTest() == RoundState::PlayerTurn; ++i)
            engine.tick(0.0f, game);
    }
}

// ---------------------------------------------------------------------------
// Perfect Pairs: the player's two cards only (indices 0 and 2). Same rank
// required; payout scales with how tightly the pair matches (Perfect 30 > Colored
// 25 > Mixed 10). Gross payout = stake + winnings = wager * (1 + ratio).
// ---------------------------------------------------------------------------

TEST_CASE("Round: Perfect Pairs pays 30:1 for a same-rank, same-suit pair", "[SideBets][PerfectPairs][Round]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    game.setDeck({makeCard(1, Suit::Spades),   // player card 0 -> Ace
                  makeCard(9, Suit::Hearts),   // dealer up-card (irrelevant to Perfect Pairs)
                  makeCard(1, Suit::Spades),   // player card 1 -> second Ace of Spades (Perfect Pair)
                  makeCard(2, Suit::Diamonds)});

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
    game.setDeck({makeCard(7, Suit::Hearts),   // both red sevens -> Colored Pair 25:1
                  makeCard(2, Suit::Clubs),
                  makeCard(7, Suit::Diamonds),
                  makeCard(3, Suit::Clubs)});

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
    game.setDeck({makeCard(7, Suit::Spades),   // black seven + red seven -> Mixed Pair 10:1
                  makeCard(2, Suit::Clubs),
                  makeCard(7, Suit::Hearts),
                  makeCard(3, Suit::Diamonds)});

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
    game.setDeck({makeCard(5, Suit::Spades),   // different ranks -> nothing pays
                  makeCard(2, Suit::Hearts),
                  makeCard(6, Suit::Diamonds),
                  makeCard(3, Suit::Clubs)});

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
    game.setDeck({makeCard(4, Suit::Hearts), makeCard(5, Suit::Hearts), makeCard(6, Suit::Hearts),
                  makeCard(2, Suit::Clubs)});

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
    game.setDeck({makeCard(1, Suit::Spades), makeCard(1, Suit::Hearts), makeCard(1, Suit::Diamonds),
                  makeCard(2, Suit::Clubs)});

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
    game.setDeck({makeCard(2, Suit::Spades), makeCard(5, Suit::Spades), makeCard(9, Suit::Spades),
                  makeCard(3, Suit::Clubs)});

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
    game.setDeck({makeCard(4, Suit::Hearts), makeCard(5, Suit::Diamonds), makeCard(6, Suit::Clubs),
                  makeCard(2, Suit::Spades)});

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
    game.setDeck({makeCard(12, Suit::Spades), makeCard(13, Suit::Hearts), makeCard(1, Suit::Diamonds),
                  makeCard(2, Suit::Clubs)});

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
    game.setDeck({makeCard(8, Suit::Hearts), makeCard(8, Suit::Clubs), makeCard(3, Suit::Spades),
                  makeCard(2, Suit::Diamonds)});

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
    game.setDeck({makeCard(2, Suit::Spades), makeCard(5, Suit::Diamonds), makeCard(9, Suit::Clubs),
                  makeCard(3, Suit::Hearts)});

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
    game.setDeck({makeCard(8, Suit::Spades),   // player card 0
                  makeCard(13, Suit::Hearts),  // dealer up-card (King)
                  makeCard(8, Suit::Hearts),   // player card 1 -> pair of eights
                  makeCard(2, Suit::Diamonds),  // dealer hole + fillers below
                  makeCard(3, Suit::Clubs),
                  makeCard(4, Suit::Diamonds),
                  makeCard(5, Suit::Hearts)});

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

    tickUntilLeavesPlayerTurn(game);          // all hands finished -> DealerTurn
    for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        tickOnce(game, 100.0f);                // elapsed timers let the dealer draw/stand then evaluateHands settles round -> Cleanup


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
