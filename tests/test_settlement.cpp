// Round settlement & rule resolution tests: naturals, split aces, resplit tens, dealer blackjacks, deal/resolve path.

#include <catch2/catch_all.hpp>
#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include "Card.hpp"
#include "Constants.hpp"
#include "Game.hpp"
#include "Hand.hpp"
#include "test_support.hpp"
#include <Card.hpp>
#include <Constants.hpp>
#include <Deck.hpp>
#include <Game.hpp>
#include <Hand.hpp>
#include <Player.hpp>
#include <catch2/catch_all.hpp>

using Catch::Approx;
using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives here (declared in Constants.hpp)

// Shared fixtures/helpers in one anonymous namespace (identical copies de-duped across files).
namespace {
Hand hand(std::initializer_list<int> values, int bet)
    {
        Hand h;
        for (int v : values)
            h.addCard(test_support::make_card(v, Suit::Spades));
        h.setBet(bet);
        return h;
    }

bool splitAceIs21NotBlackjack(const Hand& h)
    {
        return h.isSplitAce() && !h.isBlackjack() && h.getValue() == 21;
    }

void driveToCleanup(Game& game)
    {
        game.deltaTime = 100.0f;
        for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
            game.advanceTurnState(100.0f);
    }

} // namespace

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

// One legitimate exception: the dealer's own natural blackjack pushes the player's.
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

// End to end: the player's natural is locked in at deal time, and the hand is already finished there.
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

// A dealer natural costs exactly one wager, evaluated on the deal - not deducted later by a buggy evaluateBlackjack().
// then evaluateHands() -> settle() deducted the same loss a second time.
TEST_CASE("Dealer natural costs the player exactly one wager", "[Blackjack][Settlement]")
{
    Game game(true);

    // Up-card K plus hole ace is a natural; no insurance detour since K is not an ace.
    test_support::start_round(game,{test_support::make_card(9, Suit::Spades),  test_support::make_card(13, Suit::Spades),
                  test_support::make_card(7, Suit::Clubs),   test_support::make_card(1, Suit::Hearts),
                  test_support::make_card(8, Suit::Clubs),   test_support::make_card(8, Suit::Clubs)});

    CHECK(game.testDealer().isBlackjack());

    test_support::settleRound(game);

    CHECK(game.testPlayer(0).getBankroll() == 9990);   // one 10 stake lost, not two
    CHECK(game.testPlayer(0).getStats().losses == 1);
    CHECK(game.testPlayer(0).getStats().handsFinished == 1);
}


TEST_CASE("Initial split of ten-value cards is always allowed", "[SplitTens][Feature2]")
{
    const bool saved = Constants::RESPLIT_TENS_ALLOWED;
    Constants::RESPLIT_TENS_ALLOWED = false; // even with the rule off, an initial split must work

    Game game(true);
    // p:{10,10}, dealer hole 7, up card 6 (non-ace -> PlayerTurn).
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(10, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds)});

    CHECK(game.split());
    CHECK(game.testPlayer(0).hands.size() == 2);

    Constants::RESPLIT_TENS_ALLOWED = saved;
}

// With resplit-tens on, splitting a value hand that itself came from a split makes another hand.
TEST_CASE("Resplitting tens is allowed when RESPLIT_TENS_ALLOWED is true", "[SplitTens][Feature2]")
{
    Constants::RESPLIT_TENS_ALLOWED = true;

    Game game(true);
    // Pair of tens vs hole 7 / up 6; the next draw is another ten, re-pairing hand[0].
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(10, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(10, Suit::Clubs),   test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(3, Suit::Spades),   test_support::make_card(4, Suit::Hearts)});

    CHECK(game.split());                       // initial pair of tens -> two hands
    test_support::tickFrame(game);                            // first hand draws a ten -> {10,10} again
    auto& p = game.testPlayer(0);
    CHECK(p.hands[0].size() == 2);
    CHECK(p.hands[0].isSplit());               // already a split hand, so this is a re-split

    CHECK(game.split());                       // re-split tens -> three hands
    CHECK(p.hands.size() == 3);
}

