#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include <Constants.hpp>
#include <Card.hpp>
#include <Hand.hpp>
#include <BasicStrategy.hpp>
#include <Tournament.hpp>

// ---------------------------------------------------------------------------
// Tournament mode (Feature): the rules controller + AI strategy are tested headless-first against an injected shoe,
// mirroring test_strategy_trainer.cpp. All pure-logic cases run over the Graphics-only Blackjack2Tests binary with no SFML.
// Game-integration cases then confirm the shared play path runs under tournament rules.
// ---------------------------------------------------------------------------

using namespace BasicStrategy;

namespace
{
    // Build a two-card hand from (value,suit) pairs for the advisor tests.
    Hand makeHand(int v0, int s0, int v1, int s1)
    {
        Hand h;
        h.addCard(test_support::make_card(v0, static_cast<Suit>(s0)));
        h.addCard(test_support::make_card(v1, static_cast<Suit>(s1)));
        return h;
    }
}

TEST_CASE("Tournament: config defaults are a six-stage tournament of four-deck shoes", "[Tournament][Setup]")
{
    Tournament t;
    Tournament::TournamentConfig cfg; // all defaults
    t.start(cfg);

    CHECK(t.stages() == 6);
    CHECK(t.decksPerShoe() == 4);
    // stage-1 minimum is ten, growing five units per stage across the six stages (10,15,20,25,30,35).
    for (int s = 1; s <= 6; ++s)
        CHECK(t.minBetForStageForTest(s) == 10 + (s - 1) * 5);
    CHECK(!t.isCompleteForTest());
    CHECK(t.currentStageForTest() == 1);
}

TEST_CASE("Tournament: min bet escalates per stage and clamps outside the range", "[Tournament][Betting]")
{
    Tournament t;
    Tournament::TournamentConfig cfg;
    cfg.stages = 4;
    cfg.minBetStage1 = 20;
    cfg.minBetGrowth = 7;   // 20, 27, 34, 41
    t.start(cfg);

    CHECK(t.minBetForStageForTest(1) == 20);
    CHECK(t.minBetForStageForTest(2) == 27);
    CHECK(t.minBetForStageForTest(4) == 41);
    // clamps: below range -> stage-1 minimum; past the end -> final stage's minimum.
    CHECK(t.minBetForStageForTest(0) == 20);
    CHECK(t.minBetForStageForTest(99) == 41);
}

TEST_CASE("Tournament: advancing through every stage completes it, then holds", "[Tournament][Stages]")
{
    Tournament t;
    Tournament::TournamentConfig cfg;
    cfg.stages = 3;
    t.start(cfg);

    CHECK(t.advanceStage() == true);   // -> stage 2 (another remains)
    CHECK(t.currentStageForTest() == 2);
    CHECK(t.advanceStage() == true);   // -> stage 3 (the final stage)
    CHECK(t.currentStageForTest() == 3);
    CHECK(t.advanceStage() == false);  // final stage run out -> tournament complete
    CHECK(t.isCompleteForTest());
    CHECK(t.shoesPlayedForTest() == 3);

    // Staying on the completed tournament is a no-op.
    CHECK(t.advanceStage() == false);
}

TEST_CASE("Tournament: AI opponents split tens and double any hard eight, else follow basic strategy", "[Tournament][Strategy]")
{
    Tournament t;
    Tournament::TournamentConfig cfg;
    t.start(cfg);

    // Always split tens (a hard 20) against ANY dealer up-card.
    auto tens = makeHand(10, 0, 10, 1);   // two ten-value cards
    CHECK(t.aiAdvice(tens, test_support::make_card(6, Suit::Spades)) == 'P');
    CHECK(t.aiAdvice(tens, test_support::make_card(11, Suit::Clubs)) == 'P'); // vs an ace too

    // Always double a hard eight, regardless of the dealer.
    auto eight = makeHand(5, 0, 3, 1);    // hard 8, not a pair
    CHECK(t.aiAdvice(eight, test_support::make_card(6, Suit::Spades)) == 'D');
    CHECK(t.aiAdvice(eight, test_support::make_card(10, Suit::Clubs)) == 'D');

    // Every other hand follows ordinary basic strategy unchanged: compare directly for the same parameters.
    auto sixteen = makeHand(10, 2, 6, 3); // hard 16
    Card up = test_support::make_card(6, Suit::Hearts);
    bool isPair = sixteen.size() == 2 &&
                  sixteen.getCard(0).getBlackjackValue() == sixteen.getCard(1).getBlackjackValue();
    int pairValue = isPair ? sixteen.getCard(0).getBlackjackValue() : 0;
    CHECK(t.aiAdvice(sixteen, up) ==
          getRecommendedAction(sixteen.getValue(), up.getBlackjackValue(), sixteen.isSoft(), isPair, pairValue));

    // Sanity: the plain advisor itself does NOT split tens and does not double a hard eight vs a 7 (so the override is real).
    CHECK(getRecommendedAction(20, 6, false, true, 10) != 'P');
    CHECK(getRecommendedAction(8, 7, false, false, 0) != 'D');
}

TEST_CASE("Tournament: leaderboard ranks by bankroll descending with stable ties", "[Tournament][Leaderboard]")
{
    Tournament t;
    std::vector<std::string> names = {"Alice", "Bob", "Carol", "Dan"};
    std::vector<int> chips = {1000, 2500, 2500, 500};

    auto order = t.rankOrder(names, chips);

    // Richest first: Bob and Carol (tie) keep original order, then Alice, then Dan.
    CHECK(order.size() == 4);
    CHECK(order[0] == 1);   // Bob
    CHECK(order[1] == 2);   // Carol (stable tie with Bob)
    CHECK(order[2] == 0);   // Alice
    CHECK(order[3] == 3);   // Dan
    // The winner is the first ranked seat.
    CHECK(names[order.front()] == "Bob");
}

