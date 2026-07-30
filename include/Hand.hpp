#pragma once

#include <vector> 
#include <cstddef>
#include "Card.hpp"

class Hand
{
public:
    void addCard(const Card& card);
    void clear();

    [[nodiscard]] int getValue() const;

    [[nodiscard]] bool isBlackjack() const;
    [[nodiscard]] bool isBust() const;

    [[nodiscard]] bool canSplit() const;
    [[nodiscard]] bool canDouble() const;

    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] const Card& getCard(std::size_t index) const;
    const Card& getDealerUpCard() const;
    const std::string getDealerUpCardToString() const;

    Card takeLastCard();
    void setBet(int amount);
    int getBet();

    bool finished = false;
    bool doubledDown = false;
    bool surrendered = false;
    bool split = false;

    int currentBet = 0;
    int insuranceBet = 0;
private:
    std::vector<Card> cards;
     
};

/*
inline void to_json(json& j, const Hand& h)
{
    std::vector<Card> cards;

    for (size_t i = 0; i < h.size(); i++)
        cards.push_back(h.getCard(i));

    j =
    {
        {"cards", cards},
        {"bet", h.getBet()},
        {"finished", h.finished},
        {"doubledDown", h.doubledDown},
        {"surrendered", h.surrendered},
        {"split", h.split}
    };
}
inline void from_json(const json& j, Hand& h)
{
    h.clear();

    for (const Card& c : j.at("cards"))
        h.addCard(c);

    h.setBet(j.value("bet",0));

    h.finished =
        j.value("finished",false);

    h.doubledDown =
        j.value("doubledDown",false);

    h.surrendered =
        j.value("surrendered",false);

    h.split =
        j.value("split",false);
}
 */