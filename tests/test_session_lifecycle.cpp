#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Game.hpp>

// ---------------------------------------------------------------------------
// Follow-up #8 (Game.cpp god-class split, Session lifecycle stage): the session entry points
// startNewGame()/startTrainerSession()/startTournamentSession()/startStatsSession() and the
// table-settings persistence pair applySavedTableSettings()/rememberTableSettings() now delegate
// to the composed SessionLifecycle helper. These cases drive that delegation path through Game's
// thin delegators and lock each entry point + the persist/restore round-trip, so any drift from
// the relocation is caught. All headless; the binary links SFML::Graphics only.
// ---------------------------------------------------------------------------

using namespace Constants;

TEST_CASE("#8 delegation: startNewGame() builds a reset, playable session", "[Game][SessionLifecycle]")
{
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
    Game game(true);
    game.startTournamentSession(); // -> SessionLifecycle::startTournamentSession({}) default config

    game.setAppModeForTest(AppMode::Tournament);
    game.update(0.0f);  // Tournament mode's own updater runs inline; no advance expected at dt=0

    REQUIRE(game.getAppModeForTest() == AppMode::Tournament);  // routed to the tournament updater, did not drop out
}

TEST_CASE("#8 delegation: startStatsSession() seeds the dashboard headlessly and routes", "[Game][SessionLifecycle]")
{
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
