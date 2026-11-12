#include <catch2/catch_all.hpp>

#include <Constants.hpp>
#include <GameRecord.hpp>
#include <PlayerProfile.hpp>
#include <Serialization.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "test_support.hpp"

namespace fs = std::filesystem;

// =============================================================================
// Shoe mode + the addShoeRecord router.
//
// The profile's career stats are DERIVED from longHistory (careerStatsFromHistory + applyRollup). So
// "record a Daily/tournament shoe" and "do not let it count as career progress" cannot both be
// satisfied inside one vector -- a shoe that lives in longHistory counts, by construction. The router
// in PlayerProfile::addShoeRecord() is what makes the separation structural: every ShoeMode has an
// explicit case, so a mode cannot fall through into the career history by omission.
//
// These tests guard that invariant, plus the per-mode caps, the aggregates and the schema contract.
//
// TWO CONTRACTS, and the difference between them is the whole point of addModeShoe():
//   * The CAREER branch (addCareerShoe) OVERWRITES record.rounds from the profile's own currentHistory,
//     because live play appends every settled round there (HandEvaluator) and the shoe boundary moves
//     them. A caller that hands addShoeRecord() a record with pre-filled rounds gets them DISCARDED --
//     so career cases must drive addRoundRecord() first. (Same contract test_history_rollup.cpp:101
//     documents.)
//   * The MODE branch (addModeShoe) takes the caller's rounds VERBATIM, because on the narrow-commit
//     path currentHistory is the CAREER's in-progress shoe, not this session's. Filling it there would
//     attach the wrong hands to the record.
//
// `shoesFinished` is deliberately NOT asserted on derived stats anywhere below: it is not record-derived
// (it is incremented live at Game.cpp:1753), so careerStatsFromHistory() always returns 0 for it and any
// assertion on it is vacuous.
// =============================================================================

// handsWon is PER ROUND, so a shoe's hands-won total is handsWon * rounds.
static ShoeRecord makeShoe(ShoeMode mode, int bankrollAtShoeEnd, int handsWon, int rounds = 1,
                           bool isContinuation = false)
{
    ShoeRecord shoe;
    shoe.mode = mode;
    shoe.bankrollAtShoeEnd = bankrollAtShoeEnd;
    shoe.isContinuation = isContinuation;
    shoe.completed = true;
    shoe.shuffleSeed = 0xC0FFEEull;
    shoe.shoeDecks = Constants::NUM_DECKS;
    shoe.shoeCardCount = Constants::NUM_DECKS * 52 + 1;
    for (int r = 0; r < rounds; ++r)
    {
        RoundRecord round;
        round.timestamp = 1000 + r;
        for (int i = 0; i < handsWon; ++i)
        {
            HandOutcome hand;
            hand.playerIndex = 0;
            hand.won = true;
            hand.wager = 10;
            hand.payout = 20;
            round.playerHands.push_back(hand);
        }
        HandOutcome lost;               // one lost hand per round, so a shoe is not all-wins
        lost.playerIndex = 0;
        lost.won = false;
        lost.wager = 10;
        lost.payout = 0;
        round.playerHands.push_back(lost);
        shoe.rounds.push_back(round);
    }
    return shoe;
}

// The CAREER contract: rounds live in the profile's currentHistory until the boundary moves them.
static void recordCareerShoe(PlayerProfile& p, const ShoeRecord& metadata)
{
    for (const auto& round : metadata.rounds)
        p.addRoundRecord(round);
    p.addShoeRecord(metadata);
}

