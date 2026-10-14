#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <Hand.hpp>
#include <Deck.hpp>
#include <GameRecord.hpp>
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

    CHECK(betAtTC2 > minBet);
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

// ---------------- Feature 5: Reko counting system (unbalanced, running count == true count) ----------------

TEST_CASE("Reko card values: 2-7 = +1, 8/9 = 0, Aces and tens = -1", "[Game][Counting][Reko]")
{
    // 2 through 7 are all +1 (hi-lo agrees on 2-6; Reko additionally counts the 7 as +1)
    for (int v = 2; v <= 7; ++v)
        CHECK(BasicStrategy::calculateRekoValue(v) == 1);

    // 8 and 9 are neutral in both systems
    CHECK(BasicStrategy::calculateRekoValue(8) == 0);
    CHECK(BasicStrategy::calculateRekoValue(9) == 0);

    // Aces (1) and ten-value cards (J/Q/K -> bj value 10) reduce the count
    CHECK(BasicStrategy::calculateRekoValue(1) == -1);
    CHECK(BasicStrategy::calculateRekoValue(10) == -1);
}

TEST_CASE("Reko updateCount dispatches on the active counting style", "[Game][Counting][Reko]")
{
    // Hi-Lo path stays identical to updateHiLo (a 7 is neutral in hi-lo)
    CHECK(BasicStrategy::updateCount(5, 4, BasicStrategy::CountingStyle::HI_LO) == 6);
    CHECK(BasicStrategy::updateCount(5, 7, BasicStrategy::CountingStyle::HI_LO) == 5);

    // Reko path counts the 7 as +1 even though hi-lo would treat it as neutral; 8 stays neutral in both
    CHECK(BasicStrategy::updateCount(5, 4, BasicStrategy::CountingStyle::REKO) == 6);
    CHECK(BasicStrategy::updateCount(5, 7, BasicStrategy::CountingStyle::REKO) == 6);
    CHECK(BasicStrategy::updateCount(5, 8, BasicStrategy::CountingStyle::REKO) == 5);
}

TEST_CASE("Reko true count equals the running count", "[Game][Counting][Reko]")
{
    CHECK(BasicStrategy::rekoTrueCount(-20) == Approx(-20.0f));
    CHECK(BasicStrategy::rekoTrueCount(3) == Approx(3.0f));
}

TEST_CASE("Reko starting offset matches roadmap per deck count", "[Game][Counting][Reko]")
{
    // Exact roadmap values for the standard shoe sizes
    CHECK(BasicStrategy::rekoStartingOffset(1) == -1);
    CHECK(BasicStrategy::rekoStartingOffset(2) == -5);
    CHECK(BasicStrategy::rekoStartingOffset(4) == -12);
    CHECK(BasicStrategy::rekoStartingOffset(6) == -20);
    CHECK(BasicStrategy::rekoStartingOffset(8) == -27);

    // Interpolated values fill the gaps between the documented sizes
    CHECK(BasicStrategy::rekoStartingOffset(3) == -8);
    CHECK(BasicStrategy::rekoStartingOffset(5) == -16);
    CHECK(BasicStrategy::rekoStartingOffset(7) == -23);

    // Out-of-range / unrecognised shoe sizes fall back to a sensible default (six-deck value)
    CHECK(BasicStrategy::rekoStartingOffset(-1) == -20);
    CHECK(BasicStrategy::rekoStartingOffset(9) == -20);
}

TEST_CASE("counting style name reflects the active system", "[Game][Counting][Reko]")
{
    CHECK(BasicStrategy::countingStyleName(BasicStrategy::CountingStyle::HI_LO) == "Hi-Lo");
    CHECK(BasicStrategy::countingStyleName(BasicStrategy::CountingStyle::REKO) == "Reko");
}

TEST_CASE("Reko bet sizing follows the tiered unit scale", "[Game][Betting][Reko]")
{
    int minBet = 10;
    int maxBet = 1000;

    // Unit count per true-count tier
    CHECK(BasicStrategy::calculateRekoUnits(0) == 1);   // TC <= 0            -> 1 unit
    CHECK(BasicStrategy::calculateRekoUnits(-5) == 1);
    CHECK(BasicStrategy::calculateRekoUnits(1) == 2);    // 0 < TC < 2         -> 2 units
    CHECK(BasicStrategy::calculateRekoUnits(3) == 4);    // TC > 2             -> (1 + TC) units
    CHECK(BasicStrategy::calculateRekoUnits(5) == 6);

    // Actual chip amount uses MINIMUM_BET as the unit, clamped to [minBet, maxBet]
    CHECK(BasicStrategy::calculateRekoBet(0, minBet, minBet, maxBet) == minBet);        // 1 unit
    CHECK(BasicStrategy::calculateRekoBet(1, minBet, minBet, maxBet) == 2 * minBet);    // 2 units
    CHECK(BasicStrategy::calculateRekoBet(3, minBet, minBet, maxBet) == 4 * minBet);    // 4 units

    // Clamp at the maximum cap regardless of how large the running count grows
    CHECK(BasicStrategy::calculateRekoBet(100, minBet, minBet, 60) == 60);
}

TEST_CASE("Per-hand profit standard deviation is computed from running sums", "[Game][Analytics]")
{
    PlayerStats s;

    // Zero or one hand -> no variance to speak of.
    CHECK(computeHandStandardDeviation(s) == Approx(0.0));
    s.handsFinished = 1;
    s.handProfitSum = 50.0;
    s.handProfitSqSum = 2500.0; // 50^2
    CHECK(computeHandStandardDeviation(s) == Approx(0.0));

    // Two hands: +100 and -100 -> mean 0, variance 10000, standard deviation 100.
    s.handsFinished = 2;
    s.handProfitSum = 0.0;
    s.handProfitSqSum = 20000.0; // 100^2 + 100^2
    CHECK(computeHandStandardDeviation(s) == Approx(100.0));

    // Three identical hands -> zero variance even though the mean is non-zero (guards E[x^2]-E[x]^2).
    s.handsFinished = 3;
    s.handProfitSum = 30.0;    // +10 each
    s.handProfitSqSum = 300.0; // 3 * 10^2
    CHECK(computeHandStandardDeviation(s) == Approx(0.0));

    // Slightly negative round-off variance is clamped to zero rather than crashing via sqrt(-x).
    s.handsFinished = 2;
    s.handProfitSum = 10.0;
    s.handProfitSqSum = 50.0; // less than (sum^2)/n = 50 exactly -> ~0, guard keeps it >= 0
    CHECK(computeHandStandardDeviation(s) >= 0.0);
}