#include <catch2/catch_all.hpp>

#include <DailyChallenge.hpp>
#include <Game.hpp>
#include <PlayerProfile.hpp>
#include <Serialization.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "test_support.hpp"

namespace fs = std::filesystem;

// =============================================================================
// Daily Challenge core: the seed function, civil-date arithmetic, streak rules and the additive
// profile schema. All of it is headless by design.
// =============================================================================

// The seed function is the Daily's whole promise ("the same shoe on every machine"), so it is frozen
// here against values computed OUTSIDE this build. They came from a standalone program checked
// against the published FNV-1a vectors -- NOT by running this code and pasting its output. A golden
// test generated from your own output proves only that the code equals itself.
//
// If these fail, every Daily shoe in history changes. Fix the code, or bump Daily::kSeedSalt
// deliberately; never regenerate these numbers to make a red test green.
TEST_CASE("Golden: the Daily seed for a date is fixed for all time", "[Golden][Daily][Seed]")
{
    CHECK(Daily::seedForDateKey("2026-11-12") == 5749705219177586999ull);
    CHECK(Daily::seedForDateKey("2027-01-01") == 13167299879534641243ull);
    CHECK(Daily::seedForDateKey("2024-02-29") == 16704300417910721275ull); // leap day
    CHECK(Daily::seedForDateKey("1970-01-01") == 6156513940161069031ull);  // epoch
    CHECK(Daily::seedForDateKey("2038-01-19") == 15774331074708500134ull); // 32-bit overflow eve
}

// FNV-1a is a published algorithm with published vectors. Asserting them catches a wrong constant:
// the offset basis here was first written with a dropped digit, which produced a perfectly
// deterministic hash that was not FNV-1a at all, and a self-generated golden test would not have
// noticed.
TEST_CASE("fnv1a matches the published FNV-1a 64 test vectors", "[Daily][Seed]")
{
    CHECK(Daily::fnv1a("") == 0xcbf29ce484222325ull);
    CHECK(Daily::fnv1a("a") == 0xaf63dc4c8601ec8cull);
    CHECK(Daily::fnv1a("foobar") == 0x85944171f73967e8ull); // the canonical idempotence vector
}

TEST_CASE("Daily seeds are stable, distinct per day, and never zero", "[Daily][Seed]")
{
    CHECK(Daily::seedForDateKey("2026-11-12") == Daily::seedForDateKey("2026-11-12"));

    // Distinctness: a one-character change must move the whole hash.
    CHECK(Daily::seedForDateKey("2026-11-12") != Daily::seedForDateKey("2026-11-13"));
    CHECK(Daily::seedForDateKey("2026-11-12") != Daily::seedForDateKey("2026-12-12"));

    // Zero is what Deck::shuffleSeed() reports for a shoe that was never shuffled. A Daily seed of
    // 0 would be indistinguishable from that state.
    CHECK(Daily::seedForDateKey("2026-11-12") != 0ull);
}

TEST_CASE("Daily date arithmetic handles month, year and leap boundaries", "[Daily][Dates]")
{
    CHECK(Daily::previousDateKey("2026-11-12") == "2026-11-11");
    CHECK(Daily::previousDateKey("2026-11-01") == "2026-10-31"); // month boundary
    CHECK(Daily::previousDateKey("2027-01-01") == "2026-12-31"); // year boundary
    CHECK(Daily::previousDateKey("2026-03-01") == "2026-02-28"); // non-leap March
    CHECK(Daily::previousDateKey("2024-03-01") == "2024-02-29"); // leap February
    CHECK(Daily::previousDateKey("2000-03-01") == "2000-02-29"); // century leap (divisible by 400)
    CHECK(Daily::previousDateKey("1900-03-01") == "1900-02-28"); // century non-leap (not /400)

    CHECK(Daily::daysBetweenKeys("2026-11-12", "2026-11-13") == 1);
    CHECK(Daily::daysBetweenKeys("2026-11-13", "2026-11-12") == -1);
    CHECK(Daily::daysBetweenKeys("2026-11-12", "2026-11-12") == 0);
    CHECK(Daily::daysBetweenKeys("2026-02-28", "2026-03-01") == 1); // 2026 is not a leap year
    CHECK(Daily::daysBetweenKeys("2024-02-28", "2024-03-01") == 2); // 2024 is
    CHECK(Daily::daysBetweenKeys("nonsense", "2026-11-12") == 0);   // malformed degrades to 0
}

