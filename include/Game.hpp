#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

enum class AppMode
{
    MainMenu,
    Playing,
    Paused,
    ReplayHistory,
    ReplayPlaying
};
enum class MenuOption
{
    PlayGame,
    PlayerCount,
    GameSpeed,
    ReviewHistory
};
enum class PauseOption
{
    Resume,
    Restart,
    QuitToMenu
};
#include "Constants.hpp"
#include "BasicStrategy.hpp"
#include "Deck.hpp"
#include "Hand.hpp"
#include "Player.hpp"
#include "GameRecord.hpp"
#include "HUD.hpp"
// Forward-declared: unique_ptr<Replay> needs only an incomplete type here. Game's destructor is
// defined in Game.cpp (which includes Replay.hpp) where the type is complete.
class Replay;
#include "GameRenderer.hpp"
#include "InputHandler.hpp"
#include "HandEvaluator.hpp"

enum class RoundState
{
    PlaceBets,
    OfferInsurance,
    PlayerTurn,
    DealerTurn,
    Cleanup
};

enum class GameState
{
    Started,
    LastHand,
    GameOver
};

class Game
{
  public:
    Game();
    explicit Game(bool headless); // test-only: no SFML window/assets/replay/HUD, pure game state only
    ~Game();

    // ---- Test support: headless construction + deterministic deck injection ----
    // Replace the shoe with an exact card order so tests can deal a fully determined sequence.
    void setDeck(const std::vector<Card>& cards);
    // Set up a single in-memory player with a minimum bet and begin the round (deals the initial two
    // cards plus the dealer up-card from the injected shoe), leaving roundState at PlayerTurn when the
    // dealer's up-card is not an ace. Used to exercise split/hit/stand end-to-end without SFML.
    void startTestRound();
    // Same, but seats up `seats - 1` extra in-memory NPC seats (Alice, Bob, ... from NPC_CONFIGS) behind the human.
    // Used by the analytics tests to prove the HUD readouts follow the human seat alone and ignore the NPCs.
    void startTestRound(int seats);

    const Hand& testHand(size_t playerIndex, size_t handIndex) const; // read a player hand back
    Player& testPlayer(int index = 0) { return players[index]; }      // mutate/read a player (bankroll etc.)
    const Hand& testDealer() const { return dealer; }
    RoundState roundStateForTest() const { return roundState; }
    GameState gameStateForTest() const { return gameState; }
    size_t activePlayerIndexForTest() const { return activePlayerIndex; }
    // Feature 1: expose the advisor verdict set by logStrategyDeviation (the source of the Book box color).
    AdvisorFeedback advisorFeedbackForTest() const { return advisorFeedback; }
    char advisorActionForTest() const { return advisorAction; }

    // Feature 3: the per-shoe analytics + the shoe-recalibration counter, exposed so tests can assert every readout
    // (edge readout, Edge Over Time curve, streaks, std dev, strategy %, bet histogram) actually returns to
    // baseline at a shoe boundary — and that nothing else clears them mid-shoe.
    // updateHUDStateForTest/edgePercentForTest/hudStateForTest read back what the stat boxes show.
    const std::vector<double>& edgeSeriesForTest() const { return shoe_.edgeSeries; }
    const std::vector<float>& bankrollSeriesForTest() const { return shoe_.bankrollSeries; }
    const ShoeAnalytics& shoeAnalyticsForTest() const { return shoe_; }
    int runningCountForTest() const { return runningCount; }
    int backgroundCountForTest() const { return backgroundRunningCount; }
    float hiLoTrueCountForTest() const { return hiLoTrueCount(); }
    const HUDState& hudStateForTest() const { return hudState; }
    int shoeGenerationForTest() const { return shoeGeneration; }
    // Drive the real shoe boundary (the same path the R key and auto-play advance take) from a test.
    void startNewShoeForTest() { startNewShoe(); }
    // Deal the next round at the same table without touching the shoe (what R does between rounds).
    void nextRoundForTest() { nextRound(); }
    void updateHUDStateForTest() { updateHUDState(); }
    double edgePercentForTest() const { return hudState.edgePercent; }

