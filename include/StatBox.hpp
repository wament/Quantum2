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

    // World-space bounds of the title text, used for click hit-testing (e.g. toggling the counting style).
    sf::FloatRect getTitleBounds() const;

    // Union of the title+value bounds so a click anywhere on a whole clickable box (e.g. the counting-style
    // toggle) registers, not just on its label line. In world space, like getTitleBounds().
    sf::FloatRect getTitleValueBounds() const;

    void setValue(const std::string &value);

    // Recolor the value text (e.g. yellow true count when Reko is active). Non-const; matches draw().
    void setValueColor(sf::Color color);

    void setPosition(sf::Vector2f position);

    void draw(sf::RenderWindow &window);

private:
    sf::Text *m_title = nullptr;
    sf::Text *m_value = nullptr;

    std::string m_titleText;
};