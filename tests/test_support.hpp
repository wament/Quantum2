#pragma once

#include <vector>
#include "Card.hpp"
#include "Deck.hpp"
#include "RoundEngine.hpp"   // RoundEngine + its tick(); RoundState is declared in Game.hpp below.
#include "Game.hpp"

// Shared test infrastructure for the Blackjack2 unit suite (one TU per feature, but they repeat setup).
// Header-only + inline so it adds no link units and never triggers ODR issues across translation units.
namespace test_support {

    // A deterministic playing card (value/suit), replacing the identical helper each game-flow file used to define
    // locally as makeCard()/card(). Pure data construction, so behavior is unchanged wherever it's substituted in.
    inline Card make_card(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    // Seed a headless Game with an optional deterministic shoe. Mirror of Game::setDeck (reversed into deal order);
    // an empty vector leaves the default shuffled shoe. Reference-based because Game is non-movable.
    inline void seed_deck(Game& game, const std::vector<Card>& cards = {})
    {
        if (!cards.empty())
            game.setDeck(cards);
    }

    // Seed a headless Game's shoe AND advance it to the first PlayerTurn in one call. For cases where setDeck is
    // immediately followed by startTestRound() (nothing else must happen between, e.g. registering a side-bet intent),
    // collapsing them here removes that boilerplate; it does NOT reorder anything. seats == seat count (1 = human only).
    inline void start_round(Game& game, const std::vector<Card>& cards = {}, int seats = 1)
    {
        seed_deck(game, cards);
        game.startTestRound(seats);
    }

    // Advance the shared round-state machine by exactly one frame. deltaTime is ignored by player/turn transitions, so this
    // replaces the identical per-file tickOnce()/card() driver and guarantees every test drives RoundEngine::tick the same way.
    inline void tickFrame(Game& game, float deltaTime = 0.0f)
    {
        RoundEngine engine;
        engine.tick(deltaTime, game);
    }

    // Drive frames until the round leaves PlayerTurn (all hands resolved -> DealerTurn). Bounded so a stuck state cannot spin
    // forever; never invokes a player action, which is how the "no input after split" clauses are proven end-to-end.
    inline void advanceToDealerTurn(Game& game)
    {
        for (int i = 0; i < 25 && game.roundStateForTest() == RoundState::PlayerTurn; ++i)
            tickFrame(game);
    }

    // Drive frames until the round settles to Cleanup. A large delta advances the dealer-turn reveal-pacing timers every frame
    // (they elapse on Game::deltaTime, a member, NOT the tick() arg), so the dealer draws/stands and evaluateHands() completes
    // in finite time. settleDelta defaults to 100.0f, matching what the settlement tests already use.
    inline void settleRound(Game& game, float settleDelta = 100.0f)
    {
        game.deltaTime = settleDelta;          // reveal pacing reads this member each dealerTurn, so set it before ticking
        RoundEngine engine;
        for (int i = 0; i < 200; ++i)         // generous cap; the loop breaks as soon as the round leaves Player/DealerTurn
        {
            RoundState rs = game.roundStateForTest();
            if (rs == RoundState::PlayerTurn || rs == RoundState::DealerTurn)
                engine.tick(settleDelta, game);
            else
                break;
        }
    }

} // namespace test_support
