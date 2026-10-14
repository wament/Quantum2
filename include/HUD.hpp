#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

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

    // Feature 3: bankroll-curve, edge-tracking and bet-distribution charts drawn with SFML primitives on the
    // right panel. No-op when the right panel is hidden or no analytics data has been collected yet.
    void drawAnalyticsCharts(sf::RenderWindow &window);

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

    // Feature 1 & 3: analytics dashboard rendered on the left panel (always-visible stat boxes). The order of
    // m_leftBoxes must match the value vector rebuilt each frame in HUD::update.
    std::vector<StatBox*> m_leftBoxes;

    // Feature 3: snapshot of the live stats taken in HUD::update, so drawAnalyticsCharts (called from the
    // renderer without a HUDState parameter) can render charts against the same data.
    const PlayerStats* m_analyticsStats = nullptr;

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

    void drawRiskSlider(sf::RenderWindow &window);

    void updateControls(sf::Font &font);
};

