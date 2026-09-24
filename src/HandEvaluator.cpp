#include "HandEvaluator.hpp"
#include "Game.hpp"
#include "Constants.hpp" 

#include <iostream>
#include <sstream>
#include <chrono>

HandEvaluator::HandEvaluator(Game& game) : game(game) {}

void HandEvaluator::evaluateHands()
{
    PlayerStats& stats = game.getStats();

    bool roundWon = false;
    bool roundLost = false;

    game.currentRoundRecord.dealerCards.clear();
    for (size_t i = 0; i < game.dealer.size(); i++)
        game.currentRoundRecord.dealerCards.push_back(game.dealer.getCard(i));

    game.currentRoundRecord.playerHands.clear();
    game.currentRoundRecord.runningCount = game.runningCount;
    game.currentRoundRecord.trueCount = BasicStrategy::computeTrueCount(game.runningCount, game.deck.size());
    game.currentRoundRecord.cardsRemaining = game.deck.size();
    game.currentRoundRecord.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                           std::chrono::system_clock::now().time_since_epoch())
                                           .count();

    for (size_t pIdx = 0; pIdx < game.players.size(); pIdx++)
    {
        auto& p = game.players[pIdx];
        for (size_t handIndex = 0; handIndex < p.hands.size(); handIndex++)
        {
            const Hand& hand = p.hands[handIndex];

            std::cout << "Entering evaluateHands() on hand " << (handIndex + 1)
                      << " for " << p.getName() << ". Bankroll = " << p.getBankroll() << std::endl;

            HandOutcome outcome;
            outcome.wager = hand.getBet();
            outcome.playerIndex = static_cast<int>(pIdx);
            outcome.actions = game.handActions[game.getGlobalHandIndexForPlayer(pIdx, handIndex)];
            for (size_t c = 0; c < hand.size(); c++)
                outcome.cards.push_back(hand.getCard(c));

            stats.handsFinished++;
            int netProfitOnHand = 0;
            bool isNaturalBlackjack = hand.isBlackjack() && !hand.isSplit();

            if (hand.isSurrendered())
            {
                p.winBet(hand.getBet() / 2);
                outcome.payout = hand.getBet() / 2;
                outcome.won = false;
                stats.losses++;
                netProfitOnHand = hand.getBet() / 2 - hand.getBet();
            }
            else if (hand.isBust() || (hand.getValue() < game.dealer.getValue() && !game.dealer.isBust()))
            {
                if (!isNaturalBlackjack) p.loseBet(hand.getBet());
                outcome.payout = 0;
                outcome.won = false;
                stats.losses++;
                netProfitOnHand = -hand.getBet();
                roundLost = true;
            }
            else if (hand.getValue() > game.dealer.getValue() || game.dealer.isBust())
            {
                if (isNaturalBlackjack)
                {
                    p.winBet(hand.getBet() * Constants::BLACKJACK_PAYOUT);
                    outcome.payout = hand.getBet() * Constants::BLACKJACK_PAYOUT;
                }
                else
                {
                    p.winBet(hand.getBet() * 2);
                    outcome.payout = hand.getBet() * 2;
                }
                outcome.won = true;
                stats.wins++;
                netProfitOnHand = outcome.payout - hand.getBet();
                roundWon = true;
            }
            else
            {
                if (!isNaturalBlackjack) p.pushBet(hand.getBet());
                outcome.payout = hand.getBet();
                outcome.won = true;
                stats.pushes++;
                netProfitOnHand = 0;
            }

            game.currentRoundRecord.playerHands.push_back(outcome);

            if (netProfitOnHand > stats.greatestHand)
                stats.greatestHand = netProfitOnHand;

            game.shoeCumulativeWinLoss += netProfitOnHand;
            game.sessionCumulativeWinLoss += netProfitOnHand;

            if (game.shoeCumulativeWinLoss > stats.greatestWin)
                stats.greatestWin = game.shoeCumulativeWinLoss;
            if (game.shoeCumulativeWinLoss < stats.greatestLoss)
                stats.greatestLoss = game.shoeCumulativeWinLoss;

            if (game.sessionCumulativeWinLoss > stats.greatestSessionWin)
                stats.greatestSessionWin = game.sessionCumulativeWinLoss;
            if (game.sessionCumulativeWinLoss < stats.greatestSessionLoss)
                stats.greatestSessionLoss = game.sessionCumulativeWinLoss;

            game.incrementGlobalHandIndex(game.getGlobalHandIndexForPlayer(pIdx, handIndex));
        }
    }

    if (roundWon && !roundLost)
    {
        if (game.hudState.streakIsWin)
            game.hudState.streak++;
        else
        {
            game.hudState.streak = 1;
            game.hudState.streakIsWin = true;
        }
    }
    else if (roundLost && !roundWon)
    {
        if (!game.hudState.streakIsWin)
            game.hudState.streak++;
        else
        {
            game.hudState.streak = 1;
            game.hudState.streakIsWin = false;
        }
    }
    else
    {
        game.hudState.streak = 0;
    }

    game.hudState.playerSummaries.clear();
    for (size_t i = 0; i < game.players.size(); i++)
    {
        std::ostringstream oss;
        int playerNet = 0;
        for (auto& outcome : game.currentRoundRecord.playerHands)
        {
            if (outcome.playerIndex != static_cast<int>(i))
                continue;
            int handProfit = outcome.payout - outcome.wager;
            playerNet += handProfit;
        }
        oss << game.players[i].getName() << ": ";
        if (playerNet > 0)
            oss << "Win +" << playerNet;
        else if (playerNet < 0)
            oss << "Loss " << playerNet;
        else
            oss << "Push";
        game.hudState.playerSummaries.push_back(oss.str());
    }
    game.hudState.summaryTimer = 10.0f;

    std::cout << "Exiting evaluation with " << game.players[0].getBankroll() << std::endl;

    PlayerProfile profile = game.players[0].getProfile();
    profile.addRoundRecord(game.currentRoundRecord);
    game.players[0].updateProfile(profile);
    game.players[0].saveProfile();

    game.currentShoeRecord.rounds.push_back(game.currentRoundRecord);

    game.handActions.clear();
    game.roundState = RoundState::Cleanup;
}

void HandEvaluator::evaluateBlackjack()
{
    auto& p = game.players[game.activePlayerIndex];
    Hand& hand = p.hands[p.currentHand];

    int bet = hand.getBet();

    if (game.dealer.isBlackjack())
    {
        if (hand.getInsuranceBet() > 0)
        {
            p.winBet(hand.getInsuranceBet() * 3);
            std::cout << "Insurance wins!\n";
        }

        if (hand.isBlackjack())
        {
            p.pushBet(bet);
        }
        else
        {
            p.loseBet(hand.getBet());
        }

        game.roundState = RoundState::DealerTurn;
        return;
    }

    if (hand.getInsuranceBet() > 0)
    {
        std::cout << "Insurance lost.\n";
        hand.setInsuranceBet(0);
    }
    else
    {
        game.roundState = RoundState::PlayerTurn;
    }

    if (hand.isBlackjack() && !hand.isSplit())
    {
        std::cout << "Blackjack!\n";
        game.getStats().blackjacks++;
        hand.setFinished(true);
    }
}