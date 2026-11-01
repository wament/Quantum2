// Stats-batching refactor (CONTEXT_refactor.md #9e): prove computeCareerStats reproduces the live per-hand PlayerStats
// increments by hand-computing the expected career from a recorded shoe and asserting field-by-field equality, plus an
// equivalence cross-check against the existing production analytics recompute in StatsDashboard::feed() on the fields they
// agree on (locking, rather than hiding, the intentional wins/pushes divergence).

#include <catch2/catch_all.hpp>
#include "test_support.hpp"     // test_support::make_card(value, suit) -> Card
#include "Card.hpp"
#include "Constants.hpp"
#include "GameRecord.hpp"       // computeCareerStats, PlayerStats, ShoeRecord, RoundRecord, HandOutcome
#include "StatsDashboard.hpp"   // production recompute oracle

using Catch::Approx;

namespace {

    // A single human-seat (playerIndex 0) hand with explicit settlement + optional side-bet fields.
    HandOutcome humanHand(std::vector<Card> cards, int wager, int payout, bool won,
                          int sideBetWager = 0, int sideBetPayout = 0)
    {
        HandOutcome h;
        h.cards = std::move(cards);
        h.wager = wager;
        h.payout = payout;
        h.won = won;
        h.playerIndex = 0;                 // human seat only is what the profile stores (HandEvaluator gates on pIdx == 0)
        h.sideBetWager = sideBetWager;
        h.sideBetPayout = sideBetPayout;
        return h;
    }

    RoundRecord roundOf(HandOutcome&& hand)
    {
        RoundRecord r;
        r.playerHands.push_back(std::move(hand));
        return r;
    }

    // A single-player shoe exercising every derivable field: a 3:2 natural, a bust, a push, a surrender (recorded as !won -> loss),
    // an ordinary win, and winning/losing side bets on hands whose MAIN result differs from the side-bet outcome.
    ShoeRecord diverseShoe()
    {
        ShoeRecord s;
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(1, Suit::Hearts), test_support::make_card(10, Suit::Spades)}, 10, 30, true)));             // R1: natural BJ, net +20 -> win + BJ
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(7, Suit::Hearts), test_support::make_card(8, Suit::Spades), test_support::make_card(8, Suit::Diamonds)}, 20, 0, false))); // R2: bust, net -20 -> loss
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(5, Suit::Clubs), test_support::make_card(6, Suit::Hearts)}, 15, 15, true)));                 // R3: push (net 0)
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(4, Suit::Diamonds), test_support::make_card(9, Suit::Clubs)}, 25, 0, false)));               // R4: surrender/loss, net -25
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(3, Suit::Hearts), test_support::make_card(10, Suit::Diamonds)}, 10, 20, true)));             // R5: ordinary win, net +10
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(2, Suit::Spades), test_support::make_card(9, Suit::Hearts)}, 10, 0, false, /*side*/ 5, 15))); // R6: lost main (net -10) + winning side bet
        s.rounds.push_back(roundOf(humanHand({test_support::make_card(6, Suit::Diamonds), test_support::make_card(10, Suit::Hearts)}, 10, 20, true, /*side*/ 5, 0))); // R7: won main (net +10) + lost side bet
        return s;
    }

} // namespace

