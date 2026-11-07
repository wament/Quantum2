#include <catch2/catch_all.hpp>
#include <Deck.hpp>
#include "Constants.hpp"

// B8: Deck::draw() has no empty-shoe guard. Two unguarded cards.back()/pop_back() pairs sit in Deck::draw(): the first
// when the shoe is already empty, the second after only the cut card was drawn (a single-card shoe whose one card is the
// cut card). Both are undefined behavior on a std::vector::back() of an empty container. This is purely defensive -- every
// production caller guards deck.empty() before calling draw(), so real gameplay never reaches it, but a headless harness can
// reach it directly via setCards({}). The guard makes empty-draw return a benign default instead of UB/crash; the Card return
// type and all 5 callers are unchanged. This test must FAIL (UB/crash / non-default value) before the fix and pass after.

TEST_CASE("Draw from a completely empty deck returns a benign default without UB", "[Deck]")
{
    Deck deck;
    deck.setCards({});   // never-allocated empty shoe, reachable only via the test harness

    Card first = deck.draw();          // pre-fix: unguarded back() on an empty (unallocated) vector -> UB/crash
    REQUIRE(first.value == 0);         // post-fix returns a default-constructed Card
    REQUIRE(first.suit == Suit::Spades);

    Card second = deck.draw();         // stays safe on repeated empty draws, and does not corrupt the empty shoe
    REQUIRE(second.value == 0);

    CHECK(deck.size() == 0);           // an empty shoe stays empty (pre-fix pop_back-on-empty can underflow/corrupt)
}

TEST_CASE("Draw the single remaining cut card does not UB", "[Deck]")
{
    Deck deck;
    deck.setCards({ Card{Constants::CUT_CARD_VALUE, Suit::Spades} });

    Card c = deck.draw();              // pre-fix: after popping the only card, a second back() on empty -> UB/crash
    REQUIRE(c.value == Constants::CUT_CARD_VALUE);
}
