#include "StatBox.hpp"
#include "Constants.hpp"

StatBox::StatBox()
{

}

void StatBox::initialize(sf::Font& font, const std::string& title)
{
    m_titleText = title;

    m_title.setFont(font);
    m_title.setColor(sf::Color::White);
    m_title.setCharacterSize(Constants::HUD_FONT_SIZE);
    m_title.setString(title);

    m_value.setFont(font);
    m_value.setColor(sf::Color::White);
    m_value.setCharacterSize(Constants::HUD_FONT_SIZE);
}

void StatBox::setTitle(const std::string& title)
{
    m_titleText = title;
}

std::string StatBox::getTitle() const
{
    return m_titleText;
}

void StatBox::setValue(const std::string& value)
{
    m_value.setString(value);
}

void StatBox::setPosition(sf::Vector2f position)
{
    m_title.setPosition(position);

    m_value.setPosition(
        position.x,
        position.y + 18.f);
}

void StatBox::draw(sf::RenderWindow& window)
{
    window.draw(m_title);
    window.draw(m_value);
}
