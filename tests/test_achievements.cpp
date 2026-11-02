// Achievement unlock/progression through Game plus composed AchievementFeeder settlement/aftermath path.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include "Achievement.hpp"
#include "Card.hpp"
#include "GameRecord.hpp"  // computeCareerStatsHistory: single record-derived source of truth for career blackjacks
#include "Constants.hpp"
#include "Game.hpp"
#include "Hand.hpp"
#include "PlayerProfile.hpp"
#include "RoundEngine.hpp"
#include "Serialization.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <catch2/catch_all.hpp>

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives here (declared in Constants.hpp)

// Shared test fixtures/helpers lifted from every member file into one anonymous namespace (identical copies de-duped).
namespace {
inline bool anyOf(std::vector<Achievement::Id> ids, Achievement::Id want)
{
    return std::find(ids.begin(), ids.end(), want) != ids.end();
}

std::vector<Card> twoCards(int v0, int v1)
{
    return {test_support::make_card(v0, Suit::Spades), test_support::make_card(v1, Suit::Hearts)};
}

} // namespace

TEST_CASE("AchievementManager defines all 12 achievements, none unlocked initially", "[Achievement][Definitions]")
{
    AchievementManager m;
    CHECK(m.totalAchievements() == 12);
    CHECK(m.unlockedCount() == 0);
    for (const auto& def : m.all())
        CHECK(def.name[0] != '\0');
    for (const auto& def : m.all())
        CHECK(def.description[0] != '\0');
    for (size_t i = 0; i < m.all().size(); ++i)
        for (size_t j = i + 1; j < m.all().size(); ++j)
        {
            CHECK(m.all()[i].id != m.all()[j].id);
            CHECK(std::string(m.all()[i].name) != std::string(m.all()[j].name));
        }
}

TEST_CASE("First Deal unlocks on the very first hand", "[Achievement][FirstSteps]")
{
    AchievementManager m;
    CHECK(!m.isUnlocked(Achievement::Id::FirstDeal));
    auto first = m.recordHand({});
    CHECK(first.size() == 1);
    CHECK(first[0] == Achievement::Id::FirstDeal);
    CHECK(m.isUnlocked(Achievement::Id::FirstDeal));
    CHECK(m.careerHands() == 1);

    auto again = m.recordHand({});
    CHECK(again.empty());
    CHECK(m.careerHands() == 2);
}

TEST_CASE("Natural 21 unlocks on the first blackjack", "[Achievement][FirstSteps]")
{
    AchievementManager m;
    CHECK(!m.isUnlocked(Achievement::Id::Natural21));
    auto res = m.recordHand({ .isBlackjack = true });
    CHECK(res.size() == 2);
    CHECK(anyOf(res, Achievement::Id::FirstDeal));
    CHECK(anyOf(res, Achievement::Id::Natural21));
    CHECK(m.isUnlocked(Achievement::Id::Natural21));
    CHECK(m.careerBlackjacks() == 1);

    m.recordHand({ .isBlackjack = true });
    CHECK(m.careerBlackjacks() == 2);
}

TEST_CASE("Split Success unlocks the first split that wins at least one hand", "[Achievement][FirstSteps]")
{
    AchievementManager m;
    m.recordHand({ .splitHand = true, .wonAny = false });   // a losing split does not count
    CHECK(!m.isUnlocked(Achievement::Id::SplitSuccess));
    CHECK(m.careerSplitWins() == 0);

    auto res = m.recordHand({ .splitHand = true, .wonAny = true });
    CHECK(anyOf(res, Achievement::Id::SplitSuccess));
    CHECK(m.isUnlocked(Achievement::Id::SplitSuccess));
}

TEST_CASE("Big Bet unlocks when the wager is at least 10% of the bankroll", "[Achievement][CardCounting]")
{
    AchievementManager m;
    CHECK(anyOf(m.recordHand({ .bet = 50, .bankrollAfter = 400 }), Achievement::Id::BigBet));   // 12.5%
    CHECK(m.isUnlocked(Achievement::Id::BigBet));

    AchievementManager m2;
    CHECK(!anyOf(m2.recordHand({ .bet = 30, .bankrollAfter = 1000 }), Achievement::Id::BigBet)); // 3%
    CHECK(!m2.isUnlocked(Achievement::Id::BigBet));
}

