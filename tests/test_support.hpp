#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include "Card.hpp"
#include "Deck.hpp"
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

    // Advance the shared round-state machine by exactly one frame via Game::advanceTurnState (the folded RoundEngine::tick
    // body). deltaTime is ignored by player/turn transitions, so every test drives it identically — no per-file driver and no
    // dependency on the now-removed RoundEngine class. Calling advance() here mirrors update()'s Playing path but skips that
    // mode guard, exactly as the old tick() did regardless of AppMode.
    inline void tickFrame(Game& game, float deltaTime = 0.0f)
    {
        game.advanceTurnState(deltaTime);
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
        for (int i = 0; i < 200; ++i)         // generous cap; the loop breaks as soon as the round leaves Player/DealerTurn
        {
            RoundState rs = game.roundStateForTest();
            if (rs == RoundState::PlayerTurn || rs == RoundState::DealerTurn)
                game.advanceTurnState(settleDelta);
            else
                break;
        }
    }

    // Build a unique-per-tag temporary profile path for roundtrip/legacy tests, replacing the identical per-file helper.
    inline std::filesystem::path temp_path(const std::string& tag, const std::string& name)
    {
        return std::filesystem::temp_directory_path() / ("bj_" + tag + "_test_" + name + ".json");
    }

} // namespace test_support
