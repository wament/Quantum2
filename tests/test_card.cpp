#include <catch2/catch_all.hpp>
#include "Card.hpp"

// Each TEST_CASE is one group of tests
// The first string is the test name, the second is a tag for organizing tests

TEST_CASE("Card values are calculated correctly", "[Card]")
{
    Card card;

    // Test regular numbered card (value 5)
    card.value = 5;
    CHECK(card.getBlackjackValue() == 5);

    // Test face cards (Jack=11, Queen=12, King=13) all count as 10
    card.value = 11;
    CHECK(card.getBlackjackValue() == 10);
    card.value = 12;
    CHECK(card.getBlackjackValue() == 10);
    card.value = 13;
    CHECK(card.getBlackjackValue() == 10);

    // Test Ace counts as 11
    card.value = 1;
    CHECK(card.getBlackjackValue() == 11);
}

TEST_CASE("Card identification functions work", "[Card]")
{
    Card card;

    // Test isAce()
    card.value = 1;
    CHECK(card.isAce() == true);

    card.value = 7;
    CHECK(card.isAce() == false);

    // Test isFaceCard()
    card.value = 11;
    CHECK(card.isFaceCard() == true);

    card.value = 12;
    CHECK(card.isFaceCard() == true);

    card.value = 13;
    CHECK(card.isFaceCard() == true);

    card.value = 10;
    CHECK(card.isFaceCard() == false);

    card.value = 5;
    CHECK(card.isFaceCard() == false);
}

TEST_CASE("Card value strings are correct", "[Card]")
{
    Card card;

    card.value = 1;
    CHECK(card.getBlackjackValueToString() == "Ace");

    card.value = 5;
    CHECK(card.getBlackjackValueToString() == "5");

    card.value = 11;
    CHECK(card.getBlackjackValueToString() == "Face");
}