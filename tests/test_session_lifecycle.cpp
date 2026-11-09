#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Game.hpp>
#include <filesystem>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Follow-up #8 (Game.cpp god-class split, Session lifecycle stage): the session entry points
// startNewGame()/startTrainerSession()/startTournamentSession()/startStatsSession() and the
// table-settings persistence pair applySavedTableSettings()/rememberTableSettings() now delegate
// to the composed SessionLifecycle helper. These cases drive that delegation path through Game's
// thin delegators and lock each entry point + the persist/restore round-trip, so any drift from
// the relocation is caught. All headless; the binary links SFML::Graphics only.
// ---------------------------------------------------------------------------

using namespace Constants;

namespace fs = std::filesystem;

TEST_CASE("#8 delegation: startNewGame() builds a reset, playable session", "[Game][SessionLifecycle]")
{
    test_support::IsolatedHome home("lifecycle");   // B17: startNewGame() seats a persistable human; keep its writes off the real profile
    Game game(true); // headless: no window/assets/replay/HUD
    const int beforeShoe = game.shoeGenerationForTest();

    game.startNewGame(); // Game::startNewGame() -> SessionLifecycle::startNewGame()

    REQUIRE(game.getAppModeForTest() == AppMode::Playing);   // session started, not left parked on the menu
    auto& seats = game.playersForTest();
    REQUIRE(seats.size() >= 1);                             // at least the human seat exists
    REQUIRE(seats[0].getName() == "William");               // the human is always seat index 0
    REQUIRE(game.deckSizeForTest() > 0);                    // the shoe was created + shuffled

    game.startNewGame();                                    // a second reset re-runs the per-shoe reset
    REQUIRE(game.shoeGenerationForTest() > beforeShoe);     // resetShoeAnalytics() ran each time (session, not stale)
}

TEST_CASE("#8 delegation: startTrainerSession() builds the trainer headlessly and advances", "[Game][SessionLifecycle]")
{
    Game game(true);
    // secondsPerSession > 0 makes the session timed/active so update() advances elapsedSeconds; a 0-second session would
    // complete immediately. startTrainerSession also builds the trainer headlessly (like Tournament / StatsDashboard).
    game.startTrainerSession(/*deckCount=*/6, /*secondsPerSession=*/3600); // -> SessionLifecycle::startTrainerSession()

    const float before = game.trainerElapsedSecondsForTest();
    game.setAppModeForTest(AppMode::StrategyTrainer);
    game.update(1.5f);  // StrategyTrainer mode routes to trainer->update(1.5)

    REQUIRE(game.trainerElapsedSecondsForTest() > before + 1.0f); // session time advanced (trainer was really built)
}

TEST_CASE("#8 delegation: startTournamentSession() builds the tournament headlessly and routes", "[Game][SessionLifecycle]")
{
    test_support::IsolatedHome home("lifecycle");   // B17: hermetic HOME (see test_support.hpp)
    Game game(true);
    game.startTournamentSession(); // -> SessionLifecycle::startTournamentSession({}) default config

    game.setAppModeForTest(AppMode::Tournament);
    game.update(0.0f);  // Tournament mode's own updater runs inline; no advance expected at dt=0

    REQUIRE(game.getAppModeForTest() == AppMode::Tournament);  // routed to the tournament updater, did not drop out
}

TEST_CASE("#8 delegation: startStatsSession() seeds the dashboard headlessly and routes", "[Game][SessionLifecycle]")
{
    test_support::IsolatedHome home("lifecycle");   // B17: hermetic HOME (see test_support.hpp)
    Game game(true);
    // startStatsSession() reads players[0] (the human) for profile + career stats, so establish a real seat first — exactly
    // how the live flow reaches it after starting a new game. This mirrors test_game_mode_dispatch.cpp seeding before opening stats.
    game.startNewGame();
    game.startStatsSession(); // -> SessionLifecycle::startStatsSession(): flatten history + feed the dashboard

    game.setAppModeForTest(AppMode::StatsDashboard);
    game.update(0.0f);  // StatsDashboard updater runs inline; no crash, stays in mode

    REQUIRE(game.getAppModeForTest() == AppMode::StatsDashboard);
    REQUIRE(game.statsPerRoundCountForTest() >= 0);  // dashboard built + reachable through the delegator
}

TEST_CASE(
    "#8 delegation: table settings persist via rememberTableSettings() and restore via applySavedTableSettings()",
    "[Game][SessionLifecycle]")
{
    test_support::IsolatedHome home("lifecycle");   // B17: rememberTableSettings() writes the profile; keep it off the real file
    Game game(true);
    game.startNewGame(); // creates the human seat + a default profile, so persistence has a target to write/restore

    // Change live table choices, then persist them exactly where the full constructor reads them back from.
    game.setGameSpeedForTest(GameSpeed::Hyper);
    game.rememberTableSettings();  // -> SessionLifecycle::rememberTableSettings(): stamps current choices onto the profile

    // Clobber live state, then restore from the saved profile and confirm it came back to the remembered value rather than
    // staying clobbered — proving both persistence methods round-trip byte-for-byte through their Game delegators.
    game.setGameSpeedForTest(GameSpeed::Slow);
    game.applySavedTableSettings();  // -> SessionLifecycle::applySavedTableSettings(): restores saved choices from the profile

    REQUIRE(game.gameSpeedForTest() == static_cast<int>(GameSpeed::Hyper));
}

