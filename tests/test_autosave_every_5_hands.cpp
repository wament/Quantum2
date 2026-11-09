// T1 — auto-save cadence: the profile is rewritten to disk about once per AUTO_SAVE_EVERY_HANDS human hands, not every settled round.
//
// #1 (unit) proves the gate predicate deterministically: shouldAutosave() only fires at/after AUTO_SAVE_EVERY_HANDS accumulated hands,
// reset clears it, and a multi-hand (split) round can cross the threshold in a single call.
// #2 (byte-counting) drives real headless rounds and asserts players[0] actually writes the profile far fewer times than there are
// settled rounds -- the red/green anchor against the pre-fix behaviour of saving every round.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"     // start_round / settleRound (drive a full headless round)
#include "Card.hpp"
#include "Constants.hpp"
#include "Player.hpp"           // addAutosaveHand / shouldAutosave / resetAutosaveCounter / saveWriteCount
#include <cstdlib>              // setenv/unsetenv/getenv for the hermetic HOME redirect in the byte-counting test
#include <iostream>             // T1 diagnostic: report actual write/win counts to ctest's log while calibrating assertions

TEST_CASE("auto-save cadence fires every AUTO_SAVE_EVERY_HANDS human hands", "[T1]")
{
    Player p("William");
    for (unsigned i = 0; i < Constants::AUTO_SAVE_EVERY_HANDS - 1; ++i)
        p.addAutosaveHand(1);
    CHECK(!p.shouldAutosave());          // below threshold: no autosave decision

    p.addAutosaveHand(1);                // reaches exactly the cadence
    CHECK(p.shouldAutosave());           // cadence reached -> a write is due

    p.resetAutosaveCounter();
    CHECK(!p.shouldAutosave());          // reset clears the counter

    // A split round settles several hands at once and can cross the threshold in a single call -> autosaves immediately.
    Player q("William");
    q.addAutosaveHand(static_cast<int>(Constants::AUTO_SAVE_EVERY_HANDS) + 2);
    CHECK(q.shouldAutosave());
}

TEST_CASE("settling N rounds writes the profile far fewer times than there are hands", "[T1]")
{
    // Hermetic: redirect the profile's save path into a temp dir so driving rounds writes nowhere that matters (save() mkdir -p's it).
    // B17: this used the fixed name /tmp/bj_t1_autosave_home, which every concurrently-scheduled ctest process shared.
    test_support::IsolatedHome home("t1");

    constexpr int rounds = 12;           // > AUTO_SAVE_EVERY_HANDS, so the cadence fires at least twice
    Game game(true);
    // B18: seed a deterministic shoe so EXACTLY ONE hand settles per round. Values alternate 10/7, so the human's two
    // cards are always a hard 17 -- never a pair (so never a split, which would settle two hands in one round) and
    // never a natural blackjack (which settles immediately). The autosave counter accumulates SETTLED HUMAN HANDS and
    // resets on a write, so a round that settles two hands discards the overflow and the write count lands BELOW
    // floor(hands/5) -- that is what made the exact-gating assertion below true ~19 runs in 20. Pinning the deal order
    // is what makes it true every run.
    std::vector<Card> shoe;
    shoe.reserve(160);
    const Suit suits[4] = {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs};
    for (int i = 0; i < 160; ++i)
        shoe.push_back(test_support::make_card((i % 2 == 0) ? 10 : 7, suits[i % 4]));
    test_support::seed_deck(game, shoe);
    game.startTestRound();               // first round (mirrors test_analytics.cpp's multi-round driver)
    game.testPlayer(0).setPersistable(true);   // startTestRound seats an ephemeral NPC (persistable=false); the real human persists, so
                                               // enable it here or saveProfile() short-circuits before ++saveWriteCount and writes stays 0.
    for (int r = 0; r < rounds; ++r)
    {
        if (r > 0) game.nextRoundForTest();   // advance to the next round in the SAME game
        game.confirmBet();                      // place the default bet for this round
        game.stand();                           // auto-resolve the human hand -> exactly one settled hand per round
        test_support::settleRound(game);        // plays through evaluateHands(), which now gates the autosave
    }

    const int writes = game.testPlayer(0).saveWriteCount;                    // persistable human seat only (direct access to the real seat)
    const int handsEvaluated = game.testPlayer(0).getStats().handsFinished;   // every settled hand counted by evaluateHands()
    std::cerr << "T1_DIAG writes=" << writes << " handsEvaluated=" << handsEvaluated << " rounds=" << rounds << std::endl;
    REQUIRE(writes >= 1);                     // cadence reached and a write actually happened (RED before fix: 0)
    CHECK(handsEvaluated >= rounds / 2);      // driver reached evaluateHands on essentially every round -- guards against an early short-circuit that would make writes meaningless
    CHECK(writes < rounds);                   // RED pre-fix: one write per settled round (writes==rounds=12), NOT written each round
    // B18: this used to assert `writes == rounds / AUTO_SAVE_EVERY_HANDS`, i.e. against the number of DRIVER ROUNDS.
    // The cadence counts SETTLED HUMAN HANDS, and an unseeded shoe settles a varying number per round (a natural
    // settles at once, a split settles two): handsEvaluated measured 10, 11 and 12 across 12 runs, so the fixed
    // floor(12/5)==2 was only sometimes true and the case flaked. With the deterministic one-hand-per-round shoe
    // above, settled hands equal driver rounds and the exact-gating assertion is the real invariant: one write per
    // AUTO_SAVE_EVERY_HANDS settled hands.
    CHECK(handsEvaluated == rounds);          // the seeded shoe settles exactly one hand per round -- the precondition the next line needs
    CHECK(writes == handsEvaluated / Constants::AUTO_SAVE_EVERY_HANDS); // exact gating: floor(12/5) == 2 writes over 12 settled hands

    // HOME restoration and temp-dir removal happen in ~IsolatedHome, so they run even if a CHECK/REQUIRE throws.
}
