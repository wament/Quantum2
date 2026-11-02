#pragma once

#include <vector>
#include "Deck.hpp"
#include "BasicStrategy.hpp"

// Betting core (Feature): pure-logic betting sizing + true-count derivation, with no SFML dependency. Game owns one as
// a std::unique_ptr and delegates its public methods
// only the math, while all mutable game state (players/shoe/count) stays in Game and is passed in per call. This keeps
// betting behavior byte-for-byte identical to before the split and makes each function directly unit-testable.
class BettingHandler
{
  public:
    // Active true count for the displayed system: Reko returns the raw running count ("the true count is the running
    // count"); Hi-Lo normalizes by decks remaining in the shoe. Betting paths use this
    // its own true count (mirrors Game::activeTrueCount, which it replaces).
    float activeTrueCount(const Deck& deck, int runningCount, BasicStrategy::CountingStyle style) const;

    // The Hi-Lo true count regardless of which system is displayed: the Hi-Lo buffer when Hi-Lo is active, otherwise
    // the background Reko buffer (both tallied every draw). The edge model is calibrated in Hi-Lo true-count points.
    float hiLoTrueCount(const Deck& deck, int hiloRunning, int backgroundRunning, BasicStrategy::CountingStyle style) const;

    // Optimal bet for the active system's pattern: Reko steps by table-minimum units (one unit = the minimum); Hi-Lo
    // uses the Kelly criterion. Mirrors Game::calculateOptimalBet, which it replaces.
    int calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet,
                            BasicStrategy::CountingStyle style) const;
};
