#pragma once

#include <SFML/Graphics.hpp>

#include "Panel.hpp"
#include "HUDState.hpp"
#include "StatBox.hpp"

class HUD
{
public:
    HUD();

    ~HUD();

    void initialize(sf::Font &font);

    void update(const HUDState &hudState);

    void draw(sf::RenderWindow &window);

    void toggleLeftPanel();
    void toggleRightPanel();

    bool leftVisible() const;
    bool rightVisible() const;

    // Expose slider bounds so Game can handle input
    const sf::FloatRect &getRiskSliderBounds() const;

private:
    bool m_showLeft = true;
    bool m_showRight = true;

    Panel m_leftPanel;
    Panel m_rightPanel;
    Panel m_bottomPanel;

    StatBox *m_bankrollBox = nullptr;
    StatBox *m_betBox = nullptr;
    StatBox *m_statusBox = nullptr;

    // Counting/stat boxes
    StatBox *m_countBox = nullptr;
    StatBox *m_trueCountBox = nullptr;
    StatBox *m_cardsRemainingBox = nullptr;
    StatBox *m_streakBox = nullptr;
    StatBox *m_strategyBox = nullptr;

    // Slider state from HUDState (for rendering)
    float sliderValue = 0.33f;
    bool sliderDragging = false;

    // Slider geometry (owned by HUD)
    sf::FloatRect riskSliderBounds;

    // Persistent slider label (set font once, reuse every frame)

    sf::Text *m_sliderLabel = nullptr;

    // Controls display
    sf::Text *m_controlsText = nullptr;

    // Basic Strategy Advisor hint
    sf::Text *m_strategyHintText = nullptr;
    std::string m_strategyHint;

    // Player bankrolls display
    sf::Text *m_playerBankrollsText = nullptr;

    // End-of-round summary overlay
    std::vector<std::string> m_playerSummaries;
    float m_summaryTimer = 0.0f;

    void drawRiskSlider(sf::RenderWindow &window);
    void drawSummaryOverlay(sf::RenderWindow &window);

    void updateControls(sf::Font &font);
};