    // Table settings as they stand after the profile was applied at launch.
    int playerCountForTest() const { return playerCount; }
    int gameSpeedForTest() const { return static_cast<int>(gameSpeed); }
    int countingStyleForTest() const { return static_cast<int>(countingStyle); }

    // ---------------- Player Actions (public so the round-state machine and tests can drive them) ----

    void hit();
    void stand();
    bool doubleDown();
    bool split();
    bool surrender();

    void run();

    // Click the count-style label ("Count (Hi-Lo)") to toggle between counting systems. Both are always
    // counted in the background; this swaps which buffer drives the active count and flips the style.
    void toggleCountingStyle();
    float deltaTime;

    friend class GameRenderer;
    friend class InputHandler;
    friend class HandEvaluator;
    friend class RoundEngine;

  private:
    AppMode appMode = AppMode::MainMenu;
    std::unique_ptr<Replay> replay;
    GameRenderer renderer;
    InputHandler inputHandler;
    HandEvaluator evaluator;
    // ---------------- SFML ----------------

    sf::RenderWindow window;

    sf::View gameView;

    sf::Texture backgroundTexture;
    std::unique_ptr<sf::Sprite> backgroundSprite;

    sf::Texture cardTexture;
    sf::Font font;

    // ---------------- UI ----------------

    HUD hud;
    HUDState hudState;

    MenuOption menuSelection = MenuOption::PlayGame;
    PauseOption pauseSelection = PauseOption::Resume;
    GameSpeed gameSpeed = GameSpeed::Normal;
    BasicStrategy::CountingStyle countingStyle = BasicStrategy::CountingStyle::REKO;

    // NPC action timing
    float npcActionTimer = 0.0f;
    float npcActionDelay = 1.0f;

    float dealerStartDelayTimer = 0.0f;
    bool dealerTurnStarted = false;

    int playerCount = GameConfig::DEFAULT_PLAYER_COUNT;

    void setPlayerCount(int count);

    // ---------------- Blackjack ----------------

    Deck deck;

    Hand dealer;

    std::vector<Player> players;
    int activePlayerIndex = 0;

    RoundState roundState = RoundState::PlayerTurn;

    GameState gameState = GameState::Started;

    int initialBet = 0;
    int runningCount = 0;
    // Other counting system's running count, kept current every draw so toggling between systems is instant.
    int backgroundRunningCount = 0;
    float trueCount = 0;

    RoundRecord currentRoundRecord;
    ShoeRecord currentShoeRecord;

    // Per-hand action tracking (parallel to player.hands)
    std::vector<std::vector<std::string>> handActions;

    // ---------------- Stats Tracking ----------------

    int shoeCumulativeWinLoss = 0;    // running win/loss for current shoe
    int sessionCumulativeWinLoss = 0; // running win/loss for entire session

    PlayerStats& getStats()
    {
        return players[0].getStats();
    }

    // Basic Strategy advisor feedback shown on the Book box after each completed player action.
    AdvisorFeedback advisorFeedback = AdvisorFeedback::None;
    char advisorAction = 0; // book's advice letter for the just-completed hand (the green/red target)

    // Per global-hand-index deviation records — source of truth during a round, mirroring handActions' indexing.
    // Popped into by logStrategyDeviation so evaluation can stamp DeviationRecord.won and count winsAfterDeviation.
    std::vector<std::vector<DeviationRecord>> handDeviations_;

    // Everything the HUD stat boxes + left-panel graphs report for the CURRENT shoe only: std-dev accumulators,
    // the hand-to-hand and round-to-round streaks, strategy accuracy, the bet distribution and both graph series.
    // resetShoeAnalytics() zeroes the whole struct, which is why the bankroll is the only value that carries over.
    ShoeAnalytics shoe_;

