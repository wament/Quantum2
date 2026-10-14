#pragma once

#include <vector>
#include <stdexcept>
#include <cstddef>
#include "Card.hpp"
#include <random>

class Deck
{
  public:
    Deck() = default;

    void create(int numberOfDecks);
    void shuffle();
    // Reshuffle the cards still in the shoe together with ones that were dealt (offTable), then reinsert
    // the cut card. Used to start a fresh shoe after the cut card is reached or on reshuffle requests.
    void reshuffleWithRemaining(const std::vector<Card>& offTable);
    // Replace the entire shoe with an exact card order. Used by unit tests to deal a deterministic hand.
    void setCards(const std::vector<Card>& cards);

    Card draw();
    const Card& getCard(std::size_t index) const;
    const std::vector<Card>& getCards() const;

    bool empty() const;
    std::size_t size() const;

    bool reachedCutCard() const;

    /**
     * @brief Returns the shoe record from the previous cycle.
     *
     * This should be called BEFORE reshuffling in your game logic.
     * Returns nullptr if there is no previous record (e.g., start of game).
     * Resets the internal flag so the record is only retrieved once.
     */
    const std::vector<Card>* getPreviousShoeRecord();

  private:
    std::vector<Card> cards;
    std::mt19937 rng;
    unsigned long long lastShuffleSeed;
    bool cutCardReached = false;

    // The record of the currently active shoe
    std::vector<Card> activeShoeRecord;

    // The record of the previous shoe, waiting to be saved
    std::vector<Card> previousShoeRecord;
    bool hasPreviousRecord = false;
};