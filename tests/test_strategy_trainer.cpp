#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include "BasicStrategy.hpp"
#include "StrategyTrainer.hpp"

using namespace BasicStrategy;
using Catch::Approx;

namespace
{

    Hand makeHand(std::initializer_list<int> values)
    {
        std::vector<Card> cards;
        for (int v : values)
            cards.push_back(test_support::make_card(v, Suit::Spades));
        return Hand(cards);
    }

    // Recompute what basic strategy prescribes for a question hand + dealer up-card — the same advice the trainer must
    // grade against. Kept as a free helper so the generation test can assert the trainer and the advisor agree exactly.
    char expectedAction(const Hand& player, const Card& dealerUp)
    {
        int playerValue = player.getValue();
        bool hasAce = player.isSoft();
        bool isPair = player.size() == 2 &&
                      player.getCard(0).getBlackjackValue() == player.getCard(1).getBlackjackValue();
        int pairValue = isPair ? player.getCard(0).getBlackjackValue() : 0;
        return getRecommendedAction(playerValue, dealerUp.getBlackjackValue(), hasAce, isPair, pairValue);
    }
}

// ---------------------------------------------------------------------------
// Feature 2 (Strategy Trainer): the trainer grades a chosen action against basic strategy. This file covers the
// pure-logic layer first (grading accuracy, streaks, timed-session completion, generation-vs-advisor agreement),
// exactly mirroring how test_split_aces / test_side_bets assert logic before any UI is written.
// ---------------------------------------------------------------------------

TEST_CASE("StrategyTrainer: advisor matches BasicStrategy for representative hands", "[StrategyTrainer][Grading]")
{
    StrategyTrainer trainer;
    auto& t = trainer;

    // Hard 16 vs a dealer 2 -> Stand (HARD row 16, column 2 is 'S').
    CHECK(t.loadQuestion(makeHand({10, 6}), test_support::make_card(2, Suit::Hearts)) == 'S');

    // Hard 15 vs a ten -> Surrender (SURRENDER[15][T] == 'R').
    CHECK(t.loadQuestion(makeHand({10, 5}), test_support::make_card(10, Suit::Hearts)) == 'R');

    // Pair of eights vs a seven -> Split (PAIRS[8,8][7] resolves to 'P').
    CHECK(t.loadQuestion(makeHand({8, 8}), test_support::make_card(7, Suit::Hearts)) == 'P');

    // Soft 18 (Ace-7) vs a two -> Double Down (SOFT[A7][2] == 'D').
    CHECK(t.loadQuestion(makeHand({1, 7}), test_support::make_card(2, Suit::Hearts)) == 'D');
}

TEST_CASE("StrategyTrainer: submitAction grades correct and incorrect answers", "[StrategyTrainer][Grading]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(6, 0); // graded session so submitAction tallies
    t.loadQuestion(makeHand({10, 6}), test_support::make_card(2, Suit::Hearts)); // book says Stand

    CHECK(t.submitAction('S') == true);
    CHECK(t.lastVerdictForTest().find("Correct") != std::string::npos);

    // Reload the same question and take a wrong action; it must grade as incorrect.
    t.loadQuestion(makeHand({10, 6}), test_support::make_card(2, Suit::Hearts));
    CHECK(t.submitAction('H') == false);
    CHECK(t.lastVerdictForTest().find("Incorrect") != std::string::npos);
}

TEST_CASE("StrategyTrainer: accuracy and streaks track a known answer sequence", "[StrategyTrainer][Stats]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    const Hand q = makeHand({10, 6}); // book always says Stand here
    t.startSession(6, 0); // graded session; stats start at zero

    t.loadQuestion(q, test_support::make_card(2, Suit::Hearts)); CHECK(t.submitAction('S')); // T streak 1
    t.loadQuestion(q, test_support::make_card(2, Suit::Hearts)); CHECK(t.submitAction('S')); // T streak 2
    t.loadQuestion(q, test_support::make_card(2, Suit::Spades));  CHECK(!t.submitAction('H'));// F streak 0
    t.loadQuestion(q, test_support::make_card(2, Suit::Spades));  CHECK(t.submitAction('S')); // T streak 1

    const auto s = t.statsForTest();
    CHECK(s.totalQuestions == 4);
    CHECK(s.correctAnswers == 3);
    CHECK(s.currentStreak == 1);
    CHECK(s.bestStreak == 2);
    CHECK(s.accuracyPercent() == Approx(75.0f).margin(0.01f));
}

