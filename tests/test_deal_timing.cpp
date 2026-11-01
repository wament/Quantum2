// Deal-timing / game-speed handling: Gamer refresh-rate derivation, Feature-8 dealing-animation pacing, card-animation reveal clocks.

#include <catch2/catch_all.hpp>
#include "test_support.hpp"
#include "Card.hpp"
#include "Graphics.hpp"
#include "Game.hpp"
#include "GameRenderer.hpp"
#include "Hand.hpp"
#include "Player.hpp"
#include "test_support.hpp"
#include <Constants.hpp>
#include <Game.hpp>
#include <algorithm>  // std::max for per-card animation elapsed time across the reveal-clock tests
#include <catch2/catch_all.hpp>

using Catch::Approx;

TEST_CASE("Gamer speed derives its per-step interval from the monitor refresh rate", "[GameSpeed][Constants]")
{
    // One step advances per frame, so seconds-per-step = 1/refreshRate: a higher-refresh monitor plays strictly faster.
    CHECK(GameTiming::gamerStepInterval(60.0f) == Approx(1.0f / 60.0f));
    CHECK(GameTiming::gamerStepInterval(144.0f) == Approx(1.0f / 144.0f));
    CHECK(GameTiming::gamerStepInterval(240.0f) == Approx(1.0f / 240.0f));

    // An undetected or absurdly low rate never yields a negative or exploding interval; it falls back to the default.
    CHECK(GameTiming::gamerStepInterval(0.0f) == GameTiming::DEFAULT_REFRESH_HZ);
    CHECK(GameTiming::gamerStepInterval(1.0f) == Approx(GameTiming::DEFAULT_REFRESH_HZ));

    // At a high refresh rate Gamer is strictly faster per step than Hyper's fixed tiny delay, honoring the goal of
    // pushing playback as fast as the hardware allows.
    CHECK(GameTiming::gamerStepInterval(240.0f) < GameTiming::npcActionDelay(GameSpeed::Hyper));
}

// The injected refresh rate flows through to what Gamer timing reads, so pure-logic paths can be driven headlessly
// (the full-mode constructor instead detects the real monitor rate from hardware at startup).
TEST_CASE("Gamer speed reads its injectable monitor refresh rate off Game", "[GameSpeed]")
{
    Game game(true); // headless: no SFML window, no audio device

    game.setRefreshRateForTest(144.0f);
    CHECK(game.refreshRateForTest() == Approx(144.0f));

    game.setGameSpeedForTest(GameSpeed::Gamer);
    CHECK(static_cast<GameSpeed>(game.gameSpeedForTest()) == GameSpeed::Gamer);
}

TEST_CASE("isAutoPlaying treats AutoPlay, Hyper, and refresh-rate Gamer as continuous autoplay", "[GameSpeed]")
{
    // The three automated tiers (AutoPlay, Hyper, refresh-rate Gamer) advance every seat on basic strategy without
    // waiting for input...
    CHECK(GameTiming::isAutoPlaying(GameSpeed::AutoPlay));
    CHECK(GameTiming::isAutoPlaying(GameSpeed::Gamer));
    CHECK(GameTiming::isAutoPlaying(GameSpeed::Hyper));

    // ...while the manual tiers (Fast/Normal/Slow) still let the player drive their own cards.
    CHECK(!GameTiming::isAutoPlaying(GameSpeed::Fast));
    CHECK(!GameTiming::isAutoPlaying(GameSpeed::Normal));
    CHECK(!GameTiming::isAutoPlaying(GameSpeed::Slow));
}

TEST_CASE("dealerRevealInterval matches player pace in Gamer/Hyper, fixed beat otherwise", "[GameSpeed]")
{
    // In the automated-fast tiers the dealer reveals cards at the SAME interval players act...
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Gamer, 240.0f) == Approx(GameTiming::gamerStepInterval(240.0f)));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Hyper, 0.0f) == Approx(GameTiming::npcActionDelay(GameSpeed::Hyper)));

    // ...while every other speed keeps the fixed, watchable beat so a casual player can follow each card reveal.
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Fast, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Normal, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::Slow, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
    CHECK(GameTiming::dealerRevealInterval(GameSpeed::AutoPlay, 0.0f) == Approx(GameTiming::DEALER_ACTION_DELAY));
}


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

    // Hyper and Gamer deliver no animation: the factor collapses to 0, so every Graphics::CardDealTween settles instantly on its
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