    // Bumped every time a new shoe begins (new game, R at end of shoe, auto-play, mid-shoe reshuffle). Exposed to
    // the HUD via HUDState::currentShoeId as the per-shoe recalibration signal. Every boundary that bumps it also
    // zeroes shoe_ and re-seeds both running counts (see resetShoeAnalytics).
    int shoeGeneration = 0;

    // ---------------- Setup ----------------

    void startNewGame();

    // Table settings persistence. applySavedTableSettings() seeds playerCount / gameSpeed / countingStyle
    // from the profile loaded at launch, so the main menu opens on what the player last played with.
    // rememberTableSettings() writes the current choices back into the profile; it runs at every point the
    // profile is already being saved, so a settings change survives the session with no extra I/O.
    void applySavedTableSettings();
    void rememberTableSettings();

    // Shoe boundaries. resetShoeAnalytics() is the ONE place a shoe boundary is handled: it bumps the recalibration
    // signal, clears the per-shoe analytics curves (so the edge readout + Edge Over Time chart return to baseline)
    // and re-seeds the running count for the new shoe. startNewShoe() layers a freshly created + shuffled shoe and
    // a new shoe record on top of it. Every path that begins a shoe (new game, R at end of shoe, auto-play, the
    // mid-shoe reshuffle) goes through these, so no boundary can silently skip the reset.
    void resetShoeAnalytics();
    void startNewShoe();

    void loadAssets();

    void nextRound();

    void deal();

    // ---------------- Game Loop ----------------

    void update(float deltaTime);

    void updateHUDState();

    // Shared HUD timer countdown for summary / after-hand / after-shoe timers.
    void tickTimer(float& timerField, float deltaTime);
    // Advance + wrap activePlayerIndex (and evaluate Blackjack) at insurance-loop boundaries.
    void advanceToNextInsurancePlayer();

    // Removed: void draw(); - now in GameRenderer
    // ---------------- Betting ----------------

  public:
    // Place the human's current hand bet and record it for the session bet-distribution histogram. Public (with
    // activeTrueCount() below) so tests can drive a full betting round without SFML.
    void confirmBet();
    // Kelly-based optimal bet calculation (uses kellyFraction)
    int calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const;

    // The true count the betting logic should act on for the active counting system: Reko uses the raw running
    // count ("the true count is the running count"), Hi-Lo normalizes by decks remaining. Drives the human's
    // auto-bet paths (manual Left-arrow bet and AutoPlay) so Reko betting follows its spec tiered pattern.
    float activeTrueCount() const;

    // The Hi-Lo true count regardless of which style is on screen: `runningCount` holds the active system's count
    // and `backgroundRunningCount` the other one (see afterDraw). The edge estimate is calibrated against Hi-Lo
    // true counts, so the Edge % box and the Edge Over Time chart read from this rather than from the displayed
    // count - feeding Reko's raw (unbalanced, offset) count into that calibration would put the readout wildly
    // out of range on the first round of every shoe.
    float hiLoTrueCount() const;

  private:
    void confirmInsurance();

    void declineInsurance();

    // ---------------- Dealer ----------------

    // Central helper: update running count and check cut card after each draw
    void afterDraw(Card card);

    // Collect all cards currently on the table (for reshuffle fallback)
    std::vector<Card> collectTableCards() const;

    // Handle rare case where deck runs out before cut card
    void handleEmptyDeck();

    void dealCardToPlayer(Hand& hand);
    void dealCardToDealer();

    void dealerTurn();

    void offerInsurance();

    char giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand);
    void logStrategyDeviation(char actionTaken);

    // NPC decision making
    void npcTakeTurn();

    // Compute global hand index for handActions (flat list across all players)
    size_t getGlobalHandIndexForPlayer(size_t playerIdx, size_t handIdx);

    // Accessors for the seat whose turn it is, so action methods don't re-index every time.
    Player& activePlayer() { return players[activePlayerIndex]; }
    Hand& activeHand() { return activePlayer().hands[activePlayer().currentHand]; }
};
