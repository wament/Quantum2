// Phase 0.5 shared seam (IDEAS-REVIEWED.md §B.3): `Game::persistsProgress()`.
//
// Five planned features (#1 Decision Fork, #2 Simulation Lab, #4 Shoe Codes, #8 Count-Only Drill,
// #10 Onboarding Campaign) run on the LIVE `Game` and must never touch the saved career profile.
// The two gates the codebase already has do not cover them: `Player::setPersistable(false)` is a
// BLANKET write-off for a seat that builds its own table (the Daily, the tournament), and the narrow
// commit (`commitDailyResult` / `commitTournamentShoe`) records exactly one thing. A drill running on
// the live seat needs neither — it needs "play normally, persist nothing".
//
// The predicate generalises the first gate, and it has to cover THREE paths, not one:
//   1. the write path       — every seat-save site (the list B1 enumerated as "seven"; it is nine)
//   2. the history path     — addRoundRecord / addShoeRecord pollute the seat's IN-MEMORY profile, so
//                             the NEXT ordinary save would write the drill's rounds. Gating only the
//                             write is not enough.
//   3. the achievement path — achievementManager_ unlocks live and is stamped onto every save.
//
// The headline test is the one §B.3 specifies: **a run leaves the saved profile byte-identical**.
// Test IDs are provenance, not addresses (NEXT_STEPS, "Reading these documents").

#include <catch2/catch_all.hpp>

#include "test_support.hpp"

#include "Achievement.hpp"
#include "Card.hpp"
#include "Constants.hpp"
#include "Game.hpp"
#include "Player.hpp"
#include "PlayerProfile.hpp"
#include "Serialization.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// The alternating 10/7 shoe the T1 cadence test pins: the human's two cards are always a hard 17,
// never a pair (so never a split) and never a natural, so EXACTLY ONE hand settles per driven round.
// "Hands settled" then equals "rounds driven", which is what makes the write-count assertions exact.
std::vector<Card> neutral_shoe(int n = 240)
{
    const Suit suits[4] = {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs};
    std::vector<Card> shoe;
    shoe.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        shoe.push_back(test_support::make_card((i % 2 == 0) ? 10 : 7, suits[i % 4]));
    return shoe;
}

// Drives `rounds` complete rounds through the real round machine: bet -> stand -> dealer -> settle.
// Every iteration calls nextRoundForTest() first, including the first. That is not redundant:
// evaluateHands() CLEARS game.handActions at the end of a round, and the action methods index it by
// getGlobalHandIndexForPlayer() with operator[] — so a driver that starts a second batch of rounds
// without re-entering nextRound() leaves stand() indexing an empty vector. ASan caught exactly that as
// a heap-use-after-free. nextRound() is what re-sizes handActions to the seat vector, so it is part of
// "begin a round", not part of "continue one".
void drive_rounds(Game& game, int rounds)
{
    for (int r = 0; r < rounds; ++r)
    {
        game.nextRoundForTest();
        game.confirmBet();
        game.stand();
        test_support::settleRound(game);
    }
}

// A deterministic shoe plus a PERSISTING human seat. startTestRound() seats the human through the NPC
// constructor, which leaves the seat non-persistable, so the flag is set here exactly like
// tests/test_autosave_every_5_hands.cpp does it. Game is non-movable, so this configures in place.
void seat_persisting_human(Game& game)
{
    test_support::seed_deck(game, neutral_shoe());
    game.startTestRound();
    game.testPlayer(0).setPersistable(true);
}

std::string read_whole_file(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Rounds recorded on the seat's in-memory profile, read through the shipped schema. `currentHistory`
// is private on PlayerProfile with no accessor, and the schema is the contract that matters here —
// it is what a later save would write.
std::size_t profile_round_records(Game& game)
{
    const nlohmann::json j = game.testPlayer(0).getProfile();
    return j.at("currentHistory").size();
}

std::size_t profile_shoe_records(Game& game)
{
    return game.testPlayer(0).getProfile().getLongHistory().size();
}

// A settled round whose human seat holds a natural blackjack: enough to unlock Natural21 through the
// real recordHandAchievements() path (AchievementFeeder derives isBlackjack from the record's cards).
RoundRecord blackjack_round()
{
    RoundRecord round;
    round.trueCount = 1.0f;
    HandOutcome human;
    human.playerIndex = 0;
    human.cards = {test_support::make_card(1, Suit::Spades), test_support::make_card(10, Suit::Hearts)};
    human.wager = 10;
    human.payout = 25;
    human.won = true;
    round.playerHands.push_back(human);
    return round;
}

} // namespace

