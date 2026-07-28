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
};

/*

inline void to_json(json& j, const Card& c)
{
    j =
    {
        {"value", c.value},
        {"suit", static_cast<int>(c.suit)}
    };
}

inline void from_json(const json& j, Card& c)
{
    c.value = j.at("value");
    c.suit = static_cast<Suit>(j.at("suit").get<int>());
}

*/