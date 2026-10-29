#include <catch2/catch_all.hpp>

#include "Constants.hpp"
#include "Card.hpp"
#include "Hand.hpp"
#include "BasicStrategy.hpp"
#include "StrategyTrainer.hpp"

using namespace BasicStrategy;
using Catch::Approx;

namespace
{
    Card makeCard(int value, Suit suit)
    {
        Card c;
        c.value = value;
        c.suit = suit;
        return c;
    }

    Hand makeHand(std::initializer_list<int> values)
    {
        std::vector<Card> cards;
        for (int v : values)
            cards.push_back(makeCard(v, Suit::Spades));
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
    CHECK(t.loadQuestion(makeHand({10, 6}), makeCard(2, Suit::Hearts)) == 'S');

    // Hard 15 vs a ten -> Surrender (SURRENDER[15][T] == 'R').
    CHECK(t.loadQuestion(makeHand({10, 5}), makeCard(10, Suit::Hearts)) == 'R');

    // Pair of eights vs a seven -> Split (PAIRS[8,8][7] resolves to 'P').
    CHECK(t.loadQuestion(makeHand({8, 8}), makeCard(7, Suit::Hearts)) == 'P');

    // Soft 18 (Ace-7) vs a two -> Double Down (SOFT[A7][2] == 'D').
    CHECK(t.loadQuestion(makeHand({1, 7}), makeCard(2, Suit::Hearts)) == 'D');
}

TEST_CASE("StrategyTrainer: submitAction grades correct and incorrect answers", "[StrategyTrainer][Grading]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    t.startSession(6, 0); // graded session so submitAction tallies
    t.loadQuestion(makeHand({10, 6}), makeCard(2, Suit::Hearts)); // book says Stand

    CHECK(t.submitAction('S') == true);
    CHECK(t.lastVerdictForTest().find("Correct") != std::string::npos);

    // Reload the same question and take a wrong action; it must grade as incorrect.
    t.loadQuestion(makeHand({10, 6}), makeCard(2, Suit::Hearts));
    CHECK(t.submitAction('H') == false);
    CHECK(t.lastVerdictForTest().find("Incorrect") != std::string::npos);
}

TEST_CASE("StrategyTrainer: accuracy and streaks track a known answer sequence", "[StrategyTrainer][Stats]")
{
    StrategyTrainer trainer;
    auto& t = trainer;
    const Hand q = makeHand({10, 6}); // book always says Stand here
    t.startSession(6, 0); // graded session; stats start at zero

    t.loadQuestion(q, makeCard(2, Suit::Hearts)); CHECK(t.submitAction('S')); // T streak 1
    t.loadQuestion(q, makeCard(2, Suit::Hearts)); CHECK(t.submitAction('S')); // T streak 2
    t.loadQuestion(q, makeCard(2, Suit::Spades));  CHECK(!t.submitAction('H'));// F streak 0
    t.loadQuestion(q, makeCard(2, Suit::Spades));  CHECK(t.submitAction('S')); // T streak 1

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
        t.loadQuestion(q, makeCard(2, Suit::Hearts));
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

    t.loadQuestion(q, makeCard(10, Suit::Hearts));
    CHECK(t.submitAction('R'));              // the explicit surrender letter

    t.loadQuestion(q, makeCard(10, Suit::Spades));
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
    t.loadQuestion(q, makeCard(2, Suit::Hearts)); // load only sets the question; nothing is graded yet
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
    StrategyTrainer trainer;
    trainer.startSession(6, 0); // the first question is dealt via nextQuestion(), which skips blackjacks

    int guard = 0;
    while (trainer.sessionActiveForTest() && guard++ < 100)
    {
        CHECK(!trainer.questionHandForTest().isBlackjack()); // every presented hand requires a real decision
        trainer.nextQuestion();
    }
    CHECK(guard > 1); // a fresh shoe yields many non-blackjack questions before it could run out
}