TEST_CASE("Daily date keys round-trip and reject malformed input", "[Daily][Dates]")
{
    int y = 0;
    unsigned m = 0, d = 0;
    CHECK(Daily::parseDateKey("2026-11-12", y, m, d));
    CHECK((y == 2026 && m == 11 && d == 12));
    CHECK(Daily::makeDateKey(2026, 11, 12) == "2026-11-12");
    CHECK(Daily::makeDateKey(1999, 1, 5) == "1999-01-05"); // zero-padded

    // A corrupt save must degrade, never throw: the load path's recovery is "move the file aside,
    // start fresh", which loses the WHOLE profile. A field that throws is a field that deletes
    // progress.
    CHECK_FALSE(Daily::parseDateKey("", y, m, d));
    CHECK_FALSE(Daily::parseDateKey("2026-1-12", y, m, d));  // not zero-padded
    CHECK_FALSE(Daily::parseDateKey("2026/11/12", y, m, d)); // wrong separator
    CHECK_FALSE(Daily::parseDateKey("2026-13-01", y, m, d)); // month 13
    CHECK_FALSE(Daily::parseDateKey("2026-00-01", y, m, d)); // month 0
    CHECK_FALSE(Daily::parseDateKey("2026-11-32", y, m, d)); // day 32
    CHECK_FALSE(Daily::parseDateKey("2026-11-1", y, m, d));  // short
    CHECK(Daily::previousDateKey("garbage").empty());
}


TEST_CASE("Daily dateKeyFromEpochSeconds maps an instant to a local date", "[Daily][Dates]")
{
    // 2026-11-12T12:00:00Z (day 20769 * 86400 + 43200). The result is the LOCAL date, so the exact
    // key depends on the machine's zone. What is zone-independent: a midday-UTC instant lands within
    // one day of the UTC date, and todayDateKey() is parseable.
    const long long noonUtc = 1794484800ll;
    const std::string key = Daily::dateKeyFromEpochSeconds(noonUtc);
    const long long offset = Daily::daysBetweenKeys("2026-11-12", key);
    CHECK(offset >= -1);
    CHECK(offset <= 1);

    int y = 0;
    unsigned m = 0, d = 0;
    CHECK(Daily::parseDateKey(Daily::todayDateKey(), y, m, d));
}

// =============================================================================
// Streak rules. No clock is read anywhere in this path: every function takes the date key it needs,
// so the whole state machine is exercised with fabricated dates.
// =============================================================================

static Daily::DailyResult makeResult(const std::string& dateKey, int net, bool practice = false)
{
    Daily::DailyResult r;
    r.dateKey = dateKey;
    r.net = net;
    r.practice = practice;
    return r;
}

TEST_CASE("Daily streak: first play starts at 1, consecutive days extend, a lapse restarts at 1", "[Daily][Streak]")
{
    PlayerProfile p("Ada");
    CHECK(p.dailyStreak == 0);
    CHECK_FALSE(p.dailyScoredOn("2026-11-12"));

    p.commitDailyResult(makeResult("2026-11-12", 40), 1794484800ll);
    CHECK(p.dailyStreak == 1);
    CHECK(p.dailyScoredOn("2026-11-12"));
    CHECK(p.dailyLastPlayed == "2026-11-12");

    // Consecutive days extend.
    p.commitDailyResult(makeResult("2026-11-13", -20), 1794571200ll);
    CHECK(p.dailyStreak == 2);
    p.commitDailyResult(makeResult("2026-11-14", 5), 1794657600ll);
    CHECK(p.dailyStreak == 3);

    // A lapse restarts at 1, NOT 0: missing a day must not wipe the reward for playing today.
    p.commitDailyResult(makeResult("2026-11-20", 0), 1795176000ll);
    CHECK(p.dailyStreak == 1);
    CHECK(p.dailyLastPlayed == "2026-11-20");
}

