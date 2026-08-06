#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "StatBox.hpp"

class Panel
{
public:

    Panel();

    void initialize(sf::Font& font);

    void setBounds(
        sf::Vector2f position,
        sf::Vector2f size);

    StatBox* addStatBox(const std::string& title);

    void draw(sf::RenderWindow& window);

private:

    sf::RectangleShape m_background;

    std::vector<StatBox*> m_stats;
};