TEST_CASE("Graphics::easeOutCubic has correct endpoints and clamps out-of-range input", "[card-animation]")
{
    REQUIRE(Graphics::easeOutCubic(0.0f) == Approx(0.0f)); // starts at rest
    REQUIRE(Graphics::easeOutCubic(1.0f) == Approx(1.0f)); // ends exactly settled
    REQUIRE(Graphics::easeOutCubic(-5.0f) == Approx(0.0f)); // clamps below 0
    REQUIRE(Graphics::easeOutCubic(99.0f) == Approx(1.0f)); // clamps above 1
}

TEST_CASE("Graphics::easeOutCubic is monotonic increasing and hits its known mid value", "[card-animation]")
{
    float prev = -1.0f;
    for (int s = 0; s <= 20; ++s)
    {
        const float t = static_cast<float>(s) / 20.0f;
        const float v = Graphics::easeOutCubic(t);
        REQUIRE(v >= prev - 1e-6f); // never goes backwards: smooth, decelerating sweep
        prev = v;
    }
    // Standard cubic-ease-out at the midpoint: 1 - (0.5)^3 == 0.875.
    REQUIRE(Graphics::easeOutCubic(0.5f) == Approx(0.875f));
}

TEST_CASE("Graphics::easeInCubic starts at rest, ends flat and is monotonic", "[card-animation]")
{
    REQUIRE(Graphics::easeInCubic(0.0f) == Approx(0.0f));
    REQUIRE(Graphics::easeInCubic(1.0f) == Approx(1.0f));
    float prev = -1.0f;
    for (int s = 0; s <= 20; ++s)
    {
        const float v = Graphics::easeInCubic(static_cast<float>(s) / 20.0f);
        REQUIRE(v >= prev - 1e-6f);
        prev = v;
    }
    // Cubic ease-in at the midpoint: (0.5)^3 == 0.125.
    REQUIRE(Graphics::easeInCubic(0.5f) == Approx(0.125f));
}

TEST_CASE("Graphics::CardDealTween progress ramps from 0 to exactly 1 and clamps", "[card-animation]")
{
    Graphics::CardDealTween tween;
    tween.duration = 1.0f;

    REQUIRE(tween.progress() == Approx(0.0f));   // nothing elapsed -> fully at the start
    tween.elapsed = 0.5f;
    REQUIRE(tween.progress() == Approx(0.875f)); // eased midpoint
    tween.elapsed = 1.0f;                        // exactly duration settled
    REQUIRE(tween.progress() == Approx(1.0f));
    tween.elapsed = 5.0f;                        // overshoot must not exceed 1 (settled stays put)
    REQUIRE(tween.progress() == Approx(1.0f));

    // A zero-duration tween is already settled regardless of elapsed.
    Graphics::CardDealTween instant;
    instant.duration = 0.f;
    instant.elapsed = 100.f;
    REQUIRE(instant.progress() == Approx(1.0f));
}

TEST_CASE("Graphics::CardDealTween position starts on `start` and settles exactly on `end`", "[card-animation]")
{
    Graphics::CardDealTween tween;
    tween.start = sf::Vector2f(0.f, 0.f);
    tween.end = sf::Vector2f(100.f, 50.f);
    tween.duration = 1.0f;

    tween.elapsed = 0.f;
    REQUIRE(tween.position().x == Approx(0.f));   // fully at the shoe on frame one
    REQUIRE(tween.position().y == Approx(0.f));

    tween.elapsed = 1.0f;
    REQUIRE(tween.position().x == Approx(100.f));   // settles exactly on-slot...
    REQUIRE(tween.position().y == Approx(50.f));    // ...so settled frames match the pre-animation layout pixel-for-pixel

    // Eased midpoint: start + (end - start) * 0.875.
    tween.elapsed = 0.5f;
    REQUIRE(tween.position().x == Approx(87.5f));
    REQUIRE(tween.position().y == Approx(43.75f));
}

