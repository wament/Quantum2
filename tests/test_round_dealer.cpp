#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives here (declared in Constants.hpp)

// ---------------------------------------------------------------------------
// Follow-up #8 (Game.cpp god-class split, Settlement/deal() stage): Game::deal() +
// Game::resolveSideBets() now delegate to the composed RoundDealer helper. These cases
// drive that delegation path end-to-end through a real headless startTestRound() deal and
// lock each branch of the extracted resolveSideBets() (win / lose) plus deal()'s round-state
// advance and insurance-offer branch, so any drift from the relocation is caught. Side-bet
// fixtures mirror test_side_bet_round.cpp exactly.
// ---------------------------------------------------------------------------

TEST_CASE("#8 delegation: a winning side bet resolves through RoundDealer::deal()", "[Settlement][RoundDealer]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::seed_deck(game,{test_support::make_card(1, Suit::Spades),   // player card 0 -> Ace
                  test_support::make_card(9, Suit::Hearts),   // dealer up-card (irrelevant to Perfect Pairs)
                  test_support::make_card(1, Suit::Spades),   // player card 1 -> second Ace of Spades (Perfect Pair)
                  test_support::make_card(2, Suit::Diamonds)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound(); // Game::deal() -> RoundDealer::deal() -> resolveSideBets()

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetKind() == SideBetKind::PerfectPairs);
    CHECK(hand.getSideBetPayout() == 10 * (1 + 30)); // same-rank, same-suit -> 30:1

    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 1);
    CHECK(s.sideBetLosses == 0);
    CHECK(s.sideBetProfitSum == static_cast<long long>(10) * 30);
    // Main hand not settled yet: bankroll grew by the pure side-bet net (+300).
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL + 300);
}

TEST_CASE("#8 delegation: a losing side bet resolves through RoundDealer::deal()", "[Settlement][RoundDealer]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::seed_deck(game,{test_support::make_card(5, Suit::Spades),   // different ranks -> nothing pays
                  test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(3, Suit::Clubs)});

    game.placeSideBet(SideBetKind::PerfectPairs, 10);
    game.startTestRound(); // Game::deal() -> RoundDealer::deal() -> resolveSideBets()

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.getSideBetPayout() == 0); // gross returned on a loss is 0

    PlayerStats& s = game.testPlayer(0).getStats();
    CHECK(s.sideBetWins == 0);
    CHECK(s.sideBetLosses == 1);
    CHECK(s.sideBetProfitSum == static_cast<long long>(-10));
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL - 10);
}

TEST_CASE("#8 delegation: deal advances the round to PlayerTurn with no side bet", "[Settlement][RoundDealer]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::seed_deck(game,{test_support::make_card(5, Suit::Spades),   // player 11 vs dealer 9 -> PlayerTurn
                  test_support::make_card(9, Suit::Hearts),
                  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(3, Suit::Clubs)});

    game.startTestRound(); // Game::deal() -> RoundDealer::deal() (no side-bet intent to apply/resolve)

    CHECK(game.roundStateForTest() == RoundState::PlayerTurn);
    CHECK(game.testHand(0, 0).size() == 2); // the initial two cards were dealt from the shoe
}

TEST_CASE("#8 delegation: deal offers insurance when the dealer shows an Ace", "[Settlement][RoundDealer]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::seed_deck(game,{test_support::make_card(7, Suit::Spades),   // player card 0
                  test_support::make_card(1, Suit::Hearts),   // dealer up-card -> Ace (offers insurance)
                  test_support::make_card(8, Suit::Diamonds), // player card 1
                  test_support::make_card(2, Suit::Clubs)});

    game.startTestRound(); // Game::deal() -> RoundDealer::deal()

    CHECK(game.roundStateForTest() == RoundState::OfferInsurance);
}
