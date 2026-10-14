#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <BasicStrategy.hpp>
#include <Game.hpp>
#include <Player.hpp>

using namespace Constants;
using Catch::Approx;

// ---------------------------------------------------------------------------
// Feature 5 (Reko counting): the "true count is the running count" rule. The
// active-count path (activeTrueCount) must return the raw running count for Reko
// and a decks-normalized true count for Hi-Lo; toggleCountingStyle swaps which
// buffer feeds it, so this exercises production's real counting bookkeeping.
// ---------------------------------------------------------------------------

TEST_CASE("Reko true count is the raw running count; Hi-Lo normalizes by decks", "[Feature5][Counting]")
{
    Game game(true);

    // The initial deal draws four cards (player, dealer hole, player, dealer up) = six(6), seven(7), eight(8) and one
    // filler(8). In Reko those sum to reko(6)+reko(7)+reko(8)+reko(8)=2; in Hi-Lo hilo(6)+hilo(7)+hilo(8)+hilo(8)=1.
    // The rest of the deck is filler so draw() never runs dry and stays untouched until after these assertions run.
    Card six;   six.value = 6; six.suit = Suit::Hearts;
    Card seven; seven.value = 7; seven.suit = Suit::Spades;
    Card eight; eight.value = 8; eight.suit = Suit::Diamonds;
    std::vector<Card> deck = {six, seven, eight};
    for (int i = 0; i < 200; ++i)
    {
        Card filler;
        filler.value = 8; // neutral in BOTH systems (Reko 0, Hi-Lo 0), so it never perturbs either count
        filler.suit = Suit::Clubs;
        deck.push_back(filler);
    }

    game.setDeck(deck);
    game.startTestRound();

    // Default counting style is Reko. The raw running count after the initial deal is reko(6)+reko(7)=2 (the dealer's
    // 8 is neutral in both systems). activeTrueCount must return this RAW value, not divide it by decks remaining.
    const float reko = game.activeTrueCount();
    CHECK(reko == Approx(2.0f));

    // Toggling to Hi-Lo swaps in the hi-lo buffer (raw sum of the two player cards = hilo(6)+hilo(7)=1). The active
    // true count must now be normalized by decks remaining, so it can no longer equal the raw running count of 1.
    game.toggleCountingStyle();
    const float hilo = game.activeTrueCount();
    CHECK(hilo != Approx(1.0f));
}

// ---------------------------------------------------------------------------
// Feature 3 (Advanced Analytics): confirmBet records each wager into the session
// bet-distribution histogram that backs the bet-histogram dashboard chart.
// ---------------------------------------------------------------------------

TEST_CASE("confirmBet records the wager into the session bet-distribution histogram", "[Feature3][Analytics]")
{
    Game game(true);

    // Any deck with enough cards for two deals; card values are irrelevant to this assertion.
    std::vector<Card> deck;
    for (int i = 0; i < 210; ++i)
    {
        Card c;
        c.value = 5;
        c.suit = Suit::Hearts;
        deck.push_back(c);
    }
    game.setDeck(deck);
    game.startTestRound();

    // startTestRound sets up a single minimum bet but does not confirm/place it; that is confirmBet()'s job.
    CHECK(game.testPlayer(0).hands[0].getBet() == Constants::MINIMUM_BET);

    game.confirmBet();

    const PlayerStats& stats = game.testPlayer(0).getStats();
    CHECK(stats.betsPlaced == 1);
    CHECK(stats.betDistribution.size() == 1);
    CHECK(stats.betDistribution.at(Constants::MINIMUM_BET) == 1);
}
