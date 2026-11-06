#include <catch2/catch_all.hpp>

#include "Game.hpp"

// B6 -- the rules-panel keyboard edit is edge-triggered, so holding a direction key for many frames steps the selected rule
// exactly once (one tap = one step), not once per frame. We hold Right down and drive update() across 25 frames; the result
// must be identical to driving it with the key held for just ONE frame. Before the fix every frame re-edited, so holding
// longer stepped many more places through the deck brackets -- the "single tap jumped six decks" symptom. Headless: held-key
// state is injected via setRulesKeyHeldForTest() instead of real SFML hardware keys (isKeyPressed always reads false here).
// Up/Down highlight navigation keeps its held-key repeat by design; only the Left/Right edit was unthrottled.

TEST_CASE("B6: a held rules-panel key edits exactly once regardless of how long it stays down", "[GameRules][Input]")
{
    // Scenario A -- hold Right down for 25 frames while driving update() (simulates an uncapped-fps tap spanning many frames).
    {
        Game game(true);                              // headless: no window / assets / replay / HUD
        game.setAppModeForTest(AppMode::GameRules);   // panel open; gameRulesSelectedRow defaults to the Decks row (0)
        GameRules base;                               // deterministic start
        base.numDecks = 4;                            // a DECK_BRACKETS value so stepping actually moves it
        game.setGameRulesForTest(base);
        game.setRulesKeyHeldForTest(/*left=*/false, /*right=*/true);  // Right held from frame one
        for (int i = 0; i < 25; ++i)
            game.update(0.0f);                        // GameRules case ignores deltaTime and returns before RoundEngine
        const int afterLongHold = game.getGameRulesForTest().numDecks;

        // Scenario B -- identical starting state, hold Right down but drive only ONE frame (a single tap).
        Game other(true);
        other.setAppModeForTest(AppMode::GameRules);
        other.setGameRulesForTest(base);
        other.setRulesKeyHeldForTest(/*left=*/false, /*right=*/true);
        other.update(0.0f);
        const int afterOneTap = other.getGameRulesForTest().numDecks;

        // One tap is exactly one step: holding longer must not step the rule any further (a repeat-storm makes A != B).
        CHECK(afterLongHold == afterOneTap);

        // Injected held state must reset so later tests read real isKeyPressed() again.
        game.resetRulesKeyOverrideForTest();
        other.resetRulesKeyOverrideForTest();
    }
}