// With resplit-tens off, re-splitting tens is refused but an ordinary initial split still works.
TEST_CASE("Resplitting tens is refused when RESPLIT_TENS_ALLOWED is false", "[SplitTens][Feature2]")
{
    Constants::RESPLIT_TENS_ALLOWED = false;

    Game game(true);
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(10, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(10, Suit::Clubs),   test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(3, Suit::Spades),   test_support::make_card(4, Suit::Hearts)});

    CHECK(game.split());                       // initial split of tens still allowed
    test_support::tickFrame(game);                            // first hand draws a ten -> {10,10}
    auto& p = game.testPlayer(0);
    int handsAfterInitialSplit = static_cast<int>(p.hands.size());

    CHECK_FALSE(game.split());                 // re-split of tens is blocked by the flag
    CHECK(static_cast<int>(p.hands.size()) == handsAfterInitialSplit); // no new hand created

    // The blocked re-split left the hand intact and actable, not finished.
    CHECK(p.hands[0].size() == 2);
    CHECK_FALSE(p.hands[0].isFinished());
}

// With tens off, eights can still re-split; the flag only governs tens.
TEST_CASE("Non-ten pairs can still be re-split when RESPLIT_TENS_ALLOWED is false", "[SplitTens][Feature2]")
{
    Constants::RESPLIT_TENS_ALLOWED = false;

    Game game(true);
    // Pair of eights vs hole 7 / up 6; the next draw re-pairs hand[0].
    test_support::start_round(game,{test_support::make_card(8, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(8, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(8, Suit::Clubs),   test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(3, Suit::Spades),   test_support::make_card(4, Suit::Hearts)});

    CHECK(game.split());                       // initial pair of eights -> two hands
    test_support::tickFrame(game);                            // first hand draws an eight -> {8,8}
    auto& p = game.testPlayer(0);

    CHECK(game.split());                       // re-split of eights still allowed despite the flag off
    CHECK(p.hands.size() == 3);
}


TEST_CASE("Initial deal of two aces puts the player in PlayerTurn, ready to split", "[SplitAces][Flow]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades),   // player card 1 -> Ace
                  test_support::make_card(7, Suit::Hearts),   // dealer hole card
                  test_support::make_card(1, Suit::Hearts),   // player card 2 -> Ace
                  test_support::make_card(6, Suit::Diamonds)});

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.size() == 2);
    CHECK(hand.getCard(0).isAce());
    CHECK(hand.getCard(1).isAce());
    CHECK(game.roundStateForTest() == RoundState::PlayerTurn);
}

// Splitting aces yields two hands, each exactly one ace, flagged split-ace and unfinished.
TEST_CASE("Splitting aces creates two single-card split-ace hands", "[SplitAces][Flow]")
{
    Game game(true);
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds)});

    CHECK(game.split());

    auto& p = game.testPlayer(0);
    CHECK(p.hands.size() == 2);
    for (size_t i = 0; i < p.hands.size(); ++i)
    {
        CHECK(p.hands[i].size() == 1);            // exactly one ace each, nothing dealt yet
        CHECK(p.hands[i].getCard(0).isAce());
        CHECK(p.hands[i].isSplitAce());           // both hands are split aces
        CHECK(p.hands[i].isSplit());
        CHECK(!p.hands[i].isFinished());          // not finished until its one extra card arrives
    }
}
// Spec: each split ace gets one card then stands, with no player prompt.
TEST_CASE("Each split ace receives exactly one card and auto-stands with no user input", "[SplitAces][Flow]")
{
    Game game(true);
    // The next two draws (K then 9) are each ace-hand's single permitted card.
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(13, Suit::Clubs),  test_support::make_card(9, Suit::Spades)});

    CHECK(game.split()); // player splits the aces

    // Drive the round to completion WITHOUT ever calling hit()/stand()/doubleDown().
    test_support::advanceToDealerTurn(game);

    // The round auto-advanced all the way to the dealer; no input was consumed.
    CHECK(game.roundStateForTest() == RoundState::DealerTurn);

    auto& p = game.testPlayer(0);
    CHECK(p.hands.size() == 2);
    // Both split hands took exactly one extra card and finished (stood) at that size.
    CHECK(p.hands[0].size() == 2);
    CHECK(p.hands[0].isSplitAce());
    CHECK(p.hands[0].isFinished());
    CHECK(!p.hands[0].isBlackjack()); // split ace + face is 21 but not a natural blackjack
    CHECK(p.hands[0].getValue() == 21);

    CHECK(p.hands[1].size() == 2);
    CHECK(p.hands[1].isSplitAce());
    CHECK(p.hands[1].isFinished());
    CHECK(!p.hands[1].isBlackjack());
}

