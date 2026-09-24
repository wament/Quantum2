#pragma once

#include <SFML/Graphics.hpp>
#include <memory>

class Game;

class GameRenderer
{
public:
    void draw(Game &game);
    void resizeView(sf::RenderWindow &window, sf::View &view);

private:
    void drawDealer(Game &game);
    void drawPlayers(Game &game);
    void drawChips(Game &game);
    void drawDeck(Game &game);
    void drawInsurance(Game &game);
    void drawMenuItem(Game &game, const std::string &label, bool selected, float y);
    void drawMainMenu(Game &game);
    void drawPauseMenu(Game &game);
    void drawSummaryOverlay(Game &game);
    sf::Vector2f getPlayerPosition(Game &game, size_t playerIndex, size_t totalPlayers) const;
};
