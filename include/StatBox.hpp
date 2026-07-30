#pragma once

#include <SFML/Graphics.hpp>
#include <string>

class StatBox
{
public:

    StatBox();

    void initialize(
        sf::Font& font,
        const std::string& title);

    void setValue(
        const std::string& value);

    void setPosition(
        sf::Vector2f position);

    void draw(
        sf::RenderWindow& window);

private:

    sf::Text m_title;

    sf::Text m_value;
};