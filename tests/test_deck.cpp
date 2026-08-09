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