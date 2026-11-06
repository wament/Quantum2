#include <catch2/catch_all.hpp>
#include <BasicStrategy.hpp>
#include <Game.hpp>
#include <GameRules.hpp>

// ---------------------------------------------------------------------------
// Feature 5 (Reko counting): the per-deck starting-offset table from the roadmap
// and the Reko betting pattern. These are pure functions, so they are tested
// directly against BasicStrategy without driving a full round.
// ---------------------------------------------------------------------------

TEST_CASE("Reko starting offset follows the roadmap deck-count table", "[Reko][Counting]")
{
    CHECK(BasicStrategy::rekoStartingOffset(1) == -1);
    CHECK(BasicStrategy::rekoStartingOffset(2) == -5);
    CHECK(BasicStrategy::rekoStartingOffset(4) == -12);
    CHECK(BasicStrategy::rekoStartingOffset(6) == -20);
    CHECK(BasicStrategy::rekoStartingOffset(8) == -27);
}

TEST_CASE("Reko starting offset brackets deck counts outside the table", "[Reko][Counting]")
{
    // Nearest lower bracket: 3 behaves like 4 (-12), 5 like 6 (-20).
    CHECK(BasicStrategy::rekoStartingOffset(3) == -12);
    CHECK(BasicStrategy::rekoStartingOffset(5) == -20);
    // Counts beyond 8 stay capped at the 8-deck value.
    CHECK(BasicStrategy::rekoStartingOffset(9) == -27);
    CHECK(BasicStrategy::rekoStartingOffset(12) == -27);
}

TEST_CASE("Reko betting pattern: TC <= 0 bets one unit", "[Reko][Betting]")
{
    const int base = 10; // one unit = the table minimum
    CHECK(BasicStrategy::calculateRekoBet(0.0f, base, 10, 1000) == base);       // exactly 0 -> 1 unit
    CHECK(BasicStrategy::calculateRekoBet(-3.5f, base, 10, 1000) == base);      // negative -> 1 unit
}

TEST_CASE("Reko betting pattern: 0 < TC < 2 bets two units", "[Reko][Betting]")
{
    const int base = 10;
    CHECK(BasicStrategy::calculateRekoBet(0.1f, base, 10, 1000) == 2 * base);
    CHECK(BasicStrategy::calculateRekoBet(1.9f, base, 10, 1000) == 2 * base);
}

TEST_CASE("Reko betting pattern: TC > 2 bets floor(1 + TC) units", "[Reko][Betting]")
{
    const int base = 10;
    CHECK(BasicStrategy::calculateRekoBet(2.0f, base, 10, 1000) == 3 * base);   // floor(1+2)=3 units
    CHECK(BasicStrategy::calculateRekoBet(2.4f, base, 10, 1000) == 3 * base);   // floor(1+2.4)=3 units
    CHECK(BasicStrategy::calculateRekoBet(3.7f, base, 10, 1000) == 4 * base);   // floor(1+3.7)=4 units
}

TEST_CASE("Reko betting pattern clamps to table limits", "[Reko][Betting]")
{
    const int base = 10;
    // 50 units would be 500, but the max cap is 100.
    CHECK(BasicStrategy::calculateRekoBet(49.0f, base, 10, 100) == 100);
    // A tiny positive TC still bets two units (20), never below the minimum.
    CHECK(BasicStrategy::calculateRekoBet(0.5f, 20, 20, 500) == 40);
}

// ---------------------------------------------------------------------------
// B4: the Reko seed must follow the *live* deck-count rule, not the compile-time
// Constants::NUM_DECKS default. A shoe dealt at Decks=1 should start at -1, an
// 8-deck shoe at -27 — today both start at -20 because Game.cpp seeds from the
// constant. These drive a real headless Game through startNewShoeForTest().
// ---------------------------------------------------------------------------

TEST_CASE("Reko seed follows the live deck-count rule for every shoe size", "[Reko][DeckCount][B4]")
{
    Game game(true); // countingStyle defaults to REKO, so runningCount tracks the per-deck seed

    const std::vector<std::pair<int,int>> roadmap = {{1,-1},{2,-5},{4,-12},{6,-20},{8,-27}};
    for (const auto& entry : roadmap)
    {
        GameRules rules;
        rules.numDecks = entry.first;
        game.setGameRulesForTest(rules);
        game.startNewShoeForTest();
        CHECK(game.runningCountForTest() == entry.second);
    }
}

// ---------------------------------------------------------------------------
// B4 (Deck): a mid-shoe reshuffle must preserve the shoe it was created with,
// not rebuild as if it were the compile-time default of six decks.
// ---------------------------------------------------------------------------

TEST_CASE("Deck::reshuffleWithRemaining preserves a non-6-deck shoe size", "[Deck][B4]")
{
    Deck d;
    d.create(1); // single-deck shoe: reshuffle must stay near 52 cards, not blow up to six decks

    std::vector<Card> offTable = {{1, Suit::Hearts}, {2, Suit::Spades}};
    d.reshuffleWithRemaining(offTable);

    // Rebuilt from a one-deck shoe: 52 real cards, minus the two dealt (removed by value+suit), plus the cut card.
    CHECK(d.size() == 52 + 1 - offTable.size());
}