TEST_CASE("Counter's Edge unlocks at 10 consecutive positive true-count hands and resets per shoe", "[Achievement][CardCounting]")
{
    AchievementManager m;
    // The first hand also unlocks First Deal (career) in addition to advancing the count streak, so only it returns a
    // non-empty list here; hands 2–9 unlock nothing new until the streak reaches ten.
    CHECK(anyOf(m.recordHand({ .positiveTrueCount = true }), Achievement::Id::FirstDeal));
    for (int i = 0; i < 8; ++i)
        CHECK(m.recordHand({ .positiveTrueCount = true }).empty());
    CHECK(!m.isUnlocked(Achievement::Id::CountersEdge));

    m.recordHand({});   // a non-positive hand breaks the streak
    for (int i = 0; i < 9; ++i)
        m.recordHand({ .positiveTrueCount = true });
    CHECK(!m.isUnlocked(Achievement::Id::CountersEdge));
    m.recordHand({ .positiveTrueCount = true });   // 10th consecutive
    CHECK(m.isUnlocked(Achievement::Id::CountersEdge));

    m.onNewShoe();   // new shoe resets the per-shoe counter
    for (int i = 0; i < 9; ++i)
        CHECK(m.recordHand({ .positiveTrueCount = true }).empty());
    CHECK(anyOf(m.recordHand({ .positiveTrueCount = true }), Achievement::Id::CountersEdge));
}

TEST_CASE("Blackjack Streak unlocks at 3 blackjacks in a single shoe, then resets", "[Achievement][Performance]")
{
    AchievementManager m;
    m.recordHand({ .isBlackjack = true });
    m.recordHand({ .isBlackjack = true });
    CHECK(!m.isUnlocked(Achievement::Id::BlackJackStreak));
    m.recordHand({ .isBlackjack = true });   // third -> three blackjacks in the shoe unlocks it
    CHECK(m.isUnlocked(Achievement::Id::BlackJackStreak));

    // Then resets after a new shoe: two blackjacks must not immediately re-unlock.
    m.onNewShoe();
    m.recordHand({ .isBlackjack = true });
    m.recordHand({ .isBlackjack = true });
    CHECK(!m.isUnlocked(Achievement::Id::BlackJackStreak));
}

TEST_CASE("Textbook Player unlocks at 50 consecutive strategy-following hands (career, spans shoes)", "[Achievement][Strategy]")
{
    AchievementManager m;
    // Correct play advances the streak; a wrong play resets it to zero.
    for (int i = 0; i < 12; ++i)   // include a wrong hand partway through to prove the reset
        m.recordHand({ .followedStrategy = (i != 5) });
    CHECK(!m.isUnlocked(Achievement::Id::TextbookPlayer));

    for (int i = 0; i < 60; ++i)   // one clean run past the 50-hand threshold (streak carries over from loop 1)
        m.recordHand({ .followedStrategy = true });
    CHECK(m.isUnlocked(Achievement::Id::TextbookPlayer));
}

TEST_CASE("Deviation Free unlocks only for a whole shoe with zero strategy deviations", "[Achievement][Strategy]")
{
    AchievementManager m;
    for (int i = 0; i < 8; ++i)
        m.recordHand({ .followedStrategy = true });
    CHECK(!m.isUnlocked(Achievement::Id::DeviationFree));   // not resolved until the shoe ends
    auto res = m.onNewShoe();
    CHECK(anyOf(res, Achievement::Id::DeviationFree));
    CHECK(m.isUnlocked(Achievement::Id::DeviationFree));

    AchievementManager m2;
    for (int i = 0; i < 8; ++i)
        m2.recordHand({ .followedStrategy = (i != 3) });   // one deviation this shoe
    CHECK(!anyOf(m2.onNewShoe(), Achievement::Id::DeviationFree));
    CHECK(!m2.isUnlocked(Achievement::Id::DeviationFree));
}

TEST_CASE("Comeback King unlocks after being down >= $500 then recovering >= $500 above that low (per shoe)", "[Achievement][Performance]")
{
    AchievementManager m;
    m.recordHand({ .bankrollAfter = 1000 });   // seeds the shoe baseline at 1000
    m.recordHand({ .bankrollAfter = 400 });    // down 600 from start -> low is 400
    CHECK(!anyOf(m.onNewShoe(), Achievement::Id::ComebackKing));   // not yet recovered $500 above the low
    m.recordHand({ .bankrollAfter = 1000 });   // back to start: +600 above the 400 low
    CHECK(anyOf(m.onNewShoe(), Achievement::Id::ComebackKing));
    CHECK(m.isUnlocked(Achievement::Id::ComebackKing));

    AchievementManager m2;
    m2.recordHand({ .bankrollAfter = 1000 });
    m2.recordHand({ .bankrollAfter = 800 });   // only down 200 -> never meets the $500 threshold
    CHECK(!anyOf(m2.onNewShoe(), Achievement::Id::ComebackKing));
}

