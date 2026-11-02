#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

#include "Deck.hpp"
#include "Hand.hpp"
#include "Card.hpp"

// a self-contained basic-strategy training mode. The trainer presents a player hand
// plus the dealer's up-card and asks the player to call the play basic strategy prescribes; it grades each answer
// against BasicStrategy::getRecommendedAction (the same primitive Game::giveBasicStrategyAdvice wraps), tracks
// accuracy / streaks, and can bound a session by wall-clock time.
//
// All grading + stats are pure logic (no SFML draw)
// presentation lives in draw(). This mirrors Replay/SoundManager: it is its own state machine that Game constructs
// and dispatches through AppMode::StrategyTrainer, not ad-hoc booleans. It deliberately does NOT touch PlayerProfile
// (persistence belongs to later features), so the JSON schema stays untouched here.
class StrategyTrainer
{
  public:
    // Action letters the trainer grades against: exactly what BasicStrategy::getRecommendedAction returns ('H','D',
    // 'S','P','R'). Surrender is 'R'; the game's surrender key is 'Y' and normalizes to it
    enum class Action : char
    {
        Hit = 'H',
        DoubleDown = 'D',
        Stand = 'S',
        Split = 'P',
        Surrender = 'R'
    };

    // Session tallies. accuracyPercent() is guarded against divide-by-zero (0.0 before the first answer).
    struct SessionStats
    {
        int totalQuestions = 0;
        int correctAnswers = 0;
        int currentStreak = 0;
        int bestStreak = 0;

        float accuracyPercent() const { return totalQuestions > 0 ? (100.0 * correctAnswers / totalQuestions) : 0.0f; }
        bool isPerfect() const { return totalQuestions > 0 && correctAnswers == totalQuestions; }
    };

    StrategyTrainer();

    // Start (or restart) a session from a fresh shoe of `deckCount` decks. A positive `secondsPerSession` bounds the
    // session by wall-clock time via update(); <=0 means untimed: it ends only when the shoe runs out.
    void startSession(int deckCount = 6, int secondsPerSession = 0);

    // Present a specific player hand + dealer up-card as the current question (tests inject known hands here; the
    // game's auto-generation path calls nextQuestion() instead). Recomputes the book's action and returns it.
    char loadQuestion(const Hand& playerHand, const Card& dealerUpCard);

    // Grade the player's chosen action against the current question's correct play. Advances to the next hand
    // automatically (unless the shoe is exhausted). Returns true when the answer matched basic strategy; a no-op
    // while no live question exists. Surrender's in-game key ('Y') normalizes to 'R'
    bool submitAction(char chosen);
    bool submitAction(Action chosen) { return submitAction(static_cast<char>(chosen)); }

    // Frame update: advances the wall-clock timer for timed sessions only (pure logic; draw() owns all rendering).
    void update(float deltaTime);

    // Generate + deal the next hand from the shoe and compute its book action. Auto-ends the session when fewer than
    // three cards remain (not enough to build a player hand + dealer up-card). Public
    // deterministically and assert it agrees with the advisor on whatever hands were actually dealt.
    void nextQuestion();

    // --- Test accessors (mirror the *ForTest() convention) ---
    SessionStats statsForTest() const { return stats; }
    bool sessionActiveForTest() const { return active; }
    bool sessionCompleteForTest() const { return complete; }
    int deckCountForTest() const { return deckCount; }
    float elapsedSecondsForTest() const { return elapsedSeconds; }
    // A timed session is complete once its wall-clock budget has elapsed; an untimed one never reports completion here.
    bool timedSessionCompleteForTest() const { return timed && elapsedSeconds >= static_cast<float>(secondsPerSession); }
    int remainingCardsForTest() const { return static_cast<int>(deck.size()); }
    char currentCorrectActionForTest() const { return currentCorrect; }
    const Hand& questionHandForTest() const { return questionHand; }
    const Hand& questionDealerUpForTest() const { return questionDealer; }
    const std::string& lastVerdictForTest() const { return lastVerdict; }

    // --- Presentation (graphics-only, no game-state mutation) ---
    // cardTexture is the shared tilemap already loaded by Game (see Game.cpp), passed in
    // free of a Game reference -- draw() never mutates any game state.
    void draw(sf::RenderWindow& window, const sf::Texture& cardTexture) const;

    // Fixed left-to-right order of the five action buttons drawn by draw(); also their keyboard shortcuts (H/D/S/P/R).
    // Shared by draw(), buttonIndexAt() and InputHandler
    static std::vector<Action> buttonActions();

    // Geometry for the five action buttons given current window size
    // buttonIndexAt() so click boxes match what's drawn exactly. Pure layout, no game state read or written.
    std::vector<sf::Rect<float>> buttonRects(float w, float h) const;

    // Index of the action button under window-pixel (x,y), or -1 if outside every button. Hit-test backing InputHandler mouse clicks.
    int buttonIndexAt(float x, float y, float w, float h) const;

  private:
    Deck deck;
    int deckCount = 6;

    bool active = false;   // a session is running (answers being graded)
    bool timed = false;    // session bounded by wall-clock time
    int secondsPerSession = 0;
    float elapsedSeconds = 0.0f;
    bool complete = false; // untimed: shoe exhausted; or timed: timer elapsed

    Hand questionHand;   // the player's two-card hand for the current question
    Hand questionDealer; // just the dealer's up-card so getRecommendedAction reads the right up-value
    char currentCorrect = 'S';
    std::string lastVerdict;

    SessionStats stats{};

    void computeBookAction();
};