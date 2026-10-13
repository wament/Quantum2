#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>
#include <Hand.hpp>
#include <Deck.hpp>
#include <Player.hpp>

using namespace Constants;

// ---------------------------------------------------------------------------
// These tests follow the split-aces flow all of the way through, driving the
// real Game round-state machine (Game::startTestRound -> split/hit/stand and
// RoundEngine::tick) end-to-end. Every case maps to one clause of the expected
// behavior so a regression here would have caught the persisting bug before it
// shipped.
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

    // One frame of the shared state machine. deltaTime is irrelevant to the split-ace logic (the
    // single-card deal + auto-finish run regardless of timing).
    void tickOnce(Game& game)
    {
        RoundEngine engine;
        engine.tick(0.0f, game);
    }

    // Drive frames until the round leaves PlayerTurn (all hands resolved -> DealerTurn). Bounded so a
    // stuck state cannot spin forever. Never invokes any player action, which is how we prove the
    // "no input required after splitting aces" clause.
    void tickUntilLeavesPlayerTurn(Game& game)
    {
        RoundEngine engine;
        for (int i = 0; i < 25 && game.roundStateForTest() == RoundState::PlayerTurn; ++i)
            engine.tick(0.0f, game);
    }

    // Is this hand a finished split ace totaling 21 without being a natural (3:2) blackjack?
    bool splitAceIs21NotBlackjack(const Hand& h)
    {
        return h.isSplitAce() && !h.isBlackjack() && h.getValue() == 21;
    }
}

// Initial deal of two aces lands the player in PlayerTurn ready to split.
TEST_CASE("Initial deal of two aces puts the player in PlayerTurn, ready to split", "[SplitAces][Flow]")
{
    Game game(true); // headless: no window/assets/replay/HUD
    game.setDeck({makeCard(1, Suit::Spades),   // player card 1 -> Ace
                  makeCard(7, Suit::Hearts),   // dealer hole card
                  makeCard(1, Suit::Hearts),   // player card 2 -> Ace
                  makeCard(6, Suit::Diamonds)}); // dealer up card (non-ace -> PlayerTurn)

    game.startTestRound();

    const Hand& hand = game.testHand(0, 0);
    CHECK(hand.size() == 2);
    CHECK(hand.getCard(0).isAce());
    CHECK(hand.getCard(1).isAce());
    CHECK(game.roundStateForTest() == RoundState::PlayerTurn);
}