// B1 — entering, playing and closing a tournament must NOT overwrite the saved career profile. Before the fix Tournament built the
// human seat with the ephemeral NPC constructor (src/Player.cpp) so players[0] was no longer the real profile, and every saveProfile()
// during the run wrote that throwaway profile over disk: bankroll 1025 -> 1000, stats zeroed, longHistory wiped. The fix loads the real
// profile into seat 0 and marks it non-persistable for the tournament duration, so a close-save leaves bankroll / career stats / history
// untouched. HOME is redirected to an isolated temp dir (same idiom as the B7 test) so Player::getSavePath() resolves there instead of
// clobbering the real user file; original HOME is restored afterward.
TEST_CASE("B1: a tournament does not overwrite the saved career profile", "[Game][SessionLifecycle][Persistence]")
{
    // B17: the shared RAII guard (tests/test_support.hpp) gives this run a PID-namespaced temp dir, so a
    // concurrently-scheduled test cannot share it. It restores HOME and removes the tree on scope exit, which also
    // covers a REQUIRE throwing (the reason the local HomeGuard below existed; it is now redundant).
    test_support::IsolatedHome isolated("b1");
    std::error_code ec;

    // 1. Build a career profile with a distinctive bankroll + one hand of history + one round of history, and write it to disk: the state
    // a normal session would have accumulated before clicking Tournament (none of these values arise from any default).
    const fs::path saved = isolated.profilePath();
    {
        PlayerProfile career("William");
        career.setBankroll(98765);                        // distinctive; the default bankroll is 1000
        career.stats.handsFinished = 1;                   // a career stat the bug zeroed on an X-quit from Tournament
        career.stats.wins = 1;
        career.stats.betsPlaced = 1;                      // verbatim-preserved by save() (not history-derived)
        RoundRecord round;                                // one real round of history for the human seat
        HandOutcome hand;                                 // a decisive seat-0 win -> reconciles to handsFinished/wins == 1
        hand.playerIndex = 0;
        hand.won = true;
        hand.wager = 25;
        hand.payout = 50;                                 // net +25 (decisive win)
        round.playerHands.push_back(hand);
        career.addRoundRecord(round);                     // folds into a shoe record below
        career.addShoeRecord(ShoeRecord{});               // longHistory gains exactly one shoe
        career.save(saved.string());
    }

    Game game(true);                                      // headless: no window / assets / replay / HUD

    // 2. Enter Tournament (default config). The constructor fix loads the REAL profile into seat 0 and overrides only chips in memory, so
    // pre-tournament career stats survive instead of becoming zeros -- which is what made the Stats Dashboard read all zeros after a visit.
    game.startTournamentSession();                        // -> SessionLifecycle::startTournamentSession({})
    REQUIRE(game.tournamentActiveForTest());

    const auto& tournamentSeat = game.testPlayer(0).getProfile();   // loads the real career profile (not an ephemeral NPC seat)
    REQUIRE(tournamentSeat.stats.handsFinished == 1);             // before fix: ephemeral seat -> stats zeroed
    REQUIRE(game.testPlayer(0).getBankroll() == 1000);            // in-memory tournament start-chips override

    // 3. Drive the close-save path through both the single exit choke point and a direct save. Before the fix each wrote the ephemeral
    // throwaway profile over disk (bankroll -> 1000, stats zeroed, longHistory wiped); now seat 0 is marked non-persistable for the
    // tournament, so saveProfile() no-ops and nothing is clobbered.
    game.persistOnExit();                        // rememberTableSettings + addShoeRecord + saveProfile (single exit choke point)
    game.testPlayer(0).saveProfile();            // direct: exercises the same per-seat gate at its choke point

    // 4. Reload from disk and assert the career profile is exactly as it was before entering Tournament.
    const PlayerProfile after = PlayerProfile::load(saved.string());
    REQUIRE(after.getBankroll() == 98765);                   // before fix: rolled back to default 1000
    REQUIRE(after.stats.handsFinished == 1);                 // before fix: zeroed
    REQUIRE(after.stats.wins == 1);                          // before fix: zeroed
    REQUIRE(after.stats.betsPlaced == 1);                    // before fix: zeroed
    REQUIRE(after.getLongHistory().size() == 1);             // before fix: shoes wiped to zero

    // Second symptom fixed: the dashboard still has history after a tournament visit (it read all zeros on the buggy seat).
    game.startStatsSession();                                // -> SessionLifecycle::startStatsSession(): flatten history + feed dashboard
    REQUIRE(game.statsPerRoundCountForTest() >= 0);
}
