#include "StatBox.hpp"
#include "Constants.hpp"
#include <algorithm>


StatBox::StatBox() {}

StatBox::~StatBox() { delete m_title; delete m_value; }

void StatBox::initialize(sf::Font &font, const std::string &title)
{
    m_titleText = title;

    m_title = new sf::Text(font, title, static_cast<unsigned int>(Constants::HUD_FONT_SIZE));
    m_title->setFillColor(sf::Color::White);

    m_value = new sf::Text(font, "", static_cast<unsigned int>(Constants::HUD_FONT_SIZE));
    m_value->setFillColor(sf::Color::White);
}

void StatBox::setTitle(const std::string &title) { m_titleText = title; }

std::string StatBox::getTitle() const { return m_titleText; }

sf::FloatRect StatBox::getTitleBounds() const
{
    // Union of the title and value bounds so a click anywhere on a clickable box registers
    sf::Vector2f minPos(1e9f, 1e9f);
    sf::Vector2f maxPos(-1e9f, -1e9f);
    bool has = false;

    auto includeRect = [&](const sf::FloatRect &r) {
        const sf::Vector2f p = r.position;
        const sf::Vector2f s = r.size;
        minPos.x = std::min(minPos.x, p.x);
        minPos.y = std::min(minPos.y, p.y);
        maxPos.x = std::max(maxPos.x, p.x + s.x);
        maxPos.y = std::max(maxPos.y, p.y + s.y);
        has = true;
    };

    if (m_title) includeRect(m_title->getGlobalBounds());
    if (m_value) includeRect(m_value->getGlobalBounds());

    if (!has) return sf::FloatRect{};

    return sf::FloatRect(minPos, maxPos - minPos);
}

void StatBox::setValue(const std::string &value) { m_value->setString(value); }

void StatBox::setValueColor(sf::Color color) { m_value->setFillColor(color); }

void StatBox::setPosition(sf::Vector2f position) {  m_title->setPosition(position);  m_value->setPosition(sf::Vector2f(position.x, position.y + 18.f)); }

void StatBox::draw(sf::RenderWindow &window) { window.draw(*m_title); window.draw(*m_value); }
