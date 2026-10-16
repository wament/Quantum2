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

    // Line pitch between title and value, derived from the font size rather than a fixed 18px, so the box stays
    // proportional at any HUD font size.
    m_lineGap = static_cast<float>(Constants::HUD_FONT_SIZE) * 1.15f;
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

void StatBox::setPosition(sf::Vector2f position) {  m_title->setPosition(position);  m_value->setPosition(sf::Vector2f(position.x, position.y + m_lineGap)); }

// Full vertical extent of the box: two text lines (title + value). Measured as the line box rather than from the
// current strings, so it is stable whether or not a value has been filled in yet -- which is what lets layout code
// center/distribute boxes before any data exists.
float StatBox::blockHeight() const
{
    return 2.f * m_lineGap;
}

sf::Vector2f StatBox::getPosition() const
{
    return m_title ? m_title->getPosition() : sf::Vector2f(0.f, 0.f);
}

void StatBox::draw(sf::RenderWindow &window) { window.draw(*m_title); window.draw(*m_value); }
