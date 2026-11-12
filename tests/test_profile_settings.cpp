#include <catch2/catch_all.hpp>
#include <Constants.hpp>
#include <Deck.hpp>
#include <PlayerProfile.hpp>
#include <Serialization.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
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

// T1 + B21 (schema). Every saved shoe used to carry a full copy of its 313-card deck; the ONLY consumer of that array
// was Replay, and it read just the shoe's SIZE (src/Replay.cpp). On the real capped profile the array cost 792,200 of
// 9,591,391 bytes. A shoe is now stored as the two integers that rebuild it -- the number of decks it was created from
// and the seed that shuffle used. B21 is what makes that sound: the seed used to be serialized as a constant 0 because
// nothing assigned it. History was reset alongside this change, so there is no legacy "initialDeck" reader left -- the
// array, the field and its compatibility branch are all removed.
namespace {

// Builds a record the way production does: shuffle a real shoe, then capture what the Deck reports.
ShoeRecord recordFromShuffle(int decks)
{
    Deck deck;
    deck.create(decks);
    deck.shuffle();

    ShoeRecord s;
    s.shoeDecks = deck.shoeDecks();
    s.shuffleSeed = deck.shuffleSeed();
    RoundRecord r;
    r.timestamp = 1;
    s.rounds.push_back(r);
    s.bankrollAtShoeEnd = 1234;
    return s;
}

// Rebuilds the shoe a record describes -- exactly what Replay::buildSteps() does.
std::vector<Card> shoeFromRecord(const ShoeRecord& s)
{
    Deck deck;
    deck.create(s.shoeDecks);
    deck.shuffleWithSeed(s.shuffleSeed);
    return deck.getCards();
}

bool sameShoe(const std::vector<Card>& a, const std::vector<Card>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].value != b[i].value || a[i].suit != b[i].suit) return false;
    return true;
}

} // namespace

TEST_CASE("T1: a saved shoe stores the seed and deck count, never the cards", "[Profile][History][T1]")
{
    const ShoeRecord s = recordFromShuffle(6);
    const json j = s;

    CHECK_FALSE(j.contains("initialDeck"));                        // the 313-card array is gone from the schema
    REQUIRE(j.contains("shuffleSeed"));
    REQUIRE(j.contains("shoeDecks"));
    CHECK(j["shuffleSeed"].get<unsigned long long>() != 0ull);     // B21: a real seed, not the old constant 0
    CHECK(j["shoeDecks"].get<int>() == 6);

    const ShoeRecord back = j.get<ShoeRecord>();
    CHECK(back.shuffleSeed == s.shuffleSeed);
    CHECK(back.shoeDecks == 6);
    CHECK(back.bankrollAtShoeEnd == 1234);
    REQUIRE(back.rounds.size() == 1u);
}

TEST_CASE("B21: a record's seed rebuilds the shoe Replay counts cards against", "[Profile][History][B21]")
{
    Deck original;
    original.create(6);
    original.shuffle();

    ShoeRecord s;
    s.shoeDecks = original.shoeDecks();
    s.shuffleSeed = original.shuffleSeed();

    const ShoeRecord back = json(s).get<ShoeRecord>();             // through JSON and back
    const std::vector<Card> rebuilt = shoeFromRecord(back);

    CHECK(rebuilt.size() == 313u);                                 // 312 + cut card: drives cardsRemaining/trueCount
    CHECK(sameShoe(rebuilt, original.getCards()));                 // the shoe that was actually dealt
}

TEST_CASE("B21: a record written without a seed still rebuilds a valid shoe", "[Profile][History][B21]")
{
    json j;
    j["rounds"] = json::array();
    const ShoeRecord s = j.get<ShoeRecord>();
    CHECK(s.shoeDecks == Constants::NUM_DECKS);
    CHECK(s.shuffleSeed == 0ull);
    CHECK(shoeFromRecord(s).size() == 313u);   // seed 0 is deterministic, not a crash and not garbage
}

TEST_CASE("T1: the profile is written compact, not pretty-printed", "[Profile][Save][T1]")
{
    const fs::path path = test_support::temp_path("t1compact", "profile");
    {
        PlayerProfile p("William");
        p.addShoeRecord(recordFromShuffle(6));
        p.save(path.string());
    }

    std::ifstream in(path);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    fs::remove(path);

    // dump(2) emitted one line per key (the real profile was 427,145 lines, 64.6% whitespace); compact emits none.
    CHECK(text.find('\n') == std::string::npos);
    CHECK(text.size() < 20000);                      // one shoe; the stored deck array alone was ~8 KB of JSON
}

TEST_CASE("T1: storing a shoe as seed + deck count is what shrinks a capped 100-shoe history", "[Profile][History][T1]")
{
    json arr = json::array();
    for (int i = 0; i < 100; ++i)
        arr.push_back(recordFromShuffle(6));
    const std::size_t bytes = json(arr).dump().size();

    // Measured on the real profile: the 100 stored decks alone were 792,200 bytes. Rounds here are one trivial
    // record per shoe, and 100 whole shoes now serialize to ~21 KB -- so anything approaching the old figure means
    // the deck array crept back into the schema. Bound set at 40 KB: far above real use, far below the regression.
    CHECK(bytes < 40000);
}