TEST_CASE("Daily streak: same-day replay, practice, and clock rollback never extend it", "[Daily][Streak]")
{
    PlayerProfile p("Ada");
    p.commitDailyResult(makeResult("2026-11-12", 100), 1794484800ll);
    CHECK(p.dailyStreak == 1);

    // Same day again (the gate should stop this; the bookkeeping must be safe anyway).
    p.commitDailyResult(makeResult("2026-11-12", 999), 1794484800ll);
    CHECK(p.dailyStreak == 1);
    CHECK(p.dailyBest == 999); // best is a max, so a bigger same-day score still lands

    // Practice never moves the streak, the anchor, or the best -- even on a fresh date.
    p.commitDailyResult(makeResult("2026-11-13", 5000, /*practice=*/true), 1794571200ll);
    CHECK(p.dailyStreak == 1);
    CHECK(p.dailyLastPlayed == "2026-11-12");
    CHECK(p.dailyBest == 999);

    // Clock rollback: a result dated in the past must not extend, and must not rewind the anchor.
    p.commitDailyResult(makeResult("2026-11-05", 77), 1793880000ll);
    CHECK(p.dailyStreak == 1);
    CHECK(p.dailyLastPlayed == "2026-11-12");
}

TEST_CASE("Daily: the active-shoe gate keys on the day the shoe STARTED", "[Daily][Streak]")
{
    PlayerProfile p("Ada");
    CHECK_FALSE(p.dailyHasActiveShoe());

    p.beginDailyShoe("2026-11-12", 1794484800ll, 5749705219177586999ull);
    CHECK(p.dailyHasActiveShoe());
    CHECK(p.dailyActiveSeed == 5749705219177586999ull);
    // Beginning a shoe must NOT mark today as scored, or a player who quits and returns would be
    // locked out of the shoe they are mid-way through.
    CHECK_FALSE(p.dailyScoredOn("2026-11-12"));

    // Quitting abandons the shoe and clears the gate, so today can be started again.
    p.clearDailyShoe();
    CHECK_FALSE(p.dailyHasActiveShoe());
    CHECK(p.dailyActiveSeed == 0);

    // Committing books the result and clears the active shoe in one step.
    p.beginDailyShoe("2026-11-12", 1794484800ll, 5749705219177586999ull);
    p.commitDailyResult(makeResult("2026-11-12", 30), 1794484800ll);
    CHECK_FALSE(p.dailyHasActiveShoe());
    CHECK(p.dailyScoredOn("2026-11-12"));
}

TEST_CASE("Daily history is newest-first and capped so the save file cannot grow without bound", "[Daily][Streak]")
{
    PlayerProfile p("Ada");
    const int total = Constants::Daily::MAX_HISTORY_RESULTS + 12;
    for (int i = 0; i < total; ++i)
        p.commitDailyResult(makeResult("2026-11-12", i), 1794484800ll);

    CHECK(p.dailyHistory.size() == static_cast<std::size_t>(Constants::Daily::MAX_HISTORY_RESULTS));
    REQUIRE(!p.dailyHistory.empty());
    CHECK(p.dailyHistory.front().net == total - 1); // the most recent commit is first
}

// =============================================================================
// Schema. The Daily fields are additive: written unconditionally, read with j.value defaults.
// =============================================================================

