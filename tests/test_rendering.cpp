// Rendering mode backdrop/render table and Game::update() AppMode routing to each subsystem.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include "Game.hpp"
#include "GameRenderer.hpp"
#include "test_support.hpp"
#include <Card.hpp>
#include <Constants.hpp>
#include <Game.hpp>
#include <catch2/catch_all.hpp>

using namespace Constants;

// Shared test fixtures/helpers lifted from every member file into one anonymous namespace (identical copies de-duped).
namespace {
void seedRound(Game& game, std::initializer_list<Card> deck)
    {
        test_support::seed_deck(game,deck);
        game.startTestRound();
    }

std::vector<std::pair<AppMode, std::optional<sf::Color>>> expectedBackdrops()
{
    return {
        // nullopt backdrops clear to default (plain clear + backgroundSprite felt); the themed modes set their own color.
        {AppMode::MainMenu,         std::nullopt},   // felt behind the menu; drawMainMenu paints it itself
        {AppMode::Playing,          std::nullopt},   // felt behind the table
        {AppMode::Paused,           std::nullopt},   // felt behind the paused table
        {AppMode::StrategyTrainer,  sf::Color(96, 0, 128)}, // royal-purple training backdrop
        {AppMode::Tournament,       std::nullopt},    // felt like Playing; tournament overlays are drawn on top
        {AppMode::Achievements,     sf::Color(18, 28, 24)}, // dark-slate list backdrop
        {AppMode::GameRules,        sf::Color(30, 30, 40)}, // rules-config backdrop
        {AppMode::StatsDashboard,   sf::Color(15, 22, 30)}, // deep-teal dashboard (own paint)
        {AppMode::ReplayHistory,    std::nullopt},   // felt behind replay
        {AppMode::ReplayPlaying,    std::nullopt},   // felt behind replay
    };
}

} // namespace

TEST_CASE("Game::update routes Playing to RoundEngine and advances past PlayerTurn", "[Game][UpdateDispatch]")
{
    Game game(true);
    seedRound(game, {test_support::make_card(10, Suit::Spades), test_support::make_card(6, Suit::Hearts),
                    test_support::make_card(8, Suit::Clubs),     test_support::make_card(1, Suit::Diamonds),
                    test_support::make_card(10, Suit::Hearts),   test_support::make_card(2, Suit::Clubs)});
    game.stand();  // human acts -> round proceeds toward the dealer turn

    game.setAppModeForTest(AppMode::Playing);
    for (int i = 0; i < 200 && game.roundStateForTest() != RoundState::Cleanup; ++i)
        game.update(100.0f);  // Playing -> RoundEngine.tick(deltaTime, *this)

    // The ONLY thing that advances roundState is RoundEngine::tick (invoked here via update()'s Playing branch). Leaving the
    // player's turn therefore proves update() routed Playing to RoundEngine and actually ticked — a regression returning early
    // would leave it parked at PlayerTurn. Frame-robust: no dependence on reaching Cleanup or any specific card values, so a
    // reshuffle mid-drive cannot break the assertion.
    REQUIRE(game.roundStateForTest() != RoundState::PlayerTurn);
}

// Every non-Playing AppMode routes to its own updater and returns before RoundEngine (identical early-return wiring to the
// original guards). Driving update(0) in any of these must therefore leave the live round untouched while appMode stays put.
// Sub-system modes self-build their state machine on entry, so build them first (like the real game's full constructor).
TEST_CASE("Game::update routes every non-Playing mode without advancing the playing round", "[Game][UpdateDispatch]")
{
    Game game(true);
    seedRound(game, {test_support::make_card(5, Suit::Hearts), test_support::make_card(6, Suit::Clubs)});  // one easy dealt round

    auto driveInMode = [&](AppMode mode) {
        game.setAppModeForTest(mode);
        const RoundState beforeUpdate = game.roundStateForTest();   // the round's state just before this update()
        game.update(0.0f);
        REQUIRE(game.getAppModeForTest() == mode);       // routed to this mode's own updater, did not switch modes away
        REQUIRE(game.roundStateForTest() == beforeUpdate);  // RoundEngine was skipped -> no turn/round progress happened
    };

    // Null-safe view/pause/rules modes: their updater runs inline (GameRules) or returns early; none touch the playing round.
    driveInMode(AppMode::MainMenu);
    driveInMode(AppMode::Paused);
    driveInMode(AppMode::Achievements);
    driveInMode(AppMode::GameRules);

    // Sub-system modes: start*Session builds their state machine headlessly (mirroring the full constructor), then update()
    // advances only that mode's own updater before returning to the guard below.
    game.startStatsSession();
    driveInMode(AppMode::StatsDashboard);

    // A tournament builds a real table and opens this stage's PlaceBets screen (a fresh four-deck shoe) so the human places a bet to
    // deal; update(0) with no auto-play must not advance past that betting screen. driveInMode captures the post-start state as its reference.
    game.startTournamentSession();
    driveInMode(AppMode::Tournament);

    game.startTrainerSession();
    driveInMode(AppMode::StrategyTrainer);
}

// Strong routing proof for the StrategyTrainer mode: its updater advances session time on update(), which an empty/omitted case
// body would NOT. startTrainerSession now builds the trainer headlessly (like Tournament / StatsDashboard) so this needs no UI.
TEST_CASE("Game::update routes StrategyTrainer to its updater and advances session time", "[Game][UpdateDispatch]")
{
    Game game(true);
    // secondsPerSession > 0 makes the session timed/active so update() advances elapsedSeconds (a 0-second session completes
    // immediately and later updates early-return). startTrainerSession also builds the trainer headlessly here.
    game.startTrainerSession(/*deckCount=*/6, /*secondsPerSession=*/3600);
    const float before = game.trainerElapsedSecondsForTest();

    game.setAppModeForTest(AppMode::StrategyTrainer);
    game.update(1.5f);  // StrategyTrainer -> trainer->update(1.5)

    REQUIRE(game.trainerElapsedSecondsForTest() > before + 1.0f);
}


TEST_CASE("GameRenderer routes every mode through the single unified dispatch table", "[renderer][mode-dispatch]")
{
    GameRenderer r;

    for (const auto& [mode, expectedBackdrop] : expectedBackdrops())
    {
        const ModeEntry entry = r.modeEntryForTest(mode);

        // Backdrop color matches its documented value: an explicit sf::Color for the five themed modes, nullopt for felt
        // modes (default clear + backgroundSprite). One table => a mode's clear color can't drift from its draw fn.
        REQUIRE(entry.backdrop == expectedBackdrop);

        // Every known mode routes to a non-empty render fn so draw() reaches per-mode presentation; an unknown AppMode gets
        // the default entry (nullopt backdrop + no-op render), matching the old else-branch + switch fall-through.
        REQUIRE(entry.render != nullptr);
    }
}

TEST_CASE("GameRenderer falls back to the default entry for an unknown mode", "[renderer][mode-dispatch]")
{
    GameRenderer r;

    const ModeEntry fallback = r.modeEntryForTest(static_cast<AppMode>(99));   // no such AppMode -> find() misses -> fallback

    REQUIRE(!fallback.backdrop.has_value());          // default backdrop (plain clear + backgroundSprite felt)
    REQUIRE(fallback.render != nullptr);              // safe no-op render, so draw() never throws on an unknown mode
}
