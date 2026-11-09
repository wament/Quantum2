#pragma once

#include <vector>
#include <stdexcept>
#include <cstddef>
#include "Card.hpp"
#include "Constants.hpp"
#include <random>

class Deck
{
  public:
    Deck() = default;

    void create(int numberOfDecks);
    void shuffle();
    // Deterministic variant of shuffle() for tests: identical shoe construction (shuffle + cut card + shoe record),
    // but the shuffle is driven by `seed` instead of std::random_device. Production shuffle() keeps the OS seed, so
    // gameplay order stays unpredictable; this seam exists so a test can pin an exact shoe order (B18).
    void shuffleWithSeed(unsigned long long seed);
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

    // * * @brief Returns the shoe record from the previous cycle. * * This should be called BEFORE reshuffling in your game logic. * Returns nullptr if there is no previous record (e.g., start of game). * Resets the internal flag
    const std::vector<Card>* getPreviousShoeRecord();

  private:
    std::vector<Card> cards;
    std::mt19937 rng;
    unsigned long long lastShuffleSeed;

    // Shared body of shuffle() / shuffleWithSeed(): the only difference is where the seed comes from.
    void shuffleWith(unsigned long long seed);
    bool cutCardReached = false;

    // Shoe size this deck was created with. reshuffleWithRemaining() rebuilds from this (not the compile-time
    // NUM_DECKS default) so a mid-shoe reshuffle on a non-default shoe keeps its real card count.
    int shoeDecks_ = Constants::NUM_DECKS;

    // The record of the currently active shoe
    std::vector<Card> activeShoeRecord;

    // The record of the previous shoe, waiting to be saved
    std::vector<Card> previousShoeRecord;
    bool hasPreviousRecord = false;
};