TEST_CASE("Graphics::CardDealTween flip envelope is inert until flipping and then eases to 1", "[card-animation]")
{
    Graphics::CardDealTween tween;
    tween.flipping = false;
    REQUIRE(tween.flipProgress() == Approx(0.0f)); // face-down / edge-on while not flipping

    tween.flipping = true;
    tween.flipDuration = 0.4f;
    tween.flipElapsed = 0.f;
    REQUIRE(tween.flipProgress() == Approx(0.0f)); // flip starts from rest

    tween.flipElapsed = 0.2f;                       // halfway -> ease-in cubic(0.5) == 0.125
    REQUIRE(tween.flipProgress() == Approx(0.125f));

    tween.flipElapsed = 1.0f;                        // overshoot clamps to fully flat
    REQUIRE(tween.flipProgress() == Approx(1.0f));
}

TEST_CASE("reveal staggering orders cards in deal order with no instant-pop", "[card-animation]")
{
    Game g(true);
    // Two seats, each dealt two cards -> player card ordinals 0..3 (seat 0 then seat 1 / NPC), dealer ordinals 4..5.
    g.playersForTest().clear();
    g.playersForTest().emplace_back(Player("William"));
    g.playersForTest().emplace_back(Player("Alice"));
    // Give each seat a single empty hand to deal into (mirrors how the game prepares seats before dealing).
    for (int s = 0; s < 2; ++s)
        g.testPlayer(s).resetRound(0);
    for (int s = 0; s < 2; ++s)
        for (int c = 0; c < 2; ++c)
            g.testPlayer(s).hands[0].addCard(Card{2 + s * 2 + c, Suit::Spades});

    GameRenderer r;                 // fresh -> dealClock_ == 0, so the reveal "now" this test observes is exactly 0.
    const float now = 0.f;

    // revealAt must grow with deal-order ordinal: earlier-dealt cards begin sliding before later ones, and every reveal
    // time is >= now (in the future or at `now`) so each card starts at progress ~0 instead of already past
    // DEAL_SLIDE_SECONDS. Under the old minus sign, ordinals 1..5 were < 0 -- instantly past their slide duration -- so
    // later seats / NPCs / dealer popped into place with no animation; that ordering + no-past property now holds.
    const float s0c0 = r.playerRevealAtForForTest(g, 0, 0, 0);   // ordinal 0
    const float s0c1 = r.playerRevealAtForForTest(g, 0, 0, 1);   // ordinal 1
    const float npc0 = r.playerRevealAtForForTest(g, 1, 0, 0);   // ordinal 2 (first NPC card)
    const float npc1 = r.playerRevealAtForForTest(g, 1, 0, 1);   // ordinal 3
    const float d0 = r.dealerRevealAtForForTest(g, 0);           // ordinal 4
    const float d1 = r.dealerRevealAtForForTest(g, 1);           // ordinal 5

    REQUIRE(s0c0 < s0c1);
    REQUIRE(s0c1 < npc0);   // player seat before NPC seat
    REQUIRE(npc0 < npc1);
    REQUIRE(npc1 < d0);     // every player card (incl. NPCs) before the dealer
    REQUIRE(d0 < d1);
    REQUIRE(s0c0 >= now);   // earliest card starts at/after `now` -- never already in the past (no instant pop)
}

// --- Feature 8 cross-round reveal-clock reset regression ([card-animation][regression]) -----------------------------
// advanceRevealClock() wipes stale reveal state only when on-table card count drops (a new round begins with empty
// hands) and then advances the shared deal clock by game.deltaTime. prevTotalCards_ must be RE-BASELINED to the current
// low count on that drop. If it instead RETAINED the previous round's peak, every later round whose card count stayed
// below that retained peak would re-trigger `total < prevTotalCards_` on EVERY frame -- clearing and re-seeding the
// reveals each frame, which froze a card's progress near 0 so it never finished sliding into its slot (the reported
// "cards work on round 1 but get stuck / wrong spot afterwards; split-heavy rounds only animate once enough cards pile
// up past the stale peak; sparse/NPC rounds never animate" bug). These cases drive advanceRevealClock over a
// high-card round followed by shorter follow-up rounds and assert the cached reveal time stays stable across frames
// (so elapsed grows past DEAL_SLIDE_SECONDS and the card actually settles) for both the player and dealer paths.

