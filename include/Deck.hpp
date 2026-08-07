#pragma once

#include <vector>
#include <stdexcept>
#include <cstddef>
#include "Card.hpp"

class Deck
{
public:
    Deck() = default;

    void create(int numberOfDecks);
    void shuffle();

    [[nodiscard]] Card draw();
    const Card& getCard(std::size_t index) const;
    const std::vector<Card>& getCards() const;

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] bool reachedCutCard() const;

    /**
     * @brief Returns the shoe record from the previous cycle.
     * 
     * This should be called BEFORE reshuffling in your game logic.
     * Returns nullptr if there is no previous record (e.g., start of game).
     * Resets the internal flag so the record is only retrieved once.
     */
    [[nodiscard]] const std::vector<Card>* getPreviousShoeRecord();

    /**
     * @brief Returns the shoe record currently in play (or most recently shuffled).
     * 
     * Use this when the player exits the game to save the final shoe state.
     */
    [[nodiscard]] const std::vector<Card>& getFinalShoeRecord() const;

private:
    std::vector<Card> cards;

    bool cutCardReached = false;

    // The record of the currently active shoe
    std::vector<Card> activeShoeRecord;
    
    // The record of the previous shoe, waiting to be saved
    std::vector<Card> previousShoeRecord;
    bool hasPreviousRecord = false;
};