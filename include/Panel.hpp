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

    // Read-only view of the boxes owned by this panel, so layout (and layout tests) can measure real box bounds
    // instead of re-deriving them. The Panel keeps ownership; callers must not delete.
    const std::vector<StatBox*>& statBoxes() const;

    StatBox* addStatBox(const std::string& title);

    void draw(sf::RenderWindow& window);

private:

    sf::RectangleShape m_background;

    std::vector<StatBox*> m_stats;
};