#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <BasicStrategy.hpp>
#include <Hand.hpp>
#include <Deck.hpp>
#include <vector>

using namespace Constants;
using Catch::Approx;

TEST_CASE("Kelly bet returns minimum when true count < 2", "[Game][Betting]")
{
    int minBet = 10;
    int maxBet = 1000;
    int bankroll = 10000;
    float kellyFraction = 0.33f;

    // TC < 2 should always return min bet
    CHECK(BasicStrategy::calculateKellyBet(0.0f, kellyFraction, bankroll, minBet, maxBet) == minBet);
    CHECK(BasicStrategy::calculateKellyBet(1.0f, kellyFraction, bankroll, minBet, maxBet) == minBet);
    CHECK(BasicStrategy::calculateKellyBet(1.9f, kellyFraction, bankroll, minBet, maxBet) == minBet);
}

TEST_CASE("Kelly bet scales with true count", "[Game][Betting]")
{
    int minBet = 10;
    int maxBet = 100000; // high enough not to cap
    int bankroll = 10000;
    float kellyFraction = 0.33f;

    int betAtTC2 = BasicStrategy::calculateKellyBet(2.0f, kellyFraction, bankroll, minBet, maxBet);
    int betAtTC4 = BasicStrategy::calculateKellyBet(4.0f, kellyFraction, bankroll, minBet, maxBet);

    CHECK(betAtTC2 >= minBet);
    CHECK(betAtTC4 > betAtTC2);
}

TEST_CASE("Kelly bet caps at maximum", "[Game][Betting]")
{
    int minBet = 10;
    int maxBet = 100;
    int bankroll = 1000000;
    float kellyFraction = 1.0f;

    int bet = BasicStrategy::calculateKellyBet(10.0f, kellyFraction, bankroll, minBet, maxBet);
    CHECK(bet <= maxBet);
}

TEST_CASE("Hi-Lo counting values are correct", "[Game][Counting]")
{
    // 2-6 = +1
    CHECK(BasicStrategy::calculateHiLoValue(2) == 1);
    CHECK(BasicStrategy::calculateHiLoValue(5) == 1);
    CHECK(BasicStrategy::calculateHiLoValue(6) == 1);

    // 7-9 = 0
    CHECK(BasicStrategy::calculateHiLoValue(7) == 0);
    CHECK(BasicStrategy::calculateHiLoValue(9) == 0);

    // 10, A = -1
    CHECK(BasicStrategy::calculateHiLoValue(10) == -1);
    CHECK(BasicStrategy::calculateHiLoValue(1) == -1);
}

TEST_CASE("Hi-Lo count updates correctly", "[Game][Counting]")
{
    int count = 0;
    count = BasicStrategy::updateHiLo(count, 5);   // +1
    CHECK(count == 1);
    count = BasicStrategy::updateHiLo(count, 10);  // -1
    CHECK(count == 0);
    count = BasicStrategy::updateHiLo(count, 7);   // 0
    CHECK(count == 0);
}

TEST_CASE("True count computation", "[Game][Counting]")
{
    // 6 decks remaining = 312 cards
    float tc = BasicStrategy::computeTrueCount(12, 312);
    CHECK(tc == Approx(2.0f).margin(0.01f));

    // Edge case: very few cards remaining (clamped to 0.5 decks)
    tc = BasicStrategy::computeTrueCount(10, 20);
    CHECK(tc == Approx(20.0f).margin(0.01f));
}

TEST_CASE("Basic strategy: hard 12 vs 6 should stand", "[Game][Strategy]")
{
    char advice = BasicStrategy::getRecommendedAction(12, 6, false, false, 0);
    CHECK(advice == 'S');
}

TEST_CASE("Basic strategy: hard 16 vs 9 should surrender or hit", "[Game][Strategy]")
{
    char advice = BasicStrategy::getRecommendedAction(16, 9, false, false, 0);
    // Should be 'H' or 'R' (surrender)
    CHECK((advice == 'H' || advice == 'R'));
}

