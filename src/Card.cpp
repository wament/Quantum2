#include "Card.hpp"
#include "Constants.hpp"

int Card::getBlackjackValue() const
{
    if (value > 10)
        return 10;

    if (value == 1)
        return 11;

    return value;
}

std::string Card::getBlackjackValueToString() const
{
    if (value > 10)
        return "Face";

    if (value == 1)
        return "Ace";

    return std::to_string(value);
}

bool Card::isAce() const { return value == 1; }

bool Card::isFaceCard() const { return value >= 11 && value <= 13; }

bool Card::isCutCard() const { return value == Constants::CUT_CARD_VALUE; }


