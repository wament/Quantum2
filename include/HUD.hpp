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

    // World-space bounds of the Counting Style toggle box (label + value), used for click hit-testing.
    sf::FloatRect countTitleBounds() const;

private:
    bool m_showLeft = true;
    bool m_showRight = true;

    Panel m_leftPanel;
    Panel m_rightPanel;
    Panel m_bottomPanel;

    StatBox *m_bankrollBox = nullptr;
    StatBox *m_betBox = nullptr;

    // Counting/stat boxes — Counting Style sits at the top of the right panel and is clickable to toggle.
    StatBox *m_countingStyleBox = nullptr;
    StatBox *m_countBox = nullptr;
    StatBox *m_trueCountBox = nullptr;
    StatBox *m_cardsRemainingBox = nullptr;
    StatBox *m_streakBox = nullptr;
    StatBox *m_strategyBox = nullptr;

    // Book advisor: advises basic strategy for the current hand
    StatBox *m_bookBox = nullptr;

    // Controls display
    sf::Text *m_controlsText = nullptr;

    void updateControls(sf::Font &font);
};

