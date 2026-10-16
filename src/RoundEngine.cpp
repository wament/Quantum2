#include "RoundEngine.hpp"
#include "BasicStrategy.hpp"
#include "Game.hpp"
#include "Logger.hpp"

// State-machine step moved from Game::update(); every access below is qualified with the
// passed-in `game` because those members/methods are private to Game; Game grants RoundEngine
// friendship so this direct access compiles and behaves identically.

void RoundEngine::tick(float deltaTime, Game& game)
{
    // Shared timer countdown for the three HUD timers.
    game.tickTimer(game.hudState.summaryTimer, deltaTime);
    game.tickTimer(game.hudState.afterHandTimer, deltaTime);
    game.tickTimer(game.hudState.afterShoeTimer, deltaTime);

    // Auto-play: auto-advance to new shoe after shoe delay
    if (game.gameState == GameState::GameOver && game.gameSpeed == GameSpeed::AutoPlay)
    {
        if (game.hudState.afterShoeTimer <= 0.0f)
        {
            Logger::info() << "Auto-play: starting new shoe.";
            // startNewShoe() is the shared shoe boundary: it rebuilds + shuffles the shoe, starts a fresh shoe
            // record, and runs resetShoeAnalytics() (shoeGeneration++, clears the bankroll/edge series, re-seeds the
            // running count for the active style). This path used to hand-roll only part of that — it left the edge
            // series and the recalibration signal untouched, so the Edge % readout and the Edge Over Time chart kept
            // the previous shoe's drift instead of returning to baseline.
            game.startNewShoe();
            game.nextRound();
        }
    }

    // Auto-play: auto-bet with Kelly strategy
    if (game.roundState == RoundState::PlaceBets && game.gameSpeed == GameSpeed::AutoPlay)
    {
        float tc = game.activeTrueCount();
        int optimal = game.calculateOptimalBet(tc, GameConfig::KELLY_FRACTION, game.players[0].getBankroll(),
                                               Constants::MINIMUM_BET, Constants::MAXIMUM_BET);
        game.players[0].hands[0].setBet(optimal);
        Logger::info() << "Auto-play betting " << optimal << " (TC=" << tc << ")";
        game.confirmBet();
    }

    if (game.roundState == RoundState::OfferInsurance)
    {
        game.offerInsurance();
    }

    if (game.roundState == RoundState::PlayerTurn)
    {
        // Check if all players are done (check ALL hands, not just current)
        bool allDone = true;
        for (auto& p : game.players)
        {
            // Skip players who didn't bet
            if (p.hands[0].getBet() == 0)
                continue;

            for (auto& hand : p.hands)
            {
                if (!hand.isFinished())
                {
                    allDone = false;
                    break;
                }
            }
            if (!allDone)
                break;
        }
        if (allDone)
        {
            game.roundState = RoundState::DealerTurn;
            return;
        }

        // Advance active player to next unfinished hand or next player
        while (game.activePlayerIndex < game.players.size())
        {
            auto& p = game.players[game.activePlayerIndex];
            // Skip players who didn't bet (sitting out)
            if (p.hands[0].getBet() == 0)
            {
                ++game.activePlayerIndex;
                continue;
            }
            // Skip finished hands
            while (p.currentHand < p.hands.size() && p.hands[p.currentHand].isFinished())
            {
                ++p.currentHand;
            }
            // If this player has unfinished hands, process them
            if (p.currentHand < p.hands.size())
            {
                break;
            }
            ++game.activePlayerIndex; // next player
        }

        if (game.activePlayerIndex >= game.players.size())
        {
            game.roundState = RoundState::DealerTurn;
            return;
        }

        auto& p = game.players[game.activePlayerIndex];

        // Each split hand must reach two cards before it stands; a split ace takes exactly one more card
        // then auto-finishes unless the rules allow re-splitting aces.
        if (p.hands[p.currentHand].size() == 1)
        {
            game.dealCardToPlayer(p.hands[p.currentHand]);
            if (p.hands[p.currentHand].isSplitAce())
            {
                // Re-split only when permitted and another split slot is open
                bool canResplit = Constants::RESPLIT_ACES_ALLOWED &&
                                  p.hands[p.currentHand].canSplit() &&
                                  p.hands.size() < Constants::MAX_SPLIT_HANDS;
                if (!canResplit)
                {
                    p.hands[p.currentHand].setFinished(true);
                }
            }
        }

        if (game.activePlayerIndex > 0 || game.gameSpeed == GameSpeed::AutoPlay)
        {
            // NPC pacing is taken from the single timing table (GameTiming), keyed by the selected speed, so
            // it can't drift apart from the dealer/feedback timings tuned in the same place.
            game.npcActionDelay = GameTiming::npcActionDelay(game.gameSpeed);

            // Use timer to delay NPC actions
            game.npcActionTimer -= deltaTime;
            if (game.npcActionTimer <= 0.0f)
            {
                game.npcTakeTurn();
                game.npcActionTimer = game.npcActionDelay;
            }
        }
        else
        {
            // Human player - no delay
            game.npcActionTimer = game.npcActionDelay;
        }
    }

    if (game.roundState == RoundState::DealerTurn)
    {
        game.dealerTurn();
    }

    if (game.roundState == RoundState::Cleanup && game.gameSpeed == GameSpeed::AutoPlay)
    {
        if (game.gameState != GameState::GameOver && game.hudState.afterHandTimer <= 0.0f)
        {
            game.nextRound();
        }
    }
}
