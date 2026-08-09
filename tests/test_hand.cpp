#include <catch2/catch_all.hpp>
#include <Hand.hpp>
#include <Deck.hpp>

TEST_CASE("Empty hand has value zero", "[Hand]")
{
    Hand hand;
    CHECK(hand.getValue() == 0);
    CHECK(hand.size() == 0);
}

TEST_CASE("Simple hand values are correct", "[Hand]")
{
    Hand hand;
    
    // Add 5 and 7 = 12
    Card card5, card7;
    card5.value = 5;
    card7.value = 7;
    hand.addCard(card5);
    hand.addCard(card7);
    
    CHECK(hand.getValue() == 12);
}

TEST_CASE("Ace counting works correctly", "[Hand]")
{
    Hand hand;
    
    Card ace, seven;
    ace.value = 1;      // Ace
    seven.value = 7;    // Seven
    
    hand.addCard(ace);
    hand.addCard(seven);
    
    // A + 7 = 18 (Ace counts as 11)
    CHECK(hand.getValue() == 18);
    CHECK(hand.isSoft() == true);
    
    // Add another card to make it hard
    Card ten;
    ten.value = 10;
    hand.addCard(ten);
    
    // A(1) + 7 + 10 = 18 (Ace now counts as 1, not 11)
    CHECK(hand.getValue() == 18);
    CHECK(hand.isSoft() == false);
}

TEST_CASE("Blackjack is detected correctly", "[Hand]")
{
    Hand hand;
    
    Card ace, king;
    ace.value = 1;
    king.value = 13;
    
    hand.addCard(ace);
    hand.addCard(king);
    
    CHECK(hand.isBlackjack() == true);
    CHECK(hand.getValue() == 21);
    
    // Not blackjack if more than 2 cards
    Hand hand2;
    Card ten;
    ten.value = 10;
    hand2.addCard(ace);
    hand2.addCard(ten);
    hand2.addCard(ten);
    
    CHECK(hand2.isBlackjack() == false);
    CHECK(hand2.getValue() == 21); // Still 21, but not blackjack
}

TEST_CASE("Bust is detected correctly", "[Hand]")
{
    Hand hand;
    
    // 10 + 9 + 5 = 24 (bust)
    Card ten, nine, five;
    ten.value = 10;
    nine.value = 9;
    five.value = 5;
    
    hand.addCard(ten);
    hand.addCard(nine);
    
    CHECK(hand.isBust() == false);
    
    hand.addCard(five);
    
    CHECK(hand.isBust() == true);
    CHECK(hand.getValue() == 24);
}

TEST_CASE("Hand clear works", "[Hand]")
{
    Hand hand;
    
    Card card;
    card.value = 5;
    hand.addCard(card);
    
    CHECK(hand.size() == 1);
    
    hand.clear();
    
    CHECK(hand.size() == 0);
    CHECK(hand.getValue() == 0);
}

TEST_CASE("Hand split rules are correct", "[Hand]")
{
    Hand hand;
    
    // Same value = can split
    Card tenA, tenB;
    tenA.value = 10;
    tenB.value = 10;
    hand.addCard(tenA);
    hand.addCard(tenB);
    
    CHECK(hand.canSplit() == true);
    
    // Different value = cannot split
    Hand hand2;
    Card five;
    five.value = 5;
    hand2.addCard(tenA);
    hand2.addCard(five);
    
    CHECK(hand2.canSplit() == false);
}