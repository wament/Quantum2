#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include <Constants.hpp>
#include <Game.hpp>

using Catch::Approx;

// Feature 8 (speed-linked dealing animation) + Feature 7 (sound gating): the card-deal tempo must track the selected game
// action speed so animations neither race ahead of nor lag behind faster actions, and Hyper/Gamer deliver nothing at all.
// Every assertion here is pure-logic/headless-safe: dealDurationScale/animationDisabled live in GameTiming (Constants.hpp),
// and the sound gate is exercised through Game's headless playSound queue via *ForTest() accessors -- no window/audio device.

TEST_CASE("dealDurationScale keys card-deal tempo to game speed", "[GameSpeed][Animation]")
{
    // Slower speeds stretch for visibility; faster speeds compress so cards land with the action/sound rather than trailing.
    CHECK(GameTiming::dealDurationScale(GameSpeed::Slow) == Approx(2.0f));
    CHECK(GameTiming::dealDurationScale(GameSpeed::Normal) == Approx(1.0f));
    CHECK(GameTiming::dealDurationScale(GameSpeed::Fast) == Approx(0.4f));
    CHECK(GameTiming::dealDurationScale(GameSpeed::AutoPlay) == Approx(0.3f));

    // Monotonic: every step down in speed is a step up (or equal) in animation-duration factor, so the whole deal sweep
    // speeds up / slows down as one unit with the action cadence.
    CHECK(GameTiming::dealDurationScale(GameSpeed::Slow) > GameTiming::dealDurationScale(GameSpeed::Normal));
    CHECK(GameTiming::dealDurationScale(GameSpeed::Normal) > GameTiming::dealDurationScale(GameSpeed::Fast));
    CHECK(GameTiming::dealDurationScale(GameSpeed::Fast) > GameTiming::dealDurationScale(GameSpeed::AutoPlay));

    // Hyper and Gamer deliver no animation: the factor collapses to 0, so every CardDealTween settles instantly on its
    // resting slot (progress()==1 when duration==0), i.e. cards snap into place with no visible sweep.
    CHECK(GameTiming::dealDurationScale(GameSpeed::Hyper) == Approx(0.0f));
    CHECK(GameTiming::dealDurationScale(GameSpeed::Gamer) == Approx(0.0f));
    CHECK(GameTiming::dealDurationScale(GameSpeed::Hyper) == GameTiming::dealDurationScale(GameSpeed::Gamer));
}

TEST_CASE("animationDisabled is true only for Hyper and Gamer", "[GameSpeed][Animation]")
{
    // The two pure-speed tiers skip animation (and sound effects); the watchable/manual tiers keep it.
    CHECK(GameTiming::animationDisabled(GameSpeed::Hyper));
    CHECK(GameTiming::animationDisabled(GameSpeed::Gamer));
    CHECK_FALSE(GameTiming::animationDisabled(GameSpeed::Slow));
    CHECK_FALSE(GameTiming::animationDisabled(GameSpeed::Normal));
    CHECK_FALSE(GameTiming::animationDisabled(GameSpeed::Fast));
    CHECK_FALSE(GameTiming::animationDisabled(GameSpeed::AutoPlay));
}

// Headless proof that Hyper/Gamer suppress the bet/card sound effects while every other speed still plays them. The game
// logic/tick path is untouched by speed, so startTestRound deals normally; only the effect queue differs per speed. This
// mirrors the deterministic shoe + assertions in tests/test_sound_manager.cpp so the positive control is identical there.
TEST_CASE("Hyper and Gamer suppress card/bet sound effects; other speeds keep them", "[GameSpeed][Sound][Animation]")
{
    const std::vector<Card> shoe = {
        Card{2, Suit::Spades}, Card{3, Suit::Hearts}, Card{4, Suit::Diamonds}, Card{5, Suit::Clubs},
        Card{6, Suit::Spades}, Card{7, Suit::Hearts}, Card{8, Suit::Diamonds}, Card{9, Suit::Clubs},
        Card{10, Suit::Spades}, Card{2, Suit::Hearts}, Card{3, Suit::Diamonds}, Card{4, Suit::Clubs}
    };

    for (const auto speed : {GameSpeed::Hyper, GameSpeed::Gamer})
    {
        Game game(true); // headless: playSound() only enqueues when buffers are absent
        test_support::seed_deck(game,shoe);
        game.setGameSpeedForTest(speed);

        game.startTestRound(); // deals 4 cards -> normally 'card' effects, suppressed in Hyper/Gamer
        CHECK(game.soundQueueForTest().empty());

        game.confirmBet();     // normally one 'bet' effect
        const auto q = game.soundQueueForTest();
        CHECK(std::count(q.begin(), q.end(), "card") == 0);
        CHECK(std::count(q.begin(), q.end(), "bet") == 0);
    }

    // Positive control: Normal still plays the effects (the watchable tiers are unchanged by this rewrite).
    {
        Game game(true);
        test_support::seed_deck(game,shoe);
        game.setGameSpeedForTest(GameSpeed::Normal);

        int cardsBefore = game.soundTotalPlayedForTest();
        game.startTestRound();
        CHECK(game.soundTotalPlayedForTest() > cardsBefore);
        const auto q0 = game.soundQueueForTest();
        CHECK(std::count(q0.begin(), q0.end(), "card") >= 1);

        const int betsBefore = static_cast<int>(std::count(q0.begin(), q0.end(), "bet"));
        game.confirmBet();
        const auto q1 = game.soundQueueForTest();
        CHECK(std::count(q1.begin(), q1.end(), "bet") == betsBefore + 1);

        if (game.roundStateForTest() == RoundState::PlayerTurn && !game.testHand(0, 0).isFinished())
        {
            const int beforeHit = game.soundTotalPlayedForTest();
            game.hit(); // a drawn card -> exactly one 'card' effect
            CHECK(game.soundTotalPlayedForTest() == beforeHit + 1);
        }
    }
}
