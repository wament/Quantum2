#pragma once

#include <SFML/Graphics.hpp>
#include <string>

class StatBox
{
public:
    StatBox();
    ~StatBox();

    void initialize(sf::Font &font, const std::string &title);

    void setTitle(const std::string &title);

    std::string getTitle() const;
    sf::FloatRect getTitleBounds() const;

    void setValue(const std::string &value);

    // Recolor the value text (e.g. yellow true count when Reko is active). Non-const; matches draw().
    void setValueColor(sf::Color color);

    // Full vertical extent of the box (title line + value line). Position-independent
    // center/distribute boxes inside a panel before placing them.
    float blockHeight() const;

    // The box's placement anchor (top-left of its title line). Layout code positions boxes
    // this rather than the rendered bounds
    sf::Vector2f getPosition() const;

    void setPosition(sf::Vector2f position);

    void draw(sf::RenderWindow &window);

private:
    sf::Text *m_title = nullptr;
    sf::Text *m_value = nullptr;

    std::string m_titleText;

    // Vertical pitch between the title line and the value line, derived from the font size in initialize()
    // box stays proportional when the HUD font is scaled.
    float m_lineGap = 18.f;
};