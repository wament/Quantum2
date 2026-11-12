#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <cstdlib>            // getenv for IsolatedHome; setenv/unsetenv are wrapped in env_set/env_unset below
#ifdef _WIN32
#include <process.h>          // _getpid (MSVC)
#include <stdlib.h>           // _putenv_s (MSVC)
#else
#include <unistd.h>           // getpid (POSIX)
#endif
#include "Card.hpp"
#include "Deck.hpp"
#include "Game.hpp"

// Shared test infrastructure for the Blackjack2 unit suite (one TU per feature, but they repeat setup).
// Header-only + inline so it adds no link units and never triggers ODR issues across translation units.
namespace test_support {

    // ---- Platform shims. The release workflow builds EVERY target on Windows/MSVC, and this header
    // is included by ~25 test TUs, so an unguarded <unistd.h> / ::getpid / ::setenv made the whole
    // Blackjack2Tests target fail to compile on Windows (MSVC C1083) and thereby blocked a release
    // that never runs a single test. These three wrappers are the entire POSIX surface the suite uses.
    //
    // What is NOT fixed, and cannot be by a shim: Player::getSavePath() (src/Player.cpp) resolves
    // $HOME and falls back to a RELATIVE path when it is unset, which is what happens on Windows.
    // So the HOME isolation below compiles on Windows but redirects a variable the product never
    // reads there -- the suite is hermetic on POSIX only, and on Windows the profile-writing tests
    // write into the build directory. That is why the release jobs build the shipped target only
    // (see .github/workflows/release.yml) and ctest stays on the Linux job.
    inline long process_id()
    {
#ifdef _WIN32
        return static_cast<long>(_getpid());
#else
        return static_cast<long>(::getpid());
#endif
    }

    inline const char* env_get(const char* name)
    {
        return std::getenv(name);   // MSVC deprecates the un-prefixed name (C4996); one wrapper, one place to silence
    }

    inline void env_set(const char* name, const char* value)
    {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        ::setenv(name, value, 1);
#endif
    }

    inline void env_unset(const char* name)
    {
#ifdef _WIN32
        _putenv_s(name, "");   // MSVC removes the variable when the value is empty
#else
        ::unsetenv(name);
#endif
    }


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
            : hadHome_(env_get("HOME") != nullptr)
        {
            if (hadHome_)
                original_ = env_get("HOME");

            static int counter = 0;   // per-process; disambiguates several guards inside one TEST_CASE
            dir = std::filesystem::temp_directory_path() /
                  ("bj_" + tag + "_home_" + std::to_string(process_id()) + "_" +
                   std::to_string(counter++));
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);       // never inherit a previous run's profile
            std::filesystem::create_directories(dir, ec);
            env_set("HOME", dir.string().c_str());
        }

        ~IsolatedHome()
        {
            if (hadHome_)
                env_set("HOME", original_.c_str());
            else
                env_unset("HOME");
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

    // -----------------------------------------------------------------------------------------------
    // B17 follow-up (2026-11-09): the per-TEST_CASE guard did not make the suite hermetic.
    //
    // B17 redirected five files and the soak measured 12 clean / 0 failing -- green, and the developer's profile md5
    // stable. It was not complete. A serial bisect over all 326 tests (md5 the profile before and after each index)
    // found **Test #99, "Resplitting tens is refused when RESPLIT_TENS_ALLOWED is false"** in
    // tests/test_settlement.cpp writing the real career profile: the live 9.3 MB file came out 1.34 MB, i.e. the
    // developer's history was replaced by a test's game state. Eight files construct `Game(true)` and drive real
    // rounds (start_round / tickFrame / settle) with **zero** guards: test_settlement (21 cases), test_side_bets (18),
    // test_game_rules (12), test_achievements (7), test_advisor_feedback (4), test_counting_box_label (3),
    // test_max_split_hand_rules (2), test_side_bet_paytable (1). Patching 68 cases by hand is exactly the
    // hand-rolled-redirect sprawl B17 decided to stop centralising, and the next round-driving test added to any of
    // them re-opens the hole silently.
    //
    // So isolate at PROCESS scope instead. `catch_discover_tests` launches every TEST_CASE as its own process of the
    // single Blackjack2Tests binary, so one object constructed during static initialisation -- before main, therefore
    // before any test body -- redirects HOME for the whole process, and every test inherits it. A per-case
    // IsolatedHome still works unchanged: it captures the process temp dir as its "original" and restores to it.
    //
    // This is test infrastructure only: no product code, no persisted field, no schema change.
    class ProcessHomeIsolation
    {
      public:
        ProcessHomeIsolation()
        {
            dir = std::filesystem::temp_directory_path() /
                  ("bj_proc_home_" + std::to_string(process_id()));
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);   // never inherit a previous run's profile
            std::filesystem::create_directories(dir, ec);
            env_set("HOME", dir.string().c_str());
        }

        ~ProcessHomeIsolation()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }

        ProcessHomeIsolation(const ProcessHomeIsolation&) = delete;
        ProcessHomeIsolation& operator=(const ProcessHomeIsolation&) = delete;

        const std::filesystem::path& path() const { return dir; }

      private:
        std::filesystem::path dir;
    };

    extern const ProcessHomeIsolation g_processHomeIsolation;

    // Instantiated in tests/test_process_isolation.cpp -- the one TU always compiled into Blackjack2Tests -- so the
    // guard exists in every test process regardless of which headers an individual test file chose to include.
    // An `inline` object here would require every TU to include this header; test_betting_controls_hint.cpp,
    // test_presentation_layout.cpp and others do not, and would silently run unisolated.
} // namespace test_support
