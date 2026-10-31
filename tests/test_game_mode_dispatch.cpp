#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <Card.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>
// For StrategyTrainer::elapsedSecondsForTest(), read through Game::trainerElapsedSecondsForTest() (an out-of-line accessor whose
// incomplete-type usage is contained in Game.cpp) to prove the trainer AppMode routes to trainer->update() (Phase 1 refactor:
// Game::update dispatch). The test binary links SFML::Graphics only; this stays headless.

using namespace Constants;

namespace
{
    Card card(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    // Populate a headless game with an injected shoe and one dealt round, exactly like test_game_rules.cpp. The human seat
    // then holds its initial cards and betting is set, so update()'s modes can be exercised deterministically without UI.
    void seedRound(Game& game, std::initializer_list<Card> deck)
    {
        game.setDeck(deck);
        game.startTestRound();
    }
} // namespace

// Playing must still reach RoundEngine, the sole turn/round state machine. Driving update() with a big dt advances the turn
// machine out of PlayerTurn through the SAME dispatch switch that previously used an `if (appMode == ...)` guard — if dispatch
// ever returns early for Playing, roundState stays parked at PlayerTurn and this REQUIRE fails. The soft-17 setup (hard 18 vs
// dealer up 6, hole Ace -> soft 17) forces the dealer to play its own cards under default rules so the turn actually progresses.
TEST_CASE("Game::update routes Playing to RoundEngine and advances past PlayerTurn", "[Game][UpdateDispatch]")
{
    Game game(true);
    seedRound(game, {card(10, Suit::Spades), card(6, Suit::Hearts),
                    card(8, Suit::Clubs),     card(1, Suit::Diamonds),
                    card(10, Suit::Hearts),   card(2, Suit::Clubs)});
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
    seedRound(game, {card(5, Suit::Hearts), card(6, Suit::Clubs)});  // one easy dealt round
    const RoundState initialRound = game.roundStateForTest();

    auto driveInMode = [&](AppMode mode) {
        game.setAppModeForTest(mode);
        game.update(0.0f);
        REQUIRE(game.getAppModeForTest() == mode);       // routed to this mode's own updater, did not switch modes away
        REQUIRE(game.roundStateForTest() == initialRound);  // RoundEngine was skipped -> no turn/round progress happened
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
