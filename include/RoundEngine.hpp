#pragma once

// Forward declaration
// RoundEngine::tick() takes a Game& and relies on Game granting it
// friendship to reach the turn/round state machine's private members.
class Game;

class RoundEngine
{
public:
    // Advance the round/turn state machine by one frame. Operates on the passed game so
    // Game needs no data member for the engine (keeps Game.hpp unchanged).
    void tick(float deltaTime, Game& game);
};
