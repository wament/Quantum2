#pragma once

#include "Tournament.hpp"  // for Tournament::TournamentConfig in startTournamentSession(). Tournament.hpp has no Game dep, so this adds no cycle.

// Feature (Game.cpp god-class split — Follow-up #8 "Session lifecycle" stage): the session entry points and table-settings
// persistence moved out of the Game god class. Game owns one as a value member and delegates startNewGame()/startTrainerSession/
// startTournamentSession/startStatsSession/applySavedTableSettings/rememberTableSettings; the component holds a reference to Game
// (mirroring RoundDealer / HandEvaluator) so it can drive the real player/deck/profile/shoe state, while all mutable game state
// stays in Game. This keeps every session entry point byte-for-byte identical to before the split and makes each path exercisable
// headlessly through Game's delegating methods. Forward-declared Game matches the HandEvaluator/RoundDealer pattern: only a
// reference is held here, so the complete type is needed solely in SessionLifecycle.cpp.
class Game;

class SessionLifecycle
{
  public:
    SessionLifecycle(Game& game);

    // Begin/restart a whole session: build fresh seats + a shuffled shoe, reset per-shoe readouts to zero (bankroll persists), and
    // enter the Playing state. The one method that touches the most Game state; all of it stays in Game so behavior is identical.
    void startNewGame();

    // Feature 2 (Strategy Trainer): begin/restart a training session. Guards against nullptr so headless construction, which never
    // builds the trainer, can't crash when the menu calls this.
    void startTrainerSession(int deckCount, int secondsPerSession);

    // Feature 9 (Tournament Mode): begin/restart a tournament. Guards against nullptr so headless construction can't crash; on first
    // entry it allocates the tournament state machine.
    void startTournamentSession(const Tournament::TournamentConfig& config);

    // Feature 4 (Statistics Dashboard): begin/restart the dashboard from the main menu by flattening completed-shoe history plus any
    // current-shoe rounds into one round-history list and feeding it to the dashboard with the player's career stats. Guards against nullptr.
    void startStatsSession();

    // Restore saved table settings (player count / speed / counting style + custom rules) from the human seat's profile onto live state.
    void applySavedTableSettings();

    // Stamp the current table choices (count / speed / rules + sound prefs) back into the human seat's profile so they survive a session,
    // written on every existing save point with no extra I/O. Additive schema: unchanged for profiles saved before custom rules.
    void rememberTableSettings();

  private:
    Game& game;
};
