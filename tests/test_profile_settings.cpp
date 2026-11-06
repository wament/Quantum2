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

namespace {
    // Isolated directory for B14 recovery tests: clean slate so a recovered .corrupt.<ts>.json from one run cannot be
    // confused with another, keeping the "file survives" assertion unambiguous.
    fs::path b14_dir(const std::string& tag)
    {
        const fs::path dir = fs::temp_directory_path() / ("bj_b14_" + tag);
        fs::remove_all(dir);   // clean slate
        fs::create_directories(dir);
        return dir;
    }
} // namespace

// B14 half 2 -- a truncated (corrupt) save must be relocated to a dated .corrupt.<ts>.json copy and never silently
// blanked. Before the fix load() catches the parse error, logs "Starting fresh.", returns a default profile while
// leaving the damaged file in place for the game's next save to overwrite; after the fix it relocates the bad file so
// recovery stays possible. This is the whole defect expressed as an assertion: without the fix no .corrupt.* exists.
TEST_CASE("B14: a truncated corrupt save is recovered under a .corrupt.<ts> name, not silently blanked", "[Schema][Persistence]")
{
    const fs::path dir = b14_dir("corrupt");
    const fs::path save = dir / "player_profile.json";

    // A partially-written file: valid-looking keys then cut off mid-object -- exactly what a crash/power-loss leaves.
    {
        std::ofstream f(save);
        f << "{ \"name\": \"Ada\", \"bankroll\": 5000, ";   // truncated -> nlohmann throws on load
    }

    PlayerProfile p = PlayerProfile::load(save.string());
    CHECK(p.getName() == "");       // fresh profile, as the recovery contract requires
    CHECK(p.getBankroll() == 1000); // ...with the standard starting bankroll

    // The damaged file must still be recoverable: load renamed it to a dated .corrupt.<ts>.json copy.
    std::error_code ec;
    bool recovered = false;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.path().filename().string().find(".corrupt.") != std::string::npos)
            recovered = true;
    CHECK(recovered);

    // Nothing clobbers the recovered copy: a fresh save next to it leaves both files intact.
    p.setBankroll(1234);
    p.save(save.string());
    fs::remove(save, ec);
    bool still_recovered = false;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.path().filename().string().find(".corrupt.") != std::string::npos)
            still_recovered = true;
    CHECK(still_recovered);

    fs::remove_all(dir);
}

// B14 half 3 -- a valid-but-partial JSON must keep its present core fields and fill the rest with defaults rather than
// throwing on a missing key and discarding everything. Also asserts the atomic write leaves no .tmp behind after save.
TEST_CASE("B14: partial-but-valid JSON keeps present fields; saves leave no temp file behind", "[Schema][Persistence]")
{
    const fs::path dir = b14_dir("partial");
    const fs::path save = dir / "player_profile.json";

    // Valid JSON, but missing name/stats/longHistory -- only bankroll present.
    {
        std::ofstream f(save);
        f << "{ \"bankroll\": 5000 }";
    }

    PlayerProfile q = PlayerProfile::load(save.string());
    CHECK(q.getBankroll() == 5000);   // the present field is kept (throws -> fresh -> 1000 before the fix)
    CHECK(q.getName() == "");         // missing fields fall back to their defaults

    // The atomic write must leave no sibling temp file once save returns.
    q.setBankroll(777);
    q.save(save.string());
    std::error_code ec;
    CHECK(!fs::exists(save.string() + ".tmp", ec));

    fs::remove_all(dir);
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

// Read a file's raw bytes so tests can assert on the exact JSON keys written to disk (not just round-tripped values).
static std::string read_file(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// B12 the typo `resplitTents` was baked into every save file. A fresh profile must serialise the corrected key and no
// longer write the misspelled one. This reads the on-disk JSON directly, so it fails at runtime if to_json still writes
// "resplitTentsAllowed" even once the field has been renamed -- proving the writer was actually fixed too.
TEST_CASE("B12: a fresh profile serialises the corrected resplitTensAllowed key", "[Schema][Persistence]")
{
    const fs::path path = test_support::temp_path("resplittens", "write");

    {
        PlayerProfile p("Ada");
        p.gameRules.resplitTensAllowed = false;   // new (correct) field name -- only exists after the B12 rename
        p.save(path.string());
    }

    const std::string content = read_file(path);
    CHECK(content.find("\"resplitTensAllowed\"") != std::string::npos);   // corrected key is written
    CHECK(content.find("\"resplitTentsAllowed\"") == std::string::npos);  // typo'd key no longer appears

    fs::remove(path);
}

// B12 old profiles already contain the typo'd key; the additive read must map it onto the corrected field so nothing is
// lost. This regression guard only passes once from_json reads BOTH keys (new winning over the legacy one).
TEST_CASE("B12: a legacy save carrying only resplitTentsAllowed still loads into resplitTensAllowed", "[Schema][Persistence]")
{
    const fs::path path = test_support::temp_path("resplittens", "legacy");

    {
        // Legacy file: ONLY the typo'd key present, nothing else.
        std::ofstream out(path);
        out << "{ \"gameRules\": { \"resplitTentsAllowed\": true } }";
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.gameRules.resplitTensAllowed == true);   // additive read maps the legacy key onto the corrected field

    fs::remove(path);
}

// B12 when both keys are present (a file written after the fix that still predates one migration) the corrected key must
// win, and only fall back to the typo'd key for genuinely old saves. Fails at runtime if from_json still reads the old key.
TEST_CASE("B12: when both keys are present the corrected key wins over the typo'd one", "[Schema][Persistence]")
{
    const fs::path path = test_support::temp_path("resplittens", "precedence");

    {
        std::ofstream out(path);
        out << "{ \"gameRules\": { \"resplitTentsAllowed\": true, \"resplitTensAllowed\": false } }";
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.gameRules.resplitTensAllowed == false);   // corrected key wins; typo'd key is only the fallback

    fs::remove(path);
}

