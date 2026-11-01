#pragma once

// Feature (Game.cpp god-class split — Follow-up #8 "Settlement/deal()" stage): the round-dealing + side-bet settlement
// logic moved out of the Game god class. Game owns one as a value member and delegates deal()/resolveSideBets(); the
// component holds a reference to Game (mirroring HandEvaluator) so it can drive the real deck/count/state, while all
// mutable game state stays in Game. This keeps deal + side-bet settlement byte-for-byte identical to before the split
// and makes each path exercisable headlessly through Game's delegating methods. Forward-declared Game matches the
// HandEvaluator pattern: only a reference is held here, so the complete type is needed solely in RoundDealer.cpp.
class Game;

class RoundDealer
{
  public:
    RoundDealer(Game& game);

    // Deal the initial two player cards + dealer up/hole card from the shoe, apply + resolve the human seat's pending
    // side-bet choice (Feature 1), and advance roundState to PlayerTurn (or OfferInsurance if the dealer shows an Ace).
    void deal();

    // Evaluate + bankroll every seat's side bet once all cards are known, BEFORE any main-hand settlement. Called from
    // deal() right after the hole card. Never invokes a player action.
    void resolveSideBets();

  private:
    Game& game;
};
