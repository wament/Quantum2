#pragma once

#include <SFML/Graphics.hpp>

#include "Panel.hpp"
#include "HUDState.hpp"



class HUD
{
public:
    HUD();

    void initialize(sf::Font& font);

    void update(const HUDState& hudState);

    void draw(sf::RenderWindow& window);

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
};

