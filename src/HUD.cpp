#include "HUD.hpp"

#include "Game.hpp"
#include "Constants.hpp"

HUD::HUD()
{

}

HUD::~HUD()
{
    // Clean up stat boxes owned by panels
    delete m_bankrollBox;
    delete m_betBox;
    delete m_statusBox;
}

void HUD::initialize(sf::Font& font)
{
    constexpr float SIDE = 220.f;
    constexpr float BOTTOM = 100.f;

    m_leftPanel.setBounds(
        {0.f,0.f},
        {SIDE,
         Constants::WINDOW_HEIGHT - BOTTOM});

    m_rightPanel.setBounds(
        {Constants::WINDOW_WIDTH - SIDE,0.f},
        {SIDE,
         Constants::WINDOW_HEIGHT - BOTTOM});

    m_bottomPanel.setBounds(
        {0.f,
         Constants::WINDOW_HEIGHT - BOTTOM},
        {Constants::WINDOW_WIDTH,
         BOTTOM});

    m_leftPanel.initialize(font);
    m_rightPanel.initialize(font);
    m_bottomPanel.initialize(font);

    // Create bottom panel stat boxes
    m_bankrollBox = m_bottomPanel.addStatBox("Bankroll");
    m_betBox = m_bottomPanel.addStatBox("Bet");
    m_statusBox = m_bottomPanel.addStatBox("");

    // Position: Bankroll left, Bet center, Status right
    m_bankrollBox->setPosition({15.f, 15.f});
    m_betBox->setPosition({Constants::WINDOW_WIDTH * 0.5f - 40.f, 15.f});
    m_statusBox->setPosition({Constants::WINDOW_WIDTH - 250.f, 15.f});
}

void HUD::update(const HUDState& state)
{
    m_bankrollBox->setValue(
        "$" + std::to_string(state.bankroll));

    m_betBox->setValue(
        "$" + std::to_string(state.currentBet));

    if (!state.statusMessage.empty())
    {
        m_statusBox->setValue(state.statusMessage);
        m_statusBox->setPosition({Constants::WINDOW_WIDTH - 250.f, 45.f});
    }
    else
    {
        m_statusBox->setValue("");
        m_statusBox->setPosition({Constants::WINDOW_WIDTH - 250.f, 15.f});
    }
}

void HUD::draw(sf::RenderWindow& window)
{
    if(m_showLeft)
        m_leftPanel.draw(window);

    if(m_showRight)
        m_rightPanel.draw(window);

    m_bottomPanel.draw(window);
}

void HUD::toggleLeftPanel()
{
    m_showLeft = !m_showLeft;
}

void HUD::toggleRightPanel()
{
    m_showRight = !m_showRight;
}

bool HUD::leftVisible() const
{
    return m_showLeft;
}

bool HUD::rightVisible() const
{
    return m_showRight;
}