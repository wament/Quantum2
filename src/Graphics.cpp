#include "Graphics.hpp"

#include <algorithm>  // std::clamp for the easing curves below (folded in from CardAnimation)

#include "Constants.hpp"

bool Graphics::loadCardTexture(
    sf::Texture &texture,
    const std::string &filename)
{
    return texture.loadFromFile(filename);
}

sf::Sprite Graphics::makeCardSprite(
    const Card &card,
    const sf::Texture &texture)
{
    sf::Sprite sprite(texture);

    int x =
        (card.value - 1) *
        Constants::CARD_WIDTH;

    int y =
        static_cast<int>(card.suit) *
        Constants::CARD_HEIGHT;

    sprite.setTextureRect(
        sf::IntRect(
            {x + Constants::CARD_X_PADDING, y + Constants::CARD_Y_PADDING},
            {Constants::CARD_WIDTH - Constants::CARD_X_PADDING,
             Constants::CARD_HEIGHT - Constants::CARD_Y_PADDING}));

    return sprite;
}

sf::Sprite Graphics::makeCardBackSprite(
    const sf::Texture &texture)
{
    Card back{
        Constants::CUT_CARD_VALUE,
        Suit::Hearts};

    return makeCardSprite(back, texture);
}

void Graphics::drawCard(sf::RenderWindow &window,
                        const sf::Texture &texture,
                        const Card &card,
                        float x,
                        float y)
{
    auto sprite = makeCardSprite(card, texture);

    sprite.setPosition(sf::Vector2f(x, y));

    window.draw(sprite);
}

// --- Card-deal animation (folded in from the former standalone CardAnimation module). Pure easing math + the single-card
// arrival tween used by every card-drawing site so arriving cards slide/flip into their resting slot instead of popping.
// Presentation-only; no game state held here -- each draw site owns its own reveal clock, exactly mirroring GameRenderer's
// Feature-8 deal animation.

float Graphics::easeOutCubic(float t)
{
    const float c = std::clamp(t, 0.f, 1.f); // tolerant of any input so callers need not clamp themselves
    return 1.f - (1.f - c) * (1.f - c) * (1.f - c);
}

float Graphics::easeInCubic(float t)
{
    const float c = std::clamp(t, 0.f, 1.f);
    return c * c * c;
}

float Graphics::CardDealTween::progress() const
{
    // Clamp the normalized time so a large elapsed (e.g. after a long pause or a skipped frame) never overshoots 1.
    const float t = duration > 0.f ? elapsed / duration : 1.f;
    return easeOutCubic(t);
}

sf::Vector2f Graphics::CardDealTween::position() const
{
    const float e = progress(); // eased fraction already clamped to [0,1]
    return sf::Vector2f(start.x + (end.x - start.x) * e, start.y + (end.y - start.y) * e);
}

float Graphics::CardDealTween::flipProgress() const
{
    if (!flipping)
        return 0.f; // face-down / edge-on while the flip has not started
    const float t = flipDuration > 0.f ? flipElapsed / flipDuration : 1.f;
    return easeInCubic(t);
}