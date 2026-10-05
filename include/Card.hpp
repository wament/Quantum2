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

    [[nodiscard]] int getBlackjackValue() const;

    [[nodiscard]] std::string getBlackjackValueToString() const;

    [[nodiscard]] bool isAce() const;

    [[nodiscard]] bool isFaceCard() const;

    [[nodiscard]] bool isCutCard() const;
    [[nodiscard]] int getHiLoValue() const;  // Hi-Lo counting value (-1, 0, +1)
    [[nodiscard]] char getSuitSymbol() const;  // Unicode suit symbol
};
