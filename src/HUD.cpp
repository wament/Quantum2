#include "HUD.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <Constants.hpp>
#include "FontManager.hpp"

HUD::HUD()
{
}

HUD::~HUD()
{
    // StatBox pointers are owned by Panels and cleaned up there

    if (m_controlsText)
        delete m_controlsText;
    if (m_strategyHintText)
        delete m_strategyHintText;
}

void HUD::initialize(sf::Font &font)
{
    m_leftPanel.initialize(font);
    m_rightPanel.initialize(font);
    m_bottomPanel.initialize(font);

    const float BOTTOM_HEIGHT = 80.f;

    // Panels are flush against screen edges (no external margin)

    m_bottomPanel.setBounds(
        sf::Vector2f(0.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT),
        sf::Vector2f(Constants::WINDOW_WIDTH, BOTTOM_HEIGHT));

    m_leftPanel.setBounds(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));

    m_rightPanel.setBounds(
        sf::Vector2f(Constants::WINDOW_WIDTH - 200.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));

    m_bankrollBox = m_bottomPanel.addStatBox("Bankroll");
    m_bankrollBox->initialize(font, "Bankroll");

    m_betBox = m_bottomPanel.addStatBox("Bet");
    m_betBox->initialize(font, "Bet");

    m_statusBox = m_bottomPanel.addStatBox("Status");
    m_statusBox->initialize(font, "Status");

    // New counting/state boxes
    m_countBox = m_bottomPanel.addStatBox("Count");
    m_countBox->initialize(font, "Count");

    m_trueCountBox = m_bottomPanel.addStatBox("True Count");
    m_trueCountBox->initialize(font, "True Count");

    m_cardsRemainingBox = m_bottomPanel.addStatBox("Cards Left");
    m_cardsRemainingBox->initialize(font, "Cards Left");

    m_streakBox = m_bottomPanel.addStatBox("Streak");
    m_streakBox->initialize(font, "Streak");

    m_strategyBox = m_bottomPanel.addStatBox("Strategy");
    m_strategyBox->initialize(font, "Strategy");

    // Stat box positions use PANEL_MARGIN relative to bottom panel's top-left
    float panelX = Constants::PANEL_MARGIN + 5.f;
    float panelY = Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT + Constants::PANEL_MARGIN;

    m_bankrollBox->setPosition(sf::Vector2f(panelX, panelY));
    m_betBox->setPosition(sf::Vector2f(panelX + 110.f, panelY));
    m_statusBox->setPosition(sf::Vector2f(panelX + 220.f, panelY));
    m_countBox->setPosition(sf::Vector2f(panelX + 330.f, panelY));
    m_trueCountBox->setPosition(sf::Vector2f(panelX + 440.f, panelY));
    m_cardsRemainingBox->setPosition(sf::Vector2f(panelX + 560.f, panelY));
    m_streakBox->setPosition(sf::Vector2f(panelX + 680.f, panelY));
    m_strategyBox->setPosition(sf::Vector2f(panelX + 790.f, panelY));

    updateControls(font);

    m_strategyHintText = new sf::Text(font, "", 20u);
    m_strategyHintText->setFillColor(sf::Color(255, 215, 0));
    m_strategyHintText->setPosition(sf::Vector2f(Constants::WINDOW_WIDTH / 2.f - 100.f, 20.f));


}

void HUD::updateControls(sf::Font &font)
{

    m_controlsText = new sf::Text(font, "", 13u);
    m_controlsText->setFillColor(sf::Color(200, 200, 200));
    m_controlsText->setPosition(sf::Vector2f(0.f, Constants::WINDOW_HEIGHT - 80.f - 20.f));

    // Build a single-line controls display
    std::string controls =
        " [Up/Down | Adjust Bet]   [ <- | Auto Kelly]   [ -> | Confirm]                     "
        "[H | Hit]    [S | Stand]    [D | Double]    [P | Split]    [Y | Surrender]                     "
        "[R | Next Round/Restart]";

    m_controlsText->setString(controls);

    // Center horizontally
    sf::FloatRect bounds = m_controlsText->getLocalBounds();
    m_controlsText->setOrigin(sf::Vector2f(bounds.size.x / 2.f, 0.f));
    m_controlsText->setPosition(sf::Vector2f(Constants::WINDOW_WIDTH / 2.f, Constants::WINDOW_HEIGHT - 80.f - 20.f));
}

void HUD::update(const HUDState &hudState)
{
    if (m_bankrollBox)
        m_bankrollBox->setValue(std::to_string(hudState.bankroll));

    if (m_betBox)
        m_betBox->setValue(std::to_string(hudState.currentBet));

    if (m_statusBox)
    {
        m_statusBox->setValue(
            hudState.statusMessage.empty() ? "Playing" : hudState.statusMessage);
    }

    if (m_countBox)
        m_countBox->setValue(std::to_string(hudState.runningCount));

    if (m_trueCountBox)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << hudState.trueCount;
        m_trueCountBox->setValue(oss.str());
    }

    if (m_cardsRemainingBox)
        m_cardsRemainingBox->setValue(std::to_string(hudState.cardsRemaining));

    if (m_streakBox)
    {
        std::string streakStr;
        if (hudState.streak == 0)
        {
            streakStr = "-";
        }
        else
        {
            streakStr = std::to_string(hudState.streak); 
            streakStr += hudState.streakIsWin ? "W" : "L";
        }
        m_streakBox->setValue(streakStr);
    }

    if (m_strategyBox)
    {
        std::string strategyStr;
        if (hudState.playerStats && hudState.playerStats->strategyTotal > 0)
        {
            int pct = (hudState.playerStats->strategyHits * 100) / hudState.playerStats->strategyTotal;
            strategyStr = std::to_string(pct) + "%";
        }
        else
        {
            strategyStr = "-";
        }
        m_strategyBox->setValue(strategyStr);
    }

    // Update strategy hint
    m_strategyHint = hudState.basicStrategyHint;
    if (m_strategyHintText)
    {
        m_strategyHintText->setString(m_strategyHint);
    }



}

void HUD::draw(sf::RenderWindow &window)
{
    if (m_showLeft)
        m_leftPanel.draw(window);

    if (m_showRight)
        m_rightPanel.draw(window);

    // Bottom panel always visible
    m_bottomPanel.draw(window);

    // Draw controls above bottom panel
    window.draw(*m_controlsText);
    // Draw strategy hint if present
    if (m_strategyHintText && !m_strategyHint.empty())
    {
        window.draw(*m_strategyHintText);
    }
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