// Field-by-field golden comparison: every value below is hand-computed from the live HandEvaluator/RoundDealer semantics that
// produce the record in the first place, so an equality here proves computeCareerStats reproduces them exactly.
TEST_CASE("computeCareerStats reproduces a recorded shoe's career field-by-field", "[stats-batching]")
{
    const PlayerStats got = computeCareerStats(diverseShoe());

    // Career win/loss/push/hand counts (human seat: R1,R5,R7 wins; R2 bust, R4 surrender, R6 main lost losses; R3 push).
    CHECK(got.wins == 3);
    CHECK(got.losses == 3);
    CHECK(got.pushes == 1);
    CHECK(got.handsFinished == 7);

    // Natural blackjacks: only R1.
    CHECK(got.blackjacks == 1);

    // Per-hand extremes: best single-hand win +20 (R1), worst single-hand loss -25 (R4).
    CHECK(got.greatestHand == 20);
    CHECK(got.greatestHandLoss == -25);

    // Human-seat cumulative extremes for the shoe; session fields coincide with a single first shoe.
    // Running cumulative across R1..R7: +20, 0, 0, -25, -15, -25, -15 -> peak +20, trough -25.
    CHECK(got.greatestWin == 20);
    CHECK(got.greatestLoss == -25);
    CHECK(got.greatestSessionWin == 20);
    CHECK(got.greatestSessionLoss == -25);

    // Longest win/lose streaks over the Win/Lose/Push sequence R1..R7 (W,L,P,L,W,L,W): max run of each is 1.
    CHECK(got.longestWinningStreak == 1);
    CHECK(got.longestLosingStreak == 1);

    // Per-hand profit variance accumulators over every hand's net {20,-20,0,-25,10,-10,10}.
    CHECK(got.handProfitCount == 7);
    CHECK(got.handProfitSum == -15);     // sum of the nets (40 positive + -55 negative)
    CHECK(got.handProfitSumSq == 1725);  // 400+400+0+625+100+100+100

    // Side bets: one win (R6) and one loss (R7); net is payout - wager each time.
    CHECK(got.sideBetWins == 1);
    CHECK(got.sideBetLosses == 1);
    CHECK(got.sideBetProfitSum == 5);    // (+10) + (-5)
}

// Each player owns their own PlayerStats (HandEvaluator uses p.getStats()), so the human profile reflects only seat 0.
TEST_CASE("computeCareerStats counts only the human seat for career fields", "[stats-batching]")
{
    ShoeRecord shoe;

    HandOutcome bj; bj.cards = {test_support::make_card(1, Suit::Hearts), test_support::make_card(10, Suit::Spades)};
    bj.wager = 10; bj.payout = 30; bj.won = true; bj.playerIndex = 0; shoe.rounds.push_back(roundOf(std::move(bj)));

    HandOutcome npcLoss; npcLoss.cards = {test_support::make_card(2, Suit::Spades), test_support::make_card(9, Suit::Hearts)};
    npcLoss.wager = 10; npcLoss.payout = 0; npcLoss.won = false; npcLoss.playerIndex = 1; shoe.rounds.push_back(roundOf(std::move(npcLoss)));

    const PlayerStats got = computeCareerStats(shoe);

    CHECK(got.wins == 1);          // human only
    CHECK(got.losses == 0);        // NPC loss excluded from the profile
    CHECK(got.pushes == 0);
    CHECK(got.handsFinished == 1); // human hands only
    CHECK(got.blackjacks == 1);
    CHECK(got.greatestHand == 20);
    CHECK(got.greatestHandLoss == 0);        // human's only hand net was +20; default (never negative) stays 0
    CHECK(got.handProfitCount == 1);
    CHECK(got.handProfitSum == 20);
    CHECK(got.longestWinningStreak == 1);    // streak tracked over the human seat's hands only
    CHECK(got.longestLosingStreak == 0);
    CHECK(got.greatestWin == 20);
    CHECK(got.greatestLoss == 0);            // human cumulative reached +20, never negative
}

// Equivalence to the existing production recompute on the record-derived fields they share; documents the intentional
// divergence where feed() folds pushes into wins while live play separates them.
TEST_CASE("computeCareerMatches agrees with StatsDashboard::feed() on shared record-derived fields", "[stats-batching]")
{
    const ShoeRecord shoe = diverseShoe();
    const PlayerStats got = computeCareerStats(shoe);

    StatsDashboard dashboard;
    dashboard.feed(PlayerStats{}, shoe.rounds);
    const auto& c = dashboard.computed();

    // Fields where the production recompute and computeCareerMatches share identical record-derived logic.
    CHECK(c.losses == got.losses);         // both count every !won hand as a loss (records never flag surrender)
    CHECK(c.blackjacks == got.blackjacks);
    CHECK(c.handsFinished == got.handsFinished);
    CHECK(c.greatestHandProfit == got.greatestHand);   // both max over per-hand payout - wager
    CHECK(c.worstHandLoss == got.greatestHandLoss);    // both min over per-hand net

    // Known, intentional divergence: feed() counts every won hand as a win (records never flag surrender), whereas live play
    // separates pushes from wins. computeCareerMatches matches the profile/live accounting -- one push stands apart from wins.
    CHECK(got.pushes == 1);
    CHECK(c.wins == got.wins + got.pushes);   // feed_ folds the single push into its win count
}
