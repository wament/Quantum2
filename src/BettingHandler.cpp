#include "BettingHandler.hpp"

// These methods are faithful re-exports of the logic that lived in Game::activeTrueCount / hiLoTrueCount /
// calculateOptimalBet: they take shared game state as parameters (Game owns players/shoe/count) and apply the same
// counting-system rules, so betting behavior is byte-for-byte identical to before the split.

float BettingHandler::activeTrueCount(const Deck& deck, int runningCount, BasicStrategy::CountingStyle style) const
{
    if (style == BasicStrategy::CountingStyle::REKO)
        return static_cast<float>(runningCount);

    return BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size()));
}

float BettingHandler::hiLoTrueCount(const Deck& deck, int hiloRunning, int backgroundRunning, BasicStrategy::CountingStyle style) const
{
    // Hi-Lo is calibrated in its own buffer when active; otherwise the background (Reko) buffer carries that value.
    const int hilo = (style == BasicStrategy::CountingStyle::HI_LO) ? hiloRunning : backgroundRunning;
    return BasicStrategy::computeTrueCount(hilo, static_cast<int>(deck.size()));
}

int BettingHandler::calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet,
                                        BasicStrategy::CountingStyle style) const
{
    if (style == BasicStrategy::CountingStyle::REKO)
        return BasicStrategy::calculateRekoBet(trueCount, minBet, minBet, maxBet);

    return BasicStrategy::calculateKellyBet(trueCount, kellyFraction, bankroll, minBet, maxBet);
}
