#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <cstdlib>            // getenv/setenv/unsetenv for IsolatedHome
#include <unistd.h>           // getpid (POSIX; the suite is Linux-only, as is Player::getSavePath's $HOME)
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

    // ---------------------------------------------------------------------
    // B17 — hermetic HOME for the tests that touch the real profile path.
    //
    // Player::getSavePath() (src/Player.cpp:14) resolves
    //   $HOME/.local/share/blackjack/saves/player_profile.json
    // from the environment on every call, so a test that drives a real Game/Player writes the DEVELOPER's career
    // profile unless HOME is redirected. Five test files each rolled their own redirect, and every one of them used a
    // FIXED directory name (/tmp/bj_b1_test, /tmp/bj_u8_test, /tmp/bj_b7_test, /tmp/bj_t1_autosave_home,
    // /tmp/bj_name_entry_0). ctest + Catch2 run each TEST_CASE in its OWN PROCESS, so those directories are shared by
    // every concurrently-scheduled test: one test's committed profile is loaded by another, and assertions about a
    // "fresh" profile become a race. (test_name_entry.cpp's counter is `static int` -- per-process, so it is always 0
    // and its "per-invocation unique" dir is in fact bj_name_entry_0 for every process.)
    //
    // This RAII guard fixes that in one place. The directory name embeds the PID (unique across the processes ctest
    // spawns) and a per-process counter (unique across several guards inside one process), so no two live tests can
    // collide. HOME is restored and the tree removed on destruction, including when a Catch2 REQUIRE throws.
    // ---------------------------------------------------------------------
    class IsolatedHome
    {
      public:
        explicit IsolatedHome(const std::string& tag = "home")
            : hadHome_(std::getenv("HOME") != nullptr)
        {
            if (hadHome_)
                original_ = std::getenv("HOME");

            static int counter = 0;   // per-process; disambiguates several guards inside one TEST_CASE
            dir = std::filesystem::temp_directory_path() /
                  ("bj_" + tag + "_home_" + std::to_string(static_cast<long>(::getpid())) + "_" +
                   std::to_string(counter++));
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);       // never inherit a previous run's profile
            std::filesystem::create_directories(dir, ec);
            ::setenv("HOME", dir.string().c_str(), 1);
        }

        ~IsolatedHome()
        {
            if (hadHome_)
                ::setenv("HOME", original_.c_str(), 1);
            else
                ::unsetenv("HOME");
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }

        IsolatedHome(const IsolatedHome&) = delete;
        IsolatedHome& operator=(const IsolatedHome&) = delete;

        // The temp dir HOME currently points at.
        const std::filesystem::path& path() const { return dir; }

        // Where Player::getSavePath() will resolve while this guard is alive.
        std::filesystem::path profilePath() const
        {
            return dir / ".local" / "share" / "blackjack" / "saves" / "player_profile.json";
        }

      private:
        std::filesystem::path dir;
        bool hadHome_;
        std::string original_;
    };

} // namespace test_support
