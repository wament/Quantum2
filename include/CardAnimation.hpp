#pragma once

#include <SFML/Graphics.hpp> // sf::Vector2f is Graphics-only, so this module runs over the Graphics-only test binary.

// Feature 8 (Animated Card Dealing): presentation-only tween helpers used by GameRenderer to animate cards sliding in
// from the shoe into their resting slot and flipping the dealer hole card face-up. This holds NO game state -- it only
// maps an elapsed time to a eased position/angle, so it is pure logic, SFML-Graphics-only (no window/audio/network), and
// therefore unit-testable headlessly over the Graphics-only Blackjack2Tests binary. Kept in its own module (matching the
// SoundManager / StatsDashboard / Tournament deliverable shape) rather than buried in GameRenderer's anonymous namespace
// so the math is directly assertable by tests (see tests/test_card_animation.cpp).

// Standard ease-out cubic: fast start, decelerating to rest. Gives arriving cards weight so they "land" instead of
// moving at a constant rate. Input t is any float; it is clamped to [0,1] before the curve is applied.
float easeOutCubic(float t);

// Ease-in cubic: accelerates from rest. Used for the dealer hole-card flip as it rotates into its flat, face-up pose.
float easeInCubic(float t);

// A single card's arrival animation: eases position along a straight line from `start` (the shoe) to `end` (its slot)
// over `duration` seconds using an ease-out curve, and -- when flipping is requested -- drives a separate flip angle via
// an ease-in envelope. Both envelopes are independent so the slide and the card turn do not have to share a timeline.
struct CardDealTween {
    sf::Vector2f start;                 // screen position where the card begins (the shoe)
    sf::Vector2f end;                   // resting screen position (its slot on the felt); when complete, position()==end exactly
    float duration = 0.32f;             // seconds over which the card slides from start to end
    float elapsed = 0.f;                // seconds spent animating so far (driven by GameRenderer from game.deltaTime)

    bool flipping = false;              // when true, flipProgress() eases a turn-from-edge -> flat rotation for the hole card
    float flipDuration = 0.4f;          // seconds over which the flip completes once flipping is enabled
    float flipElapsed = 0.f;            // seconds spent on the flip so far

    // Eased slide progress in [0,1]. Monotonic and clamped: 0 at elapsed==0, exactly 1 once elapsed >= duration.
    [[nodiscard]] float progress() const;

    // Current eased position. Always starts exactly at `start` and settles exactly on `end` (so a fully-arrived card
    // sits pixel-for-pixel where the pre-animation layout puts it -- settled frames are unchanged).
    [[nodiscard]] sf::Vector2f position() const;

    // Eased flip amount in [0,1]: 0 while not flipping, otherwise an ease-in curve that reaches exactly 1 once the flip
    // duration elapses. Multiply by 90 degrees to get a rotation edge->flat for the hole card.
    [[nodiscard]] float flipProgress() const;
};
