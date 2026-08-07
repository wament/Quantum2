#include "Hand.hpp"

Hand::Hand()
{
}

void Hand::addCard(const Card& card)
{
    cards.push_back(card);
}

void Hand::clear()
{
    cards.clear();

    finished = false;
    doubledDown = false;
    surrendered = false;
    split = false;

    currentBet = 0;
    insuranceBet = 0;
}

int Hand::getValue() const
{
    int total = 0;
    int aces = 0;

    for (const Card& card : cards)
    {
        total += card.getBlackjackValue();

        if (card.isAce())
            aces++;
    }

    while (total > 21 && aces > 0)
    {
        total -= 10;
        aces--;
    }

    return total;
}

bool Hand::isBlackjack() const
{
    return cards.size() == 2 &&
           getValue() == 21;
}

bool Hand::isSoft() const
{
    // A hand is "soft" if it contains an Ace counting as 11.
    // After calculating value, if any Aces remain unconverted,
    // the hand is soft.
    int total = 0;
    int aces = 0;

    for (const Card& card : cards)
    {
        total += card.getBlackjackValue();

        if (card.isAce())
            aces++;
    }

    while (total > 21 && aces > 0)
    {
        total -= 10;
        aces--;
    }

    return aces > 0;
}

bool Hand::isBust() const
{
    return getValue() > 21;
}

bool Hand::canSplit() const
{
    if (cards.size() != 2)
        return false;

    return cards[0].getBlackjackValue() ==
           cards[1].getBlackjackValue();
}

bool Hand::canDouble() const
{
    return cards.size() == 2 &&
           !doubledDown;
}

std::size_t Hand::size() const
{
    return cards.size();
}

const Card& Hand::getCard(std::size_t index) const
{
    return cards.at(index);
}

const Card& Hand::getDealerUpCard() const
{
    return cards.at(0);
}

const std::string Hand::getDealerUpCardToString() const
{
    Card upCard = cards.at(0);
    if ( upCard.getBlackjackValue() == 11){
        return "Ace";
    }else if ( upCard.getBlackjackValue() == 10){
        return "Face card";
    }
    return std::to_string(upCard.value);
}

Card Hand::takeLastCard()
{
    Card card = cards.back();
    cards.pop_back();
    return card;
}

void Hand::setBet(int amount){
    currentBet = amount;
}

int Hand::getBet() const {
    return currentBet;
}

void Hand::setInsuranceBet(int amount)
{
    insuranceBet = amount;
}

int Hand::getInsuranceBet() const
{
    return insuranceBet;
}

void Hand::setFinished(bool value)
{
    finished = value;
}

bool Hand::isFinished() const
{
    return finished;
}

void Hand::setDoubledDown(bool value)
{
    doubledDown = value;
}

bool Hand::isDoubledDown() const
{
    return doubledDown;
}

void Hand::setSurrendered(bool value)
{
    surrendered = value;
}

bool Hand::isSurrendered() const
{
    return surrendered;
}

void Hand::setSplit(bool value)
{
    split = value;
}

bool Hand::isSplit() const
{
    return split;
}
