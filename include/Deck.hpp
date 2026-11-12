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

    // B21: the seed that produced the current shoe's order. Set by shuffle() (from std::random_device), by
    // shuffleWithSeed() (from the caller), and by reshuffleWithRemaining() (a fresh OS seed) — every path that
    // reorders the shoe records what it used, so a shoe is reproducible from its own record. This is what makes
    // "store the seed, not the deck" sound: create(shoeDecks()) + shuffleWithSeed(shuffleSeed()) rebuilds the
    // identical shoe, card for card.
    unsigned long long shuffleSeed() const { return lastShuffleSeed; }

    // The number of decks this shoe was built from (NOT the card count — shuffleWith() inserts a cut card, so a
    // 6-deck shoe is 313 cards). Recorded alongside the seed because the seed alone does not fix the shoe size.
    int shoeDecks() const { return shoeDecks_; }

    bool empty() const;
    std::size_t size() const;

    bool reachedCutCard() const;

    // * * @brief Returns the shoe record from the previous cycle. * * This should be called BEFORE reshuffling in your game logic. * Returns nullptr if there is no previous record (e.g., start of game). * Resets the internal flag
    const std::vector<Card>* getPreviousShoeRecord();

  private:
    std::vector<Card> cards;
    std::mt19937 rng;
    // Must start at 0: shuffleSeed() is read when a shoe record is written, and a Deck that was filled by
    // setCards()/create() without a shuffle has no seed. An indeterminate value here would be serialized as a
    // plausible-looking seed and silently corrupt replay reconstruction.
    unsigned long long lastShuffleSeed = 0;

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