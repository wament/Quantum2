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
}