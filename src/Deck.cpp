#include "Deck.hpp"
#include "Constants.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <random>

void Deck::create(int numberOfDecks)
{
    cards.clear();
    cutCardReached = false;
    activeShoeRecord.clear();
    previousShoeRecord.clear();
    hasPreviousRecord = false;

    for (int deck = 0; deck < numberOfDecks; deck++)
    {
        for (int suit = 0; suit < 4; suit++)
        {
            for (int value = 1; value <= 13; value++)
            {
                cards.push_back({
                    value,
                    static_cast<Suit>(suit)
                });
            }
        }
    }
}

void Deck::shuffle()
{
    // If a prior shoe has no saver yet, move its record to "previous" so it can be saved next.
    if (!activeShoeRecord.empty())
    {
        previousShoeRecord = activeShoeRecord;
        hasPreviousRecord = true;
    }

    // Reseed from the OS RNG so each shoe's order is unpredictable.
    lastShuffleSeed = std::random_device{}();
    rng = std::mt19937(static_cast<std::mt19937::result_type>(lastShuffleSeed));
    std::shuffle(cards.begin(), cards.end(), rng);


    cutCardReached = false;

    // Cut card sits 25 from the end, so most of a full shoe is dealt before it turns.
    const std::size_t cutPosition = 25;

    cards.insert(
        cards.begin() + cutPosition,
        { Constants::CUT_CARD_VALUE, Suit::Spades }
    );

    activeShoeRecord = cards;

    Logger::debug() << "Shoe shuffled. Cards: " << cards.size() << ".";
}

Card Deck::draw()
{
    Card card = cards.back();
    cards.pop_back();

    if (card.isCutCard())
    {
        cutCardReached = true;

        card = cards.back();
        cards.pop_back();
    }

    return card;
}

const Card& Deck::getCard(std::size_t index) const
{
    return cards.at(index);
}

const std::vector<Card>& Deck::getCards() const
{
    return cards;
}

bool Deck::empty() const
{
    return cards.empty();
}

std::size_t Deck::size() const
{
    return cards.size();
}

bool Deck::reachedCutCard() const
{
    return cutCardReached;
}

void Deck::reshuffleWithRemaining(const std::vector<Card>& tableCards)
{
    // Return every card on the table to the shoe before reshuffling, so nothing is lost.
    cards.insert(cards.end(), tableCards.begin(), tableCards.end());

    // Move the active shoe to "previous" so it can be saved after this one completes.
    if (!activeShoeRecord.empty())
    {
        previousShoeRecord = activeShoeRecord;
        hasPreviousRecord = true;
    }

    lastShuffleSeed = std::random_device{}();
    rng = std::mt19937(static_cast<std::mt19937::result_type>(lastShuffleSeed));
    std::shuffle(cards.begin(), cards.end(), rng);

    cutCardReached = false;

    // Cut card sits 25 from the end, clamped to the deck's end when it is too short.
    std::size_t cutPosition;
    if (cards.size() > 25)
    {
        cutPosition = cards.size() - 25;
    }
    else
    {
        cutPosition = cards.size();
    }

    cards.insert(
        cards.begin() + cutPosition,
        { Constants::CUT_CARD_VALUE, Suit::Spades }
    );

    activeShoeRecord = cards;
}

const std::vector<Card>* Deck::getPreviousShoeRecord()
{
    if (!hasPreviousRecord)
    {
        return nullptr;
    }

    // Mark as retrieved so we don't save it twice
    hasPreviousRecord = false;
    return &previousShoeRecord;
}

const std::vector<Card>& Deck::getFinalShoeRecord() const
{
    return activeShoeRecord;
}

const unsigned long long Deck::getLastShuffleSeed() const 
{
    return lastShuffleSeed;
}
