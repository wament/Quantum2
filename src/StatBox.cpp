#include "StatBox.hpp"

StatBox::StatBox()
{

}

void StatBox::initialize(
    sf::Font& font,
    const std::string& title)
{
    m_title.setFont(font);
    m_title.setCharacterSize(18);
    m_title.setString(title);

    m_value.setFont(font);
    m_value.setCharacterSize(24);
}

void StatBox::setValue(
    const std::string& value)
{
    m_value.setString(value);
}

void StatBox::setPosition(
    sf::Vector2f position)
{
    m_title.setPosition(position);

    m_value.setPosition(
        position.x,
        position.y + 22.f);
}

void StatBox::draw(
    sf::RenderWindow& window)
{
    window.draw(m_title);
    window.draw(m_value);
}