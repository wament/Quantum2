#include "RoundDealer.hpp"
#include "Game.hpp"
#include "Logger.hpp"   // Logger::info used by the side-bet resolve + deal log lines.

RoundDealer::RoundDealer(Game& game) : game(game) {}

// evaluate + bankroll every seat's side bet once all cards are known. Each side bet resolves against
// the seat's two hand cards plus the dealer's up-card, and it pays out / loses INDEPENDENTLY of the main hand
// for all seats here: before evaluateHands() settles the main hands (deal() calls it right after the hole card).
void RoundDealer::resolveSideBets()
{
    if (game.players.empty() || game.dealer.size() < 1)
        return;

    const Card& dealerUp = game.dealer.getCard(0);
    for (size_t i = 0; i < game.players.size(); ++i)
    {
        auto& p = game.players[i];
        Hand& hand = p.hands[0]; // the main hand carries the side bet; split hands do not in this mode
        if (hand.getSideBetKind() == SideBetKind::None || hand.getSideBetWager() <= 0 || hand.size() < 2)
            continue;

        const Card& c0 = hand.getCard(0);
        const Card& c1 = hand.getCard(1);
        int wager = hand.getSideBetWager();
        SideBetResult result = (hand.getSideBetKind() == SideBetKind::PerfectPairs)
                                   ? SideBetEvaluator::perfectPairs(c0, c1)
                                   : SideBetEvaluator::twentyOneThree(c0, c1, dealerUp);

        PlayerStats& stats = p.getStats();
        if (result.won())
        {
            // Net gain is wager * ratio. The stake was never removed at placement (placeBet only guards)
            // exactly the winnings here; gross payout for replay/analytics is stake returned + winnings.
            p.winBet(wager * result.multiplier);
            hand.setSideBetPayout(wager * (1 + result.multiplier));
            stats.sideBetWins++;
            stats.sideBetProfitSum += static_cast<long long>(wager) * result.multiplier;
            Logger::info() << p.getName() << "'s side bet wins " << result.multiplier << ":1 on $" << wager
                           << " (+" << (wager * result.multiplier) << ").";
        }
        else
        {
            // Nothing qualified: the whole side-bet wager is lost. Gross payout 0 => net -wager, matching the bankroll move.
            p.loseBet(wager);
            hand.setSideBetPayout(0);
            stats.sideBetLosses++;
            stats.sideBetProfitSum -= wager;
            Logger::info() << p.getName() << "'s side bet of $" << wager << " lost.";
        }
    }
}

// Deck / Deal
void RoundDealer::deal()
{
    for (auto& p : game.players)
    {
        if (p.hands[0].getBet() > 0)
        {
            game.dealCardToPlayer(p.hands[0]);
        }
    }
    game.dealCardToDealer(); // dealer's first card: index 0, the face-up UP card (see Hand::getDealerUpCard)
    for (auto& p : game.players)
    {
        if (p.hands[0].getBet() > 0)
        {
            game.dealCardToPlayer(p.hands[0]);
        }
    }
    game.dealCardToDealer(); // dealer's second card: the face-down hole card, revealed on the dealer's turn

    // apply the human seat's pending side-bet choice to its main hand and resolve every placed side
    // bet now: after all cards are known but BEFORE any main-hand settlement. Side bets pay / lose independently of the main
    // hand, so a natural blackjack still resolves its side bet immediately (the round then flows through the BJ or dealer path
    // below). The intent is consumed here so it is never re-applied on the next round.
    if (game.sideBetIntentWager_ > 0 && game.sideBetIntentKind_ != SideBetKind::None)
    {
        game.players[0].hands[0].setSideBet(game.sideBetIntentKind_, game.sideBetIntentWager_);
        game.sideBetIntentKind_ = SideBetKind::None;
        game.sideBetIntentWager_ = 0;
    }
    resolveSideBets();

    game.roundState = RoundState::PlayerTurn;

    if (game.dealer.getDealerUpCard().isAce())
    {
        Logger::info("Dealer shows an Ace. Insurance offered.");
        game.roundState = RoundState::OfferInsurance;
    }
    else
        game.evaluator.evaluateBlackjack();

    Logger::info() << game.players[0].getName() << " has a " << game.players[0].hands[0].getValue()
                   << " against dealer's " << game.dealer.getDealerUpCardToString() << ".";
}
