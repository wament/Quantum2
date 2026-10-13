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

    // --- Bottom bar boxes (Bankroll + Bet keep their original left positions) ---
    m_bankrollBox = m_bottomPanel.addStatBox("Bankroll");
    m_bankrollBox->initialize(font, "Bankroll");

    m_betBox = m_bottomPanel.addStatBox("Bet");
    m_betBox->initialize(font, "Bet");

    // Streak + Strategy moved down next to Bankroll/Bet in the bottom bar
    m_streakBox = m_bottomPanel.addStatBox("Streak");
    m_streakBox->initialize(font, "Streak");

    m_strategyBox = m_bottomPanel.addStatBox("Strategy");
    m_strategyBox->initialize(font, "Strategy");

    // New: Book advisor — advises basic strategy for the current hand
    m_bookBox = m_bottomPanel.addStatBox("Book");
    m_bookBox->initialize(font, "Book");

    // --- Right panel boxes stacked vertically at the top of the panel ---
    // Counting Style is first so it sits at the very top; clicking it toggles between Hi-Lo and Reko.
    m_countingStyleBox = m_rightPanel.addStatBox("Counting Style");
    m_countingStyleBox->initialize(font, "Counting Style");

    m_countBox = m_rightPanel.addStatBox("Count");
    m_countBox->initialize(font, "Count");

    m_trueCountBox = m_rightPanel.addStatBox("True Count");
    m_trueCountBox->initialize(font, "True Count");

    m_cardsRemainingBox = m_rightPanel.addStatBox("Cards Left");
    m_cardsRemainingBox->initialize(font, "Cards Left");

    // Bottom bar: horizontal stack. Bankroll and Bet keep their original left positions; Streak,
    // Strategy and Book follow to the right with even spacing. Positions are absolute screen coords.
    float panelX = Constants::PANEL_MARGIN + 5.f;
    float panelY = Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT + Constants::PANEL_MARGIN;

    const float SPACING = 140.f;
    m_bankrollBox->setPosition(sf::Vector2f(panelX, panelY));
    m_betBox->setPosition(sf::Vector2f(panelX + SPACING, panelY));
    m_streakBox->setPosition(sf::Vector2f(panelX + 2.f * SPACING, panelY));
    m_strategyBox->setPosition(sf::Vector2f(panelX + 3.f * SPACING, panelY));
    m_bookBox->setPosition(sf::Vector2f(panelX + 4.f * SPACING, panelY));

    // Right panel: vertical stack at the top of the panel. Positions are absolute screen coords so the
    // boxes sit inside the right panel (whose top-left is WINDOW_WIDTH-200, 0).
    const float boxHeight = 46.f;
    const float rightPanelX = Constants::WINDOW_WIDTH - 200.f + Constants::PANEL_MARGIN;
    const float rightPanelY = Constants::PANEL_MARGIN;

    m_countingStyleBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY));
    m_countBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY + boxHeight));
    m_trueCountBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY + 2.f * boxHeight));
    m_cardsRemainingBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY + 3.f * boxHeight));

    updateControls(font);
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

    if (m_countingStyleBox)
        m_countingStyleBox->setValue(hudState.countingStyleName);

    if (m_countBox)
    {
        m_countBox->setTitle("Count");
        m_countBox->setValue(std::to_string(hudState.runningCount));
    }

    if (m_trueCountBox)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << hudState.trueCount;
        m_trueCountBox->setValue(oss.str());

        // White when Hi-Lo is active (running + true count are the same system); yellow when Reko is active so
        // the surfaced Hi-Lo true count stands out as a cross-check. sf::Color(255, 215, 0) matches the game's
        // gold/yellow accent used for the strategy hint.
        m_trueCountBox->setValueColor(hudState.trueCountHighlighted ? sf::Color(255, 215, 0) : sf::Color::White);
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

    // Book advisor: show the advised action during the player's turn, and hide it (dash) on the
    // dealer's turn or when there is no active hand. basicStrategyHint is already empty in those cases.
    if (m_bookBox)
    {
        if (hudState.isDealerTurn || hudState.basicStrategyHint.empty())
            m_bookBox->setValue("-");
        else
            m_bookBox->setValue(hudState.basicStrategyHint);
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

sf::FloatRect HUD::countTitleBounds() const
{
    // The Counting Style box (e.g. "Counting Style" / "Hi-Lo") is clickable — see InputHandler toggling the
    // counting style. Bounds span both its label and value lines so clicks anywhere on it register.
    return m_countingStyleBox ? m_countingStyleBox->getTitleValueBounds() : sf::FloatRect{};
}