TEST_CASE("addShoeRecord routes each mode to its own history", "[ShoeMode][Router]")
{
    PlayerProfile p("Ada");

    p.addShoeRecord(makeShoe(ShoeMode::Regular, 1200, 5));
    p.addShoeRecord(makeShoe(ShoeMode::DailyChallenge, 1300, 8));
    p.addShoeRecord(makeShoe(ShoeMode::Tournament, 1400, 3));
    p.addShoeRecord(makeShoe(ShoeMode::Multiplayer, 1500, 4));

    REQUIRE(p.getLongHistory().size() == 1);
    REQUIRE(p.getDailyShoes().size() == 1);
    REQUIRE(p.getTournamentShoes().size() == 1);
    REQUIRE(p.getMultiplayerShoes().size() == 1);

    CHECK(p.getLongHistory()[0].mode == ShoeMode::Regular);
    CHECK(p.getDailyShoes()[0].mode == ShoeMode::DailyChallenge);
    CHECK(p.getTournamentShoes()[0].mode == ShoeMode::Tournament);
    CHECK(p.getMultiplayerShoes()[0].mode == ShoeMode::Multiplayer);
}

// THE invariant of this change, stated as the failure it prevents: a Daily, tournament or (future)
// multiplayer shoe must never reach the vector career stats are computed from.
TEST_CASE("Non-career modes contribute NOTHING to career stats", "[ShoeMode][Router][Stats]")
{
    PlayerProfile p("Ada");

    p.addShoeRecord(makeShoe(ShoeMode::DailyChallenge, 5000, 40, 12));
    p.addShoeRecord(makeShoe(ShoeMode::Tournament, 9000, 60, 20));
    p.addShoeRecord(makeShoe(ShoeMode::Multiplayer, 7000, 30, 10));

    const PlayerStats derived = p.careerStatsFromHistory();
    CHECK(derived.handsFinished == 0);
    CHECK(derived.wins == 0);
    CHECK(derived.losses == 0);
    CHECK(derived.blackjacks == 0);
    CHECK(derived.handProfitSum == 0);
    CHECK(derived.greatestWin == 0);

    // applyRollup is what save() writes as the persisted stats block: it must be zero too, or the
    // profile's stored numbers would drift from the derived ones.
    const PlayerStats persisted = p.applyRollup();
    CHECK(persisted.handsFinished == 0);
    CHECK(persisted.wins == 0);
    CHECK(persisted.losses == 0);
    CHECK(persisted.handProfitSum == 0);

    // And the mode aggregates DID record, so the data is not simply being dropped. mostHandsWon is a
    // per-SHOE total (handsWonInShoe sums the whole shoe), hence handsWon * rounds.
    CHECK(p.dailyModeStats().shoesPlayed == 1);
    CHECK(p.tournamentModeStats().shoesPlayed == 1);
    CHECK(p.multiplayerModeStats().shoesPlayed == 1);
    CHECK(p.dailyModeStats().mostHandsWon == 40 * 12);
    CHECK(p.tournamentModeStats().mostHandsWon == 60 * 20);
    CHECK(p.dailyModeStats().bestBankroll == 5000);
    CHECK(p.tournamentModeStats().bestBankroll == 9000);
}

TEST_CASE("A regular shoe still feeds career stats exactly as before", "[ShoeMode][Router][Stats]")
{
    PlayerProfile p("Ada");
    recordCareerShoe(p, makeShoe(ShoeMode::Regular, 1200, 7, 3));

    REQUIRE(p.getLongHistory().size() == 1);
    CHECK(p.getLongHistory()[0].rounds.size() == 3);   // the boundary moved currentHistory into the record

    const PlayerStats derived = p.careerStatsFromHistory();
    CHECK(derived.handsFinished == 24);   // 3 rounds x (7 won + 1 lost)
    CHECK(derived.wins == 21);            // 3 rounds x 7 won hands, all seat 0
    CHECK(derived.losses == 3);
    CHECK(p.applyRollup().handsFinished == 24);
}

