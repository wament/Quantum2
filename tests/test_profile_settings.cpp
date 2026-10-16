#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <PlayerProfile.hpp>
#include <Serialization.hpp>
#include <filesystem>
#include <fstream>

using namespace Constants;
namespace fs = std::filesystem;

// The main menu opens on the last-used table settings instead of the compile-time defaults, so the values
// have to survive a save/load round trip -- and profiles written before these fields existed must still load.

namespace
{
    fs::path tempProfile(const std::string& name)
    {
        return fs::temp_directory_path() / ("bj_settings_test_" + name + ".json");
    }
}

TEST_CASE("Table settings survive a profile save/load round trip", "[Profile][Settings]")
{
    const fs::path path = tempProfile("roundtrip");

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
    const fs::path path = tempProfile("legacy");

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