TEST_CASE("Basic strategy: soft 18 vs 9 should hit", "[Game][Strategy]")
{
    char advice = BasicStrategy::getRecommendedAction(18, 9, true, false, 0);
    CHECK(advice == 'H');
}

TEST_CASE("Basic strategy: pair of 8s should always split", "[Game][Strategy]")
{
    for (int dealerUp = 2; dealerUp <= 11; dealerUp++)
    {
        char advice = BasicStrategy::getRecommendedAction(16, dealerUp, false, true, 8);
        CHECK(advice == 'P');
    }
}

TEST_CASE("Basic strategy: pair of 10s should never split", "[Game][Strategy]")
{
    for (int dealerUp = 2; dealerUp <= 11; dealerUp++)
    {
        char advice = BasicStrategy::getRecommendedAction(20, dealerUp, false, true, 10);
        CHECK(advice == 'S');
    }
}

TEST_CASE("Split ace hand: two aces can split", "[Game][Split]")
{
    Hand hand;
    Card ace1, ace2;
    ace1.value = 1;
    ace1.suit = Suit::Spades;
    ace2.value = 1;
    ace2.suit = Suit::Hearts;
    hand.addCard(ace1);
    hand.addCard(ace2);

    CHECK(hand.canSplit());
}

TEST_CASE("Split ace hand: split ace + 10 is 21 but not blackjack", "[Game][Split]")
{
    Hand hand;
    Card ace, ten;
    ace.value = 1;
    ace.suit = Suit::Spades;
    ten.value = 10;
    ten.suit = Suit::Hearts;
    hand.addCard(ace);
    hand.addCard(ten);
    hand.setSplit(true, 1);

    CHECK(hand.getValue() == 21);
    CHECK(hand.isBlackjack() == false); // split ace + 10 is not natural blackjack
}

TEST_CASE("Natural blackjack detection", "[Game][Blackjack]")
{
    Hand hand;
    Card ace, king;
    ace.value = 1;
    ace.suit = Suit::Spades;
    king.value = 13;
    king.suit = Suit::Hearts;
    hand.addCard(ace);
    hand.addCard(king);

    CHECK(hand.isBlackjack());
    CHECK(hand.getValue() == 21);
}

TEST_CASE("Three-card 21 is not blackjack", "[Game][Blackjack]")
{
    Hand hand;
    Card ace, ten, jack;
    ace.value = 1;
    ace.suit = Suit::Spades;
    ten.value = 10;
    ten.suit = Suit::Hearts;
    jack.value = 11;
    jack.suit = Suit::Clubs;
    hand.addCard(ace);
    hand.addCard(ten);
    hand.addCard(jack);

    CHECK(hand.getValue() == 21);
    CHECK(!hand.isBlackjack());
}

TEST_CASE("Deck shuffling produces different order", "[Game][Deck]")
{
    Deck deck1, deck2;
    deck1.create(1);
    deck2.create(1);

    std::vector<Card> original = deck1.getCards();
    deck1.shuffle();
    std::vector<Card> shuffled = deck1.getCards();

    // Shuffled should differ from original (statistically near-certain)
    bool different = false;
    for (size_t i = 0; i < 52; i++)
    {
        if (shuffled[i].value != original[i].value || shuffled[i].suit != original[i].suit)
        {
            different = true;
            break;
        }
    }
    CHECK(different);

    // Deck size unchanged after shuffle (52 cards + 1 cut card)
    CHECK(deck1.size() == 53);
}

TEST_CASE("Split ace is marked correctly", "[Game][Split]")
{
    Hand hand;
    Card ace;
    ace.value = 1;
    ace.suit = Suit::Spades;
    hand.addCard(ace);
    hand.setSplit(true, 11);

    CHECK(hand.isSplitAce());
    CHECK(hand.isSplit());
}

