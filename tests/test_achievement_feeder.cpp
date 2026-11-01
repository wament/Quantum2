#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include "Game.hpp"
#include "Achievement.hpp"
#include "PlayerProfile.hpp"

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives here (declared in Constants.hpp)

namespace {
// Two-card hand fixture used to build a natural 21, mirroring test_achievements.cpp.
std::vector<Card> twoCards(int v0, int v1)
{
    return {test_support::make_card(v0, Suit::Spades), test_support::make_card(v1, Suit::Hearts)};
}
}  // anonymous namespace: internal linkage keeps this fixture local so it does not collide with the identically named
// helper defined at file scope in test_achievements.cpp (both compile into one Catch2 executable).

// ---------------------------------------------------------------------------
// Follow-up #8 (Game.cpp god-class split, settlement/aftermath stage): Game::recordHandAchievements() now delegates to the
// composed AchievementFeeder helper. These cases lock that delegation path end-to-end — a direct recordHandAchievements() call
// (human-seat-only aggregation + split/natural detection), a live deal->settle cycle through the headless round machine, and
// the shared surfaceNewlyUnlocked() popup path used by a shoe boundary — all byte-for-byte identical to before the split. They
// mirror test_achievements.cpp integration cases as a regression lock for the relocation.
// ---------------------------------------------------------------------------

TEST_CASE("#8 delegation: recordHandAchievements records the human seat only; split + natural detected", "[Settlement][AchievementFeeder]")
{
    Game game(true);   // headless, empty manager at start
    CHECK(game.achievementsForTest().unlockedCount() == 0);

    RoundRecord round;
    {
        HandOutcome h;
        h.playerIndex = 0;
        h.wager = 50;
        h.won = true;
        h.cards = twoCards(1, 10);   // Ace + ten -> natural 21 (Hand::isBlackjack)
        round.playerHands.push_back(h);
    }
    {
        HandOutcome npc;          // an NPC seat with its own natural must be ignored for the human's tallies
        npc.playerIndex = 1;
        npc.won = true;
        npc.cards = twoCards(1, 10);
        round.playerHands.push_back(npc);
    }

    game.recordHandAchievements(round);
    auto& mgr = game.achievementsForTest();
    CHECK(mgr.isUnlocked(Achievement::Id::FirstDeal));
    CHECK(mgr.isUnlocked(Achievement::Id::Natural21));   // detected from the human seat's cards only
    CHECK(mgr.careerBlackjacks() == 1);                  // the NPC natural did not count toward the human

    // First Deal (id 0) is defined before Natural 21, so it is the highest-priority just-unlocked one and drives the popup.
    CHECK(game.hudStateForTest().pendingAchievementName == "First Deal");

    // A split producing two human-seat outcomes is recognised as a split; combined with a win it unlocks Split Success.
    RoundRecord round2;
    {
        HandOutcome a; a.playerIndex = 0; a.wager = 25; a.won = true; a.cards = twoCards(8, 8);
        round2.playerHands.push_back(a);
        HandOutcome b; b.playerIndex = 0; b.wager = 25; b.won = false; b.cards = twoCards(3, 9);
        round2.playerHands.push_back(b);   // second hand of the split -> two seat-0 outcomes => a split
    }
    game.recordHandAchievements(round2);
    CHECK(mgr.isUnlocked(Achievement::Id::SplitSuccess));
}

TEST_CASE("#8 delegation: a real deal->settle cycle feeds the achievement engine through AchievementFeeder", "[Settlement][AchievementFeeder]")
{
    Game game(true);
    // Player is dealt a hard 15 (7 + 8) vs a dealer 2 -> reaches PlayerTurn; standing then runs evaluateHands().
    test_support::start_round(game,{test_support::make_card(7, Suit::Spades),   // player card 0
                    test_support::make_card(2, Suit::Hearts),   // dealer up-card
                    test_support::make_card(8, Suit::Diamonds), // player card 1 -> 15
                    test_support::make_card(3, Suit::Clubs),
                    test_support::make_card(4, Suit::Spades),
                    test_support::make_card(5, Suit::Hearts),
                    test_support::make_card(6, Suit::Diamonds)});

    CHECK(game.achievementsForTest().unlockedCount() == 0);   // nothing unlocked before the round settles

    game.stand();
    game.deltaTime = 100.0f;                                // elapse dealer reveal pacing timers
    test_support::advanceToDealerTurn(game);                // all hands resolved -> DealerTurn
    for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        test_support::tickFrame(game, 100.0f);              // dealer draws/stands then evaluateHands settles the round

    CHECK(game.roundStateForTest() == RoundState::Cleanup);
    // Any settled hand unlocks First Deal, proving the whole deal -> settle -> recordHandAchievements(feeder) chain works live.
    CHECK(game.achievementsForTest().isUnlocked(Achievement::Id::FirstDeal));
}

TEST_CASE("#8 delegation: a shoe boundary surfaces the just-unlocked popup via AchievementFeeder", "[Settlement][AchievementFeeder]")
{
    Game game(true);
    // A clean, strategy-following round (no deviation) so the shoe-end Deviation Free resolves at the next boundary. Bet 0 keeps
    // bankroll flat so Perfect Shoe / Comeback King stay off and only Deviation Free unlocks -> a deterministic popup target. This
    // locks the modified resetShoeAnalytics() call site, which routes its onNewShoe() result through the composed feeder.
    RoundRecord round;
    {
        HandOutcome h;
        h.playerIndex = 0;
        h.wager = 0;
        h.won = true;
        h.cards = twoCards(1, 10);   // natural 21
        round.playerHands.push_back(h);
    }

    game.recordHandAchievements(round);    // career First Deal + Natural 21 unlock (per-hand, inline)

    game.startNewShoe();                   // resetShoeAnalytics() -> AchievementFeeder::surfaceNewlyUnlocked(onNewShoe())

    CHECK(game.achievementsForTest().isUnlocked(Achievement::Id::DeviationFree));
    // surfaceNewlyUnlocked preserves the first-in-definition-order priority exactly as it did from Game.cpp.
    CHECK(game.hudStateForTest().pendingAchievementName == "Deviation Free");
}
