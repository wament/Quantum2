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

    // World-space bounds of the panel background (position + size). Lets callers (HUD) lay out content relative
    // to the real panel size instead of hard-coding geometry that breaks if the panel dimensions ever change.
    sf::Rect<float> bounds() const;

    StatBox* addStatBox(const std::string& title);

    void draw(sf::RenderWindow& window);

private:

    sf::RectangleShape m_background;

    std::vector<StatBox*> m_stats;
};