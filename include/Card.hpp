#pragma once

#include <string>
enum class Suit
{
    Spades,
    Hearts,
    Diamonds,
    Clubs
};

struct Card
{
    int value{};
    Suit suit{Suit::Spades};

    int getBlackjackValue() const;

    std::string getBlackjackValueToString() const;

    bool isAce() const;

    bool isFaceCard() const;

    bool isCutCard() const;
    char getSuitSymbol() const; // Unicode suit symbol
};
