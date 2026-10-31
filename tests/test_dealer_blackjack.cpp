#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include "Game.hpp"
#include "RoundEngine.hpp"

// ---------------------------------------------------------------------------
// Feature (dealer-turn correctness): the dealer must never draw extra cards on a
// natural blackjack. This file reproduces two bugs -- first the one reported by the
// user through the insurance path ("declined insurance, dealer had blackjack but hit
// twice and busted"), then a direct non-insurance reproduction of the identical root
// cause (dealerTurn drawing on a natural). The fix lives in Game::dealerTurn().
// ---------------------------------------------------------------------------

namespace
{

    // Drive frames until the round leaves for Cleanup. The dealer reveal pacing timers only elapse with a large delta, so
    // settle ticks use 100.0f (matching test_blackjack_settlement.cpp) to let the dealer draw/stand and evaluateHands()
    // finish in finite time.
    void driveToCleanup(Game& game)
    {
        RoundEngine engine;
        game.deltaTime = 100.0f;
        for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
            engine.tick(100.0f, game);
    }
}

// The reported bug: dealer shows an Ace, player declines insurance, and the hidden card makes a natural (Ace + ten). A
// natural is a finished 21 -- no more cards are dealt -- but Hand::isSoft() reports a two-card Ace+ten as "soft", so
// under the default H17 rule Game::dealerTurn() treated its own natural like a soft-17 and drew extra cards (a 2 dropping
// it to hard 13, then a queen) busting on 23. The dealer must not draw on a natural.
TEST_CASE("Insurance path: dealer holds a natural after decline and draws no extra cards", "[Blackjack][Insurance][DealerTurn]")
{
    Game game(true); // headless: no window/assets/replay/HUD

    // Deal order (see setDeck): player card0, dealer UP, player card1, dealer hole, then fillers. Up-card is an Ace so the
    // round enters OfferInsurance; the hidden King makes a natural (Ace + 10 = 21). Player hand 9+7=16 stays unresolved.
    test_support::start_round(game,{test_support::make_card(9, Suit::Spades),      // player card 0 -> 16 with the next card
                  test_support::make_card(1, Suit::Hearts),       // dealer up-card: Ace -> insurance offered
                  test_support::make_card(7, Suit::Clubs),        // player card 1 -> hand of 16 (not a natural, stays unresolved)
                  test_support::make_card(13, Suit::Diamonds),    // dealer hole card: King -> natural Ace + 10 = 21
                  test_support::make_card(2, Suit::Clubs),        // filler (drawn only if the dealer wrongly hits)
                  test_support::make_card(10, Suit::Hearts)});

    CHECK(game.roundStateForTest() == RoundState::OfferInsurance);   // up-card is an Ace -> insurance offered
    CHECK(game.testDealer().isBlackjack());                          // natural locked in on the deal

    // Human (single seat, activePlayerIndex 0) declines insurance. Advancing past the last player wraps and evaluateBlackjack()
    // sees the dealer's natural, moving to DealerTurn WITHOUT marking the unresolved hand -- which is what lets a buggy
    // dealerTurn draw extra cards.
    game.declineInsurance();
    CHECK(game.roundStateForTest() == RoundState::DealerTurn);

    driveToCleanup(game);

    // The natural is terminal: the dealer drew zero extra cards (size stays 2, value stays 21), and the player's 16 loses.
    CHECK(game.testDealer().size() == 2);
    CHECK(game.testDealer().getValue() == 21);
    CHECK(game.testPlayer(0).getStats().losses == 1);
}

// Same root cause via the direct entry: a natural with a non-Ace up-card is detected on the deal (evaluateBlackjack), so no
// insurance detour is needed to reproduce it. Without the fix the dealer draws its single filler and ends with more than two
// cards even though it already holds a finished 21.
TEST_CASE("Dealer natural with a non-Ace up-card draws no extra cards", "[Blackjack][DealerTurn]")
{
    Game game(true);

    // Up-card is a King (not an Ace) so deal() evaluates blackjack directly; the hidden Ace makes a natural. Player hand 16.
    test_support::start_round(game,{test_support::make_card(9, Suit::Spades),     // player card 0 -> 16
                  test_support::make_card(13, Suit::Hearts),    // dealer up-card: King (not an Ace) -> evaluateBlackjack on the deal
                  test_support::make_card(7, Suit::Clubs),      // player card 1 -> hand of 16 (unresolved)
                  test_support::make_card(1, Suit::Diamonds),   // dealer hole card: Ace -> natural
                  test_support::make_card(8, Suit::Clubs)});

    CHECK(game.roundStateForTest() == RoundState::DealerTurn);   // natural detected on the deal (no insurance)
    CHECK(game.testDealer().isBlackjack());

    driveToCleanup(game);

    CHECK(game.testDealer().size() == 2);   // drew no filler -> still exactly two cards
    CHECK(game.testPlayer(0).getStats().losses == 1);
}