TEST_CASE("StrategyTrainer: all-correct session is perfect with best streak equal to total", "[StrategyTrainer][Stats]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    const Hand q = makeHand({10, 6}); // book says Stand
    t.startSession(6, 0); // graded session; stats start at zero

    for (int i = 0; i < 3; ++i)
    {
        t.loadQuestion(q, test_support::make_card(2, Suit::Hearts));
        CHECK(t.submitAction('S'));
    }

    const auto s = t.statsForTest();
    CHECK(s.totalQuestions == 3);
    CHECK(s.correctAnswers == 3);
    CHECK(s.isPerfect() == true);
    CHECK(s.accuracyPercent() == 100.0f);
    CHECK(s.bestStreak == 3);
    CHECK(s.currentStreak == 3);
}

TEST_CASE("StrategyTrainer: untimed session never reports time completion and does not self-end on update", "[StrategyTrainer][Timed]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(6, 0); // untimed

    CHECK(t.sessionActiveForTest() == true);
    CHECK(t.timedSessionCompleteForTest() == false);

    t.update(100.0f); // a large delta must not finish an untimed session
    CHECK(t.sessionActiveForTest() == true);
    CHECK(t.timedSessionCompleteForTest() == false);
}

TEST_CASE("StrategyTrainer: timed session completes once its wall-clock budget elapses", "[StrategyTrainer][Timed]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(6, 5); // 5-second budget

    CHECK(t.sessionActiveForTest() == true);
    CHECK(t.elapsedSecondsForTest() == Approx(0.0f));

    t.update(3.0f);
    CHECK(t.elapsedSecondsForTest() == Approx(3.0f));
    CHECK(t.timedSessionCompleteForTest() == false);
    CHECK(t.sessionActiveForTest() == true);

    t.update(2.0f);
    CHECK(t.elapsedSecondsForTest() == Approx(5.0f));
    // Budget elapsed: the timed session is now complete, no longer active, and recorded a time-up verdict.
    CHECK(t.timedSessionCompleteForTest() == true);
    CHECK(t.sessionActiveForTest() == false);
    CHECK(t.lastVerdictForTest().find("Time") != std::string::npos);
}

TEST_CASE("StrategyTrainer: auto-dealt questions are graded by the same advisor", "[StrategyTrainer][Generation]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(6, 0);

    int guard = 0;
    while (t.sessionActiveForTest() && guard++ < 40)
    {
        const Hand& qh = t.questionHandForTest();
        const Card& du = t.questionDealerUpForTest().getCard(0);
        CHECK(t.currentCorrectActionForTest() == expectedAction(qh, du)); // generation matches the advisor
        t.nextQuestion();
    }
    CHECK(guard > 1); // a fresh shoe should yield many valid questions before it could run out
}

TEST_CASE("StrategyTrainer: surrender keys 'Y' and 'R' both grade as surrender", "[StrategyTrainer][Surrender]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(6, 0); // graded session so submitAction tallies
    const Hand q = makeHand({10, 5}); // book says Surrender against a ten

    t.loadQuestion(q, test_support::make_card(10, Suit::Hearts));
    CHECK(t.submitAction('R'));              // the explicit surrender letter

    t.loadQuestion(q, test_support::make_card(10, Suit::Spades));
    CHECK(t.submitAction('Y'));              // the game's surrender key normalizes to 'R'
}

TEST_CASE("StrategyTrainer: untimed session ends when the shoe runs out", "[StrategyTrainer][Lifecycle]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(1, 0); // single deck -> a bounded number of questions
    CHECK(t.deckCountForTest() == 1);

    int guard = 0;
    while (t.sessionActiveForTest() && guard++ < 400)
        t.nextQuestion();

    CHECK(!t.sessionActiveForTest());
    CHECK(t.sessionCompleteForTest());
}

