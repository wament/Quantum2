#include "Deck.hpp"
#include "Constants.hpp"

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
    // 1. If we already have an active record from a prior shoe,
    //    move it to "previous" so it's ready to be saved before we start this new one.
    if (!activeShoeRecord.empty())
    {
        previousShoeRecord = activeShoeRecord;
        hasPreviousRecord = true;
    }

    // 2. Perform the shuffle
    std::shuffle(
        cards.begin(),
        cards.end(),
        std::mt19937(std::random_device{}())
    );

    cutCardReached = false;

    const std::size_t cutPosition =
        static_cast<std::size_t>(cards.size() * 0.15);

    cards.insert(
        cards.begin() + cutPosition,
        { Constants::CUT_CARD_VALUE, Suit::Spades }
    );

    // 3. Capture the new active shoe record immediately
    activeShoeRecord = cards;
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