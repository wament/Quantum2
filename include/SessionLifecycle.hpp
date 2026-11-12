#pragma once

#include "DailyChallenge.hpp"  // Daily::DailyResult + dailyRules() + seedForDateKey(); no Game dependency
#include "Tournament.hpp"  // for Tournament::TournamentConfig in startTournamentSession(). Tournament.hpp has no Game dep

// Feature (Game.cpp god-class split: Follow-up #8 "Session lifecycle" stage): the session entry points and table-settings
// persistence moved out of the Game god class. Game owns one as a value member and delegates startNewGame()/startTrainerSession/
// startTournamentSession/startStatsSession/applySavedTableSettings/rememberTableSettings; the component holds a reference to Game
// (mirroring RoundDealer / HandEvaluator)
// stays in Game. This keeps every session entry point byte-for-byte identical to before the split and makes each path exercisable
// headlessly through Game's delegating methods. Forward-declared Game matches the HandEvaluator/RoundDealer pattern: only a
// reference is held here, so the complete type is needed solely in SessionLifecycle.cpp.
class Game;

// Passed by const reference only, so the complete type is needed solely in SessionLifecycle.cpp -- the
// same minimal-dependency shape as `class Game;` above. ShoeRecord is the record both narrow commits
// append to the career profile; PlayerProfile::addShoeRecord() routes it on its `mode` field.
struct ShoeRecord;

class SessionLifecycle
{
  public:
    SessionLifecycle(Game& game);

    // Begin/restart a whole session: build fresh seats + a shuffled shoe, reset per-shoe readouts to zero (bankroll persists), and
    // enter the Playing state. The one method that touches the most Game state; all of it stays in Game
    void startNewGame();

    // Session kinds are mutually exclusive, and each entry point must clear the flags the others set.
    // Called at the top of every entry point that builds a table, BEFORE it sets its own.
    //
    // This is B25. `tournamentActive_` was set true by startTournamentSession() and cleared by nothing
    // except the Daily's own entry point, so tournament -> quit to menu -> Play Game ran the "ordinary"
    // game with tournament AI advice (Game.cpp:1432), the tournament's escalating minimum bet, and
    // finishTournamentShoe() at the shoe boundary. The Daily's flag has the identical shape, so the fix
    // is the rule rather than two patches.
    void beginExclusiveSession();

    // begin/restart a training session. Guards against nullptr
    // builds the trainer, can't crash when the menu calls this.
    void startTrainerSession(int deckCount, int secondsPerSession);

    // begin/restart a tournament. Guards against nullptr
    // entry it allocates the tournament state machine.
    void startTournamentSession(const Tournament::TournamentConfig& config);

    // begin/restart the dashboard from the main menu by flattening completed-shoe history plus any
    // current-shoe rounds into one round-history list and feeding it to the dashboard with the player's career stats. Guards against nullptr.
    void startStatsSession();

    // Begin a Daily Challenge shoe for `dateKey`: the fixed rule set, a shoe seeded from the date, the
    // fixed starting bankroll, exactly one seat, and markPersistable(false). The seat is ephemeral for
    // three separate reasons, each of which is a leak if it persists: the Daily's fixed chips are not
    // the career bankroll; the run's stats/history are not career progress; and rememberTableSettings()
    // stamps gameRules_ onto the profile at every save point, so a persisting Daily seat would
    // overwrite the player's CUSTOM rules with the Daily's fixed ones.
    void startDailySession(const std::string& dateKey, bool practice);

    // Book a finished Daily on the career profile ON DISK: load it, commit the result, append the shoe
    // record, save it. This is a read-modify-write of the real file, deliberately not a seat save --
    // the seat is ephemeral and cannot write, and this path touches the Daily fields and nothing else.
    // addShoeRecord() routes the record to the Daily history on its mode, so career stats are untouched.
    void commitDailyResult(const Daily::DailyResult& result, const ShoeRecord& shoe);

    // Record one finished tournament stage shoe on the career profile, same narrow read-modify-write.
    // `finishedStage` is the stage that just ran out (the tournament's bestFinish is a max over it),
    // and `completed` marks the run as finished once the final stage has.
    void commitTournamentShoe(const ShoeRecord& shoe, int finishedStage, bool completed);

    // Restore saved table settings (player count / speed / counting style + custom rules) from the human seat's profile onto live state.
    void applySavedTableSettings();

    // Stamp the current table choices (count / speed / rules + sound prefs) back into the human seat's profile
    // written on every existing save point with no extra I/O. Additive schema: unchanged for profiles saved before custom rules.
    void rememberTableSettings();

  private:
    Game& game;
};
