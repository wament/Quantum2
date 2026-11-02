#pragma once

#include <vector>
#include "Achievement.hpp"  // for Achievement::Id in surfaceNewlyUnlocked()'s parameter type. No Game dep -> no cycle.
#include "GameRecord.hpp"   // for const RoundRecord& in recordHandAchievements()'s parameter type. Pulls only Card/Deck/SideBet, no Game -> no cycle.

// Feature (Game.cpp god-class split: Follow-up #8 settlement/aftermath stage): feed one settled round's human-seat outcome
// into the achievement engine and surface any just-unlocked popup. Game owns one as a value member and delegates
// recordHandAchievements(); the component holds a reference to Game (mirroring RoundDealer / SessionLifecycle)
// the real player/bankroll/deviation/achievement-manager state, while all mutable game state stays in Game. This keeps
// achievement recording byte-for-byte identical to before the split and makes both paths exercisable headlessly through Game's
// delegating methods. Forward-declared Game matches the HandEvaluator/RoundDealer pattern: only a reference is held here
// complete type is needed solely in AchievementFeeder.cpp.
class Game;

class AchievementFeeder
{
  public:
    AchievementFeeder(Game& game);

    // Feed one settled round's human-seat outcome into the achievement engine and set a HUD popup for any just-unlocked one. The
    // human seat is index 0; a split yields multiple seat-0 outcomes on the record
    // from the settled round; bankroll is read live. Shoe-end achievements still resolve in Game::resetShoeAnalytics() via
    // AchievementManager::onNewShoe(), not here. Moved verbatim from Game::recordHandAchievements() (Follow-up #8).
    void recordHandAchievements(const RoundRecord& round);

    // Given the just-unlocked achievement ids, set the HUD popup to the highest-priority one (first in definition order) by name.
    // Pure lookup over the manager's fixed name table + a target string; needs no friendship and is safe at either call site
    // (Game::resetShoeAnalytics() after onNewShoe(), and recordHandAchievements() below). Lifted out of Game.cpp
    // share one implementation.
    void surfaceNewlyUnlocked(const std::vector<Achievement::Id>& newly);

  private:
    Game& game;
};
