#pragma once

#include <vector>
#include <string>

#include "Hand.hpp"
#include "Deck.hpp"

struct PlayerStats
{
    int shoesFinished = 0;
    int handsFinished = 0;

    int wins = 0;
    int losses = 0;
    int pushes = 0;
    int blackjacks = 0;
    int splits = 0;
    int doubles = 0;

    int betsPlaced = 0;
    int betSum = 0;
    int largestHand = 0;

    int greatestWin = 0;
    int greatestLoss = 0;

    int greatestSessionWin = 0;
    int greatestSessionLoss = 0;
};



struct RoundRecord
{
    Hand dealer;
    Hand player;

    std::vector<std::string> actionSequence;

    int wager = 0;
    int payout = 0;

    bool won = false;

    int netProfit() const
    {
        return payout - wager;
    }

    bool isWin() const 
    {
        return netProfit() >= 0; // a push is a win! don't let anyone tell you different.
    }
};



    //time
    //$$$
    //peak count
    //avg count
    //hands until TC=2 hit
    //hands at TC>=2
    //
struct ShoeRecord
{ 
    Deck deck;
    std::vector<RoundRecord> rounds;
};

/*
inline void to_json(json& j, const RoundRecord& r)
{
    j =
    {
        {"dealer",r.dealer},
        {"player",r.player},
        {"actions",r.actionSequence},
        {"wager",r.wager},
        {"payout",r.payout},
        {"won",r.won}
    };
}
inline void from_json(const json& j, RoundRecord& r)
{
    r.dealer =
        j.at("dealer");

    r.player =
        j.at("player");

    r.actionSequence =
        j.at("actions");

    r.wager =
        j.at("wager");

    r.payout =
        j.at("payout");

    r.won =
        j.at("won");
}
inline void to_json(json& j,const ShoeRecord& s)
{
    j =
    {
        {"deck",s.deck},
        {"rounds",s.rounds}
    };
}
inline void from_json(const json& j,ShoeRecord& s)
{
    s.deck =
        j.at("deck");

    s.rounds =
        j.at("rounds");
}
 */