TEST_CASE("StrategyTrainer: no grading before a session; stats reset each new session", "[StrategyTrainer][Lifecycle]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    const Hand q = makeHand({10, 6});
    t.loadQuestion(q, test_support::make_card(2, Suit::Hearts)); // load only sets the question; nothing is graded yet
    CHECK(t.submitAction('S') == false);          // no active session => grading is a no-op

    t.startSession(6, 0);
    CHECK(t.sessionActiveForTest());
    const auto before = t.statsForTest().totalQuestions;
    t.submitAction('S');                          // grades whatever the shoe dealt (data-dependent result)
    CHECK(t.statsForTest().totalQuestions == before + 1); // but it always tallies exactly one question

    t.startSession(6, 0);                         // a new session wipes the previous tallies
    const auto fresh = t.statsForTest();
    CHECK(fresh.totalQuestions == 0);
    CHECK(fresh.correctAnswers == 0);
    CHECK(fresh.bestStreak == 0);
}

TEST_CASE("StrategyTrainer: auto-dealt questions never include a natural blackjack", "[StrategyTrainer][Generation]")
{
    // B18: this used to sweep ONE shoe shuffled from std::random_device, so it sampled a fresh random order every run
    // and could only ever flake, never pin. Sweeping a fixed seed list makes the identical sweep reproducible, and
    // running it to shoe exhaustion (not to an arbitrary 100-question cap) is what reaches the B19 path at all.
    const unsigned long long seeds[] = {1ULL, 7ULL, 42ULL, 1234ULL, 99991ULL};
    for (unsigned long long seed : seeds)
    {
        StrategyTrainer trainer;
        trainer.startSessionWithSeedForTest(6, 0, seed);   // 6 decks, untimed: the session runs until the shoe is spent

        int guard = 0;
        int questions = 0;
        while (trainer.sessionActiveForTest() && guard++ < 400)
        {
            INFO("seed " << seed << " question " << questions);
            CHECK(!trainer.questionHandForTest().isBlackjack()); // every presented hand requires a real decision
            ++questions;
            trainer.nextQuestion();
        }
        CHECK(questions > 1);   // a fresh shoe yields many non-blackjack questions before it runs out
        INFO("seed " << seed << " swept " << questions << " questions");
    }
}

// B19 — the generation rule above has a hole the shuffled test could only ever flake on, never pin. nextQuestion()
// rejects blackjacks with a do/while bounded by `deck.size() >= 3`:
//
//     do { draw 2 player cards + 1 dealer card; }
//     while (questionHand.isBlackjack() && attempts < 500 && deck.size() >= 3);
//     computeBookAction();
//
// The entry guard only requires deck.size() >= 3, so the loop can draw the shoe dry on its very first pass. When that
// hand is a blackjack the condition's `deck.size() >= 3` term is false, the loop exits, and computeBookAction()
// presents the blackjack anyway — a natural 21, which has no decision to train. Reaching it needs the shoe to run out
// at exactly that moment, which is why #259 flaked under load instead of failing every time.
//
// Injecting an exact three-card shoe (dealer up-card, ten, ace — drawn back-to-front, so the player gets ace+ten and
// the shoe is spent) makes the exhaustion path deterministic: RED before the guard, green after.
TEST_CASE("StrategyTrainer: a blackjack at shoe exhaustion is never presented", "[StrategyTrainer][Generation]")
{
    StrategyTrainer trainer;
    trainer.startSession(6, 0);
    // Deck::draw() pops from the back: ace, then ten -> a natural blackjack, and the shoe is empty afterwards.
    trainer.setDeckForTest({test_support::make_card(7, Suit::Clubs),
                            test_support::make_card(10, Suit::Hearts),
                            test_support::make_card(1, Suit::Spades)});
    REQUIRE(trainer.remainingCardsForTest() == 3);   // passes the `deck.size() < 3` entry guard, then runs dry

    trainer.nextQuestion();

    CHECK(!trainer.questionHandForTest().isBlackjack());  // RED before the fix: the natural 21 is presented
    CHECK(!trainer.sessionActiveForTest());               // shoe spent -> the session ends rather than dealing it
}
