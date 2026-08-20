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

    void setValue(const std::string &value);

    void setPosition(sf::Vector2f position);

    void draw(sf::RenderWindow &window);

private:
    sf::Text *m_title = nullptr;
    sf::Text *m_value = nullptr;

    std::string m_titleText;
};