#pragma once

#include <vector>
#include <deque>
#include <string>
#include <chrono>

#include "Card.hpp"
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

    int betsPlaced = 0; //+1 for each hand played, and +1 anytime a hand is doubled, or split
    int betSum = 0; //total sum of bets places
    int greatestHand = 0; //greatest single hand win

    int greatestWin = 0; //cumulative win for the whole shoe
    int greatestLoss = 0; //cumulative loss for the whole shoe

    int greatestSessionWin = 0; //cumulative win for all shoes in program session 
    int greatestSessionLoss = 0; //cumulative loss for all shoes in program session 
};

struct HandOutcome
{
    std::vector<Card> cards;
    std::vector<std::string> actions;
    int wager = 0;
    int payout = 0;
    bool won = false;
};

struct RoundRecord
{
    std::vector<Card> dealerCards;
    std::vector<HandOutcome> playerHands;

    int runningCount = 0;
    float trueCount = 0;
    int cardsRemaining = 0;

    long long timestamp = 0;

    int totalWager() const;
    int totalPayout() const;
    int netProfit() const;
    bool overallWin() const;
};

struct ShoeRecord
{
    std::vector<Card> initialDeck;
    std::vector<RoundRecord> rounds;
    
    bool completed = true; // true if cut card reached, false if player quit mid-shoe
};

// Inline implementations for RoundRecord computed methods
inline int RoundRecord::totalWager() const
{
    int sum = 0;
    for (const auto& hand : playerHands)
        sum += hand.wager;
    return sum;
}

inline int RoundRecord::totalPayout() const
{
    int sum = 0;
    for (const auto& hand : playerHands)
        sum += hand.payout;
    return sum;
}

inline int RoundRecord::netProfit() const
{
    return totalPayout() - totalWager();
}

inline bool RoundRecord::overallWin() const
{
    return netProfit() >= 0;
}

// Reconstructs the deck state at the start of a specific round
// by removing all cards dealt in rounds [0, roundIndex)
inline std::deque<Card> reconstructDeckAtRound(
    const ShoeRecord& shoe, 
    size_t roundIndex)
{
    std::deque<Card> deck(shoe.initialDeck.begin(), shoe.initialDeck.end());

    for (size_t r = 0; r < roundIndex; ++r)
    {
        const auto& round = shoe.rounds[r];

        // Remove dealer cards
        for (size_t i = 0; i < round.dealerCards.size(); ++i)
            deck.pop_front();

        // Remove each player hand's cards
        for (const auto& hand : round.playerHands)
        {
            for (size_t i = 0; i < hand.cards.size(); ++i)
                deck.pop_front();
        }
    }

    return deck;
}