static std::string read_file(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

TEST_CASE("Daily fields survive a profile save/load round trip", "[Daily][Schema][Persistence]")
{
    const fs::path path = test_support::temp_path("daily", "roundtrip");

    {
        PlayerProfile p("Ada");
        p.dailyStreak = 7;
        p.dailyBest = 250;
        p.dailyLastPlayed = "2026-11-12";
        p.dailyStartedOn = 1794484800ll;
        p.dailyActiveDateKey = "2026-11-11";
        p.dailyActiveSeed = 5749705219177586999ull;
        Daily::DailyResult r = makeResult("2026-11-12", 250);
        r.isContinuation = true;
        p.dailyHistory.push_back(r);
        Daily::DailyResult q = makeResult("2026-11-11", -40, /*practice=*/true);
        p.dailyHistory.push_back(q);
        p.save(path.string());
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.dailyStreak == 7);
    CHECK(loaded.dailyBest == 250);
    CHECK(loaded.dailyLastPlayed == "2026-11-12");
    CHECK(loaded.dailyStartedOn == 1794484800ll);
    CHECK(loaded.dailyActiveDateKey == "2026-11-11");
    CHECK(loaded.dailyActiveSeed == 5749705219177586999ull);
    REQUIRE(loaded.dailyHistory.size() == 2);
    CHECK(loaded.dailyHistory[0].dateKey == "2026-11-12");
    CHECK(loaded.dailyHistory[0].net == 250);
    CHECK(loaded.dailyHistory[0].isContinuation == true);
    CHECK(loaded.dailyHistory[0].practice == false);
    CHECK(loaded.dailyHistory[1].practice == true);

    // The keys are written unconditionally, so a partial write keeps them (schema policy).
    const std::string content = read_file(path);
    CHECK(content.find("\"dailyStreak\"") != std::string::npos);
    CHECK(content.find("\"dailyHistory\"") != std::string::npos);

    fs::remove(path);
}

TEST_CASE("A profile saved before the Daily existed loads unchanged with empty Daily state", "[Daily][Schema]")
{
    const fs::path path = test_support::temp_path("daily", "preexisting");

    {
        // A pre-Daily save: none of the Daily keys present at all.
        std::ofstream out(path);
        out << "{ \"name\": \"Ada\", \"bankroll\": 4321, \"gameRules\": { \"numDecks\": 8 } }";
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.getName() == "Ada");
    CHECK(loaded.getBankroll() == 4321);
    CHECK(loaded.gameRules.numDecks == 8);
    CHECK(loaded.dailyLastPlayed.empty());
    CHECK(loaded.dailyStreak == 0);
    CHECK(loaded.dailyBest == 0);
    CHECK(loaded.dailyStartedOn == 0);
    CHECK(loaded.dailyActiveSeed == 0);
    CHECK(loaded.dailyHistory.empty());
    CHECK_FALSE(loaded.dailyHasActiveShoe());
    CHECK_FALSE(loaded.dailyScoredOn(Daily::todayDateKey()));

    fs::remove(path);
}

TEST_CASE("A corrupt top-level profile field degrades instead of deleting the whole profile", "[Daily][Schema][Persistence]")
{
    // The blast radius of a throw in from_json is the ENTIRE profile: load() catches it, relocates
    // the file to *.corrupt.<epoch>.json and starts fresh. So a single wrong-typed field used to be
    // enough to destroy a player's bankroll, shoe history and achievements.
    //
    // nlohmann's j.value(key, default) does NOT cover this: the default applies only when the key is
    // ABSENT. A key present with the wrong type throws. The jsonOr* helpers in Serialization.hpp do.
    const fs::path path = test_support::temp_path("daily", "corrupt");

    // Start from a clean slate: /tmp is shared across the whole suite and a PREVIOUS run of this
    // test (before the fix) left a relocated .corrupt. file behind. Asserting "no .corrupt. file
    // exists" without clearing that first measures history, not this run.
    fs::remove(path);
    for (const auto& entry : fs::directory_iterator(path.parent_path()))
        if (entry.path().string().rfind((path.parent_path() / "bj_daily_test_corrupt.corrupt.").string(), 0) == 0)
            fs::remove(entry.path());

    {
        std::ofstream out(path);
        out << "{ \"name\": 12345,"
            << "  \"bankroll\": \"lots\","
            << "  \"stats\": [1,2,3],"
            << "  \"longHistory\": \"not-an-array\","
            << "  \"preferredMuted\": \"yes\","
            << "  \"unlockedAchievementIds\": 7,"
            << "  \"gameRules\": 42,"
            << "  \"dailyLastPlayed\": 12345,"
            << "  \"dailyStreak\": \"seven\","
            << "  \"dailyActiveSeed\": -5,"
            << "  \"dailyHistory\": \"nope\" }";
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());

    // Every corrupt field falls back to its default...
    CHECK(loaded.getName().empty());
    CHECK(loaded.getBankroll() == 1000);
    CHECK(loaded.getLongHistory().empty());
    CHECK(loaded.preferredMuted == false);
    CHECK(loaded.unlockedAchievementIds.empty());
    CHECK(loaded.gameRules.numDecks == Constants::NUM_DECKS);
    CHECK(loaded.dailyLastPlayed.empty());
    CHECK(loaded.dailyStreak == 0);
    CHECK(loaded.dailyActiveSeed == 0); // a negative seed is rejected, not narrowed into a huge one
    CHECK(loaded.dailyHistory.empty());

    // ...and the file is NOT relocated: the profile survives to be re-saved. Targeted at this test's
    // own path -- /tmp is shared with every other test, and some of them deliberately produce
    // .corrupt. files.
    CHECK(fs::exists(path));
    const std::string relocatedPrefix = (path.parent_path() / "bj_daily_test_corrupt.corrupt.").string();
    int relocated = 0;
    for (const auto& entry : fs::directory_iterator(path.parent_path()))
        if (entry.path().string().rfind(relocatedPrefix, 0) == 0)
            ++relocated;
    CHECK(relocated == 0);

    fs::remove(path);
}

TEST_CASE("A wrong-typed nested profile field is ignored, not fatal", "[Schema][Persistence]")
{
    // "stats" and "gameRules" present but not objects: guarded by is_object() checks.
    const fs::path path = test_support::temp_path("daily", "nested");

    {
        std::ofstream out(path);
        out << "{ \"name\": \"Ada\", \"bankroll\": 4321, \"stats\": \"broken\", \"gameRules\": \"broken\" }";
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.getName() == "Ada");
    CHECK(loaded.getBankroll() == 4321);
    CHECK(loaded.stats.handsFinished == 0);
    CHECK(loaded.gameRules.numDecks == Constants::NUM_DECKS);

    fs::remove(path);
}

// The Daily's rules are the whole point of comparing results, so they are pinned. A change here
// makes every stored Daily result incomparable to a future one.
TEST_CASE("The Daily rule set is fixed and ignores the player's custom rules", "[Daily][Rules]")
{
    const GameRules r = Daily::dailyRules();
    CHECK(r.numDecks == Constants::NUM_DECKS);
    CHECK(r.dealerStandsOnSoft17 == false); // dealer hits soft 17
    CHECK(r.doublesAfterSplitAllowed == Constants::DOUBLE_AFTER_SPLIT_ALLOWED);
    CHECK(r.resplitAcesAllowed == Constants::RESPLIT_ACES_ALLOWED);
    CHECK(r.resplitTensAllowed == Constants::RESPLIT_TENS_ALLOWED);
    CHECK(r.surrenderType == SurrenderType::Late);
    CHECK(r.maxSplitHands == Constants::MAX_SPLIT_HANDS);
}

TEST_CASE("Booking a Daily result touches nothing but the Daily fields", "[Daily][Isolation]")
{
    PlayerProfile p("Ada");
    p.setBankroll(4321);
    p.stats.handsFinished = 77;
    p.unlockedAchievementIds = {1, 2, 3};

    const int bankrollBefore = p.getBankroll();
    const int handsBefore = p.stats.handsFinished;

    p.beginDailyShoe("2026-11-12", 1794484800ll, 5749705219177586999ull);
    p.commitDailyResult(makeResult("2026-11-12", 500), 1794484800ll);

    // The Daily is scored on its own fixed bankroll, never the career one. Career progress must be
    // untouched: this is the profile-level half of the isolation the ephemeral seat enforces at the
    // save level.
    CHECK(p.getBankroll() == bankrollBefore);
    CHECK(p.stats.handsFinished == handsBefore);
    CHECK(p.unlockedAchievementIds == (std::vector<int>{1, 2, 3}));
    CHECK(p.dailyStreak == 1);
    CHECK(p.dailyBest == 500);
}


// =============================================================================
// The Daily SESSION: the seeded shoe through the real path, and the isolation that keeps a Daily
// run from touching the career file. Both need HOME redirected (B17) -- startDailySession and
// commitDailyResult write the real profile path.
// =============================================================================

// Card has no operator==; value + suit is the full identity (see test_deck.cpp's sameShoe).
static bool sameShoe(const std::vector<Card>& a, const std::vector<Card>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].value != b[i].value || a[i].suit != b[i].suit)
            return false;
    return true;
}

