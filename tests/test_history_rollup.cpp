// T2 — 100-shoe history cap with lossless aggregation.
//
// Proves two things about PlayerProfile::addShoeRecord (which now caps longHistory at Constants::MAX_LONG_HISTORY_SHOES and rolls
// older shoes into an aggregate carry) and PlayerProfile::applyRollup (the save-time merge): the cap holds with oldest-first
// erasure keeping longHistory chronological; and aggregating the rolled-off shoes is LOSSLESS -- applyRollup() reproduces
// computeCareerStatsHistory over ALL played shoes field-by-field, including greatestSession* (session running-total continuity), which
// is the only recompute that does not reset per shoe. A zero carry is the merge identity, so <= 100 shoes is a no-op; this test
// deliberately exceeds the cap to exercise both branches of the session-peak merge.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"     // test_support::make_card / temp_path
#include "Card.hpp"
#include "Constants.hpp"
#include "GameRecord.hpp"       // PlayerStats, ShoeRecord, RoundRecord, HandOutcome, computeCareerStatsHistory
#include "PlayerProfile.hpp"    // addShoeRecord + applyRollup; save/load round-trip

using Catch::Approx;

namespace {

// A single human-seat (playerIndex 0) hand with explicit settlement + optional side-bet fields. computeCareerStatsHistory only counts
// playerIndex == 0 hands, so an NPC hand is the perfect "must be ignored" case.
HandOutcome humanHand(std::vector<Card> cards, int wager, int payout, bool won,
                      int sideBetWager = 0, int sideBetPayout = 0)
{
    HandOutcome h;
    h.cards = std::move(cards);
    h.wager = wager;
    h.payout = payout;
    h.won = won;
    h.playerIndex = 0;
    h.sideBetWager = sideBetWager;
    h.sideBetPayout = sideBetPayout;
    return h;
}

HandOutcome npcHand(std::vector<Card> cards, int wager, int payout, bool won)
{
    HandOutcome h;
    h.cards = std::move(cards);
    h.wager = wager;
    h.payout = payout;
    h.won = won;
    h.playerIndex = 1;   // non-human seat -> ignored by the profile recompute
    return h;
}

RoundRecord roundOf(HandOutcome&& hand)
{
    RoundRecord r;
    r.playerHands.push_back(std::move(hand));
    return r;
}

// Three shoe "templates" cycled across history so the session running total wanders (building peaks in BOTH the rolled-off and the
// still-in-window regions), with naturals, side bets and an NPC hand for coverage. bankrollAtShoeEnd is set to the shoe index so a
// preserved shoe can be identified without PlayerStats/ShoeRecord operator==.
ShoeRecord upShoe(int idx)
{
    ShoeRecord s;
    s.bankrollAtShoeEnd = idx;
    for (int i = 0; i < 4; ++i)   // four winning hands -> session total climbs, longest-streak + greatestSessionWin build
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(2, Suit::Hearts), test_support::make_card(9, Suit::Spades)}, 10, 20, true)));
    return s;
}

ShoeRecord downShoe(int idx)
{
    ShoeRecord s;
    s.bankrollAtShoeEnd = idx;
    for (int i = 0; i < 4; ++i)   // four losing hands -> session total falls, longest-streak + greatestSessionLoss build
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(7, Suit::Hearts), test_support::make_card(8, Suit::Spades)}, 10, 0, false)));
    return s;
}

ShoeRecord mixedShoe(int idx)
{
    ShoeRecord s;
    s.bankrollAtShoeEnd = idx;
    s.rounds.push_back(roundOf(humanHand({test_support::make_card(1, Suit::Hearts), test_support::make_card(10, Suit::Spades)}, 10, 30, true)));             // natural BJ, net +20
    s.rounds.push_back(roundOf(humanHand({test_support::make_card(5, Suit::Clubs), test_support::make_card(6, Suit::Hearts)}, 15, 15, true)));                   // push (net 0)
    s.rounds.push_back(roundOf(humanHand({test_support::make_card(3, Suit::Diamonds), test_support::make_card(9, Suit::Clubs)}, 25, 0, false, /*side*/ 5, 15))); // lost main + winning side bet
    s.rounds.push_back(roundOf(npcHand({test_support::make_card(4, Suit::Hearts), test_support::make_card(4, Suit::Spades)}, 30, 60, true)));                    // NPC win (ignored)
    return s;
}

ShoeRecord shoeFor(std::size_t idx)
{
    switch (idx % 3)
    {
        case 0: return upShoe(static_cast<int>(idx));
        case 1: return downShoe(static_cast<int>(idx));
        default: return mixedShoe(static_cast<int>(idx));
    }
}

void playShoe(PlayerProfile& p, const ShoeRecord& shoe)
{
    for (const auto& r : shoe.rounds)
        p.addRoundRecord(r);
    // addShoeRecord copies the passed record's metadata (bankrollAtShoeEnd == the shoe index here) and overwrites only .rounds from
    // currentHistory, so stamping it lets us identify preserved shoes without PlayerStats/ShoeRecord operator==.
    p.addShoeRecord(shoe);
}

}  // namespace

