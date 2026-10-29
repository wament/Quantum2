#include <catch2/catch_all.hpp>

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include "Game.hpp"
#include "RoundEngine.hpp"
#include "Achievement.hpp"
#include "PlayerProfile.hpp"
#include "Serialization.hpp"
#include <algorithm>

using namespace Constants;
using namespace GameConfig; // STARTING_BANKROLL lives here (declared in Constants.hpp)

namespace
{
    Card makeCard(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    // Drive one frame of the shared round-state machine. deltaTime is ignored by player/turn transitions; settle ticks use
    // 100.0f so the dealer's reveal-pacing timers (which advance on Game::deltaTime, a member) elapse in finite time.
    void tickOnce(Game& game, float deltaTime = 0.0f)
    {
        RoundEngine engine;
        engine.tick(deltaTime, game);
    }

    // Frame-loop until the round leaves PlayerTurn (all hands resolved -> DealerTurn). Bounded so a stuck state cannot spin.
    void tickUntilLeavesPlayerTurn(Game& game)
    {
        for (int i = 0; i < 25 && game.roundStateForTest() == RoundState::PlayerTurn; ++i)
            tickOnce(game);
    }
}

// C++17 std::vector has no contains(); this mirrors the intended "is id in the newly-unlocked list?" check.
inline bool anyOf(std::vector<Achievement::Id> ids, Achievement::Id want)
{
    return std::find(ids.begin(), ids.end(), want) != ids.end();
}

// Two-card hand fixture used by the integration tests (e.g. to build a natural 21).
std::vector<Card> twoCards(int v0, int v1)
{
    return {makeCard(v0, Suit::Spades), makeCard(v1, Suit::Hearts)};
}

// ---------------------------------------------------------------------------
// Feature 5 (Achievement System): logic-first + headless-tested, mirroring test_strategy_trainer / test_side_bets. The first
// suite exercises the pure AchievementManager directly with deterministic HandEvents; later cases prove the same behaviour
// survives Game::recordHandAchievements() and a real deal->settle cycle. Persistence round-trips as plain ints so the profile
// schema stays additive (legacy profiles load unlocked-ids absent).
// ---------------------------------------------------------------------------

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
    game.setDeck({makeCard(7, Suit::Spades),   // player card 0
                  makeCard(2, Suit::Hearts),   // dealer up-card
                  makeCard(8, Suit::Diamonds), // player card 1 -> 15
                  makeCard(3, Suit::Clubs),
                  makeCard(4, Suit::Spades),
                  makeCard(5, Suit::Hearts),
                  makeCard(6, Suit::Diamonds)});

    game.startTestRound();
    CHECK(game.achievementsForTest().unlockedCount() == 0);   // nothing unlocked before the round settles

    game.stand();
    game.deltaTime = 100.0f;                                // elapse dealer reveal pacing timers
    tickUntilLeavesPlayerTurn(game);                        // all hands resolved -> DealerTurn
    for (int i = 0; i < 60 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        tickOnce(game, 100.0f);                             // dealer draws/stands then evaluateHands settles the round

    CHECK(game.roundStateForTest() == RoundState::Cleanup);
    // Any settled hand unlocks First Deal, proving the whole deal -> settle -> recordHandAchievements chain works live.
    CHECK(game.achievementsForTest().isUnlocked(Achievement::Id::FirstDeal));
}