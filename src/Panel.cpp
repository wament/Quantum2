#include "Panel.hpp"

void Panel::initialize(sf::Font& font)
{
    m_background.setFillColor(
        sf::Color(40,40,40));

    m_background.setOutlineThickness(2.f);

    m_background.setOutlineColor(
        sf::Color(80,80,80));
}

void Panel::setBounds(
    sf::Vector2f position,
    sf::Vector2f size)
{
    m_background.setPosition(position);
    m_background.setSize(size);
}

void Panel::draw(sf::RenderWindow& window)
{
    window.draw(m_background);

    for(auto& stat : m_stats)
        stat.draw(window);
}

void Panel::addStat(const StatBox& stat)
{
    m_stats.push_back(stat);
}