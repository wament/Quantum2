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
}

void HUD::initialize(sf::Font& font)
{
    // Initialize all panels with the font
    m_leftPanel.initialize(font);
    m_rightPanel.initialize(font);
    m_bottomPanel.initialize(font);
     
    // Initialize slider label once with the shared font
    m_sliderLabel.setFont(FontManager::get());
    m_sliderLabel.setCharacterSize(Constants::HUD_FONT_SIZE);
    m_sliderLabel.setFillColor(sf::Color::White);

    const float BOTTOM_HEIGHT = 80.f;
    
    // Panels are flush against screen edges (no external margin)
    
    // Bottom panel: full width, anchored to bottom
    m_bottomPanel.setBounds(
        sf::Vector2f(0.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT),
        sf::Vector2f(Constants::WINDOW_WIDTH, BOTTOM_HEIGHT));
    
    // Left panel: flush left, from top to above bottom panel
    m_leftPanel.setBounds(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));
    
    // Right panel: flush right, from top to above bottom panel
    m_rightPanel.setBounds(
        sf::Vector2f(Constants::WINDOW_WIDTH - 200.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));
    
    // Bottom panel stat boxes
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

    // Slider: use PANEL_MARGIN relative to bottom panel as well
    const float sliderX = panelX + 800.f;
    const float sliderY = panelY + 32.f;
    const float sliderWidth = 350.f;
    const float sliderHeight = 14.f;
    riskSliderBounds = sf::FloatRect(sliderX, sliderY, sliderWidth, sliderHeight);

    // Initialize controls text
    updateControls(font);
}

void HUD::updateControls(sf::Font& font)
{
    m_controlsText.setFont(font);
    m_controlsText.setCharacterSize(13);
    m_controlsText.setFillColor(sf::Color(200, 200, 200));
    m_controlsText.setPosition(0.f, Constants::WINDOW_HEIGHT - 80.f - 20.f);
    
    // Build a single-line controls display
    std::string controls = 
        " [Up/Down | Adjust Bet]   [ <- | Auto Kelly]   [ -> | Confirm]                     "
        "[H | Hit]    [S | Stand]    [D | Double]    [P | Split]    [U | Surrender]                     "
        "[R | Next Round/Restart]";
    
    m_controlsText.setString(controls);
    
    // Center horizontally
    sf::FloatRect bounds = m_controlsText.getLocalBounds();
    m_controlsText.setOrigin(bounds.width / 2.f, 0.f);
    m_controlsText.setPosition(Constants::WINDOW_WIDTH / 2.f, Constants::WINDOW_HEIGHT - 80.f - 20.f);
}

void HUD::update(const HUDState& hudState)
{
    if(m_bankrollBox)
        m_bankrollBox->setValue(std::to_string(hudState.bankroll));
    
    if(m_betBox)
        m_betBox->setValue(std::to_string(hudState.currentBet));
    
    if(m_statusBox)
    {
        m_statusBox->setValue(
            hudState.statusMessage.empty() ? "Playing" : hudState.statusMessage);
    }
    
    // Counting info from HUDState
    if(m_countBox)
        m_countBox->setValue(std::to_string(hudState.runningCount));
    
    if(m_trueCountBox)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << hudState.trueCount;
        m_trueCountBox->setValue(oss.str());
    }
    
    if(m_cardsRemainingBox)
        m_cardsRemainingBox->setValue(std::to_string(hudState.cardsRemaining));
    
    if(m_streakBox)
    {
        std::string streakStr;
        if(hudState.streak == 0)
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

    // Sync slider state from HUDState
    sliderValue = hudState.kellyFraction;
    sliderDragging = hudState.riskSliderDragging;
}

void HUD::draw(sf::RenderWindow& window)
{
    if(m_showLeft)
        m_leftPanel.draw(window);
    
    if(m_showRight)
        m_rightPanel.draw(window);
    
    // Bottom panel always visible
    m_bottomPanel.draw(window);

    drawRiskSlider(window);
    
    // Draw controls above bottom panel
    window.draw(m_controlsText);
}

const sf::FloatRect& HUD::getRiskSliderBounds() const
{
    return riskSliderBounds;
}

void HUD::drawRiskSlider(sf::RenderWindow& window)
{
    if (riskSliderBounds.width <= 0.f || riskSliderBounds.height <= 0.f)
        return;

    // Track background
    sf::RectangleShape track;
    track.setPosition(riskSliderBounds.left, riskSliderBounds.top);
    track.setSize(sf::Vector2f(riskSliderBounds.width, riskSliderBounds.height));
    track.setFillColor(sf::Color(30, 30, 30));
    track.setOutlineColor(sf::Color(100, 100, 100));
    track.setOutlineThickness(1.f);
    window.draw(track);

    // Filled portion
    float fillWidth = riskSliderBounds.width * sliderValue;
    sf::RectangleShape fill;
    fill.setPosition(riskSliderBounds.left, riskSliderBounds.top);
    fill.setSize(sf::Vector2f(fillWidth, riskSliderBounds.height));
    fill.setFillColor(sliderDragging ? sf::Color(50, 180, 255) : sf::Color(0, 160, 200));
    window.draw(fill);

    // Thumb
    float thumbX = riskSliderBounds.left + fillWidth - 6.f;
    sf::CircleShape thumb(7.f);
    thumb.setPosition(thumbX, riskSliderBounds.top - 2.f);
    thumb.setFillColor(sf::Color::White);
    thumb.setOutlineColor(sf::Color(0, 140, 180));
    thumb.setOutlineThickness(2.f);
    window.draw(thumb);

    // Calculate Kelly % and RoR
    float kellyPct = sliderValue * 100.f;
    float rorApprox = 0.f;
    if (sliderValue > 0.0f)
    {
        rorApprox = 0.135f * sliderValue * sliderValue * 4.f;
        if (rorApprox > 0.9f) rorApprox = 0.9f;
    }

    // Build label string
    std::ostringstream labelStr;
    labelStr << "Kelly: " << static_cast<int>(kellyPct) << "%";
    if (kellyPct > 50.f)
    { 
        labelStr << " (~" << static_cast<int>(rorApprox * 100.f) << "% RoR) PRESS IT!" ;
    }
    else
    {
        labelStr << " (~" << static_cast<int>(rorApprox * 100.f) << "% RoR)";
    }

    // Update persistent label
    m_sliderLabel.setString(labelStr.str());

    // Color: red when above 50%, white otherwise
    if (kellyPct > 50.f)
    {
        m_sliderLabel.setFillColor(sf::Color(255, 60, 60));
    }
    else
    {
        m_sliderLabel.setFillColor(sf::Color::White);
    }

    sf::FloatRect lb = m_sliderLabel.getLocalBounds();
    m_sliderLabel.setOrigin(lb.width / 2.f, 0.f);
    m_sliderLabel.setPosition(
        riskSliderBounds.left + riskSliderBounds.width / 2.f,
        riskSliderBounds.top - 32.f
    );
    window.draw(m_sliderLabel);
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