// handsWonInShoe is the source of mostHandsWon, and a shoe has NPC seats in it: counting them would
// credit the human with the table's wins.
TEST_CASE("handsWonInShoe counts only the human seat", "[ShoeMode][Stats]")
{
    ShoeRecord shoe = makeShoe(ShoeMode::DailyChallenge, 1000, 2, 1);
    HandOutcome npc;
    npc.playerIndex = 1;                  // an NPC seat, also "won"
    npc.won = true;
    npc.wager = 10;
    npc.payout = 20;
    shoe.rounds[0].playerHands.push_back(npc);

    CHECK(handsWonInShoe(shoe) == 2);      // the 2 seat-0 wins, not 3
}

// The cap is a DISK decision (a shoe carries its full round records), so it has to be real -- and a
// front-erase must not lose a peak, which is why the aggregates are incremental rather than derived
// from the surviving window.
TEST_CASE("Per-mode history is capped oldest-first and the aggregates survive the cap", "[ShoeMode][Cap]")
{
    PlayerProfile p("Ada");
    const std::size_t cap = Constants::Daily::MAX_HISTORY_SHOES;

    // The FIRST shoe sets both peaks, so it is exactly the one the cap evicts.
    p.addShoeRecord(makeShoe(ShoeMode::DailyChallenge, 99999, 500, 2));
    for (std::size_t i = 0; i < cap + 5; ++i)
        p.addShoeRecord(makeShoe(ShoeMode::DailyChallenge, 1000 + static_cast<int>(i), 1, 1));

    CHECK(p.getDailyShoes().size() == cap);
    // shoesPlayed counts every shoe ever played, not the surviving window.
    CHECK(p.dailyModeStats().shoesPlayed == static_cast<int>(cap + 6));
    CHECK(p.dailyModeStats().bestBankroll == 99999);      // peak from the EVICTED shoe survived
    CHECK(p.dailyModeStats().mostHandsWon == 500 * 2);
    // Chronological, front-erased: six shoes were evicted, so the oldest survivor is the 1005 shoe.
    CHECK(p.getDailyShoes().front().bankrollAtShoeEnd == 1005);
    CHECK(p.getDailyShoes().back().bankrollAtShoeEnd == 1000 + static_cast<int>(cap) + 4);
}

// The failure this split exists to prevent, at scale: if routing ever fell through, 280 Daily and
// tournament shoes would evict 180 real career shoes into the rollup and silently inflate career stats.
TEST_CASE("Daily and tournament shoes never consume the career 100-shoe window", "[ShoeMode][Cap][Stats]")
{
    PlayerProfile p("Ada");
    for (int i = 0; i < 140; ++i)
    {
        p.addShoeRecord(makeShoe(ShoeMode::DailyChallenge, 1000 + i, 3, 1));
        p.addShoeRecord(makeShoe(ShoeMode::Tournament, 2000 + i, 4, 1));
    }
    recordCareerShoe(p, makeShoe(ShoeMode::Regular, 5000, 6, 2));

    REQUIRE(p.getLongHistory().size() == 1);
    CHECK(p.getLongHistory()[0].mode == ShoeMode::Regular);
    CHECK(p.applyRollup().wins == 12);   // 2 rounds x 6; the Daily/tournament wins are NOT in here
    CHECK(p.getDailyShoes().size() == Constants::Daily::MAX_HISTORY_SHOES);
    CHECK(p.getTournamentShoes().size() == Constants::TournamentHistory::MAX_HISTORY_SHOES);
}

