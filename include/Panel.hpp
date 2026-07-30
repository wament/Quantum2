#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "StatBox.hpp"

class Panel
{
public:

    void initialize(sf::Font& font);

    void setBounds(
        sf::Vector2f position,
        sf::Vector2f size);

    void draw(sf::RenderWindow& window);

    void addStat(const StatBox& stat);

private:

    sf::RectangleShape m_background;

    std::vector<StatBox> m_stats;
};