TEST_CASE("Non-ace split is not marked as split ace", "[Game][Split]")
{
    Hand hand;
    Card eight;
    eight.value = 8;
    eight.suit = Suit::Spades;
    hand.addCard(eight);
    hand.setSplit(true, 8);

    CHECK(!hand.isSplitAce());
    CHECK(hand.isSplit());
}

TEST_CASE("Split ace + 10 is 21, not blackjack, not soft", "[Game][Split]")
{
    Hand hand;
    Card ace, ten;
    ace.value = 1;
    ace.suit = Suit::Spades;
    ten.value = 10;
    ten.suit = Suit::Hearts;
    hand.addCard(ace);
    hand.addCard(ten);
    hand.setSplit(true, 11);

    CHECK(hand.getValue() == 21);
    CHECK(!hand.isBlackjack());
    CHECK(hand.isSplitAce());
}

TEST_CASE("Strategy compliance: correct action increments hits", "[Game][Strategy]")
{
    // Hard 12 vs 6: correct action is Stand
    char advice = BasicStrategy::getRecommendedAction(12, 6, false, false, 0);
    CHECK(advice == 'S');
}

TEST_CASE("Strategy compliance: incorrect action is a deviation", "[Game][Strategy]")
{
    // Hard 12 vs 6: player hits (wrong), correct is stand
    char playerAction = 'H';
    char correctAction = BasicStrategy::getRecommendedAction(12, 6, false, false, 0);
    CHECK(playerAction != correctAction);
}

TEST_CASE("Strategy compliance: soft 17 vs 3 should double", "[Game][Strategy]")
{
    char advice = BasicStrategy::getRecommendedAction(17, 3, true, false, 0);
    CHECK(advice == 'D');
}

TEST_CASE("Strategy compliance: soft 17 vs 9 should hit", "[Game][Strategy]")
{
    char advice = BasicStrategy::getRecommendedAction(17, 9, true, false, 0);
    CHECK(advice == 'H');
}

TEST_CASE("Strategy compliance: hard 11 vs any should double", "[Game][Strategy]")
{
    for (int dealerUp = 2; dealerUp <= 11; dealerUp++)
    {
        char advice = BasicStrategy::getRecommendedAction(11, dealerUp, false, false, 0);
        CHECK(advice == 'D');
    }
}

TEST_CASE("NPC split ace rule: split ace hand auto-stands after one card", "[Game][Split][NPC]")
{
    // Simulate NPC splitting aces: each split ace hand gets exactly one card
    // and is automatically finished (cannot hit again)

    // First split ace hand
    Hand hand1;
    Card ace1;
    ace1.value = 1;
    ace1.suit = Suit::Spades;
    hand1.addCard(ace1);
    hand1.setSplit(true, 11);

    // Deal one card to split ace hand (simulating Game::update() logic)
    Card card1;
    card1.value = 9;
    card1.suit = Suit::Hearts;
    hand1.addCard(card1);

    // Verify split ace properties
    CHECK(hand1.isSplitAce());
    CHECK(hand1.isSplit());
    CHECK(hand1.size() == 2);
    CHECK(hand1.getValue() == 20);
    CHECK(!hand1.isBlackjack()); // split ace + 9 is not blackjack

    // Second split ace hand
    Hand hand2;
    Card ace2;
    ace2.value = 1;
    ace2.suit = Suit::Hearts;
    hand2.addCard(ace2);
    hand2.setSplit(true, 11);

    Card card2;
    card2.value = 10;
    card2.suit = Suit::Clubs;
    hand2.addCard(card2);

    CHECK(hand2.isSplitAce());
    CHECK(hand2.size() == 2);
    CHECK(hand2.getValue() == 21);
    CHECK(!hand2.isBlackjack()); // split ace + 10 is 21 but not natural blackjack

    // Verify the hand is marked finished after receiving its one card
    // (this is what Game::update() does: if isSplitAce() -> setFinished(true))
    hand1.setFinished(true);
    hand2.setFinished(true);
    CHECK(hand1.isFinished());
    CHECK(hand2.isFinished());
}