TEST_CASE("The Daily session deals the same shoe for the same date, through the real path", "[Daily][Session]")
{
    test_support::IsolatedHome home("daily");

    Game a(true);
    Game b(true);
    a.startDailySession("2026-11-12", false);
    b.startDailySession("2026-11-12", false);

    REQUIRE(!a.deckCardsForTest().empty());
    CHECK(a.deckSizeForTest() == Constants::NUM_DECKS * 52 + 1); // 6 decks + the cut card
    CHECK(sameShoe(a.deckCardsForTest(), b.deckCardsForTest()));

    // A different day is a different shoe.
    Game c(true);
    c.startDailySession("2026-11-13", false);
    CHECK_FALSE(sameShoe(a.deckCardsForTest(), c.deckCardsForTest()));

    // Practice replays a PAST day: same shoe as that day's scored run, which is the whole point.
    Game p(true);
    p.startDailySession("2026-11-12", /*practice=*/true);
    CHECK(sameShoe(p.deckCardsForTest(), a.deckCardsForTest()));
    CHECK(p.dailyPractice());
}

TEST_CASE("The Daily ignores the player's custom rules for the run, and leaves them on disk", "[Daily][Session][Isolation]")
{
    test_support::IsolatedHome home("daily");

    {
        PlayerProfile career("Ada");
        career.setBankroll(7777);
        career.stats.handsFinished = 42;
        career.gameRules.numDecks = 2; // a distinctive custom rule the Daily must NOT adopt
        career.save(home.profilePath().string());
    }

    Game game(true);
    game.startDailySession("2026-11-12", false);

    // Live: the Daily's fixed shoe, not the player's 2-deck table.
    CHECK(game.deckSizeForTest() == Constants::NUM_DECKS * 52 + 1);
    REQUIRE(!game.playersForTest().empty());
    CHECK(game.playersForTest()[0].getBankroll() == Daily::kStartingBankroll); // fixed chips, not 7777

    // Every save point the play path reaches. rememberTableSettings() stamps gameRules_ onto the
    // profile, so this is the exact path that would overwrite the player's custom rules with the
    // Daily's if the seat persisted.
    game.playersForTest()[0].saveProfile();
    game.rememberTableSettings();
    game.persistOnExit();

    const PlayerProfile after = PlayerProfile::load(home.profilePath().string());
    CHECK(after.getBankroll() == 7777);
    CHECK(after.stats.handsFinished == 42);
    CHECK(after.gameRules.numDecks == 2); // the player's rules survive
    CHECK(game.playersForTest()[0].saveWriteCount == 0); // the seat never wrote
}