// Spec: split ace + face card totals 21 but pays even money, NOT the 3:2 blackjack premium.
TEST_CASE("Split ace plus a face card is 21 but not a 3:2 blackjack", "[SplitAces][Payout]")
{
    Game game(true);
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(12, Suit::Clubs),  test_support::make_card(8, Suit::Spades)});

    CHECK(game.split());
    test_support::advanceToDealerTurn(game);

    auto& p = game.testPlayer(0);
    const Hand& hand0 = p.hands[0];
    CHECK(splitAceIs21NotBlackjack(hand0)); // A + Q == 21, flagged split ace, not natural BJ

    // Confirms the payout contract: even money (bet * 2), never 3:2 (which would be bet * 2.5).
    int payout = 0;
    SettlementResult result = p.settle(hand0, /*dealerValue=*/20, /*dealerBust=*/false, /*dealerHasBlackjack=*/false, payout);
    CHECK(result == SettlementResult::Win);
    CHECK(payout == hand0.getBet() * 2);

    const Hand& hand1 = p.hands[1];
    CHECK(!hand1.isBlackjack());
}

// Spec: no double-down on a split ace; verified by drawing another ace so it stays resplit-eligible, exercising the double-down guard itself.
TEST_CASE("Cannot double down on a split ace", "[SplitAces][Flow]")
{
    Game game(true);
    // The post-split draw is an Ace so the hand stays open (resplit-eligible) and unfinished.
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(1, Suit::Clubs)});

    CHECK(game.split());
    test_support::tickFrame(game); // deals the Ace -> [A, A], still unfinished so it can be re-split

    auto& p = game.testPlayer(0);
    const Hand& hand0 = p.hands[0];
    CHECK(hand0.isSplitAce());
    CHECK(!hand0.isFinished());           // left open for the resplit decision
    CHECK(hand0.size() == 2);

    // Double down is refused on a split ace even though it is not yet finished.
    CHECK_FALSE(game.doubleDown());
    CHECK(p.hands[0].size() == 2);        // doubling did not deal an extra card or finish the hand

    // Hitting a completed split ace forces a stand rather than taking another card.
    game.hit();
    CHECK(p.hands[0].isFinished());       // forced stand, not a hit
    CHECK(p.hands[0].size() == 2);        // still exactly two cards (Ace + the one permitted extra)
}
// Spec: re-splitting aces continues up to MAX_SPLIT_HANDS then refuses; every ace hand takes one card then stands.
TEST_CASE("Split aces can be re-split up to the maximum number of hands", "[SplitAces][Flow]")
{
    Game game(true);
    // Indices 0/2 are the initial aces; 4 and 5 are Aces drawn as each hand's first card (triggering
    // re-splits). The rest fill out the shoe so it never runs out mid-resolution.
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(1, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds),
                  test_support::make_card(5, Suit::Hearts),  test_support::make_card(9, Suit::Spades),
                  test_support::make_card(2, Suit::Clubs),   test_support::make_card(3, Suit::Hearts)});

    CHECK(game.split()); // AA -> two aces
    test_support::tickFrame(game);      // first ace draws an Ace -> still open (re-split eligible)
    CHECK(game.testPlayer(0).hands[0].isSplitAce());
    CHECK(!game.testPlayer(0).hands[0].isFinished());

    CHECK(game.split()); // re-split -> three aces
    CHECK(game.testPlayer(0).hands.size() == 3);
    test_support::tickFrame(game);      // first ace draws an Ace -> still open (re-split eligible)
    CHECK(game.testPlayer(0).hands[0].isSplitAce());
    CHECK(!game.testPlayer(0).hands[0].isFinished());

    CHECK(game.split()); // re-split -> four aces (the cap)
    CHECK(game.testPlayer(0).hands.size() == 4);

    // At the maximum, another split is refused.
    CHECK_FALSE(game.split());

    // Each of four ace hands takes one card and stands; the round auto-advances to the dealer.
    test_support::advanceToDealerTurn(game);
    CHECK(game.roundStateForTest() == RoundState::DealerTurn);

    for (const Hand& h : game.testPlayer(0).hands)
    {
        CHECK(h.size() == 2);
        CHECK(h.isSplitAce());
        CHECK(h.isFinished());
        CHECK(!h.isBlackjack());
    }
}

