#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Card.hpp>
#include <Game.hpp>
#include <BasicStrategy.hpp>
#include <PlayerProfile.hpp>
#include <Serialization.hpp>

#include <filesystem>
#include <fstream>

using namespace Constants;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Custom Game Rules (Feature 3). Every case drives the real Game round-state
// machine headlessly (Game game(true) + setDeck -> startTestRound -> public
// actions), reading outcomes through *ForTest() accessors, exactly like the
// split-aces / settlement tests. Each rule is its own clause so a regression in
// one rule cannot hide another.
// ---------------------------------------------------------------------------

// ---- Soft 17 ---------------------------------------------------------------

// The historical H17 rule: the dealer hits a soft 17. Default rules keep this, so the dealer draws at least one extra
// card off its A+6 and only then stands (hard 17) once it reaches a standing total.
TEST_CASE("Dealer hits a soft 17 under default rules", "[GameRules][Soft17]")
{
    Game game(true);
    // Deal order: human 10 + 8 (hard 18, forces the dealer to play), dealer up 6, hole Ace -> soft 17, then a ten-value
    // card for the dealer's required extra draw.
    test_support::start_round(game,{test_support::make_card(10, Suit::Spades),   test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(8, Suit::Clubs),     test_support::make_card(1, Suit::Diamonds),
                  test_support::make_card(10, Suit::Hearts),   test_support::make_card(2, Suit::Clubs)});
    game.stand(); // human stands on hard 18 -> unresolved -> the dealer must play its own rule
    test_support::settleRound(game);

    CHECK(game.testDealer().size() > 2); // drew at least one card off the soft 17
}

// Flipping the rule makes the dealer stand on a soft 17: it keeps exactly two cards and never draws.
TEST_CASE("Dealer stands on a soft 17 when the rule requires it", "[GameRules][Soft17]")
{
    Game game(true);
    GameRules rules;
    rules.dealerStandsOnSoft17 = true;
    game.setGameRulesForTest(rules);

    test_support::start_round(game,{test_support::make_card(10, Suit::Spades),   test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(8, Suit::Clubs),     test_support::make_card(1, Suit::Diamonds)});
    game.stand();
    test_support::settleRound(game);

    CHECK(game.testDealer().size() == 2);
    CHECK(game.testDealer().getValue() == 17);
    CHECK(game.testDealer().isSoft()); // still a soft 17, untouched
}

// ---- Resplit aces ------------------------------------------------------------

TEST_CASE("Aces can be re-split when resplit-aces is allowed", "[GameRules][ResplitAces]")
{
    Game game(true);
    GameRules rules;
    rules.resplitAcesAllowed = true;
    game.setGameRulesForTest(rules);

    // Two aces, then an ace as the first hand's draw so it stays re-split eligible.
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades),  test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),   test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(1, Suit::Clubs),    test_support::make_card(1, Suit::Diamonds)});

    CHECK(game.split()); // AA -> two aces
    test_support::tickFrame(game);      // first ace draws an Ace -> still open (re-split eligible)
    CHECK(game.testPlayer(0).hands[0].isSplitAce());
    CHECK(!game.testPlayer(0).hands[0].isFinished());

    CHECK(game.split()); // re-split -> three aces
    CHECK(game.testPlayer(0).hands.size() == 3);
}

TEST_CASE("Aces cannot be re-split when resplit-aces is disallowed", "[GameRules][ResplitAces]")
{
    Game game(true);
    GameRules rules;
    rules.resplitAcesAllowed = false;
    game.setGameRulesForTest(rules);

    // Two aces, then a non-ace as the first hand's draw.
    test_support::start_round(game,{test_support::make_card(1, Suit::Spades),  test_support::make_card(7, Suit::Hearts),
                  test_support::make_card(1, Suit::Hearts),   test_support::make_card(6, Suit::Diamonds),
                  test_support::make_card(2, Suit::Clubs)});

    CHECK(game.split()); // AA -> two single-card ace hands
    test_support::tickFrame(game);      // first ace draws one card; with resplit off it is forced to stand (cannot re-split)

    auto& p = game.testPlayer(0);
    CHECK(p.hands.size() == 2);              // no re-split happened
    CHECK(p.hands[0].size() == 2);           // original ace + the one permitted card
    CHECK(p.hands[0].isFinished());          // cannot draw/re-split again -> finished here
}

// ---- Double after split ------------------------------------------------------

TEST_CASE("A split hand can be doubled when double-after-split is allowed", "[GameRules][DAS]")
{
    Game game(true);
    GameRules rules;
    rules.doublesAfterSplitAllowed = true;
    game.setGameRulesForTest(rules);

    // Pair of 5s, dealer up non-ace; filler gives the split hand its card then the double-down's extra card.
    test_support::start_round(game,{test_support::make_card(5, Suit::Spades),  test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(5, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds),
                  test_support::make_card(3, Suit::Hearts),  test_support::make_card(4, Suit::Clubs)});

    CHECK(game.split()); // pair of 5s -> two hands (size 1 each)
    test_support::tickFrame(game);      // first split hand draws one card -> size 2, ready to act
    CHECK(game.testPlayer(0).hands[0].size() == 2);

    CHECK(game.doubleDown());            // DAS allowed -> double proceeds
    CHECK(game.testHand(0, 0).size() == 3); // doubled: one extra card dealt and the hand is finished
}