TEST_CASE("Per-mode history, aggregates and the mode tag survive a save/load round-trip", "[ShoeMode][Serialization]")
{
    test_support::IsolatedHome home("shoemode");
    const std::string path = (home.path() / "roundtrip.json").string();

    PlayerProfile p("Ada");
    p.addShoeRecord(makeShoe(ShoeMode::Regular, 1200, 5, 2));
    p.addShoeRecord(makeShoe(ShoeMode::DailyChallenge, 1300, 8, 3, /*isContinuation=*/true));
    p.addShoeRecord(makeShoe(ShoeMode::Tournament, 1400, 3, 1));
    p.tournamentStats.bestFinish = 4;
    p.tournamentsCompleted = 2;
    p.save(path);

    const PlayerProfile after = PlayerProfile::load(path);
    REQUIRE(after.getDailyShoes().size() == 1);
    CHECK(after.getDailyShoes()[0].mode == ShoeMode::DailyChallenge);
    CHECK(after.getDailyShoes()[0].isContinuation == true);
    REQUIRE(after.getDailyShoes()[0].rounds.size() == 3);
    CHECK(handsWonInShoe(after.getDailyShoes()[0]) == 8 * 3);   // round records survived intact
    CHECK(after.getTournamentShoes().size() == 1);
    CHECK(after.getTournamentShoes()[0].mode == ShoeMode::Tournament);
    CHECK(after.getMultiplayerShoes().empty());
    CHECK(after.dailyModeStats().mostHandsWon == 24);
    CHECK(after.dailyModeStats().bestBankroll == 1300);
    CHECK(after.tournamentModeStats().bestFinish == 4);
    CHECK(after.tournamentsCompleted == 2);
    REQUIRE(after.getLongHistory().size() == 1);
    CHECK(after.getLongHistory()[0].mode == ShoeMode::Regular);
}

// The additive-schema contract: a save written before this split must load with the career half exactly
// as it was, and empty mode history -- never a throw, never a wiped profile.
TEST_CASE("A profile saved before per-mode history loads unchanged", "[ShoeMode][Serialization]")
{
    test_support::IsolatedHome home("shoemode_legacy");
    const std::string path = (home.path() / "legacy.json").string();

    PlayerProfile p("Ada");
    recordCareerShoe(p, makeShoe(ShoeMode::Regular, 1200, 5, 2));
    p.save(path);

    // Strip every per-mode key: exactly what a pre-split save looks like.
    nlohmann::json j;
    { std::ifstream in(path); in >> j; }
    for (const char* key : {"dailyShoes", "tournamentShoes", "multiplayerShoes",
                            "dailyStats", "tournamentStats", "multiplayerStats", "tournamentsCompleted"})
        j.erase(key);
    { std::ofstream out(path); out << j.dump(); }

    const PlayerProfile after = PlayerProfile::load(path);
    CHECK(after.getDailyShoes().empty());
    CHECK(after.getTournamentShoes().empty());
    CHECK(after.getMultiplayerShoes().empty());
    CHECK(after.dailyModeStats().shoesPlayed == 0);
    CHECK(after.tournamentsCompleted == 0);
    REQUIRE(after.getLongHistory().size() == 1);
    CHECK(after.getLongHistory()[0].mode == ShoeMode::Regular);   // pre-split shoes read as career
    CHECK(after.applyRollup().wins == 10);                        // 2 rounds x 5 -- career untouched
}

TEST_CASE("A shoe record's mode reads defensively", "[ShoeMode][Serialization]")
{
    // An out-of-range mode must fall back to Regular. A value no producer writes would silently route
    // a shoe into a history that career stats are derived from.
    nlohmann::json high;
    high["rounds"] = nlohmann::json::array();
    high["mode"] = 99;
    CHECK(high.get<ShoeRecord>().mode == ShoeMode::Regular);

    nlohmann::json negative;
    negative["rounds"] = nlohmann::json::array();
    negative["mode"] = -3;
    CHECK(negative.get<ShoeRecord>().mode == ShoeMode::Regular);

    // B24's hazard, closed at this site: a missing "rounds" key used to THROW, and the load path's
    // recovery from a throw is "relocate the profile, start fresh" -- one mistyped field destroyed the
    // whole file. It must load as an empty shoe instead.
    nlohmann::json noRounds;
    noRounds["mode"] = static_cast<int>(ShoeMode::DailyChallenge);
    const ShoeRecord safe = noRounds.get<ShoeRecord>();
    CHECK(safe.rounds.empty());
    CHECK(safe.mode == ShoeMode::DailyChallenge);
}