TEST_CASE("A completed Daily books its result on the career file and reports itself complete", "[Daily][Session]")
{
    test_support::IsolatedHome home("daily");

    {
        PlayerProfile career("Ada");
        career.setBankroll(7777);
        career.save(home.profilePath().string());
    }

    Game game(true);
    game.startDailySession("2026-11-12", false);
    CHECK(game.dailyActive());
    CHECK_FALSE(game.dailyComplete());
    CHECK(game.dailyDateKey() == "2026-11-12");

    // Win 300 over the fixed start, on a shoe that continued on deck exhaustion.
    game.playersForTest()[0].setBankroll(Daily::kStartingBankroll + 300);
    game.finishDailyShoeForTest(/*shoeWasContinuation=*/true);

    CHECK(game.dailyComplete());
    CHECK(game.dailyNet() == 300);

    const PlayerProfile after = PlayerProfile::load(home.profilePath().string());
    CHECK(after.dailyStreak == 1);
    CHECK(after.dailyBest == 300);
    CHECK(after.dailyLastPlayed == "2026-11-12");
    CHECK_FALSE(after.dailyHasActiveShoe());
    REQUIRE(after.dailyHistory.size() == 1);
    CHECK(after.dailyHistory[0].net == 300);
    CHECK(after.dailyHistory[0].isContinuation == true);

    // The career file's own progress is untouched by the run.
    CHECK(after.getBankroll() == 7777);

    // And the SEAT must show the booked streak, not the one it loaded at startup. The main-menu row
    // reads Player::dailyStreak(); a seat left unsynced shows the pre-run value -- B23's shape.
    CHECK(game.playersForTest()[0].dailyStreak() == 1);
}