// Regression: splitting a normal pair still yields one card per hand that is NOT auto-finished (unlike split aces) - player keeps acting. This pins the fix against the old asymmetric first-hand-pre-dealt bug.
TEST_CASE("Normal pair split gives each hand one card but stays actable", "[SplitAces][Regression]")
{
    Game game(true);
    // Split 8s; dealer up non-ace. The next draw is an Ace on the first active hand.
    test_support::start_round(game,{test_support::make_card(8, Suit::Spades), test_support::make_card(2, Suit::Hearts),
                  test_support::make_card(8, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(1, Suit::Clubs)});

    CHECK(game.split()); // pair of eights -> two hands
    auto& p = game.testPlayer(0);
    CHECK(p.hands.size() == 2);
    for (const Hand& h : p.hands)
    {
        CHECK(h.size() == 1);
        CHECK(!h.isSplitAce());   // ordinary split, not a split ace
    }

    test_support::tickFrame(game); // first hand draws one card -> size 2, NOT finished
    CHECK(p.hands[0].size() == 2);
    CHECK(!p.hands[0].isFinished());        // unlike a split ace, it stays actable
    CHECK(!p.hands[0].isSplitAce());
    CHECK(game.roundStateForTest() == RoundState::PlayerTurn); // still the player's turn

    // The player can hit the ordinary split hand again (a split ace could not).
    game.hit();
    CHECK(p.hands[0].size() == 3);
}


TEST_CASE("Insurance path: dealer holds a natural after decline and draws no extra cards", "[Blackjack][Insurance][DealerTurn]")
{
    Game game(true); // headless: no window/assets/replay/HUD

    // Deal order: up-card is an Ace (OfferInsurance), hidden King makes a natural; player's 16 stays unresolved.
    test_support::start_round(game,{test_support::make_card(9, Suit::Spades),      // player card 0 -> 16 with the next card
                  test_support::make_card(1, Suit::Hearts),       // dealer up-card: Ace -> insurance offered
                  test_support::make_card(7, Suit::Clubs),        // player card 1 -> hand of 16 (not a natural, stays unresolved)
                  test_support::make_card(13, Suit::Diamonds),    // dealer hole card: King -> natural Ace + 10 = 21
                  test_support::make_card(2, Suit::Clubs),        // filler (drawn only if the dealer wrongly hits)
                  test_support::make_card(10, Suit::Hearts)});

    CHECK(game.roundStateForTest() == RoundState::OfferInsurance);   // up-card is an Ace -> insurance offered
    CHECK(game.testDealer().isBlackjack());                          // natural locked in on the deal

    // Human declines insurance; advancing past the last player wraps, evaluateBlackjack sees the natural and moves to DealerTurn without marking the unresolved hand.
    // dealerTurn draw extra cards.
    game.declineInsurance();
    CHECK(game.roundStateForTest() == RoundState::DealerTurn);

    driveToCleanup(game);

    // The natural is terminal: the dealer draws no extra cards and the player's 16 loses.
    CHECK(game.testDealer().size() == 2);
    CHECK(game.testDealer().getValue() == 21);
    CHECK(game.testPlayer(0).getStats().losses == 1);
}

// Same defect via direct entry: a non-Ace-up natural is detected on the deal, needing no insurance detour; without the fix the dealer over-draws.
// cards even though it already holds a finished 21.
TEST_CASE("Dealer natural with a non-Ace up-card draws no extra cards", "[Blackjack][DealerTurn]")
{
    Game game(true);

    // Up-card King evaluates blackjack directly; hidden Ace makes a natural; player's 16 loses.
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

// --- B5: the main bet is escrowed at commit, so a main + side pair can never be over-committed against one bankroll (which let a
// $1000 main + $1000 side both clear and settle to a negative balance). getAvailableBankroll() is the single predicate every bet
// clamp + hint reads; placeSideBet()/confirmBet() gate on it. Bankroll itself only moves at settlement, so escrow excludes the
// committed stakes from available() without changing the (unchanged) winBet/loseBet arithmetic below. -----------------------------------------------------------------------

TEST_CASE("B5: getAvailableBankroll() excludes committed main + side bets", "[Player][Settlement]")
{
    Player p("William", 1000);                              // NPC ctor -> bankroll 1000
    REQUIRE(p.getBankroll() == 1000);
    REQUIRE(p.getAvailableBankroll() == 1000);              // nothing committed -> available == bankroll

    p.commitMainBet(300);
    REQUIRE(p.getAvailableBankroll() == 700);               // main bet escrowed

    p.commitSideBet(200);
    REQUIRE(p.getAvailableBankroll() == 500);               // side bet also excluded from availability

    p.clearPendingBets();                                   // cleared at each round start (resetRound)
    REQUIRE(p.getAvailableBankroll() == 1000);

    p.commitSideBet(0);                                     // a zero amount opts the side bet out
    REQUIRE(p.getAvailableBankroll() == 1000);              // nothing committed remains
}

TEST_CASE("B5: a side bet larger than available balance is refused once the main bet is committed", "[Game][Settlement]")
{
    Game game(true);
    std::vector<Card> deck;                                // filler so the shoe never runs short during these no-deal assertions
    for (int i = 0; i < 210; ++i)
    {
        Card c;
        c.value = 5;
        c.suit = Suit::Hearts;
        deck.push_back(c);
    }
    test_support::seed_deck(game, deck);
    game.startTestRound();

    // Commit the largest single hand bet confirmBet accepts. It is escrowed (available() drops) but bankroll() itself is unchanged
    // until settlement -- that is exactly what lets a side bet be checked against *remaining* availability instead of the full roll.
    const int mainWager = Constants::MAXIMUM_BET;          // 1000
    game.testPlayer(0).hands[0].setBet(mainWager);
    game.confirmBet();

    const int remaining = game.testPlayer(0).getAvailableBankroll();   // bankroll - mainWager (no side committed yet)
    REQUIRE(remaining >= Constants::MINIMUM_BET);
    CHECK(game.testPlayer(0).getBankroll() == STARTING_BANKROLL);   // escrow does not move the recorded bankroll yet
    CHECK(remaining == STARTING_BANKROLL - mainWager);             // 10000 - 1000 = 9000

    // A side bet larger than what remains is refused and commits nothing (the bug let this clear because it checked full bankroll).
    game.placeSideBet(SideBetKind::PerfectPairs, remaining + 1);              // available(9000) < 9001 -> refused
    CHECK(game.testPlayer(0).getAvailableBankroll() == remaining);            // unchanged: still only the main escrow excluded

    // A side bet within the remaining balance is accepted and commits, driving available all the way down.
    game.placeSideBet(SideBetKind::PerfectPairs, remaining);                  // available(9000) >= 9000 -> accepted
    CHECK(game.testPlayer(0).getAvailableBankroll() == 0);                    // main + side now escrow the whole bankroll
}

TEST_CASE("B5: a losing round can never drive bankroll below zero and honors bankroll conservation", "[Game][Settlement]")
{
    Game game(true);
    // Dealer natural (K up + ace hole) beats the player's non-BJ hand; 9 + 7 is not a pair, so PerfectPairs loses too. This exact
    // first-deal sequence already proves main loss == MINIMUM_BET and dealer.isBlackjack() in its own test, so both wagers are known
    // losses here. Deal order: player card, dealer up-card, player card, dealer hole. The two 8-clubs filler keep the shoe from
    // running short; a natural resolves with no further draws, so the six seeded cards hold (no reshuffle) and the outcome is exact.
    test_support::seed_deck(game,{test_support::make_card(9, Suit::Spades),     // player card 0 -> 9
                  test_support::make_card(13, Suit::Spades),    // dealer up-card (not an ace)
                  test_support::make_card(7, Suit::Clubs),      // player card 1 -> 7 (non-pair with the 9)
                  test_support::make_card(1, Suit::Hearts),     // dealer hole -> Ace: K,A = dealer natural
                  test_support::make_card(8, Suit::Clubs),      // filler
                  test_support::make_card(8, Suit::Clubs)});    // filler

    const int sideWager = 60;
    game.placeSideBet(SideBetKind::PerfectPairs, sideWager);   // intent set before the seat exists; deal() consumes it

    game.startTestRound();             // builds seats at STARTING_BANKROLL, deals; resolveSideBets() loses the non-pair side now
    test_support::settleRound(game);   // dealer natural settles the main for exactly one wager

    const int after = game.testPlayer(0).getBankroll();
    CHECK(after >= 0);                                          // no negative balance, ever (the whole point of B5)
    // Conservation: both wagers lost -> payouts are 0 -> after == STARTING_BANKROLL - MINIMUM_BET - sideWager.
    CHECK(after == STARTING_BANKROLL - Constants::MINIMUM_BET - sideWager);
}

