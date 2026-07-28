#include "Graphics.hpp"

#include "Constants.hpp"

bool Graphics::loadCardTexture(
    sf::Texture& texture,
    const std::string& filename)
{
    return texture.loadFromFile(filename);
}

sf::Sprite Graphics::makeCardSprite(
    const Card& card,
    const sf::Texture& texture)
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
            x + Constants::CARD_X_PADDING,
            y + Constants::CARD_Y_PADDING,
            Constants::CARD_WIDTH -
                Constants::CARD_X_PADDING,
            Constants::CARD_HEIGHT -
                Constants::CARD_Y_PADDING));

    return sprite;
}

sf::Sprite Graphics::makeCardBackSprite(
    const sf::Texture& texture)
{
    Card back{
        Constants::CUT_CARD_VALUE,
        Suit::Hearts
    };

    return makeCardSprite(back, texture);
}

void Graphics::drawCard(sf::RenderWindow& window,
        const sf::Texture& texture,
        const Card& card,
        float x,
        float y)
{
    auto sprite = makeCardSprite(card, texture);

    sprite.setPosition(x,y);

    window.draw(sprite);
}