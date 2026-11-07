#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include <Constants.hpp>
#include <Card.hpp>
#include <Game.hpp>
#include <GameRules.hpp>
#include <Serialization.hpp>

using namespace Constants;

// ---------------------------------------------------------------------------
// B10 - maxSplitHands. Once a dead config (enforced but never persisted and not
// exposed in the rules UI), now serialized, exposed with restricted values
// {2, 3, 4, Unlimited}, and honored at every enforcement site. The serialization,
// cycling and row/description cases below fail on pre-B10 code: without the fix
// the field is dropped by to_json/from_json, applyGameRuleEdit(MaxSplitHands) has
// no case (no cycle), and gameRuleRows()/gameRuleDescription() have no entry.
// ---------------------------------------------------------------------------

// ---- Serialization (B10 #3): value + schema key survive a round-trip --------

TEST_CASE("maxSplitHands survives a to_json/from_json round-trip", "[B10][Serialization]")
{
    for (int v : {2, 3, 4, MAX_SPLIT_HANDS_UNLIMITED}) // -1 == unlimited too
    {
        GameRules r;
        r.maxSplitHands = v;

        nlohmann::json j;
        nlohmann::to_json(j, r);                          // ADL -> include/GameRules.hpp hook
        CHECK(j.contains("maxSplitHands"));               // schema key present (#3)

        GameRules r2;
        nlohmann::from_json(j, r2);                       // ADL -> from_json hook
        CHECK(r2.maxSplitHands == v);                     // value preserved through the wire
    }
}

// ---- Load-time normalization (B10 #5): clamp stored values into {2,3,4,-1} --

TEST_CASE("from_json clamps an out-of-range maxSplitHands to a valid value", "[B10][Serialization]")
{
    // Arbitrary invalid positive -> falls back to the default cap.
    nlohmann::json bad = nlohmann::json::object();
    bad["maxSplitHands"] = 5;
    GameRules r;
    nlohmann::from_json(bad, r);
    CHECK(r.maxSplitHands == MAX_SPLIT_HANDS);

    // A negative value is accepted as the unlimited sentinel (not clamped away).
    nlohmann::json neg = nlohmann::json::object();
    neg["maxSplitHands"] = -1;
    GameRules r2;
    nlohmann::from_json(neg, r2);
    CHECK(r2.maxSplitHands == MAX_SPLIT_HANDS_UNLIMITED);

    // Absent -> struct default.
    nlohmann::json empty = nlohmann::json::object();
    GameRules r3;
    nlohmann::from_json(empty, r3);
    CHECK(r3.maxSplitHands == MAX_SPLIT_HANDS);
}
// ---- Restricted values (B10 #4): cycling visits ONLY {2,3,4,Unlimited} ------

TEST_CASE("Max Split Hands cycles strictly through 2, 3, 4 and Unlimited", "[B10][GameRules]")
{
    constexpr int expected[] = {2, 3, 4, MAX_SPLIT_HANDS_UNLIMITED};
    const auto inSet = [&](int v) { for (int e : expected) if (e == v) return true; return false; };

    // Every starting value must step forward through exactly the set and wrap back to itself.
    for (int start : expected)
    {
        GameRules r;
        r.maxSplitHands = start;
        bool visited[4] = {false, false, false, false};
        for (int k = 0; k < 4; ++k)                       // four forward presses -> one full cycle
        {
            applyGameRuleEdit(r, GameRuleId::MaxSplitHands, /*forward=*/true);
            CHECK(inSet(r.maxSplitHands));                 // never lands on an out-of-range value
            for (int idx = 0; idx < 4; ++idx)
                if (r.maxSplitHands == expected[idx]) visited[idx] = true;
        }
        bool allReached = true;
        for (int i = 0; i < 4; ++i) allReached &= visited[i];
        CHECK(allReached);                                 // reached all four values, nothing else
    }

    // Backward direction wraps the same set.
    GameRules back;
    back.maxSplitHands = 2;
    applyGameRuleEdit(back, GameRuleId::MaxSplitHands, /*forward=*/false);
    CHECK(inSet(back.maxSplitHands));                      // e.g. 2 -> Unlimited

    GameRules back2;
    back2.maxSplitHands = MAX_SPLIT_HANDS_UNLIMITED;
    applyGameRuleEdit(back2, GameRuleId::MaxSplitHands, /*forward=*/false);
    CHECK(inSet(back2.maxSplitHands));                     // e.g. Unlimited -> 4
}

// ---- gameRuleRows / description (B10 #9) ------------------------------------

TEST_CASE("Max Split Hands has a rules-panel row and label", "[B10][GameRules]")
{
    const auto& rows = gameRuleRows();
    bool foundRow = false;
    for (const auto& row : rows)
        if (row.id == GameRuleId::MaxSplitHands)
        {
            foundRow = true;
            CHECK(std::string(row.label) == "Max Split Hands");
        }
    CHECK(foundRow);

    GameRules r;
    r.maxSplitHands = MAX_SPLIT_HANDS_UNLIMITED;
    CHECK(gameRuleDescription(r, GameRuleId::MaxSplitHands) == "Unlimited");
    r.maxSplitHands = 3;
    CHECK(gameRuleDescription(r, GameRuleId::MaxSplitHands) == "3 hands");
}

// ---- Enforcement (B10 #4): unlimited allows more split hands than a finite cap

TEST_CASE("Unlimited maxSplitHands permits more splits than a finite cap", "[B10][Split]")
{
    // Deal order mirrors the proven split-aces fixture: four aces land at the positions that drive re-splits, so with
    // re-splitting allowed a single seat can grow past two hands. Anything beyond two is only possible when the cap
    // does not stop it - identical setup, differing only in the cap value.
    const auto deal = { test_support::make_card(1, Suit::Spades), test_support::make_card(7, Suit::Hearts),
                        test_support::make_card(1, Suit::Hearts),  test_support::make_card(6, Suit::Diamonds),
                        test_support::make_card(1, Suit::Clubs),   test_support::make_card(1, Suit::Diamonds),
                        test_support::make_card(1, Suit::Hearts),  test_support::make_card(9, Suit::Spades),
                        test_support::make_card(2, Suit::Clubs),   test_support::make_card(3, Suit::Diamonds) };

    // Finite cap = 2: re-splits stop after two hands.
    {
        Game game(true);
        test_support::start_round(game, deal);
        GameRules rules;
        rules.maxSplitHands = 2;
        rules.resplitAcesAllowed = true;
        game.setGameRulesForTest(rules);

        CHECK(game.split());                             // AA -> two hands (cap not yet reached)
        CHECK(game.testPlayer(0).hands.size() == 2);
        test_support::tickFrame(game);                   // first ace draws a card
        CHECK(!game.split());                            // cap of 2 refuses a third hand
    }

    // Unlimited: the same deal pushes past what cap=2 stopped above.
    {
        Game game(true);
        test_support::start_round(game, deal);
        GameRules rules;
        rules.maxSplitHands = MAX_SPLIT_HANDS_UNLIMITED; // -1
        rules.resplitAcesAllowed = true;
        game.setGameRulesForTest(rules);

        CHECK(game.split());                             // AA -> two hands
        test_support::tickFrame(game);                   // first ace draws a card (enables re-split)
        CHECK(game.split());                             // re-split succeeds where cap=2 refused
        CHECK(game.testPlayer(0).hands.size() == 3);     // strictly more than the finite-cap-2 result of 2
    }
}