namespace {
// Populate every seat's hands with exactly `totalCards` cards so the on-table card count is deterministic for driving
// advanceRevealClock(). Card values are irrelevant to animation -- reveal timing depends only on hand/card sizes.
void fillPlayerHands(Game& g, int totalCards)
{
    for (auto& p : g.playersForTest())
        p.resetRound(Constants::MINIMUM_BET);      // one empty hand each, like nextRound()
    std::size_t dealt = 0;
    for (auto& p : g.playersForTest())
        while (dealt < static_cast<std::size_t>(totalCards))
            p.hands.back().addCard(Card{2 + static_cast<int>(dealt++), Suit::Spades});
}

// Mirror nextRound()'s boundary: clear every seat's hands and the dealer so on-table card count drops to ~0.
void beginNewRound(Game& g)
{
    for (auto& p : g.playersForTest())
        p.resetRound(Constants::MINIMUM_BET);
}
} // namespace

TEST_CASE("shorter follow-up round below prior peak still lets every card finish its slide (regression)", "[card-animation][regression]")
{
    Game g(true);
    // Exactly one seat so total-card bookkeeping is deterministic (mirrors startTestRound()'s setup).
    g.playersForTest().clear();
    g.playersForTest().emplace_back(Player("William"));
    GameRenderer r;
    g.deltaTime = 0.016f; // ~60fps frame delta

    // Round 1 peak: 6 player cards -> prevTotalCards_ tracks up to 6 (no reset yet, total only grows).
    fillPlayerHands(g, 6);
    r.advanceRevealClockForTest(g);

    // Round boundary: every hand + the dealer is cleared (total drops toward 0) -- exactly what nextRound() does. One
    // clear happens here; the baseline is re-set to the current low count by the fixed advanceRevealClock().
    beginNewRound(g);
    r.advanceRevealClockForTest(g);

    // Round 2 peak (3 cards) stays BELOW round 1's retained peak (6). This is the reported failure mode: with the old
    // running-max bookkeeping, every frame of this round re-triggered `total < prevTotalCards_`, clearing and re-seeding
    // the reveals so progress froze near 0. Across ~6.4s of frames a correctly-cached reveal time must stay constant AND
    // each card's elapsed time (dealClock - revealAt) must grow past DEAL_SLIDE_SECONDS so it actually settles.
    fillPlayerHands(g, 3);
    const float slotReveal = r.playerRevealAtForForTest(g, 0, 0, 0);   // first sighting in round 2 -> fixed for the round

    float maxElapsed = 0.f;
    for (int frame = 0; frame < 400; ++frame)     // 400 * 0.016s == 6.4s >> DEAL_SLIDE_SECONDS (0.32s)
    {
        r.advanceRevealClockForTest(g);
        const float value = r.playerRevealAtForForTest(g, 0, 0, 0);
        REQUIRE(value == slotReveal);            // not re-seeded every frame -> progress can grow
        maxElapsed = std::max(maxElapsed, r.dealClockForTest() - slotReveal);
    }
    REQUIRE(maxElapsed >= Constants::DEAL_SLIDE_SECONDS);   // card finished sliding into its slot (not stuck)

    // The dealer reveal path (cards dealt after players -- which is how seats/NPCs are handled) behaves identically.
    const float dealerReveal = r.dealerRevealAtForForTest(g, 0);
    float dealerMaxElapsed = 0.f;
    for (int frame = 0; frame < 400; ++frame)
    {
        r.advanceRevealClockForTest(g);
        REQUIRE(r.dealerRevealAtForForTest(g, 0) == dealerReveal);
        dealerMaxElapsed = std::max(dealerMaxElapsed, r.dealClockForTest() - dealerReveal);
    }
    REQUIRE(dealerMaxElapsed >= Constants::DEAL_SLIDE_SECONDS);
}

