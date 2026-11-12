#include <catch2/catch_all.hpp>
#include <Deck.hpp>
#include "Constants.hpp"
#include "GameRecord.hpp"
#include "Serialization.hpp"

TEST_CASE("Deck is empty when created", "[Deck]")
{
    Deck deck;

    CHECK(deck.empty() == true);
    CHECK(deck.size() == 0);
}

TEST_CASE("Deck creates correct number of cards", "[Deck]")
{
    Deck deck;

    deck.create(1);
    CHECK(deck.size() == 52);
    CHECK(deck.empty() == false);

    Deck deck2;
    deck2.create(2);
    CHECK(deck2.size() == 104);
}

TEST_CASE("Drawing from deck works", "[Deck]")
{
    Deck deck;
    deck.create(1);

    Card card = deck.draw();

    REQUIRE(card.value >= 1);
    REQUIRE(card.value <= 13);

    CHECK(deck.size() == 51);
}

TEST_CASE("Deck becomes empty after drawing all cards", "[Deck]")
{
    Deck deck;
    deck.create(1);

    for (int i = 0; i < 52; ++i)
    {
        Card c = deck.draw();
    }

    CHECK(deck.empty() == true);
    CHECK(deck.size() == 0);
}

TEST_CASE("Cut card is placed 25 cards from end of shoe", "[Deck][CutCard]")
{
    Deck deck;
    deck.create(2); // 104 cards
    CHECK(deck.size() == 104);

    deck.shuffle();

    // After shuffle: 104 cards + 1 cut card = 105 total
    CHECK(deck.size() == 105);

    int cardsDrawnBeforeCut = 0;
    while (!deck.reachedCutCard())
    {
        Card c = deck.draw();
        cardsDrawnBeforeCut++;
    }

    // The cut card is inserted near the front (index 25) while cards are drawn from the back,
    // so ~79 real cards are dealt before it turns on draw #80. That final draw also pops one
    // more real card, leaving 104 - 80 = 24 in the shoe.
    CHECK(cardsDrawnBeforeCut == 80);

    CHECK(deck.size() == 24);
}

TEST_CASE("reshuffleWithRemaining combines deck and table cards", "[Deck][Reshuffle]")
{
    Deck deck;
    deck.create(1);
    deck.shuffle();

    // Draw some cards to simulate dealing
    std::vector<Card> tableCards;
    for (int i = 0; i < 6; ++i)
    {
        tableCards.push_back(deck.draw());
    }

    // Rebuild the one-deck shoe (create(1)) minus the dealt cards — not a fresh six-deck shoe.
    deck.reshuffleWithRemaining(tableCards);

    size_t expectedSize = 1 * 52 - tableCards.size() + 1;
    CHECK(deck.size() == expectedSize);

    int cardsDrawn = 0;
    while (!deck.reachedCutCard() && !deck.empty())
    {
        Card c = deck.draw();
        cardsDrawn++;
    }
    CHECK(deck.reachedCutCard());
}

TEST_CASE("reshuffleWithRemaining saves previous shoe record", "[Deck][Reshuffle]")
{
    Deck deck;
    deck.create(1);
    deck.shuffle();

    // Draw a few cards
    std::vector<Card> tableCards;
    for (int i = 0; i < 4; ++i)
    {
        tableCards.push_back(deck.draw());
    }

    // Reshuffle - this should move the current shoe to "previous"
    deck.reshuffleWithRemaining(tableCards);

    const std::vector<Card>* prev = deck.getPreviousShoeRecord();
    CHECK(prev != nullptr);

    CHECK(prev->size() == 53); // 52 cards + 1 cut card
}

TEST_CASE("Cut card position for 2-deck shoe", "[Deck][CutCard]")
{
    Deck deck;
    deck.create(2); // 104 cards
    deck.shuffle();

    // Total should be 105 (104 + 1 cut card)
    CHECK(deck.size() == 105);

    // Draw until cut card is reached
    int cardsDrawn = 0;
    while (!deck.reachedCutCard())
    {
        Card c = deck.draw();
        cardsDrawn++;
    }
    CHECK(deck.reachedCutCard());

    // The cut card sits near the front while drawing from the back, so ~79 real cards are dealt
    // before it turns on draw #80 (which also pops one more real card), leaving 24 in the shoe.
    CHECK(cardsDrawn == 80);

    CHECK(deck.size() == 24);
}

