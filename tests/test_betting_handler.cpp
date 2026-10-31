#include <catch2/catch_all.hpp>
#include "BettingHandler.hpp"
#include "BasicStrategy.hpp"
#include "Deck.hpp"

// ---------------------------------------------------------------------------
// Betting core: the pure math extracted from Game into a composed BettingHandler.
// These functions have no SFML dependency, so each test drives BettingHandler directly and asserts it returns exactly
// what BasicStrategy computes — the same calls Game::activeTrueCount / hiLoTrueCount / calculateOptimalBet made before
// the split. Equality here locks behavior preservation at the unit level; the full suite still guards integration.
// ---------------------------------------------------------------------------

TEST_CASE("BettingHandler::activeTrueCount returns the raw running count under Reko", "[Betting][Handler]")
{
    BettingHandler betting;
    Deck deck;
    deck.create(6); // 312-card shoe, non-empty

    CHECK(betting.activeTrueCount(deck, 0, BasicStrategy::CountingStyle::REKO) == 0.0f);
    CHECK(betting.activeTrueCount(deck, 7, BasicStrategy::CountingStyle::REKO) == 7.0f);
    CHECK(betting.activeTrueCount(deck, -5, BasicStrategy::CountingStyle::REKO) == -5.0f);
}

TEST_CASE("BettingHandler::activeTrueCount normalizes by decks remaining under Hi-Lo", "[Betting][Handler]")
{
    BettingHandler betting;
    Deck deck;
    deck.create(6);
    const int cards = static_cast<int>(deck.size());

    // Matches the exact formula Game used before the split (computeTrueCount(running, cardsRemaining)).
    CHECK(betting.activeTrueCount(deck, 12, BasicStrategy::CountingStyle::HI_LO)
          == BasicStrategy::computeTrueCount(12, cards));
}

TEST_CASE("BettingHandler::hiLoTrueCount uses Hi-Lo buffer when active, background otherwise", "[Betting][Handler]")
{
    BettingHandler betting;
    Deck deck;
    deck.create(6);

    const int hilo = 9;
    const int background = -4;

    // Active system is Hi-Lo -> the Hi-Lo buffer drives calibration.
    CHECK(betting.hiLoTrueCount(deck, hilo, background, BasicStrategy::CountingStyle::HI_LO)
          == BasicStrategy::computeTrueCount(hilo, static_cast<int>(deck.size())));
    // Display set to Reko -> edge model still wants Hi-Lo points, so the background buffer is used.
    CHECK(betting.hiLoTrueCount(deck, hilo, background, BasicStrategy::CountingStyle::REKO)
          == BasicStrategy::computeTrueCount(background, static_cast<int>(deck.size())));
}

TEST_CASE("BettingHandler::calculateOptimalBet routes Reko vs Hi-lo patterns", "[Betting][Handler]")
{
    BettingHandler betting;
    const int base = 10; // one unit == the table minimum

    // Reko: tiered unit pattern.
    CHECK(betting.calculateOptimalBet(2.4f, 0.33f, 100000, base, 100000, BasicStrategy::CountingStyle::REKO)
          == BasicStrategy::calculateRekoBet(2.4f, base, base, 100000));
    CHECK(betting.calculateOptimalBet(3.7f, 0.33f, 100000, base, 100, BasicStrategy::CountingStyle::REKO)
          == BasicStrategy::calculateRekoBet(3.7f, base, base, 100));

    // Hi-Lo: Kelly criterion.
    CHECK(betting.calculateOptimalBet(2.0f, 0.33f, 50000, base, 100000, BasicStrategy::CountingStyle::HI_LO)
          == BasicStrategy::calculateKellyBet(2.0f, 0.33f, 50000, base, 100000));
}

TEST_CASE("BettingHandler delegation preserves Game's exact pre-split values", "[Betting][Handler]")
{
    // End-to-end sanity: for a representative true count the handler returns exactly what BasicStrategy computes, so any
    // drift introduced by the split is caught even if Game's delegators themselves change.
    BettingHandler betting;
    Deck deck;
    deck.create(6);
    const int cards = static_cast<int>(deck.size());

    CHECK(betting.calculateOptimalBet(BasicStrategy::computeTrueCount(15, cards), 0.33f, 25000, 10, 5000,
                                      BasicStrategy::CountingStyle::HI_LO)
          == BasicStrategy::calculateKellyBet(BasicStrategy::computeTrueCount(15, cards), 0.33f, 25000, 10, 5000));
}
