#pragma once

#include <string>
#include <vector>

#include "Card.hpp"
#include "Hand.hpp"

// Independent tournament RULES controller consulted by the shared regular-play machinery (Game) -- it owns everything that
// makes a tournament different from an ordinary shoe, and nothing else. It deliberately holds NO deck, hands or settlement
// of its own: those stay in Game's normal play path so a tournament plays exactly like an ordinary game (the human seat at
// index 0 makes real decisions via the same input as normal play) with only these rules layered on. Because all logic here
// is pure and headless-testable there is no SFML draw() -- presentation lives in GameRenderer overlays.
//
// What it enforces: a fixed number of stages, each stage being its own self-contained four-deck shoe, a minimum bet that
// escalates every stage (the "marathon" pressure), an aggressive tournament-modified basic strategy the AI opponents play
// to try to maximise their bankroll in the short window (split tens, double any hard eight -- the standard advisor is left
// untouched for everyone else), and a bankroll-ranked leaderboard plus winner derivation. The goal stays "largest bankroll
// at the end of the tournament wins".
class Tournament
{
  public:
    // Configuration for a run. Defaults are a playable six-stage table, each stage its own four-deck shoe, starting at a
    // ten-unit minimum that grows five units per stage. Equal starting chips for every seat and the opponent names come
    // from Game when it builds the seats; this struct carries only the tournament structure + betting schedule.
    struct TournamentConfig
    {
        int startingChips = 1000;
        int decksPerShoe = 4;   // each stage is one self-contained shoe
        int stages = 6;         // number of stages (shoes) in the tournament
        int minBetStage1 = 10;  // minimum bet at stage 1
        int minBetGrowth = 5;   // per-stage increase of the minimum bet
    };

    Tournament() = default;

    // Begin (or restart) a tournament from `config`: reset to stage 1 with no shoes played and completion cleared.
    void start(const TournamentConfig& config);

    // The book play the AI opponents should take on a hand vs the dealer's up-card in a tournament: standard basic strategy
    // with two deliberate tournament aggressions applied so seats try to grow their bankroll in the short window -- always
    // split tens (a hard 20) and always double a hard eight, regardless of the dealer's up-card. Every other hand follows
    // the ordinary advisor unchanged. Returns the same action letter Game's NPC path consumes ('H','D','S','P','R'). This is
    // private to the tournament (the plain BasicStrategy advisor stays as-is elsewhere) so it never leaks into normal play.
    char aiAdvice(const Hand& hand, const Card& dealerUpCard) const;

    // Structural read-outs. currentStage() is 1-based ("Stage X of N"); minBetForStage() clamps to the range and never goes
    // below the stage-1 minimum. shoesPlayed counts finished stages; isComplete() flips true once the final stage has run out.
    int stages() const { return config_.stages; }
    int currentStage() const { return stage_; }
    int decksPerShoe() const { return config_.decksPerShoe; }
    int minBetForStage(int stage) const;
    int shoesPlayed() const { return shoesPlayed_; }
    bool isComplete() const { return complete_; }

    // Advance to the next stage after a stage's shoe has finished. Returns true when another stage remains and false once the
    // final stage has run out (the tournament is then complete). Called by Game at every shoe boundary while tournamentActive_.
    bool advanceStage();

    // Rank seats for the leaderboard / winner: indices of `bankrolls` sorted by chips descending, ties broken by original
    // seat order so the ordering is stable and deterministic. Used both live (during play) and to name the tournament winner.
    std::vector<std::size_t> rankOrder(const std::vector<std::string>& names, const std::vector<int>& bankrolls) const;

    // --- Test accessors (mirror the *ForTest() convention) ---
    TournamentConfig configForTest() const { return config_; }
    int currentStageForTest() const { return stage_; }
    bool isCompleteForTest() const { return complete_; }
    int minBetForStageForTest(int stage) const { return minBetForStage(stage); }
    int shoesPlayedForTest() const { return shoesPlayed_; }

  private:
    TournamentConfig config_{};
    int stage_ = 1;
    int shoesPlayed_ = 0;
    bool complete_ = false;
};