TEST_CASE("A split hand cannot be doubled when double-after-split is disallowed", "[GameRules][DAS]")
{
    Game game(true);
    GameRules rules;
    rules.doublesAfterSplitAllowed = false;
    game.setGameRulesForTest(rules);

    // Pair of 5s, dealer up non-ace; one filler card lets the split hand reach size 2 (so canDouble is true).
    test_support::start_round(game,{test_support::make_card(5, Suit::Spades),  test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(5, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds),
                  test_support::make_card(3, Suit::Hearts)});

    CHECK(game.split());
    test_support::tickFrame(game);
    CHECK(game.testPlayer(0).hands[0].size() == 2);

    CHECK_FALSE(game.doubleDown());      // DAS off -> refused, no extra card dealt
    CHECK(game.testHand(0, 0).size() == 2);
}

// ---- Surrender types ---------------------------------------------------------

TEST_CASE("Surrender is refused entirely when the type is None", "[GameRules][Surrender]")
{
    Game game(true);
    GameRules rules;
    rules.surrenderType = SurrenderType::None;
    game.setGameRulesForTest(rules);

    test_support::start_round(game,{test_support::make_card(5, Suit::Spades),  test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(6, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds)});

    CHECK_FALSE(game.surrender()); // surrender disabled by the rule
}

TEST_CASE("Surrender is offered on the first two cards under Late rules", "[GameRules][Surrender]")
{
    Game game(true);
    GameRules rules;
    rules.surrenderType = SurrenderType::Late;
    game.setGameRulesForTest(rules);

    test_support::start_round(game,{test_support::make_card(5, Suit::Spades),  test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(6, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds)});

    CHECK(game.surrender()); // allowed on the first two cards
}

TEST_CASE("Early surrender is only offered when the dealer shows an Ace or ten-value", "[GameRules][Surrender]")
{
    // Dealer up-card is a 6 (not Ace/ten) -> early surrender refused.
    Game game(true);
    GameRules rules;
    rules.surrenderType = SurrenderType::Early;
    game.setGameRulesForTest(rules);

    test_support::start_round(game,{test_support::make_card(5, Suit::Spades),  test_support::make_card(6, Suit::Hearts),
                  test_support::make_card(6, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds)});

    CHECK_FALSE(game.surrender());

    // Dealer up-card is a King (ten-value) -> early surrender allowed.
    Game g2(true);
    GameRules rules2;
    rules2.surrenderType = SurrenderType::Early;
    g2.setGameRulesForTest(rules2);

    g2.setDeck({test_support::make_card(5, Suit::Spades),  test_support::make_card(13, Suit::Hearts),
                test_support::make_card(6, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds)});
    g2.startTestRound();

    CHECK(g2.surrender());
}

// ---- Configurable deck count -------------------------------------------------

TEST_CASE("A configurable deck count rebuilds the shoe with that many decks", "[GameRules][DeckCount]")
{
    Game game(true);
    GameRules rules;
    rules.numDecks = 2;
    game.setGameRulesForTest(rules);

    // startNewShoe() calls Deck::create(numDecks) then shuffle(), and shuffle() inserts one cut card.
    game.startNewShoeForTest();
    CHECK(game.deckSizeForTest() == rules.numDecks * 52 + 1);
}

// ---- Advisor threading (CONTEXT.md §6 #2) ------------------------------------

TEST_CASE("Advisor offers surrender only when surrender is enabled", "[GameRules][Advisor]")
{
    GameRules on; // default Late -> enabled
    CHECK(BasicStrategy::getRecommendedAction(on, /*playerValue=*/16, /*dealerUp=*/10, /*hasAce=*/false,
                                              /*isPair=*/false, 0) == 'R');

    GameRules off; // surrender disabled -> falls through to the hard-hand advice (hit) instead of offering surrender
    off.surrenderType = SurrenderType::None;
    CHECK(BasicStrategy::getRecommendedAction(off, 16, 10, false, false, 0) == 'H');
}

TEST_CASE("Advisor splits a double-after-split pair only when DAS is allowed", "[GameRules][Advisor]")
{
    GameRules on; // default DAS true -> 'M' pair (3,3) suggests split ('P')
    CHECK(BasicStrategy::getRecommendedAction(on, /*pair 3 = value 6*/ 6, /*dealerUp=*/2, false, true, 3) == 'P');

    GameRules off; // DAS off -> the same 'M' pair becomes a hit ('H')
    off.doublesAfterSplitAllowed = false;
    CHECK(BasicStrategy::getRecommendedAction(off, 6, 2, false, true, 3) == 'H');
}