TEST_CASE("persistsProgress(false): a full run leaves the saved profile byte-identical",
          "[PersistsProgress]")
{
    test_support::IsolatedHome home("pp_bytes");
    const std::filesystem::path profilePath = home.profilePath();

    Game game(true);
    seat_persisting_human(game);

    // Phase 1 - ordinary play, so a real career file exists with rounds in it.
    drive_rounds(game, static_cast<int>(Constants::AUTO_SAVE_EVERY_HANDS) + 1);
    REQUIRE(game.testPlayer(0).saveWriteCount >= 1);
    REQUIRE(std::filesystem::exists(profilePath));
    const std::string before = read_whole_file(profilePath);
    REQUIRE_FALSE(before.empty());
    const int writesBefore = game.testPlayer(0).saveWriteCount;

    // Phase 2 - the identical play, marked non-persisting. Crosses the autosave cadence, a shoe
    // boundary and the B7 exit choke point: every write path the game owns.
    game.setPersistsProgress(false);
    CHECK_FALSE(game.persistsProgress());

    drive_rounds(game, 12);
    game.startNewShoeWithSeedForTest(0xC0FFEEULL);   // deterministic boundary (B18/B20: no OS RNG)
    test_support::seed_deck(game, neutral_shoe());   // re-seed after the boundary
    drive_rounds(game, 6);
    game.persistOnExit();

    CHECK(game.testPlayer(0).saveWriteCount == writesBefore);
    CHECK(read_whole_file(profilePath) == before);

    // Phase 3 - the gate is reversible: the next ordinary session writes again. Without this the
    // byte-identical assertion above is satisfiable by a game that simply never writes at all.
    game.setPersistsProgress(true);
    CHECK(game.persistsProgress());
    drive_rounds(game, static_cast<int>(Constants::AUTO_SAVE_EVERY_HANDS) + 1);
    CHECK(game.testPlayer(0).saveWriteCount > writesBefore);
    CHECK(read_whole_file(profilePath) != before);
}

TEST_CASE("persistsProgress defaults to true and the same driver does write",
          "[PersistsProgress]")
{
    test_support::IsolatedHome home("pp_default");

    Game game(true);
    seat_persisting_human(game);

    CHECK(game.persistsProgress());   // ordinary play persists; nothing had to opt in

    drive_rounds(game, static_cast<int>(Constants::AUTO_SAVE_EVERY_HANDS) + 1);
    CHECK(game.testPlayer(0).saveWriteCount >= 1);
    CHECK(std::filesystem::exists(home.profilePath()));
    CHECK(profile_round_records(game) >= 1);
}


TEST_CASE("persistsProgress(false): the run pollutes no history in the seat's profile",
          "[PersistsProgress]")
{
    // The half a write-only gate misses: addRoundRecord / addShoeRecord mutate the seat's IN-MEMORY
    // profile. A drill that gates the write but appends rounds leaves them sitting there, and the
    // NEXT ordinary save writes them - career progress corrupted by a practice session.
    test_support::IsolatedHome home("pp_history");

    Game game(true);
    seat_persisting_human(game);
    drive_rounds(game, 3);
    const std::size_t roundsBefore = profile_round_records(game);
    const std::size_t shoesBefore = profile_shoe_records(game);
    REQUIRE(roundsBefore >= 1);

    game.setPersistsProgress(false);
    drive_rounds(game, 12);
    game.persistOnExit();

    CHECK(profile_round_records(game) == roundsBefore);
    CHECK(profile_shoe_records(game) == shoesBefore);

    // And the seat's live counters prove the rounds really were played and settled - the gate is not
    // passing because nothing happened.
    CHECK(game.testPlayer(0).getStats().handsFinished >= 15);
}

TEST_CASE("persistsProgress(false): the shoe boundary books no shoe and writes nothing",
          "[PersistsProgress]")
{
    test_support::IsolatedHome home("pp_shoe");

    Game game(true);
    seat_persisting_human(game);

    // Control: with persistence on, the exit choke point books the finished shoe.
    drive_rounds(game, 2);
    game.persistOnExit();
    const std::size_t shoesAfterControl = profile_shoe_records(game);
    CHECK(shoesAfterControl == 1);

    // The same boundary with persistence off: no shoe booked, no write.
    const int writesBefore = game.testPlayer(0).saveWriteCount;
    game.setPersistsProgress(false);
    drive_rounds(game, 3);
    game.persistOnExit();

    CHECK(profile_shoe_records(game) == shoesAfterControl);
    CHECK(game.testPlayer(0).saveWriteCount == writesBefore);
}

