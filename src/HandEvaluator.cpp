#include "HandEvaluator.hpp"
#include "Game.hpp"
#include "Constants.hpp" 

#include <sstream>
#include <chrono>
#include "Logger.hpp"

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

            Logger::debug() << "Entering evaluateHands() on hand " << (handIndex + 1)
                            << " for " << p.getName() << ". Bankroll = " << p.getBankroll();

            HandOutcome outcome;
            outcome.wager = hand.getBet();
            outcome.playerIndex = static_cast<int>(pIdx);
            outcome.playerName = p.getName();
            outcome.actions = game.handActions[game.getGlobalHandIndexForPlayer(pIdx, handIndex)];
            for (size_t c = 0; c < hand.size(); c++)
                outcome.cards.push_back(hand.getCard(c));

            stats.handsFinished++;
            int netProfitOnHand = 0;
            // Only a natural (2-card) unsplit blackjack pays 3:2. A split hand that totals 21 is not
            // treated as one, so it settles at even money — the !hand.isSplit() guard drives every branch.
            bool isNaturalBlackjack = hand.isBlackjack() && !hand.isSplit();

            if (hand.isSurrendered())
            {
                p.loseBet(hand.getBet() / 2);
                outcome.payout = hand.getBet() / 2;
                outcome.won = false;
                stats.losses++;
                netProfitOnHand = hand.getBet() / 2 - hand.getBet();
                Logger::info() << p.getName() << " surrendered hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ").";
            }
            else if (hand.isBust() || (hand.getValue() < game.dealer.getValue() && !game.dealer.isBust()))
            {
                if (!isNaturalBlackjack) p.loseBet(hand.getBet());
                outcome.payout = 0;
                outcome.won = false;
                stats.losses++;
                netProfitOnHand = -hand.getBet();
                roundLost = true;
                Logger::info() << p.getName() << " lost hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ").";
            }
            else if (hand.getValue() > game.dealer.getValue() || game.dealer.isBust())
            {
                if (isNaturalBlackjack)
                {
                    p.winBet(hand.getBet() * Constants::BLACKJACK_PAYOUT);
                    outcome.payout = hand.getBet() * (1 + Constants::BLACKJACK_PAYOUT);
                }
                else
                {
                    p.winBet(hand.getBet());
                    outcome.payout = hand.getBet() * 2;
                }
                outcome.won = true;
                stats.wins++;
                netProfitOnHand = outcome.payout - hand.getBet();
                roundWon = true;
                Logger::info() << p.getName() << " won hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ", payout: " << outcome.payout << ").";
            }
            else
            {
                if (!isNaturalBlackjack) {}
                outcome.payout = hand.getBet();
                outcome.won = true;
                stats.pushes++;
                netProfitOnHand = 0;
                Logger::info() << p.getName() << " pushed hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ").";
            }

            game.currentRoundRecord.playerHands.push_back(outcome);

            // Record result for NPC progressive betting strategies
            if (pIdx > 0)
            {
                p.recordRoundResult(outcome.won);
            }

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
        if (playerNet > 0)
            oss << "Win +" << playerNet;
        else if (playerNet < 0)
            oss << "Loss " << playerNet;
        else
            oss << "Push";
        game.hudState.playerSummaries.push_back(oss.str());
    }
    if (game.gameSpeed == GameSpeed::Fast || game.gameSpeed == GameSpeed::AutoPlay)
    {
        game.hudState.summaryTimer = SummaryTiming::FAST_AUTOPLAY;
    }
    else
    {
        game.hudState.summaryTimer = SummaryTiming::NORMAL_SLOW;
    }

    if (game.gameSpeed == GameSpeed::AutoPlay)
    {
        game.hudState.afterHandTimer = Constants::AUTOPLAY_AFTER_HAND_DELAY;
    }

    int roundNet = 0;
    for (const auto& outcome : game.currentRoundRecord.playerHands)
    {
        roundNet += outcome.payout - outcome.wager;
    }
    Logger::info() << "Round complete. Net: " << (roundNet >= 0 ? "+" : "") << roundNet
                  << ". Bankroll: " << game.players[0].getBankroll() << ".";

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
    bool dealerHasBJ = game.dealer.isBlackjack();

    for (size_t pIdx = 0; pIdx < game.players.size(); pIdx++)
    {
        auto& p = game.players[pIdx];
        for (size_t handIndex = 0; handIndex < p.hands.size(); handIndex++)
        {
            Hand& hand = p.hands[handIndex];
            int bet = hand.getBet();

            // Insurance pays 2:1 and settles independently of the hand's win/loss, so it is paid whenever
            // it was taken. An own natural blackjack then just pushes versus a dealer blackjack.
            if (dealerHasBJ)
            {
                if (hand.getInsuranceBet() > 0)
                {
                    p.winBet(hand.getInsuranceBet() * 2);
                    Logger::info() << p.getName() << " insurance wins!";
                }

                if (hand.isBlackjack())
                {
                    Logger::info() << p.getName() << " pushes blackjack vs dealer blackjack.";
                }
                else
                {
                    p.loseBet(hand.getBet());
                    Logger::info() << p.getName() << " loses to dealer blackjack.";
                }
            }
            else
            {
                if (hand.getInsuranceBet() > 0)
                {
                    Logger::info() << p.getName() << " insurance lost.";
                    hand.setInsuranceBet(0);
                }

                if (hand.isBlackjack() && !hand.isSplit())
                {
                    Logger::info() << p.getName() << " has Blackjack!";
                    game.getStats().blackjacks++;
                    hand.setFinished(true);
                }
            }
        }
    }

    if (dealerHasBJ)
    {
        game.roundState = RoundState::DealerTurn;
    }
    else
    {
        game.roundState = RoundState::PlayerTurn;
    }
}