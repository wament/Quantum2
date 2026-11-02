#pragma once

#include <SFML/Graphics.hpp>

#include "Card.hpp"

namespace Graphics
{
    bool loadCardTexture(sf::Texture& texture,
                         const std::string& filename);

    sf::Sprite makeCardSprite(
        const Card& card,
        const sf::Texture& texture);

    sf::Sprite makeCardBackSprite(
        const sf::Texture& texture);

    void drawCard(
        sf::RenderWindow& window,
        const sf::Texture& texture,
        const Card& card,
        float x,
        float y);

    // --- Card-deal animation (folded in from the former standalone CardAnimation module). Presentation-only tween math:
    // an elapsed time maps to an eased fraction
    // game state is held here -- every draw site owns its own reveal clock, exactly mirroring GameRenderer's Feature-8 deal animation.

    // Ease-out cubic: fast start decelerating to rest, giving arriving cards weight
    // any float; it is clamped to [0,1] before the curve is applied. Pure math, headless-testable over the Graphics-only binary.
    float easeOutCubic(float t);

    // Ease-in cubic: accelerates from rest, used for the dealer hole-card flip rotating edge-on -> flat. Same clamp rule.
    float easeInCubic(float t);

    // A single card's arrival animation: eases position along a straight line from `start` (the shoe) to `end`
    // (its resting slot) over `duration` seconds with an ease-out curve, and
    // flip angle via an ease-in envelope. Independent slide + flip timelines
    // (elapsed >= duration) return position()==end exactly
    struct CardDealTween {
        sf::Vector2f start;                 // screen position where the card begins (the shoe)
        sf::Vector2f end;                   // resting screen position (its slot); when complete, position()==end exactly
        float duration = 0.32f;             // seconds over which the card slides from start to end
        float elapsed = 0.f;                // seconds spent animating so far (driven per frame by the drawing site)

        bool flipping = false;              // when true, flipProgress() eases a turn-from-edge -> flat rotation for the hole card
        float flipDuration = 0.4f;          // seconds over which the flip completes once flipping is enabled
        float flipElapsed = 0.f;            // seconds spent on the flip so far

        [[nodiscard]] float progress() const;
        [[nodiscard]] sf::Vector2f position() const;
        [[nodiscard]] float flipProgress() const;
    };
}