TEST_CASE("persistsProgress(false): the achievement engine sees no settled round",
          "[PersistsProgress]")
{
    // Achievements ride every save: the unlocked set is stamped onto the profile at each write site.
    // A drill that unlocks "First blackjack" is a practice session gifting career progress.
    Game game(true);
    test_support::seed_deck(game, neutral_shoe());
    game.startTestRound();

    game.setPersistsProgress(false);
    const std::size_t unlockedBefore = game.achievementsForTest().unlockedCount();
    game.recordHandAchievements(blackjack_round());
    CHECK(game.achievementsForTest().unlockedCount() == unlockedBefore);
    CHECK_FALSE(game.achievementsForTest().isUnlocked(Achievement::Id::Natural21));

    // Control: the same round through the same call, with the predicate on, does unlock it.
    game.setPersistsProgress(true);
    game.recordHandAchievements(blackjack_round());
    CHECK(game.achievementsForTest().isUnlocked(Achievement::Id::Natural21));
}

TEST_CASE("persistsProgress(false): the seat's own persistable gate is not the mechanism",
          "[PersistsProgress]")
{
    // The predicate must gate at the Game, not by flipping Player::persistable: the Daily and the
    // tournament already own that flag (they set it false on a seat they build themselves), and a
    // seat rebuilt mid-session (startTestRound, startNewGame) would silently drop a mirror.
    test_support::IsolatedHome home("pp_seat");

    Game game(true);
    seat_persisting_human(game);
    drive_rounds(game, static_cast<int>(Constants::AUTO_SAVE_EVERY_HANDS) + 1);
    const int writesBefore = game.testPlayer(0).saveWriteCount;
    REQUIRE(writesBefore >= 1);

    game.setPersistsProgress(false);
    CHECK(game.testPlayer(0).isPersistable());   // the seat flag is untouched - the gates are separate

    drive_rounds(game, 12);
    CHECK(game.testPlayer(0).saveWriteCount == writesBefore);

    // A seat rebuilt while the predicate is off must not start writing again. startTestRound() seats a
    // FRESH Player, so saveWriteCount restarts at 0 -- assert on the new seat, not against the old count.
    game.startTestRound();
    game.testPlayer(0).setPersistable(true);
    drive_rounds(game, static_cast<int>(Constants::AUTO_SAVE_EVERY_HANDS) + 1);
    CHECK(game.testPlayer(0).saveWriteCount == 0);
}

TEST_CASE("a round boundary re-sizes handActions before any action indexes it",
          "[PersistsProgress][handActions]")
{
    // Found by ASan while this file's driver was being written, and worth pinning: evaluateHands()
    // CLEARS game.handActions at the end of a round, and the action methods (stand / hit / double /
    // surrender) index it with operator[] at getGlobalHandIndexForPlayer(...). A caller that begins a
    // round WITHOUT re-entering nextRound() -- the function that re-sizes handActions to the seat
    // vector -- indexes an empty vector: a heap-use-after-free, not a clean failure.
    //
    // Production reaches an action only from InputHandler during PlayerTurn, which is always after
    // nextRound(), so this is a latent hazard and not a live defect. CONTEXT §6.9 cleared the ORDER of
    // the flat indexing (seats act strictly in sequence); this row is about its BOUNDS, which §6.9 does
    // not cover. The assertion pins the invariant every round-driving test silently depends on.
    Game game(true);
    seat_persisting_human(game);

    drive_rounds(game, 1);
    const std::size_t seats = game.playersForTest().size();

    // The settled round cleared the log: acting here would index past the end.
    CHECK(game.handActionsSizeForTest() == 0);

    // nextRound() is what re-opens it, and it must cover every seat before any action runs.
    game.nextRoundForTest();
    CHECK(game.handActionsSizeForTest() >= seats);

    // And the flat index a split would produce stays inside the vector, because split() appends a slot.
    game.confirmBet();
    game.stand();
    CHECK(game.handActionsSizeForTest() >= seats);
}