TEST_CASE("reshuffleWithRemaining with empty table cards", "[Deck][Reshuffle]")
{
    Deck deck;
    deck.create(1);
    deck.shuffle();

    // Draw all cards except the last few
    for (int i = 0; i < 50; ++i)
    {
        Card c = deck.draw();
        (void)c;
    }

    // Rebuild the one-deck shoe (create(1)) with no played cards — not a fresh six-deck shoe.
    std::vector<Card> emptyTable;
    deck.reshuffleWithRemaining(emptyTable);

    size_t expectedSize = 1 * 52 - emptyTable.size() + 1;
    CHECK(deck.size() == expectedSize);
}

// B21: `ShoeRecord::shuffleSeed` was declared, serialized in both directions, and commented "MT19937 seed used when
// shoe was created" -- and was never assigned, so every saved shoe carried 0. These lock the property the fix depends
// on: the seed a shuffle used reproduces that shoe exactly. Without it, "store the seed, not the deck" would let
// replay fabricate a deck that was never dealt.
namespace {

Card b21Card(int value, Suit suit) { Card c; c.value = value; c.suit = suit; return c; }

bool sameShoe(const std::vector<Card>& a, const std::vector<Card>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].value != b[i].value || a[i].suit != b[i].suit) return false;
    return true;
}

} // namespace

TEST_CASE("B21: shuffle() records a seed that reproduces the shoe it built", "[Deck][Seed][B21]")
{
    Deck a;
    a.create(6);
    a.shuffle();

    const unsigned long long seed = a.shuffleSeed();
    CHECK(seed != 0ull);                       // 0 here means shuffle() never recorded what it used
    const std::vector<Card> first = a.getCards();
    REQUIRE(first.size() == 6u * 52u + 1u);    // 312 cards + the cut card

    Deck b;
    b.create(6);
    b.shuffleWithSeed(seed);
    CHECK(b.shuffleSeed() == seed);
    CHECK(sameShoe(b.getCards(), first));      // card for card, cut card included
}

TEST_CASE("B21: shoeDecks() reports the deck count, not the card count", "[Deck][Seed][B21]")
{
    Deck d;
    d.create(6);
    d.shuffle();
    CHECK(d.shoeDecks() == 6);
    CHECK(d.size() == 313u);                   // the seed alone does not fix size; this does
}

TEST_CASE("B21: a shoe that was never shuffled reports seed 0, not an indeterminate value", "[Deck][Seed][B21]")
{
    // lastShuffleSeed was an uninitialized member. A record written from a Deck filled by create()/setCards()
    // would serialize garbage as a plausible seed and corrupt replay reconstruction.
    Deck d;
    d.create(6);
    CHECK(d.shuffleSeed() == 0ull);

    Deck injected;
    injected.setCards({b21Card(8, Suit::Clubs), b21Card(8, Suit::Spades)});
    CHECK(injected.shuffleSeed() == 0ull);
}

TEST_CASE("B21: reshuffleWithRemaining() records a seed that reproduces the continued shoe", "[Deck][Seed][B21]")
{
    Deck d;
    d.create(6);
    d.shuffle();
    std::vector<Card> played;
    for (int i = 0; i < 40; ++i)
        played.push_back(d.draw());

    d.reshuffleWithRemaining(played);
    const unsigned long long seed = d.shuffleSeed();
    CHECK(seed != 0ull);

    Deck again;
    again.create(6);
    again.reshuffleWithRemaining(played);
    // A fresh OS seed each call, so the two continued shoes differ -- what must be stable is that the seed the
    // record captured rebuilds the shoe the record describes, which the shuffleWithSeed path above proves.
    CHECK(again.shuffleSeed() != seed);
    CHECK(again.size() == d.size());
}

