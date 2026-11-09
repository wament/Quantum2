// B17 follow-up (2026-11-09): process-wide HOME isolation for the whole test binary.
//
// This TU exists so the guard is compiled into Blackjack2Tests unconditionally. `catch_discover_tests` launches every
// TEST_CASE as its own process of this one binary, so an object constructed during static initialisation -- before
// main, therefore before any test body runs -- redirects $HOME for every test process, including test files that never
// include test_support.hpp. See the ProcessHomeIsolation comment in test_support.hpp for why the per-TEST_CASE
// IsolatedHome left the suite non-hermetic (Test #99 in test_settlement.cpp was rewriting the developer's real
// 9.3 MB career profile).

#include <catch2/catch_all.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "test_support.hpp"

namespace test_support
{
    // The single instance. Constructed at static-initialisation time, so $HOME is already redirected before Catch2
    // dispatches any test case.
    const ProcessHomeIsolation g_processHomeIsolation{};
}

TEST_CASE("B17: every test process runs with HOME redirected away from the real profile", "[B17][hermeticity]")
{
    const char* home = std::getenv("HOME");
    REQUIRE(home != nullptr);
    const std::string h(home);

    // The process guard names its directory with the pid, so it is unique across concurrently-scheduled processes.
    REQUIRE(h.find("bj_proc_home_") != std::string::npos);
    REQUIRE(std::filesystem::is_directory(h));

    // The point of the guard: the real career profile is not reachable from inside a test process.
    const auto profile = std::filesystem::path(h) / ".local" / "share" / "blackjack" / "saves" / "player_profile.json";
    REQUIRE_FALSE(std::filesystem::exists(profile));

    // And the per-case guard still nests correctly inside it: it captures the process dir as its original and restores
    // to it, so a test that uses IsolatedHome leaves HOME where the process guard put it.
    {
        test_support::IsolatedHome nested("nesting");
        REQUIRE(std::string(std::getenv("HOME")) == nested.path().string());
    }
    REQUIRE(std::string(std::getenv("HOME")) == h);
}