TEST_CASE("every successive sparse round re-stabilizes and settles after a big split round (regression)", "[card-animation][regression]")
{
    Game g(true);
    g.playersForTest().clear();
    g.playersForTest().emplace_back(Player("William"));
    GameRenderer r;
    g.deltaTime = 0.016f;

    // Round 1 is a big split round: one hand of many cards -> prevTotalCards_ climbs high and stays the retained peak.
    fillPlayerHands(g, 8);
    for (int frame = 0; frame < 400; ++frame)
        r.advanceRevealClockForTest(g);

    // Several following rounds each stay well below that peak (sparse / NPC-heavy), exactly like the reported session.
    // Every one of them must clear once at its boundary, then settle each card -- never get stuck re-clearing every frame.
    for (int round = 0; round < 20; ++round)
    {
        beginNewRound(g);
        r.advanceRevealClockForTest(g);          // the single boundary clear

        fillPlayerHands(g, 3 + (round % 2));     // 3 or 4 cards -- always below round 1's peak of 8
        const float slotReveal = r.playerRevealAtForForTest(g, 0, 0, 0);

        float maxElapsed = 0.f;
        for (int frame = 0; frame < 400; ++frame)
        {
            r.advanceRevealClockForTest(g);
            REQUIRE(r.playerRevealAtForForTest(g, 0, 0, 0) == slotReveal);
            maxElapsed = std::max(maxElapsed, r.dealClockForTest() - slotReveal);
        }
        REQUIRE(maxElapsed >= Constants::DEAL_SLIDE_SECONDS);   // this round's cards all settled
    }
}

// Permanent regression: after the reveal-clock fixes, every seat -- including NPC seats and the dealer -- must sweep its
// deal in from the shoe rather than popping into place, then settle over a full round. Drives the same reveal-clock path
// drawPlayers uses (advanceRevealClock + playerRevealAtFor) headlessly through a real 3-seat deal; no window needed.
TEST_CASE("every seat including NPCs sweeps its deal from progress 0 and settles", "[card-animation][regression]")
{
    Game g(true);
    g.startTestRound(3);                 // human + 2 NPCs, all dealt their initial two cards at PlayerTurn start
    GameRenderer r;
    g.deltaTime = 0.016f;

    // (a) No instant-pop: on its first sighting every card's elapsed is <= ~0, i.e. revealAt sits at/in the future so the
    //     card starts at progress 0 and slides in -- including NPC seats and the dealer (the old minus sign put them in
    //     the past => instantly past DEAL_SLIDE_SECONDS => popped with no animation). This is what the user reported missing.
    for (int p = 0; p < static_cast<int>(g.playersForTest().size()); ++p)
        for (size_t h = 0; h < g.testPlayer(p).hands.size(); ++h)
            for (size_t i = 0; i < g.testPlayer(p).hands[h].size(); ++i)
                REQUIRE(r.dealClockForTest() - r.playerRevealAtForForTest(g, p, static_cast<int>(h), static_cast<int>(i)) <= 1e-4f);

    for (size_t i = 0; i < g.testDealer().size(); ++i)
        REQUIRE(r.dealClockForTest() - r.dealerRevealAtForForTest(g, static_cast<int>(i)) <= 1e-4f);

    // (b) The reveal clock stays healthy a whole round: an NPC seat's cached reveal time is stable across frames and its
    //     elapsed grows past DEAL_SLIDE_SECONDS so the card actually finishes sliding into its slot (not stuck, not instant).
    const int npcSeat = 1;
    const float npcReveal = r.playerRevealAtForForTest(g, npcSeat, 0, 0);
    float maxElapsed = 0.f;
    for (int frame = 0; frame < 400; ++frame)     // ~6.4s of frames >> DEAL_SLIDE_SECONDS
    {
        r.advanceRevealClockForTest(g);
        REQUIRE(r.playerRevealAtForForTest(g, npcSeat, 0, 0) == npcReveal);
        maxElapsed = std::max(maxElapsed, r.dealClockForTest() - npcReveal);
    }
    REQUIRE(maxElapsed >= Constants::DEAL_SLIDE_SECONDS);   // NPC card finished sliding into its slot
}