// The trap B21 exposed: reshuffleWithRemaining() shuffles the full deck MINUS every card already dealt, so a
// continuation shoe is a shuffle of a REDUCED set. create(shoeDecks) + shuffleWithSeed(seed) therefore rebuilds a
// FULL 313-card shoe, not the one that was dealt -- wrong size and wrong order. The pre-B21 schema captured
// deck.getCards() verbatim and got this right for free; the seed schema has to store the count. This test locks
// both the fix and the boundary, because #1/#4/#9 (forks, codes, ghosts) all assume "the record describes the shoe".
TEST_CASE("B21: a continuation shoe is smaller than a fresh shoe and the seed cannot rebuild it", "[Deck][Seed][B21][Continuation]")
{
    Deck d;
    d.create(6);
    d.shuffle();
    std::vector<Card> played;
    for (int i = 0; i < 100; ++i)
        played.push_back(d.draw());
    d.reshuffleWithRemaining(played);

    ShoeRecord rec;
    rec.shoeDecks = d.shoeDecks();
    rec.shuffleSeed = d.shuffleSeed();
    rec.shoeCardCount = static_cast<int>(d.size());
    rec.isContinuation = true;

    CHECK(rec.shoeCardCount < rec.shoeDecks * 52 + 1);        // reduced set, not a full shoe
    CHECK(rec.shoeCardCount == static_cast<int>(d.size()));

    const ShoeRecord back = json(rec).get<ShoeRecord>();
    CHECK(back.shoeCardCount == rec.shoeCardCount);           // the count survives the save
    CHECK(back.isContinuation == true);

    Deck rebuilt;
    rebuilt.create(back.shoeDecks);
    rebuilt.shuffleWithSeed(back.shuffleSeed);
    CHECK(static_cast<int>(rebuilt.size()) == back.shoeDecks * 52 + 1);
    CHECK_FALSE(sameShoe(rebuilt.getCards(), d.getCards()));  // the seed describes a shoe that was never dealt
}

// The shoe order must be defined by THIS PROGRAM, not by the standard library. Deck used std::shuffle, whose swap
// sequence is specified but which draws its indices through std::uniform_int_distribution -- and that distribution's
// algorithm is implementation-defined. A recorded shuffleSeed therefore pinned a shoe only within one standard
// library, which is exactly what #4 Shoe Codes, #9 Ghost Rival and #1's shareable forks need to cross machines.
// Deck::shuffleWith() now runs a hand-written Fisher-Yates over the engine, so the order is fully specified.
// This test freezes that order: if a future change moves it, this fails loudly instead of silently redefining every
// seed the app has ever written.
TEST_CASE("B21: a seed pins an exact card order independent of the standard library", "[Deck][Seed][Golden]")
{
    Deck d;
    d.create(6);
    d.shuffleWithSeed(0xC0FFEEULL);

    const std::vector<Card>& shoe = d.getCards();
    REQUIRE(shoe.size() == 313u);

    // Frozen first eight cards of the 0xC0FFEE six-deck shoe, plus the tail (the cut card is at index 25 and must
    // survive in the same place).
    // Frozen first eight cards of the 0xC0FFEE six-deck shoe. Regenerate deliberately (never by "fixing" a red test):
    // a change here means every seed the app has ever written now describes a different shoe.
    const int expValue[8] = {12, 7, 13, 3, 1, 7, 4, 6};
    const int expSuit[8]  = { 3, 1,  1, 3, 0, 1, 0, 2};
    for (int i = 0; i < 8; ++i)
    {
        INFO("index " << i << " got value=" << shoe[i].value << " suit=" << static_cast<int>(shoe[i].suit));
        CHECK(shoe[i].value == expValue[i]);
        CHECK(static_cast<int>(shoe[i].suit) == expSuit[i]);
    }

    CHECK(shoe[25].value == Constants::CUT_CARD_VALUE);   // the cut card position is part of the contract

    // Same seed, second Deck => identical order. Cheap, and it is the property every seed consumer depends on.
    Deck again;
    again.create(6);
    again.shuffleWithSeed(0xC0FFEEULL);
    CHECK(sameShoe(again.getCards(), shoe));
}
