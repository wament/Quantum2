#include "HandEvaluator.hpp"
#include "Game.hpp"
#include "Constants.hpp"
#include "BasicStrategy.hpp"

#include <sstream>
#include <chrono>
#include <algorithm>
#include "Logger.hpp"

HandEvaluator::HandEvaluator(Game& game) : game(game) {}

void HandEvaluator::evaluateHands()
{
    // The round streak is per-shoe (Game::shoe_) and human-seat-only: roundWon/roundLost below are set from seat
    // 0's hands alone, so an NPC's result can never flip the Streak box.
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
        // Every counter in this loop belongs to the seat being settled. It used to be taken once, outside the
        // loop, from players[0] — so each NPC win/loss/push was being written into the human player's record.
        PlayerStats& stats = p.getStats();
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

            // Feature 1 (Side Bets): record this hand's resolved side bet so replay/analytics can see it. Net contribution
            // to a round is sideBetPayout - sideBetWager (folded into humanNet/playerNet below).
            outcome.sideBetKind = hand.getSideBetKind();
            outcome.sideBetWager = hand.getSideBetWager();
            outcome.sideBetPayout = hand.getSideBetPayout();

            stats.handsFinished++;
            int payout = 0;
            // dealer.isBlackjack() is only true for the dealer's original two cards, so by the time the
            // dealer has drawn to 21 it is false and a player natural is paid 3:2 instead of pushing.
            SettlementResult result = p.settle(hand, game.dealer.getValue(), game.dealer.isBust(),
                                               game.dealer.isBlackjack(), payout);
            outcome.payout = payout;
            outcome.won = (result == SettlementResult::Win || result == SettlementResult::Push);

            // Surrender/Lose/Win share a near-identical body (counter + human-seat streak flag + log line). Route the
            // per-result counter and streak flag by outcome: Surrender+Lose both count as a loss / roundLost, Win as a
            // win. The streak box is the human seat's rounds only, so an NPC result never extends or flips it (below).
            if (result == SettlementResult::Win)
                stats.wins++;
            else if (result == SettlementResult::Surrender || result == SettlementResult::Lose)
                stats.losses++;
            else
                stats.pushes++; // Push: neither win nor loss.

            if (pIdx == 0 && result != SettlementResult::Push)
            {
                if (result == SettlementResult::Win)
                    roundWon = true;
                else
                    roundLost = true;
            }

            // One log line per logged outcome, byte-for-byte identical to the collapsed cases above: Surrender/Lose use a
            // result-specific verb with no payout; only a Win appends the payout. Push has no log line.
            const char* verb = "won";
            if (result == SettlementResult::Surrender)
                verb = "surrendered";
            else if (result == SettlementResult::Lose)
                verb = "lost";
            if (result != SettlementResult::Push)
            {
                if (result == SettlementResult::Win)
                    Logger::info() << p.getName() << " " << verb << " hand " << (handIndex + 1)
                                   << " (bet: " << hand.getBet() << ", payout: " << payout << ").";
                else
                    Logger::info() << p.getName() << " " << verb << " hand " << (handIndex + 1)
                                   << " (bet: " << hand.getBet() << ").";
            }

            int netProfitOnHand = payout - hand.getBet();

            // Feature 1 + 3 (human seat only): attribute deviations to their hand and stamp won, count wins after a
            // deviation, maintain per-hand win/lose streaks (+ running max for the dashboard), and feed the variance
            // accumulators used to derive standard deviation. NPCs are tracked separately via recordRoundResult().
            if (pIdx == 0)
            {
                size_t gidx = game.getGlobalHandIndexForPlayer(pIdx, handIndex);
                // handDeviations_ is sized per hand by nextRound()/startNewGame(); grow it defensively so a hand
                // that never deviated cannot index past the end (logStrategyDeviation only grows it on a
                // deviation, so an unguarded read here is undefined behaviour otherwise).
                while (game.handDeviations_.size() <= gidx)
                    game.handDeviations_.emplace_back();
                bool hadDeviation = !game.handDeviations_[gidx].empty();
                for (auto& dev : game.handDeviations_[gidx])
                    dev.won = (result == SettlementResult::Win);
                if (hadDeviation && result == SettlementResult::Win)
                    stats.winsAfterDeviation++;

                // Per-hand streaks: the running pair plus this shoe's longest runs, which are what the Long Win /
                // Long Loss boxes show. They live in shoe_, so a run never crosses a shoe boundary; the
                // PlayerStats copies stay career-long for the saved profile.
                if (result == SettlementResult::Win)
                {
                    ++game.shoe_.currentWinStreak;
                    game.shoe_.currentLoseStreak = 0;
                }
                else if (result == SettlementResult::Lose || result == SettlementResult::Surrender)
                {
                    ++game.shoe_.currentLoseStreak;
                    game.shoe_.currentWinStreak = 0;
                }
                else // a push is neither win nor loss: it breaks both streaks
                {
                    game.shoe_.currentWinStreak = 0;
                    game.shoe_.currentLoseStreak = 0;
                }
                stats.longestWinningStreak = std::max(stats.longestWinningStreak, game.shoe_.currentWinStreak);
                stats.longestLosingStreak = std::max(stats.longestLosingStreak, game.shoe_.currentLoseStreak);
                game.shoe_.longestWinningStreak = std::max(game.shoe_.longestWinningStreak, game.shoe_.currentWinStreak);
                game.shoe_.longestLosingStreak = std::max(game.shoe_.longestLosingStreak, game.shoe_.currentLoseStreak);

                long long net = static_cast<long long>(netProfitOnHand);
                stats.handProfitCount++;
                stats.handProfitSum += net;
                stats.handProfitSumSq += net * net;
                if (net < stats.greatestHandLoss)
                    stats.greatestHandLoss = net;

                // The Std Dev box derives from these per-shoe accumulators, so it measures this shoe's swing and
                // not every hand the profile has ever played.
                game.shoe_.handProfitCount++;
                game.shoe_.handProfitSum += net;
                game.shoe_.handProfitSumSq += net * net;
            }

            game.currentRoundRecord.playerHands.push_back(outcome);

            // Record result for NPC progressive betting strategies
            if (pIdx > 0)
            {
                p.recordRoundResult(outcome.won);
            }

            if (netProfitOnHand > stats.greatestHand)
                stats.greatestHand = netProfitOnHand;

            // The shoe/session running win-loss (and the records built from them) are the human seat's own. They
            // used to sum the whole table, so NPC results were booked as the player's shoe/session profit.
            if (pIdx == 0)
            {
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
    }

    // Refresh the "last 10 deviations" display now that every record's win/loss outcome is finalized (Feature 1). Only
    // player-seat 0's hands belong to this seat; their records live in handDeviations_ slots [0, p0Hands).
    {
        PlayerStats& dstats = game.getStats();
        dstats.recentDeviations.clear();
        const size_t p0Hands = game.players[0].hands.size();
        for (size_t slot = 0; slot < p0Hands && slot < game.handDeviations_.size(); ++slot)
        {
            for (const auto& dev : game.handDeviations_[slot])
            {
                dstats.recentDeviations.push_back(dev);
                if (dstats.recentDeviations.size() > 10)
                    dstats.recentDeviations.erase(dstats.recentDeviations.begin());
            }
        }
    }

    // Round-to-round streak for the Streak box. Human seat only (roundWon/roundLost ignore the NPCs) and per-shoe
    // (shoe_ is zeroed at every boundary), so it reads as your current run in this shoe rather than a session tally
    // that a reshuffle never interrupts.
    if (roundWon && !roundLost)
    {
        if (game.shoe_.roundStreakIsWin)
            game.shoe_.roundStreak++;
        else
        {
            game.shoe_.roundStreak = 1;
            game.shoe_.roundStreakIsWin = true;
        }
    }
    else if (roundLost && !roundWon)
    {
        if (!game.shoe_.roundStreakIsWin)
            game.shoe_.roundStreak++;
        else
        {
            game.shoe_.roundStreak = 1;
            game.shoe_.roundStreakIsWin = false;
        }
    }
    else
    {
        game.shoe_.roundStreak = 0;
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
            playerNet += outcome.sideBetPayout - outcome.sideBetWager; // Feature 1: side-bet net (independent of main-hand payout)
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
    // Gamer mode overrides this with its refresh-rate-derived interval instead of a fixed table value.
    game.hudState.summaryTimer = (game.gameSpeed == GameSpeed::Gamer) ? GameTiming::gamerStepInterval(game.refreshRateHz) : GameTiming::summaryDisplayTime(game.gameSpeed);
    game.hudState.afterHandTimer = GameTiming::afterHandWait(game.gameSpeed);

    // The bankroll curve is YOUR profit and loss, so only seat 0's hands feed it. Summing every seat here used to
    // drag the NPCs' wins and losses onto the player's curve.
    int humanNet = 0;
    for (const auto& outcome : game.currentRoundRecord.playerHands)
    {
        if (outcome.playerIndex != 0)
            continue;
        humanNet += outcome.payout - outcome.wager;
        humanNet += outcome.sideBetPayout - outcome.sideBetWager; // Feature 1: side-bet net so the bankroll curve reflects it too
    }

    // Feature 3 analytics: extend the bankroll curve (your cumulative net profit) and sample the edge, one point
    // per completed round. The edge is a LEVEL, not a running total: every round re-estimates the player's edge
    // from the count as it stands right now, because that is what actually happens at a table - the house edge
    // moves with the shoe, it does not stack up hand over hand. Integrating it instead is what made the curve
    // read -13% after ten rounds of an ordinary shoe. Both series live in shoe_, which resetShoeAnalytics()
    // zeroes at every shoe boundary.
    {
        const float lastBank = game.shoe_.bankrollSeries.empty() ? 0.f : game.shoe_.bankrollSeries.back();
        game.shoe_.bankrollSeries.push_back(lastBank + static_cast<float>(humanNet));
        game.shoe_.edgeSeries.push_back(static_cast<double>(BasicStrategy::estimateEdgePercent(game.hiLoTrueCount())));
    }

    Logger::info() << "Round complete. Net: " << (humanNet >= 0 ? "+" : "") << humanNet
                   << ". Bankroll: " << game.players[0].getBankroll() << ".";

    // Feature 5: feed this settled round into the achievement engine (sets a HUD popup for any just-unlocked achievement)
    // before persisting. Persistence stays additive — the manager's unlocked set rides along on the same profile save path.
    game.recordHandAchievements(game.currentRoundRecord);

    PlayerProfile profile = game.players[0].getProfile();
    // Feature 5: stamp the current unlocked-achievement ids onto the profile so they survive (additive schema).
    game.achievementManager_.persistUnlockedTo(profile.unlockedAchievementIds);
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

            // Insurance pays 2:1 and settles independently of the hand's win/loss, so it is paid whenever
            // it was taken. An own natural blackjack then just pushes versus a dealer blackjack.
            //
            // The hand's own wager is NOT moved here. The dealer's natural still runs the round through
            // DealerTurn -> evaluateHands, and settle() already scores it (loss, or push against the
            // player's own natural). Charging it in both places used to take the wager twice.
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
                    p.getStats().blackjacks++;
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