// ---- Persistence (additive JSON schema) --------------------------------------

TEST_CASE("Custom game rules survive a profile save/load round trip", "[GameRules][Persistence]")
{
    const fs::path path = test_support::temp_path("rules", "roundtrip");

    {
        PlayerProfile p("William");
        GameRules r;
        r.numDecks = 4;
        r.dealerStandsOnSoft17 = true;
        r.doublesAfterSplitAllowed = false;
        r.resplitAcesAllowed = false;
        r.surrenderType = SurrenderType::Early;
        p.gameRules = r;
        p.save(path.string());
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.gameRules.numDecks == 4);
    CHECK(loaded.gameRules.dealerStandsOnSoft17 == true);
    CHECK(loaded.gameRules.doublesAfterSplitAllowed == false);
    CHECK(loaded.gameRules.resplitAcesAllowed == false);
    CHECK(loaded.gameRules.surrenderType == SurrenderType::Early);

    fs::remove(path);
}

// Profiles written before custom rules existed carry no "gameRules" key; loading them must fall back to the default
// struct (which mirrors the compile-time Constants) rather than throwing or discarding the rest of the profile.
TEST_CASE("Profiles saved before custom rules existed still load with defaults", "[GameRules][Persistence]")
{
    const fs::path path = test_support::temp_path("rules", "legacy");

    {
        PlayerProfile fresh("William");
        fresh.setBankroll(4321);
        fresh.save(path.string()); // a real save, which now also writes a "gameRules" object
    }

    {
        std::fstream rw(path);
        json j;
        rw >> j;
        rw.close();

        j.erase("gameRules"); // simulate an old profile that predates custom rules

        std::ofstream out(path);
        out << j.dump(2);
    }

    const PlayerProfile loaded = PlayerProfile::load(path.string());
    CHECK(loaded.getBankroll() == 4321);
    CHECK(loaded.gameRules.numDecks == NUM_DECKS);            // default deck count preserved
    CHECK(loaded.gameRules.dealerStandsOnSoft17 == false);    // default H17 behaviour preserved
    CHECK(loaded.gameRules.doublesAfterSplitAllowed == DOUBLE_AFTER_SPLIT_ALLOWED);
    CHECK(loaded.gameRules.resplitAcesAllowed == RESPLIT_ACES_ALLOWED);
    CHECK(loaded.gameRules.surrenderType == SurrenderType::Late);

    fs::remove(path);
}

namespace {
    // A settled, winning human hand (playerIndex 0). computeCareerStatsHistory books exactly one career hand per such
    // record, so N of these play like N hands -- mirroring .summaries/0.4/hist_proof.cpp's makeRound().
    RoundRecord makeWinningHumanHand()
    {
        RoundRecord r;
        HandOutcome h;
        h.wager = 10;
        h.payout = 20;
        h.won = true;
        h.playerIndex = 0;
        h.playerName = "Ada";
        r.playerHands.push_back(h);
        return r;
    }
} // namespace

// B13 — quitting mid-shoe silently drops those hands from career stats. Root cause: to_json writes longHistory but not
// currentHistory, so a save/load round trip leaves the in-progress shoe empty and careerStatsFromHistory() recomputes to
// only the filed shoes. This mirrors .summaries/0.4/hist_proof.cpp at the PlayerProfile level; it fails before the fix
// (reload #1 recompute is 5, not 8) and passes once currentHistory is serialised additively on load.
TEST_CASE("B13: an in-progress shoe's rounds survive a save/load and still book into career stats", "[Schema][Persistence]")
{
    const fs::path path = test_support::temp_path("histproof", "b13");

    // Shoe 1 completes with five human hands; addShoeRecord() files them into longHistory and clears currentHistory.
    PlayerProfile p("Ada Lovelace");
    for (int i = 0; i < 5; ++i)
        p.addRoundRecord(makeWinningHumanHand());

    ShoeRecord shoe;
    shoe.rounds = std::vector<RoundRecord>(5, makeWinningHumanHand());
    shoe.completed = true;
    p.addShoeRecord(shoe);

    // A second shoe begins and plays three more hands before the window is closed mid-shoe.
    for (int i = 0; i < 3; ++i)
        p.addRoundRecord(makeWinningHumanHand());

    // Sanity: with all eight hands in memory, career stats already reflect them before any I/O happens.
    CHECK(p.careerStatsFromHistory().handsFinished == 8);

    p.save(path.string());

    // Reload #1: career stats must include the three in-progress hands (8 total), not just the five filed shoes.
    PlayerProfile a = PlayerProfile::load(path.string());
    CHECK(a.careerStatsFromHistory().handsFinished == 8);

    // A new session continues that shoe with one more hand, then quits mid-shoe again.
    a.addRoundRecord(makeWinningHumanHand());
    a.save(path.string());

    PlayerProfile b = PlayerProfile::load(path.string());
    CHECK(b.careerStatsFromHistory().handsFinished == 9); // every in-progress hand across both sessions must survive

    fs::remove(path);
}

