#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Card.hpp>
#include <Hand.hpp>
#include <Player.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>

using Catch::Approx;

// ---------------------------------------------------------------------------
// Blackjack ranking. A natural (two-card, unsplit) blackjack is ranked against
// the dealer's hand TYPE, not the dealer's TOTAL: it beats everything short of
// the dealer's own natural, including a dealer 21 built from three or more
// cards, and it is paid 3:2 immediately rather than pushed.
// ---------------------------------------------------------------------------

namespace
{

    Hand hand(std::initializer_list<int> values, int bet)
    {
        Hand h;
        for (int v : values)
            h.addCard(test_support::make_card(v, Suit::Spades));
        h.setBet(bet);
        return h;
    }

    } // namespace

// The reported bug: the player had a natural blackjack, the dealer drew three cards to reach 21,
// and the round scored as a 21-vs-21 push. A natural outranks a non-natural 21.
TEST_CASE("Natural blackjack beats a dealer 21 built from three or more cards", "[Blackjack][Settlement]")
{
    Player p("William", 1000);

    int payout = 0;
    SettlementResult result = p.settle(hand({1, 13}, 100), /*dealerValue=*/21, /*dealerBust=*/false,
                                       /*dealerHasBlackjack=*/false, payout);

    CHECK(result == SettlementResult::Win);
    CHECK(payout == 250);                    // stake back + 3:2
    CHECK(p.getBankroll() == 1150);          // 1000 + 150 profit
}

// The one legitimate exception: the dealer's own natural blackjack pushes the player's.
TEST_CASE("Natural blackjack pushes only against the dealer's own natural", "[Blackjack][Settlement]")
{
    Player p("William", 1000);

    int payout = 0;
    SettlementResult result = p.settle(hand({1, 13}, 100), /*dealerValue=*/21, /*dealerBust=*/false,
                                       /*dealerHasBlackjack=*/true, payout);

    CHECK(result == SettlementResult::Push);
    CHECK(payout == 100);                    // stake back only
    CHECK(p.getBankroll() == 1000);          // unchanged
}

// A hand that merely totals 21 (three cards, or a split hand) still pushes a dealer 21 -- the 3:2
// premium belongs to naturals only.
TEST_CASE("Non-natural 21 still pushes a dealer 21", "[Blackjack][Settlement]")
{
    Player p("William", 1000);

    int payout = 0;
    SettlementResult result = p.settle(hand({7, 7, 7}, 100), /*dealerValue=*/21, /*dealerBust=*/false,
                                       /*dealerHasBlackjack=*/false, payout);

    CHECK(result == SettlementResult::Push);
    CHECK(payout == 100);
    CHECK(p.getBankroll() == 1000);
}

// End to end: the player's natural is locked in the moment the cards are dealt -- the hand is finished
// and the dealer draws nothing on its account -- and it is paid 3:2.
TEST_CASE("Player natural is resolved before the dealer acts and pays 3:2", "[Blackjack][Settlement]")
{
    Game game(true);

    // Deal order: player card, dealer UP card, player card, dealer hole card.
    test_support::start_round(game,{test_support::make_card(13, Suit::Spades), test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(1, Suit::Clubs),  test_support::make_card(7, Suit::Diamonds),
                  test_support::make_card(9, Suit::Clubs),  test_support::make_card(9, Suit::Clubs)});

    CHECK(game.testHand(0, 0).isBlackjack());
    CHECK(game.testHand(0, 0).isFinished());          // settled on the deal; the player cannot act on it
    CHECK(game.testPlayer(0).getStats().blackjacks == 1);

    test_support::settleRound(game);

    CHECK(game.testDealer().size() == 2);             // dealer drew no cards for a resolved blackjack
    CHECK(game.testPlayer(0).getBankroll() == 10015); // 10 stake paid at 3:2
    CHECK(game.testPlayer(0).getStats().wins == 1);
}

// A dealer natural must cost the player exactly one wager. evaluateBlackjack() used to deduct it and
// then evaluateHands() -> settle() deducted the same loss a second time.
TEST_CASE("Dealer natural costs the player exactly one wager", "[Blackjack][Settlement]")
{
    Game game(true);

    // Dealer up card K (not an ace, so no insurance detour) + hole ace = natural blackjack.
    test_support::start_round(game,{test_support::make_card(9, Suit::Spades),  test_support::make_card(13, Suit::Spades),
                  test_support::make_card(7, Suit::Clubs),   test_support::make_card(1, Suit::Hearts),
                  test_support::make_card(8, Suit::Clubs),   test_support::make_card(8, Suit::Clubs)});

    CHECK(game.testDealer().isBlackjack());

    test_support::settleRound(game);

    CHECK(game.testPlayer(0).getBankroll() == 9990);   // one 10 stake lost, not two
    CHECK(game.testPlayer(0).getStats().losses == 1);
    CHECK(game.testPlayer(0).getStats().handsFinished == 1);
}
