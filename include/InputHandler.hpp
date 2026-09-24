#pragma once

#include <SFML/Graphics.hpp>

class Game;

class InputHandler
{
public:
    InputHandler(Game& game);
    void processEvents();

private:
    Game& game;
    void processMenuEvents(const sf::Event& event);
    void processGameEvents(const sf::Event& event);
    void processPauseEvents(const sf::Event& event);
};