TEST_CASE("A practice Daily is recorded but never moves the streak", "[Daily][Session]")
{
    test_support::IsolatedHome home("daily");

    Game game(true);
    game.startDailySession("2026-11-05", /*practice=*/true);
    game.playersForTest()[0].setBankroll(Daily::kStartingBankroll + 999);
    game.finishDailyShoeForTest(false);

    const PlayerProfile after = PlayerProfile::load(home.profilePath().string());
    CHECK(after.dailyStreak == 0);
    CHECK(after.dailyBest == 0);
    CHECK(after.dailyLastPlayed.empty());
    REQUIRE(after.dailyHistory.size() == 1);
    CHECK(after.dailyHistory[0].practice == true);
    CHECK(after.dailyHistory[0].net == 999);
}

TEST_CASE("Starting a Daily marks the shoe active on disk so a quit resumes the same one", "[Daily][Session]")
{
    test_support::IsolatedHome home("daily");

    Game game(true);
    game.startDailySession("2026-11-12", false);

    const PlayerProfile after = PlayerProfile::load(home.profilePath().string());
    CHECK(after.dailyHasActiveShoe());
    CHECK(after.dailyActiveSeed == Daily::seedForDateKey("2026-11-12"));
    // Beginning must not mark today scored, or quitting mid-shoe would lock the player out.
    CHECK_FALSE(after.dailyScoredOn("2026-11-12"));
}


// =============================================================================
// The one-attempt gate. Asserted as the pure decision it is, so every branch is reachable without a
// window, a Game, or a clock.
// =============================================================================

TEST_CASE("Daily gate: fresh day starts today's shoe; a scored day does not", "[Daily][Gate]")
{
    // Nothing played: today's shoe, scored.
    CHECK(Daily::gateFor("2026-11-12", false, "", "") == Daily::GateAction::StartToday);
    CHECK(Daily::gateFor("2026-11-12", false, "", "2026-11-11") == Daily::GateAction::StartToday);

    // Today already scored: no second scored attempt.
    CHECK(Daily::gateFor("2026-11-12", false, "", "2026-11-12") == Daily::GateAction::PracticeYesterday);
}

TEST_CASE("Daily gate: an unfinished shoe outranks a scored day", "[Daily][Gate]")
{
    // The midnight case: the shoe started on the 11th, is unfinished, and it is now the 12th -- which
    // is already scored. The player must get the shoe they are PLAYING, not a fresh one and not a
    // practice replay.
    CHECK(Daily::gateFor("2026-11-12", true, "2026-11-11", "2026-11-12") == Daily::GateAction::ResumeActive);

    // An active shoe on an unscored day resumes too, rather than restarting today.
    CHECK(Daily::gateFor("2026-11-12", true, "2026-11-12", "") == Daily::GateAction::ResumeActive);

    // An active flag with no date is unusable: there is no shoe to resume, so fall through to the
    // normal gate rather than starting a session with an empty date key.
    CHECK(Daily::gateFor("2026-11-12", true, "", "") == Daily::GateAction::StartToday);
}

TEST_CASE("Daily gate: practice replays yesterday, never today", "[Daily][Gate]")
{
    // Scored today -> yesterday's shoe as practice.
    const Daily::GateAction action = Daily::gateFor("2026-11-12", false, "", "2026-11-12");
    REQUIRE(action == Daily::GateAction::PracticeYesterday);
    CHECK(Daily::previousDateKey("2026-11-12") == "2026-11-11");

    // A malformed today key has no yesterday, so there is nothing to replay: refuse rather than start
    // a session with a garbage date key (which would seed a shoe from "garbage").
    CHECK(Daily::gateFor("garbage", false, "", "garbage") == Daily::GateAction::Nothing);
}

