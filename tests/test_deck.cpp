#include <catch2/catch_all.hpp>
#include <Deck.hpp>

TEST_CASE("Deck is empty when created", "[Deck]")
{
    Deck deck;
    
    CHECK(deck.empty() == true);
    CHECK(deck.size() == 0);
}

TEST_CASE("Deck creates correct number of cards", "[Deck]")
{
    Deck deck;
    
    // Standard deck should be 52 cards
    deck.create(1);
    CHECK(deck.size() == 52);
    CHECK(deck.empty() == false);
    
    // Two decks should be 104 cards
    Deck deck2;
    deck2.create(2);
    CHECK(deck2.size() == 104);
}

TEST_CASE("Drawing from deck works", "[Deck]")
{
    Deck deck;
    deck.create(1);
    
    // Draw a card
    Card card = deck.draw();
    
    // Check card is valid
    REQUIRE(card.value >= 1);
    REQUIRE(card.value <= 13);
    
    // Deck should have one less card
    CHECK(deck.size() == 51);
}

TEST_CASE("Deck becomes empty after drawing all cards", "[Deck]")
{
    Deck deck;
    deck.create(1);
    
    // Draw all 52 cards
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
    
    // Draw cards until cut card is reached (detected via reachedCutCard())
    int cardsDrawnBeforeCut = 0;
    while (!deck.reachedCutCard())
    {
        Card c = deck.draw();
        cardsDrawnBeforeCut++;
    }
    
    // Deck draws from the back. Cut card is at index 79 (104 - 25).
    // We draw 25 cards (indices 104-80), then the cut card at index 79,
    // which triggers reachedCutCard() and draws one more card (index 78).
    // Total: 26 draws.
    CHECK(cardsDrawnBeforeCut == 26);
    
    // After cut card is reached, 78 cards should remain (105 - 27 consumed)
    CHECK(deck.size() == 78);
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
    
    size_t remainingBefore = deck.size();
    
    // Reshuffle with table cards added back
    deck.reshuffleWithRemaining(tableCards);
    
    // New shoe should have all original cards (remaining + table)
    // Plus 1 cut card
    size_t expectedSize = remainingBefore + tableCards.size() + 1;
    CHECK(deck.size() == expectedSize);
    
    // Verify cut card is reachable
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
    
    // Get the previous shoe record
    const std::vector<Card>* prev = deck.getPreviousShoeRecord();
    CHECK(prev != nullptr);
    
    // Previous record should be the original shuffled deck (before reshuffle)
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
    
    // Deck draws from back. Cut card at index 79 (104 - 25).
    // 25 cards drawn, then cut card triggers one more draw = 26 total
    CHECK(cardsDrawn == 26);
    
    // After cut card, 78 cards remain (105 - 27 consumed)
    CHECK(deck.size() == 78);
}

TEST_CASE("reshuffleWithRemaining with empty table cards", "[Deck][Reshuffle]")
{
    Deck deck;
    deck.create(1);
    deck.shuffle();
    
    // Draw all cards except the last few
    for (int i = 0; i < 50; ++i)
    {
        deck.draw();
    }
    
    size_t remainingBefore = deck.size();
    
    // Reshuffle with no table cards
    std::vector<Card> emptyTable;
    deck.reshuffleWithRemaining(emptyTable);
    
    // Should have remaining cards + 1 cut card
    CHECK(deck.size() == remainingBefore + 1);
}