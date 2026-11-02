#include "AchievementFeeder.hpp"
#include "GameRecord.hpp"  // handIsNaturalBJ: single record-derived source of truth for every blackjack tally
#include "Game.hpp"  // complete type: RoundRecord, HandOutcome, Hand: needed to qualify game.* members used below.

AchievementFeeder::AchievementFeeder(Game& game) : game(game) {}

// feed one settled round's human-seat outcome into the achievement engine and surface any
// just-unlocked popup. The human seat is index 0; a split yields multiple seat-0 outcomes on the record
// OR'd. True count comes straight from the settled round; bankroll is read live. Shoe-end achievements resolve in
// Game::resetShoeAnalytics() via AchievementManager::onNewShoe(), not here. Moved verbatim from Game::recordHandAchievements().
void AchievementFeeder::recordHandAchievements(const RoundRecord& round)
{
    AchievementManager::HandEvent e;
    int bet = 0;
    int seat0Outcomes = 0;
    for (const auto& h : round.playerHands)
    {
        if (h.playerIndex != 0)
            continue;
        ++seat0Outcomes;
        const Hand hand(h.cards);
        e.isBlackjack = e.isBlackjack || handIsNaturalBJ(h.cards);   // same record-derived predicate computeCareerStats uses
        e.busted = e.busted || hand.getValue() > 21;
        e.wonAny = e.wonAny || h.won;
        bet += h.wager;
    }
    e.splitHand = seat0Outcomes > 1;                   // a split produces more than one seat-0 outcome on the record
    e.followedStrategy = (game.deviationsThisRound_ == 0); // whole round matched basic strategy if no deviation was logged
    e.bet = bet;
    e.bankrollAfter = game.players.empty() ? 0.0f : static_cast<float>(game.players[0].getBankroll());
    e.positiveTrueCount = round.trueCount > 0.0f;

    std::vector<Achievement::Id> newly = game.achievementManager_.recordHand(e);
    surfaceNewlyUnlocked(newly);
}

// Given the just-unlocked achievement ids, set the HUD popup to the highest-priority one (first in definition order) by name.
// Pure lookup over the manager's fixed name table + a target string; needs no friendship. Lifted verbatim from Game.cpp
// the recordHandAchievements() path and Game::resetShoeAnalytics()'s onNewShoe() call share this one implementation.
void AchievementFeeder::surfaceNewlyUnlocked(const std::vector<Achievement::Id>& newly)
{
    if (newly.empty())
        return;
    for (const auto& def : game.achievementManager_.all())
        for (auto id : newly)
            if (def.id == id)
            {
                game.hudState.pendingAchievementName = def.name;
                return;
            }
}
