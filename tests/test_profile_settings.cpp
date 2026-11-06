#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <PlayerProfile.hpp>
#include <Serialization.hpp>
#include <filesystem>
#include <fstream>
#include "test_support.hpp"

using namespace Constants;
namespace fs = std::filesystem;

// The main menu opens on the last-used table settings instead of the compile-time defaults, so the values
// have to survive a save/load round trip -- and profiles written before these fields existed must still load.

TEST_CASE("Table settings survive a profile save/load round trip", "[Profile][Settings]")
{
    const fs::path path = test_support::temp_path("settings", "roundtrip");

    {
        PlayerProfile p("William");
        p.setBankroll(5000);
        p.preferredPlayerCount = 6;
        p.preferredSpeed = static_cast<int>(GameSpeed::AutoPlay);
        p.preferredCountingStyle = 0;   // BasicStrategy::CountingStyle::HI_LO
        p.save(path.string());
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.preferredPlayerCount == 6);
    CHECK(loaded.preferredSpeed == static_cast<int>(GameSpeed::AutoPlay));
    CHECK(loaded.preferredCountingStyle == 0);
    CHECK(loaded.getBankroll() == 5000);

    fs::remove(path);
}

// Older saves have no settings block at all; loading them must fall back to the defaults rather than
// throwing or discarding the player's history. Built by stripping the keys from a real save, so it
// cannot drift out of sync with the record format.
TEST_CASE("Profiles saved before table settings existed still load with defaults", "[Profile][Settings]")
{
    const fs::path path = test_support::temp_path("settings", "legacy");

    {
        PlayerProfile fresh("William");
        fresh.setBankroll(4321);
        fresh.save(path.string());
    }

    {
        std::fstream rw(path);
        json j;
        rw >> j;
        rw.close();

        j.erase("preferredPlayerCount");
        j.erase("preferredSpeed");
        j.erase("preferredCountingStyle");

        std::ofstream out(path);
        out << j.dump(2);
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.getBankroll() == 4321);
    CHECK(loaded.preferredPlayerCount == GameConfig::DEFAULT_PLAYER_COUNT);
    CHECK(loaded.preferredSpeed == static_cast<int>(GameSpeed::Normal));
    CHECK(loaded.preferredCountingStyle == 1);   // REKO

    fs::remove(path);
}

// B15 regression guard: exactly one Serialization.hpp must exist anywhere in the repo (outside the build tree), so a
// future schema edit cannot silently diverge the shipped copy from the tested copy. Counts by walking the project root
// derived from this file's own location, which keeps the check correct regardless of the working directory ctest runs
// it under. This fails while src/Serialization.hpp and include/Serialization.hpp both exist, and passes once src/ has
// been deleted -- that is the whole B15 fix expressed as an invariant.
TEST_CASE("B15: exactly one Serialization.hpp exists outside the build tree", "[Schema][Build]")
{
    const fs::path here = fs::path(__FILE__).parent_path();          // .../tests
    const fs::path root = here.parent_path();                        // repository root

    int count = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
            break;
        const fs::path& p = entry.path();
        if (p.filename() == "Serialization.hpp" && p.string().find("/out/") == std::string::npos)
            ++count;
    }

    CHECK(count == 1);
}
