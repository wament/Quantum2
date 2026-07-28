#pragma once

#include <vector>
#include <cstddef>
#include "Card.hpp"

class Deck
{
public:
    Deck() = default;

    void create(int numberOfDecks);
    void shuffle();

    [[nodiscard]] Card draw();
    const Card& getCard(std::size_t index) const;

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] bool reachedCutCard() const;
private:
    std::vector<Card> cards;

    bool cutCardReached = false;

};

/*

    friend void to_json(nlohmann::json&, const Deck&);
    friend void from_json(const nlohmann::json&, Deck&);


    
inline void to_json(json& j, const Deck& d)
{
    std::vector<Card> cards;

    for(size_t i=0;i<d.size();i++)
        cards.push_back(d.getCard(i));

    j =
    {
        {"cards",cards}
    };
}
inline void from_json(const json& j, Deck& d)
{
    d = Deck();

    for(const Card& c : j.at("cards"))
        d.draw(); // <-- impossible
}

*/