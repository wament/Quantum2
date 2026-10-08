#include "StatBox.hpp"
#include "Constants.hpp"


StatBox::StatBox()
{
    
}

StatBox::~StatBox()
{
    delete m_title;
    delete m_value;
}

void StatBox::initialize(sf::Font &font, const std::string &title)
{
    m_titleText = title;

    m_title = new sf::Text(font, title, static_cast<unsigned int>(Constants::HUD_FONT_SIZE));
    m_title->setFillColor(sf::Color::White);

    m_value = new sf::Text(font, "", static_cast<unsigned int>(Constants::HUD_FONT_SIZE));
    m_value->setFillColor(sf::Color::White);
}

void StatBox::setTitle(const std::string &title)
{
    m_titleText = title;
}

std::string StatBox::getTitle() const
{
    return m_titleText;
}

sf::FloatRect StatBox::getTitleBounds() const
{
    // getGlobalBounds() already accounts for the title's position, so it can be compared directly with
    // mouse coordinates converted into world space.
    return m_title ? m_title->getGlobalBounds() : sf::FloatRect{};
}

void StatBox::setValue(const std::string &value)
{
    m_value->setString(value);
}

void StatBox::setPosition(sf::Vector2f position)
{
    m_title->setPosition(position);
    m_value->setPosition(sf::Vector2f(position.x, position.y + 18.f));
}

void StatBox::draw(sf::RenderWindow &window)
{
    window.draw(*m_title);
    window.draw(*m_value);
}
