#include "HandEvaluator.hpp"
#include "Game.hpp"
#include "Constants.hpp"
#include "BasicStrategy.hpp"

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
            int payout = 0;
            SettlementResult result = p.settle(hand, game.dealer.getValue(), game.dealer.isBust(), payout);
            outcome.payout = payout;
            outcome.won = (result == SettlementResult::Win || result == SettlementResult::Push);

            switch (result)
            {
            case SettlementResult::Surrender:
                stats.losses++;
                Logger::info() << p.getName() << " surrendered hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ").";
                break;
            case SettlementResult::Lose:
                stats.losses++;
                roundLost = true;
                Logger::info() << p.getName() << " lost hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ").";
                break;
            case SettlementResult::Win:
                stats.wins++;
                roundWon = true;
                Logger::info() << p.getName() << " won hand " << (handIndex + 1) << " (bet: " << hand.getBet() << ", payout: " << payout << ").";
                break;
            case SettlementResult::Push:
                stats.pushes++;
                break;
            }

            int netProfitOnHand = payout - hand.getBet();

            game.currentRoundRecord.playerHands.push_back(outcome);

            // Feature 1: credit a "win after deviation" when a hand that deviated from basic strategy still won.
            if (hand.isDeviated() && outcome.won)
                stats.strategyDeviationWins++;

            // Record result for NPC progressive betting strategies
            if (pIdx > 0)
            {
                p.recordRoundResult(outcome.won);
            }

            if (netProfitOnHand > stats.greatestHand)
                stats.greatestHand = netProfitOnHand;

            // Feature 3: per-hand variance / standard-deviation tracking and worst single-hand loss.
            double np = static_cast<double>(netProfitOnHand);
            stats.handProfitSum += np;
            stats.handProfitSqSum += np * np;
            if (np < static_cast<double>(stats.greatestSingleHandLoss))
                stats.greatestSingleHandLoss = netProfitOnHand;

            // Feature 3: human-only cumulative profit, used to estimate edge over time.
            if (pIdx == 0)
                game.sessionHumanProfit += netProfitOnHand;

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

    // Feature 3: record the longest consecutive win/loss streak seen so far, driven by this round's streak.
    if (game.hudState.streak > stats.longestWinningStreak && game.hudState.streakIsWin)
        stats.longestWinningStreak = game.hudState.streak;
    else if (game.hudState.streak > stats.longestLosingStreak && !game.hudState.streakIsWin)
        stats.longestLosingStreak = game.hudState.streak;

    // Feature 3: sample the human bankroll once per finished round and an edge-tracking point (true count vs
    // cumulative human profit). Both are capped so a very long session cannot bloat the saved profile.
    if (stats.bankrollCurve.size() < Constants::ANALYTICS_POINT_CAP)
        stats.bankrollCurve.push_back(game.players[0].getBankroll());

    AnalyticsEdgeSample edgeSample;
    edgeSample.roundIndex = static_cast<int>(game.analyticsRoundIndex++);
    edgeSample.trueCount = game.currentRoundRecord.trueCount;
    edgeSample.cumulativeProfit = static_cast<double>(game.sessionHumanProfit);
    if (stats.edgeHistory.size() < Constants::ANALYTICS_POINT_CAP)
        stats.edgeHistory.push_back(edgeSample);

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
    // End-of-round summary display time and AutoPlay inter-hand hold both come from the single timing table
    // (GameTiming), keyed by speed. Non-AutoPlay speeds wait 0s between hands, so there is no artificial delay.
    game.hudState.summaryTimer = GameTiming::summaryDisplayTime(game.gameSpeed);
    game.hudState.afterHandTimer = GameTiming::afterHandWait(game.gameSpeed);

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