// Splitting aces yields two hands, each with exactly one ace, both flagged split-ace and unfinished.
TEST_CASE("Splitting aces creates two single-card split-ace hands", "[SplitAces][Flow]")
{
    Game game(true);
    game.setDeck({makeCard(1, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(1, Suit::Hearts),  makeCard(6, Suit::Diamonds)});
    game.startTestRound();

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
// Spec: each split ace receives exactly one card then stands; the player is prompted for input at no point.
TEST_CASE("Each split ace receives exactly one card and auto-stands with no user input", "[SplitAces][Flow]")
{
    Game game(true);
    // After the initial four cards, the next two draws (K then 9) are each hand's single permitted card.
    game.setDeck({makeCard(1, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(1, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(13, Suit::Clubs),  makeCard(9, Suit::Spades)});
    game.startTestRound();

    CHECK(game.split()); // player splits the aces

    // Drive the round to completion WITHOUT ever calling hit()/stand()/doubleDown().
    tickUntilLeavesPlayerTurn(game);

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
    game.setDeck({makeCard(1, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(1, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(12, Suit::Clubs),  makeCard(8, Suit::Spades)}); // Q then 8
    game.startTestRound();

    CHECK(game.split());
    tickUntilLeavesPlayerTurn(game);

    auto& p = game.testPlayer(0);
    const Hand& hand0 = p.hands[0];
    CHECK(splitAceIs21NotBlackjack(hand0)); // A + Q == 21, flagged split ace, not natural BJ

    // Confirms the payout contract: even money (bet * 2), never 3:2 (which would be bet * 2.5).
    int payout = 0;
    SettlementResult result = p.settle(hand0, /*dealerValue=*/20, /*dealerBust=*/false, payout);
    CHECK(result == SettlementResult::Win);
    CHECK(payout == hand0.getBet() * 2);

    const Hand& hand1 = p.hands[1];
    CHECK(!hand1.isBlackjack());
}

// Spec: the player cannot double down on a split ace (verified when the ace draws another ace and is left
// resplit-eligible / unfinished, so the double-down guard -- not just the finished guard -- is exercised).
TEST_CASE("Cannot double down on a split ace", "[SplitAces][Flow]")
{
    Game game(true);
    // The draw after splitting must be an Ace so the hand stays open (resplit-eligible) and unfinished.
    game.setDeck({makeCard(1, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(1, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(1, Suit::Clubs)}); // extra Ace on the first split hand
    game.startTestRound();

    CHECK(game.split());
    tickOnce(game); // deals the Ace -> [A, A], still unfinished so it can be re-split

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
// Spec: if a split ace is dealt another ace, it may be re-split up to MAX_SPLIT_HANDS; at the cap the split
// is refused. Every resulting ace hand still takes exactly one card and then stands.
TEST_CASE("Split aces can be re-split up to the maximum number of hands", "[SplitAces][Flow]")
{
    Game game(true);
    // Indices 0/2 are the initial aces; 4 and 5 are Aces drawn as each hand's first card (triggering
    // re-splits). The rest fill out the shoe so it never runs out mid-resolution.
    game.setDeck({makeCard(1, Suit::Spades), makeCard(7, Suit::Hearts),
                  makeCard(1, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(1, Suit::Clubs),   makeCard(1, Suit::Diamonds),
                  makeCard(5, Suit::Hearts),  makeCard(9, Suit::Spades),
                  makeCard(2, Suit::Clubs),   makeCard(3, Suit::Hearts)});
    game.startTestRound();

    CHECK(game.split()); // AA -> two aces
    tickOnce(game);      // first ace draws an Ace -> still open (re-split eligible)
    CHECK(game.testPlayer(0).hands[0].isSplitAce());
    CHECK(!game.testPlayer(0).hands[0].isFinished());

    CHECK(game.split()); // re-split -> three aces
    CHECK(game.testPlayer(0).hands.size() == 3);
    tickOnce(game);      // first ace draws an Ace -> still open (re-split eligible)
    CHECK(game.testPlayer(0).hands[0].isSplitAce());
    CHECK(!game.testPlayer(0).hands[0].isFinished());

    CHECK(game.split()); // re-split -> four aces (the cap)
    CHECK(game.testPlayer(0).hands.size() == 4);

    // At the maximum, another split is refused.
    CHECK_FALSE(game.split());

    // Each of the four ace hands takes exactly one card and stands; the round auto-advances to the dealer.
    tickUntilLeavesPlayerTurn(game);
    CHECK(game.roundStateForTest() == RoundState::DealerTurn);

    for (const Hand& h : game.testPlayer(0).hands)
    {
        CHECK(h.size() == 2);
        CHECK(h.isSplitAce());
        CHECK(h.isFinished());
        CHECK(!h.isBlackjack());
    }
}

// Regression: splitting a normal pair still gives each hand exactly one card, and those hands are NOT
// auto-finished like split aces (the player may continue to act on them). This pins down the fix so the
// symmetric size-1 post-split behavior cannot regress into the old asymmetric "first hand pre-dealt" bug.
TEST_CASE("Normal pair split gives each hand one card but stays actable", "[SplitAces][Regression]")
{
    Game game(true);
    // Split 8s; dealer up non-ace. The next draw is an Ace on the first active hand.
    game.setDeck({makeCard(8, Suit::Spades), makeCard(2, Suit::Hearts),
                  makeCard(8, Suit::Hearts),  makeCard(6, Suit::Diamonds),
                  makeCard(1, Suit::Clubs)});
    game.startTestRound();

    CHECK(game.split()); // pair of eights -> two hands
    auto& p = game.testPlayer(0);
    CHECK(p.hands.size() == 2);
    for (const Hand& h : p.hands)
    {
        CHECK(h.size() == 1);
        CHECK(!h.isSplitAce());   // ordinary split, not a split ace
    }

    tickOnce(game); // first hand draws one card -> size 2, NOT finished
    CHECK(p.hands[0].size() == 2);
    CHECK(!p.hands[0].isFinished());        // unlike a split ace, it stays actable
    CHECK(!p.hands[0].isSplitAce());
    CHECK(game.roundStateForTest() == RoundState::PlayerTurn); // still the player's turn

    // The player can hit the ordinary split hand again (a split ace could not).
    game.hit();
    CHECK(p.hands[0].size() == 3);
}
