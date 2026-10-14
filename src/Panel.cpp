#include "Panel.hpp"

Panel::Panel() = default;

void Panel::initialize(sf::Font& font)
{
    m_background.setFillColor(sf::Color(40, 40, 40));
    m_background.setOutlineThickness(2.f);
    m_background.setOutlineColor(sf::Color(80, 80, 80));

    // Initialize existing stat boxes with this font
    for (auto* box : m_stats)
        box->initialize(font, box->getTitle());
}

void Panel::setBounds(sf::Vector2f position, sf::Vector2f size)
{
    m_background.setPosition(position);
    m_background.setSize(size);
}

sf::Rect<float> Panel::bounds() const
{
    return sf::Rect<float>(m_background.getPosition(), m_background.getSize());
}

StatBox* Panel::addStatBox(const std::string& title)
{
    auto* box = new StatBox();
    box->setTitle(title);
    m_stats.push_back(box);
    return box;
}

void Panel::draw(sf::RenderWindow& window)
{
    window.draw(m_background);
    for (auto* stat : m_stats)
        stat->draw(window);
}