TEST_CASE("Perfect Shoe unlocks when finishing ahead with no bust, and is denied by a bust", "[Achievement][Performance]")
{
    AchievementManager m;
    m.recordHand({ .busted = false, .bankrollAfter = 1000 });   // baseline
    m.recordHand({ .busted = false, .bankrollAfter = 1100 });   // ahead of start, no bust
    CHECK(anyOf(m.onNewShoe(), Achievement::Id::PerfectShoe));
    CHECK(m.isUnlocked(Achievement::Id::PerfectShoe));

    AchievementManager m2;
    m2.recordHand({ .bankrollAfter = 1000 });
    m2.recordHand({ .busted = true, .bankrollAfter = 1300 });   // ahead of start but busted somewhere
    CHECK(!anyOf(m2.onNewShoe(), Achievement::Id::PerfectShoe));
    CHECK(!m2.isUnlocked(Achievement::Id::PerfectShoe));

    // A flat shoe (end == start) is not "ahead".
    AchievementManager m3;
    m3.recordHand({ .bankrollAfter = 1000 });
    m3.recordHand({ .bankrollAfter = 1000 });
    CHECK(!anyOf(m3.onNewShoe(), Achievement::Id::PerfectShoe));
}

TEST_CASE("Kelly Master unlocks only when every hand in a shoe used full Kelly; an empty shoe unlocks nothing", "[Achievement][CardCounting]")
{
    AchievementManager m;
    for (int i = 0; i < 6; ++i)
        m.recordHand({ .kellyFull = true });
    CHECK(anyOf(m.onNewShoe(), Achievement::Id::KellyMaster));
    CHECK(m.isUnlocked(Achievement::Id::KellyMaster));

    AchievementManager m2;
    for (int i = 0; i < 6; ++i)
        m2.recordHand({ .kellyFull = (i != 3) });   // one non-full-kelly hand breaks the shoe
    CHECK(!anyOf(m2.onNewShoe(), Achievement::Id::KellyMaster));

    AchievementManager m3;
    CHECK(m3.onNewShoe().empty());   // a shoe with no hands resolves nothing
}

TEST_CASE("Split Master unlocks at 5 successful splits in a single shoe, then resets", "[Achievement][Performance]")
{
    AchievementManager m;
    for (int i = 0; i < 4; ++i)
        m.recordHand({ .splitHand = true, .wonAny = true });
    CHECK(!m.isUnlocked(Achievement::Id::SplitMaster));
    m.recordHand({ .splitHand = true, .wonAny = true });
    CHECK(m.isUnlocked(Achievement::Id::SplitMaster));

    m.onNewShoe();
    for (int i = 0; i < 5; ++i)
        m.recordHand({ .splitHand = true, .wonAny = true });
    CHECK(m.isUnlocked(Achievement::Id::SplitMaster));   // reached five again in the new shoe -> re-earned after boundary reset
}

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Persistence: the unlocked set round-trips as plain ints, and the profile JSON stays additive — a legacy profile with no
// achievements key loads unchanged (like the table-settings backward-compat tests), while a fresh save persists unlocks.
// ---------------------------------------------------------------------------

TEST_CASE("Unlocked achievements survive a profile save/load round trip", "[Achievement][Persistence]")
{
    const fs::path path = fs::temp_directory_path() / "bj_achievements_test.json";

    {
        PlayerProfile p("William");
        p.setBankroll(2500);
        AchievementManager mgr;
        mgr.recordHand({ .isBlackjack = true });   // FirstDeal + Natural21
        std::vector<int> ids;
        mgr.persistUnlockedTo(ids);
        p.unlockedAchievementIds = ids;
        p.save(path.string());
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.getBankroll() == 2500);
    CHECK(!loaded.unlockedAchievementIds.empty());

    AchievementManager mgr;
    mgr.loadUnlockedFrom(loaded.unlockedAchievementIds);
    for (int id : loaded.unlockedAchievementIds)
        CHECK(mgr.isUnlocked(Achievement::fromInt(id)));
    CHECK(mgr.unlockedCount() == loaded.unlockedAchievementIds.size());

    fs::remove(path);
}

TEST_CASE("A profile saved before achievements existed still loads with an empty, additive unlocked set", "[Achievement][Persistence]")
{
    const fs::path path = fs::temp_directory_path() / "bj_achievements_legacy.json";

    {
        PlayerProfile fresh("William");   // no achievements field written (schema predates Feature 5)
        fresh.setBankroll(4321);
        fresh.save(path.string());
    }
    {
        std::fstream rw(path);
        json j;
        rw >> j;
        rw.close();
        CHECK(j.find("unlockedAchievementIds") == j.end());   // a legacy file has no such key

        std::ofstream out(path);
        out << j.dump(2);
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.getBankroll() == 4321);                    // other fields preserved
    CHECK(loaded.unlockedAchievementIds.empty());           // additive default: nothing unlocked, no throw

    AchievementManager mgr;
    mgr.loadUnlockedFrom(loaded.unlockedAchievementIds);
    CHECK(mgr.unlockedCount() == 0);
    CHECK(mgr.totalAchievements() == 12);                   // definitions unaffected by the missing field

    fs::remove(path);
}