// ---------------------------------------------------------------------------
// Game integration: the shared play path runs under tournament rules (equal chips, escalating stage minimum, human acts).
// ---------------------------------------------------------------------------

TEST_CASE("Tournament game: builds an equal-chip table under active tournament rules", "[Tournament][Game]")
{
    test_support::IsolatedHome home("tournament");   // B17: hermetic HOME (see test_support.hpp)
    Game game(true);  // headless: no window/assets/replay/HUD, pure game state only
    Tournament::TournamentConfig cfg;
    cfg.stages = 6;
    cfg.startingChips = 2000;
    cfg.minBetStage1 = 25;
    cfg.minBetGrowth = 5;

    game.startTournamentSession(cfg);

    REQUIRE(game.getAppModeForTest() == AppMode::Tournament);
    REQUIRE(game.tournamentActiveForTest());
    REQUIRE(game.currentTournamentStageForTest() == 1);

    auto& seats = game.playersForTest();
    REQUIRE(seats.size() >= 2);                 // human + at least one opponent to beat
    for (auto& p : seats)
        CHECK(p.getBankroll() == cfg.startingChips);   // equal starting chips for every seat

    // The stage-1 minimum bet is enforced as the betting floor for this run.
    REQUIRE(game.activeMinBet() == 25);
}

namespace
{
    // A finished stage shoe, stamped Tournament -- what dealerTurn captures at the boundary.
    ShoeRecord makeStageShoe(int bankrollAtShoeEnd, int handsWon, int rounds)
    {
        ShoeRecord shoe;
        shoe.mode = ShoeMode::Tournament;
        shoe.bankrollAtShoeEnd = bankrollAtShoeEnd;
        shoe.completed = true;
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
            shoe.rounds.push_back(round);
        }
        return shoe;
    }
}

// The tournament's ONLY durable write, and the producer this change adds. The seat is ephemeral
// (setPersistable(false)), so without commitTournamentShoe a run records nothing at all -- and with a
// router that falls through, a run would silently rewrite the player's career. Both directions.
TEST_CASE("Tournament: a stage shoe is recorded on the career file and never touches career stats", "[Tournament][Session]")
{
    test_support::IsolatedHome home("tournament_record");   // B17: hermetic HOME
    const std::string path = Player::savePath();

    // A real career profile to protect: a bankroll and one completed career shoe.
    {
        PlayerProfile seed("Career");
        seed.setBankroll(7777);
        RoundRecord round;
        round.timestamp = 1;
        HandOutcome hand;
        hand.playerIndex = 0;
        hand.won = true;
        hand.wager = 10;
        hand.payout = 20;
        round.playerHands.push_back(hand);
        seed.addRoundRecord(round);
        ShoeRecord meta;
        meta.mode = ShoeMode::Regular;
        meta.bankrollAtShoeEnd = 1500;
        seed.addShoeRecord(meta);
        seed.save(path);
    }

    Game game(true);   // headless
    Tournament::TournamentConfig cfg;
    cfg.stages = 2;
    game.startTournamentSession(cfg);

    // Stage 1 runs out: the shoe is booked on disk and the run advances to stage 2.
    game.finishTournamentShoeForTest(makeStageShoe(2400, 9, 2));

    {
        const PlayerProfile after = PlayerProfile::load(path);
        REQUIRE(after.getTournamentShoes().size() == 1);
        CHECK(after.getTournamentShoes()[0].mode == ShoeMode::Tournament);
        CHECK(after.getTournamentShoes()[0].bankrollAtShoeEnd == 2400);
        CHECK(after.tournamentModeStats().shoesPlayed == 1);
        CHECK(after.tournamentModeStats().bestBankroll == 2400);
        CHECK(after.tournamentModeStats().mostHandsWon == 18);   // 2 rounds x 9, per-SHOE total
        CHECK(after.tournamentModeStats().bestFinish == 1);      // the stage that just ran out
        CHECK(after.tournamentsCompleted == 0);                  // stage 2 remains

        // THE invariant: the career half is untouched by the run.
        CHECK(after.getBankroll() == 7777);
        REQUIRE(after.getLongHistory().size() == 1);
        CHECK(after.getLongHistory()[0].mode == ShoeMode::Regular);
        CHECK(after.applyRollup().wins == 1);
        CHECK(after.getDailyShoes().empty());
    }

    // The final stage runs out: recorded, the peak from stage 1 survives, and the run counts complete.
    game.finishTournamentShoeForTest(makeStageShoe(3100, 4, 1));

    const PlayerProfile after = PlayerProfile::load(path);
    CHECK(after.getTournamentShoes().size() == 2);
    CHECK(after.tournamentModeStats().shoesPlayed == 2);
    CHECK(after.tournamentModeStats().bestBankroll == 3100);
    CHECK(after.tournamentModeStats().mostHandsWon == 18);   // stage 1's peak survived stage 2
    CHECK(after.tournamentModeStats().bestFinish == 2);
    CHECK(after.tournamentsCompleted == 1);
    CHECK(after.getBankroll() == 7777);
    REQUIRE(after.getLongHistory().size() == 1);
    CHECK(after.applyRollup().wins == 1);
}
