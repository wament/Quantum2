#include "HUD.hpp"

#include "Game.hpp"
#include "Constants.hpp"

HUD::HUD()
{

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
}

void HUD::update(const HUDState& state)
{
    /*
    bankrollBox.setValue(
        "$" + std::to_string(state.bankroll));

    betBox.setValue(
        "$" + std::to_string(state.currentBet));

    runningCountBox.setValue(
        std::to_string(state.runningCount));

    trueCountBox.setValue(
        formatFloat(state.trueCount));

    statusBox.setValue(
        state.statusMessage);

    */
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