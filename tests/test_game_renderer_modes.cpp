#include <catch2/catch_all.hpp>
#include "Game.hpp"
#include "GameRenderer.hpp"

// Feature 4-style (this is GameRenderer's unified mode dispatch, Finding #4 in CONTEXT_refactor.md): draw() routes every
// AppMode through one AppMode->{backdrop, render} table instead of a backdrop if/else-if chain + a separate dispatch switch.
// That single table is the whole point -- a mode's clear color and its draw fn live together so they can't drift out of sync
// (the exact way backdrops used to rot). This smoke test inspects modeEntryForTest() directly, with no SFML window needed,
// to lock that contract: each known mode clears to its documented backdrop color and routes to a non-empty render fn. Pure
// Graphics-only logic, like every other logic test in the Blackjack2Tests target.

namespace {
std::vector<std::pair<AppMode, std::optional<sf::Color>>> expectedBackdrops()
{
    return {
        // nullopt backdrops clear to default (plain clear + backgroundSprite felt); the themed modes set their own color.
        {AppMode::MainMenu,         std::nullopt},   // felt behind the menu; drawMainMenu paints it itself
        {AppMode::Playing,          std::nullopt},   // felt behind the table
        {AppMode::Paused,           std::nullopt},   // felt behind the paused table
        {AppMode::StrategyTrainer,  sf::Color(96, 0, 128)}, // royal-purple training backdrop
        {AppMode::Tournament,       sf::Color(25, 32, 48)}, // tournament slate (own paint)
        {AppMode::Achievements,     sf::Color(18, 28, 24)}, // dark-slate list backdrop
        {AppMode::GameRules,        sf::Color(30, 30, 40)}, // rules-config backdrop
        {AppMode::StatsDashboard,   sf::Color(15, 22, 30)}, // deep-teal dashboard (own paint)
        {AppMode::ReplayHistory,    std::nullopt},   // felt behind replay
        {AppMode::ReplayPlaying,    std::nullopt},   // felt behind replay
    };
}
}

// The unified dispatch table is the single source of truth for draw(); asserting every mode's backdrop color + a present
// render fn here proves a mode's color and its draw fn can't drift apart again.
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
