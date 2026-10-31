#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include "BasicStrategy.hpp"
#include "Tournament.hpp"

using namespace BasicStrategy;

namespace
{

    // A full 52-card shoe rotated by `offset`: deterministic per offset (so the same offset always yields the same
    // tournament), varied across offsets. Used to prove the winner-selection invariant over many distinct deals.
    std::vector<Card> rotatedDeck(int offset)
    {
        const int rank[13] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
        std::vector<Card> deck;
        deck.reserve(52);
        for (int i = 0; i < 52; ++i)
        {
            int idx = (i + offset) % 52;
            deck.push_back(test_support::make_card(rank[idx % 13], static_cast<Suit>(idx / 13)));
        }
        return deck;
    }
}

// ---------------------------------------------------------------------------
// Feature 9 (Tournament Mode): the logic layer is tested headless-first, exactly mirroring test_strategy_trainer.cpp
// - pure play-out + winner selection via an injected shoe and playHand(), before any UI. No SFML in these cases; they
// run over the Graphics-only Blackjack2Tests binary.
// ---------------------------------------------------------------------------

TEST_CASE("Tournament: start creates equal starting chips and honours config", "[Tournament][Setup]")
{
    Tournament t;
    Tournament::TournamentConfig cfg;
    cfg.handsToPlay = 7;
    cfg.startingChips = 500;
    cfg.blindBet = 25;
    cfg.seatNames = {"Alice", "Bob"};

    t.start(cfg, 6); // shuffled shoe (no injection here)

    CHECK(t.seatCountForTest() == 2);
    CHECK(t.handsToPlayForTest() == 7);
    CHECK(t.chipsForTest(0) == 500);
    CHECK(t.chipsForTest(1) == 500);
    CHECK(!t.isCompleteForTest());
}

TEST_CASE("Tournament: winner is the seat with the highest chips", "[Tournament][Winner]")
{
    Tournament t;
    Tournament::TournamentConfig cfg;
    cfg.handsToPlay = 1;
    cfg.startingChips = 100;
    cfg.blindBet = 10;
    cfg.seatNames = {"Alice", "Bob"};

    // Natural DEAL order (first drawn first, after setDeck's internal reversal). Round-robin two cards each seat,
    // then the dealer. Both seats resolve on their initial two cards only - Alice on a natural 21 (auto-stand), Bob
    // on a hard 16 that stands against the dealer's 3 (HARD[16][3] = 'S' in BasicStrategy) - so neither draws an extra
    // card and exactly seven cards are consumed (no overdraw). Alice's natural 21 beats the dealer's non-natural 19;
    // Bob's hard 16 loses to it. This makes the winner deterministic without depending on advisor table nuances.
    std::vector<Card> shoe = {
        test_support::make_card(1, Suit::Hearts),    // d1 -> Alice card 1 (Ace)
        test_support::make_card(13, Suit::Spades),   // d2 -> Bob   card 1 (King=10)
        test_support::make_card(12, Suit::Clubs),    // d3 -> Alice card 2 (Queen=10) -> natural blackjack 21
        test_support::make_card(6, Suit::Diamonds),  // d4 -> Bob   card 2 (6) -> hard 16
        test_support::make_card(3, Suit::Hearts),    // d5 -> dealer up-card; Bob's 16 stands vs a 3
        test_support::make_card(6, Suit::Spades),    // d6 -> dealer hole card: 3+6 = 9, hits
        test_support::make_card(13, Suit::Clubs),    // d7 -> dealer hit (King=10): 19, stands on all-17 (not busted)
    };
    t.setDeck(shoe);
    t.start(cfg, 0); // injected deck persists; deckCount is ignored when one was set
    CHECK(t.seatCountForTest() == 2);

    t.playHand();

    // Alice's natural 21 pays 3:2 (+15) -> 115; Bob's hard 16 loses to the dealer's 19 (-10) -> 90. The winner is the
    // seat with the strictly higher chip count.
    CHECK(t.isCompleteForTest());
    CHECK(t.chipsForTest(0) == 115);
    CHECK(t.chipsForTest(1) == 90);
    CHECK(t.winnerIndexForTest() == 0);
    CHECK(t.winnerNameForTest() == "Alice");
}

TEST_CASE("Tournament: declared winner always has the highest chip count across varied shoes", "[Tournament][Winner]")
{
    for (int offset = 0; offset < 24; ++offset)
    {
        Tournament t;
        Tournament::TournamentConfig cfg;
        cfg.handsToPlay = 3; // bounded so the 52-card shoe never runs out mid-hand
        cfg.startingChips = 1000;
        cfg.blindBet = 10;
        cfg.seatNames = {"P1", "P2"};
        t.setDeck(rotatedDeck(offset));
        t.start(cfg, 0);

        while (!t.isCompleteForTest())
            t.playHand();

        const int winner = t.winnerIndexForTest();
        CHECK(winner >= 0);
        CHECK(winner < static_cast<int>(t.seatCountForTest()));
        // The declared winner must be a global maximum of the final chip counts.
        for (std::size_t i = 0; i < t.seatCountForTest(); ++i)
            CHECK(t.chipsForTest(winner) >= t.chipsForTest(i));
    }
}

TEST_CASE("Tournament: identical shoe produces the same winner every run", "[Tournament][Determinism]")
{
    auto run = []() {
        Tournament t;
        Tournament::TournamentConfig cfg;
        cfg.handsToPlay = 5;
        cfg.startingChips = 1000;
        cfg.blindBet = 10;
        cfg.seatNames = {"A", "B", "C"};
        t.setDeck(rotatedDeck(5));
        t.start(cfg, 0);
        while (!t.isCompleteForTest())
            t.playHand();
        return t.winnerIndexForTest();
    };

    CHECK(run() == run());
}

TEST_CASE("Tournament: seats that cannot afford the blind bet sit out unchanged", "[Tournament][Betting]")
{
    Tournament t;
    Tournament::TournamentConfig cfg;
    cfg.handsToPlay = 3;
    cfg.startingChips = 5; // below the blind bet -> nobody can place a bet this run
    cfg.blindBet = 10;
    cfg.seatNames = {"X", "Y"};
    t.setDeck(rotatedDeck(2));
    t.start(cfg, 0);

    while (!t.isCompleteForTest())
        t.playHand();

    CHECK(t.chipsForTest(0) == 5); // untouched because the seat sat out every round
    CHECK(t.chipsForTest(1) == 5);
    CHECK(t.isCompleteForTest());
    CHECK(t.winnerIndexForTest() == 0); // a tie is broken to the lowest index
}
