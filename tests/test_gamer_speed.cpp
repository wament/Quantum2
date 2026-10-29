#include <catch2/catch_all.hpp>

#include <Constants.hpp>
#include <Game.hpp>

using Catch::Approx;

// Feature 2 (Gamer speed): per-step timing derives from the monitor's refresh rate, so playback speed scales with
// hardware. The derivation is pure and headless-safe, which is why it can be tested without an SFML window; the
// refresh-rate value itself is injected for tests and detected from hardware at full-mode startup.
TEST_CASE("Gamer speed derives its per-step interval from the monitor refresh rate", "[GameSpeed][Constants]")
{
    // One step advances per frame, so seconds-per-step = 1/refreshRate: a higher-refresh monitor plays strictly faster.
    CHECK(GameTiming::gamerStepInterval(60.0f) == Approx(1.0f / 60.0f));
    CHECK(GameTiming::gamerStepInterval(144.0f) == Approx(1.0f / 144.0f));
    CHECK(GameTiming::gamerStepInterval(240.0f) == Approx(1.0f / 240.0f));

    // An undetected or absurdly low rate never yields a negative or exploding interval; it falls back to the default.
    CHECK(GameTiming::gamerStepInterval(0.0f) == GameTiming::DEFAULT_REFRESH_HZ);
    CHECK(GameTiming::gamerStepInterval(1.0f) == Approx(GameTiming::DEFAULT_REFRESH_HZ));

    // At a high refresh rate Gamer is strictly faster per step than Hyper's fixed tiny delay, honoring the goal of
    // pushing playback as fast as the hardware allows.
    CHECK(GameTiming::gamerStepInterval(240.0f) < GameTiming::npcActionDelay(GameSpeed::Hyper));
}

// The injected refresh rate flows through to what Gamer timing reads, so pure-logic paths can be driven headlessly
// (the full-mode constructor instead detects the real monitor rate from hardware at startup).
TEST_CASE("Gamer speed reads its injectable monitor refresh rate off Game", "[GameSpeed]")
{
    Game game(true); // headless: no SFML window, no audio device

    game.setRefreshRateForTest(144.0f);
    CHECK(game.refreshRateForTest() == Approx(144.0f));

    game.setGameSpeedForTest(GameSpeed::Gamer);
    CHECK(static_cast<GameSpeed>(game.gameSpeedForTest()) == GameSpeed::Gamer);
}

TEST_CASE("isAutoPlaying treats AutoPlay, Hyper, and refresh-rate Gamer as continuous autoplay", "[GameSpeed]")
{
    // The three automated tiers (AutoPlay, Hyper, refresh-rate Gamer) advance every seat on basic strategy without
    // waiting for input...
    CHECK(GameTiming::isAutoPlaying(GameSpeed::AutoPlay));
    CHECK(GameTiming::isAutoPlaying(GameSpeed::Gamer));
    CHECK(GameTiming::isAutoPlaying(GameSpeed::Hyper));

    // ...while the manual tiers (Fast/Normal/Slow) still let the player drive their own cards.
    CHECK(!GameTiming::isAutoPlaying(GameSpeed::Fast));
    CHECK(!GameTiming::isAutoPlaying(GameSpeed::Normal));
    CHECK(!GameTiming::isAutoPlaying(GameSpeed::Slow));
}

TEST_CASE("dealerRevealInterval matches player pace in Gamer/Hyper, fixed beat otherwise", "[GameSpeed]")
{
    // In the automated-fast tiers the dealer reveals cards at the SAME interval players act...
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Gamer, 240.0f) == Approx(GameTiming::gamerStepInterval(240.0f)));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Hyper, 0.0f) == Approx(GameTiming::npcActionDelay(GameSpeed::Hyper)));

    // ...while every other speed keeps the fixed, watchable beat so a casual player can follow each card reveal.
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Fast, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Normal, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Slow, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::AutoPlay, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
}