TEST_CASE("The active Daily shoe's date key round-trips through the profile", "[Daily][Schema][Persistence]")
{
    const fs::path path = test_support::temp_path("daily", "activedate");

    {
        PlayerProfile p("Ada");
        p.beginDailyShoe("2026-11-11", 1794398400ll, 1234ull);
        CHECK(p.dailyActiveDateKey == "2026-11-11");
        p.save(path.string());
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.dailyHasActiveShoe());
    CHECK(loaded.dailyActiveDateKey == "2026-11-11");
    CHECK(loaded.dailyActiveSeed == 1234ull);
    // The gate must be able to resume it.
    CHECK(Daily::gateFor("2026-11-12", loaded.dailyHasActiveShoe(), loaded.dailyActiveDateKey,
                         loaded.dailyLastPlayed) == Daily::GateAction::ResumeActive);

    // Committing clears it, so the day can no longer be replayed as scored.
    PlayerProfile mutableProfile = loaded;
    mutableProfile.commitDailyResult(makeResult("2026-11-11", 10), 1794398400ll);
    CHECK_FALSE(mutableProfile.dailyHasActiveShoe());
    CHECK(mutableProfile.dailyActiveDateKey.empty());

    fs::remove(path);
}


// =============================================================================
// B25: session kinds are mutually exclusive. A flag set by one entry point must not outlive it.
// =============================================================================

TEST_CASE("B25: an ordinary game inherits no session kind from the game before it", "[Session][Exclusivity][B25]")
{
    test_support::IsolatedHome home("b25");

    Game game(true);

    // Daily first, then an ordinary game.
    game.startDailySession("2026-11-12", false);
    REQUIRE(game.dailyActive());
    game.startNewGame();
    CHECK_FALSE(game.dailyActive());
    CHECK_FALSE(game.dailyComplete());
    CHECK(game.dailyDateKey().empty());

    // Tournament first, then an ordinary game. This is the PRE-EXISTING half of B25: tournamentActive_
    // was set by startTournamentSession() and cleared by nothing, so the "ordinary" game that followed a
    // tournament ran with tournament AI advice (Game.cpp:1432), the tournament's escalating minimum bet,
    // and finishTournamentShoe() at the shoe boundary.
    game.startTournamentSession();
    REQUIRE(game.tournamentActiveForTest());
    game.startNewGame();
    CHECK_FALSE(game.tournamentActiveForTest());
    CHECK_FALSE(game.dailyActive());
}

TEST_CASE("B25: a tournament inherits no Daily state, and vice versa", "[Session][Exclusivity][B25]")
{
    test_support::IsolatedHome home("b25");

    Game game(true);
    game.startDailySession("2026-11-12", false);
    game.startTournamentSession();
    CHECK(game.tournamentActiveForTest());
    CHECK_FALSE(game.dailyActive());

    game.startDailySession("2026-11-13", false);
    CHECK(game.dailyActive());
    CHECK_FALSE(game.tournamentActiveForTest());
    CHECK(game.dailyDateKey() == "2026-11-13");
}

TEST_CASE("B25: leaving a Daily for an ordinary game books nothing for the ordinary shoe", "[Session][Exclusivity][B25]")
{
    test_support::IsolatedHome home("b25");

    Game game(true);
    game.startDailySession("2026-11-12", false);
    game.startNewGame();

    // The ordinary shoe must not be scored as a Daily. If dailyActive_ leaked, the shoe boundary at
    // Game.cpp's LastHand branch would call finishDailyShoe() and write a Daily result for ordinary play.
    // Starting a Daily books no result at all (beginDailyShoe only marks the shoe active), so an empty
    // history here is the exact statement "nothing was booked".
    CHECK_FALSE(game.dailyActive());
    const PlayerProfile after = PlayerProfile::load(home.profilePath().string());
    CHECK(after.dailyHistory.empty());
    CHECK(after.dailyStreak == 0);
    CHECK(after.dailyLastPlayed.empty());
}