// ---------------------------------------------------------------------------
// Integration: Game::recordHandAchievements() aggregates the human seat's outcome correctly, and a real deal->settle cycle
// through the headless round machine feeds the engine exactly like live play (mirrors test_side_bet_round / test_game).
// ---------------------------------------------------------------------------

TEST_CASE("Game records the human seat only; split + natural are detected from cards", "[Achievement][Integration]")
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

TEST_CASE("A real deal->settle cycle through the headless round machine feeds the achievement engine", "[Achievement][Integration]")
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
    // Any settled hand unlocks First Deal, proving the whole deal -> settle -> recordHandAchievements chain works live.
    CHECK(game.achievementsForTest().isUnlocked(Achievement::Id::FirstDeal));
}

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

// Reconciliation (Phase 4 follow-up): the achievement engine's incrementally-maintained blackjacks and the record-derived
// PlayerStats::blackjacks must agree -- both count naturals through the single predicate handIsNaturalBJ (CONTEXT_refactor.md
// §5 #157/#158). This drives real rounds through the LIVE feeder path and cross-checks against computeCareerStatsHistory over
// the same shoe, locking "incrementally-maintained == record-recomputed" for blackjacks exactly as the persistence/dashboard
// tests lock it. It also locks human-seat gating parity: an NPC natural is ignored by both paths.
TEST_CASE("Achievement blackjacks read the same record-derived source as PlayerStats", "[Achievement][stats-batching]")
{
    Game game(true);   // headless: no window/assets/audio device

    auto seat0 = [](std::vector<Card> cards, int wager, int payout, bool won)
    {
        HandOutcome h;
        h.cards = std::move(cards);
        h.wager = wager;
        h.payout = payout;
        h.won = won;
        h.playerIndex = 0;   // human seat only
        return h;
    };

    RoundRecord a; a.playerHands.push_back(seat0({test_support::make_card(1, Suit::Hearts), test_support::make_card(10, Suit::Spades)}, 10, 30, true));   // natural BJ (+20)
    RoundRecord b; { HandOutcome npc; npc.cards = {test_support::make_card(1, Suit::Clubs), test_support::make_card(10, Suit::Hearts)}; npc.playerIndex = 1; npc.won = true; b.playerHands.push_back(npc); }   // NPC natural -> ignored by both paths (human seat only)
    RoundRecord c; c.playerHands.push_back(seat0({test_support::make_card(7, Suit::Hearts), test_support::make_card(8, Suit::Diamonds), test_support::make_card(9, Suit::Clubs)}, 10, 0, false));   // human bust -> not BJ
    RoundRecord d; d.playerHands.push_back(seat0({test_support::make_card(10, Suit::Hearts), test_support::make_card(1, Suit::Diamonds)}, 10, 30, true));   // natural BJ (+20)

    // Feed incrementally through the LIVE path (AchievementFeeder -> AchievementManager), asserting each boundary so we also
    // lock human-seat gating parity: an NPC natural is ignored here exactly as it is by computeCareerStats.
    game.recordHandAchievements(a);   // first human natural
    CHECK(game.achievementsForTest().careerBlackjacks() == 1);
    CHECK(game.achievementsForTest().isUnlocked(Achievement::Id::Natural21));

    game.recordHandAchievements(b);   // NPC natural -> ignored by both paths (human seat only)
    CHECK(game.achievementsForTest().careerBlackjacks() == 1);

    game.recordHandAchievements(c);   // human bust -> not a natural
    CHECK(game.achievementsForTest().careerBlackjacks() == 1);

    game.recordHandAchievements(d);   // second human natural
    CHECK(game.achievementsForTest().careerBlackjacks() == 2);

    // The record-derived source of truth over the same shoe agrees: two naturals, NPC natural excluded by human-seat gating.
    ShoeRecord shoe;
    shoe.rounds.push_back(a);
    shoe.rounds.push_back(b);
    shoe.rounds.push_back(c);
    shoe.rounds.push_back(d);
    const PlayerStats cs = computeCareerStats(shoe);
    CHECK(cs.blackjacks == 2);

    // The invariant Phase-4 locks for persistence/dashboard: incrementally-maintained == record-recomputed.
    CHECK(game.achievementsForTest().careerBlackjacks() == cs.blackjacks);
}
