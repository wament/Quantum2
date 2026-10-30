#include "CardAnimation.hpp"

#include <algorithm>

float easeOutCubic(float t)
{
    const float c = std::clamp(t, 0.f, 1.f); // tolerant of any input so callers need not clamp themselves
    return 1.f - (1.f - c) * (1.f - c) * (1.f - c);
}

float easeInCubic(float t)
{
    const float c = std::clamp(t, 0.f, 1.f);
    return c * c * c;
}

float CardDealTween::progress() const
{
    // Clamp the normalized time so a large elapsed (e.g. after a long pause or a skipped frame) never overshoots 1.
    const float t = duration > 0.f ? elapsed / duration : 1.f;
    return easeOutCubic(t);
}

sf::Vector2f CardDealTween::position() const
{
    const float e = progress(); // eased fraction already clamped to [0,1]
    return sf::Vector2f(start.x + (end.x - start.x) * e, start.y + (end.y - start.y) * e);
}

float CardDealTween::flipProgress() const
{
    if (!flipping)
        return 0.f; // face-down / edge-on while the flip has not started
    const float t = flipDuration > 0.f ? flipElapsed / flipDuration : 1.f;
    return easeInCubic(t);
}