// Field-by-field golden comparison. PlayerStats has no operator==, so every record-derivable field is CHECKed against the reference
// computed directly over ALL played shoes (the ground truth a cap-free profile would write).
void assertLossless(const PlayerStats& got, const PlayerStats& ref)
{
    CHECK(got.wins == ref.wins);
    CHECK(got.losses == ref.losses);
    CHECK(got.pushes == ref.pushes);
    CHECK(got.handsFinished == ref.handsFinished);
    CHECK(got.blackjacks == ref.blackjacks);
    CHECK(got.greatestHand == ref.greatestHand);
    CHECK(got.greatestHandLoss == ref.greatestHandLoss);
    CHECK(got.handProfitCount == ref.handProfitCount);
    CHECK(got.handProfitSum == ref.handProfitSum);
    CHECK(got.handProfitSumSq == ref.handProfitSumSq);
    CHECK(got.greatestWin == ref.greatestWin);
    CHECK(got.greatestLoss == ref.greatestLoss);
    CHECK(got.greatestSessionWin == ref.greatestSessionWin);   // the session-running-total merge is the highest-risk field
    CHECK(got.greatestSessionLoss == ref.greatestSessionLoss);
    CHECK(got.longestWinningStreak == ref.longestWinningStreak);
    CHECK(got.longestLosingStreak == ref.longestLosingStreak);
    CHECK(got.sideBetWins == ref.sideBetWins);
    CHECK(got.sideBetLosses == ref.sideBetLosses);
    CHECK(got.sideBetProfitSum == ref.sideBetProfitSum);
}

// The cap holds and the window stays chronological: after playing N > MAX_LONG_HISTORY_SHOES shoes, exactly the LAST MAX are kept as
// records (front = oldest surviving shoe, back = newest). This is the red/green anchor that fails before the fix (longHistory would
// grow to 150) and bounds the region the carry must reconstruct.
TEST_CASE("longHistory caps at MAX_LONG_HISTORY_SHOES, oldest-first", "[T2][history-cap]")
{
    constexpr std::size_t N = 150;

    std::vector<ShoeRecord> shoes(N);
    for (std::size_t i = 0; i < N; ++i)
        shoes[i] = shoeFor(i);

    PlayerProfile profile("William");
    for (const auto& s : shoes)
        playShoe(profile, s);

    const auto& kept = profile.getLongHistory();
    REQUIRE(kept.size() == Constants::MAX_LONG_HISTORY_SHOES);       // RED before fix: grows unbounded
    REQUIRE(kept.front().bankrollAtShoeEnd == static_cast<int>(N - Constants::MAX_LONG_HISTORY_SHOES));  // oldest surviving == shoe 50
    REQUIRE(kept.back().bankrollAtShoeEnd == static_cast<int>(N - 1));                              // newest == shoe 149
}

// Aggregation is lossless: applyRollup() over the capped window + rolled-off carry equals computeCareerStatsHistory over ALL shoes.
TEST_CASE("rolling old shoes off longHistory preserves career totals exactly", "[T2][history-cap]")
{
    constexpr std::size_t N = 150;
    std::vector<ShoeRecord> shoes(N);
    for (std::size_t i = 0; i < N; ++i)
        shoes[i] = shoeFor(i);

    const PlayerStats reference = computeCareerStatsHistory(shoes);   // ground truth, cap-free

    PlayerProfile profile("William");
    for (const auto& s : shoes)
        playShoe(profile, s);

    CHECK(profile.getLongHistory().size() == Constants::MAX_LONG_HISTORY_SHOES);
    assertLossless(profile.applyRollup(), reference);                 // RED if any field's merge semantics drift from live play
}

// Persistence round-trip stability: a capped profile saved and reloaded keeps both the bound history length and the lossless totals.
TEST_CASE("capped+rolled history survives save/load with identical totals", "[T2][history-cap]")
{
    constexpr std::size_t N = 150;
    std::vector<ShoeRecord> shoes(N);
    for (std::size_t i = 0; i < N; ++i)
        shoes[i] = shoeFor(i);
    const PlayerStats reference = computeCareerStatsHistory(shoes);

    const std::string path = test_support::temp_path("history_rollup", "main").string();
    {
        PlayerProfile profile("William");
        for (const auto& s : shoes)
            playShoe(profile, s);
        profile.save(path);
    }

    const PlayerProfile loaded = PlayerProfile::load(path);
    std::remove(path.c_str());

    REQUIRE(loaded.getLongHistory().size() == Constants::MAX_LONG_HISTORY_SHOES);
    assertLossless(loaded.stats, reference);   // persisted rollup + capped history reproduce totals on reload
}

// Identity/no-op under the cap: with <= 100 completed shoes nothing rolls off, so applyRollup() is exactly the full-history recompute.
// Guards that T2 stays purely additive and cannot change any existing profile's output when nothing has exceeded the cap.
TEST_CASE("with no shoe past the cap, aggregation is a no-op", "[T2][history-cap]")
{
    constexpr std::size_t N = 40;   // well under MAX_LONG_HISTORY_SHOES
    std::vector<ShoeRecord> shoes(N);
    for (std::size_t i = 0; i < N; ++i)
        shoes[i] = shoeFor(i);
    const PlayerStats reference = computeCareerStatsHistory(shoes);

    PlayerProfile profile("William");
    for (const auto& s : shoes)
        playShoe(profile, s);

    REQUIRE(profile.getLongHistory().size() == N);
    assertLossless(profile.applyRollup(), reference);   // zero carry -> identity merge, totals unchanged
}

