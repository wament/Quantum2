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

    // Controls display
    sf::Text *m_controlsText = nullptr;

    // Basic Strategy Advisor hint
    sf::Text *m_strategyHintText = nullptr;
    std::string m_strategyHint;

    void updateControls(sf::Font &font);
};

