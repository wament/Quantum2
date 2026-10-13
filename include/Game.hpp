#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

enum class AppMode {
    MainMenu,
    Playing,
    Paused,
    ReplayHistory,
    ReplayPlaying
};
enum class MenuOption {
    PlayGame,
    PlayerCount,
    GameSpeed,
    ReviewHistory
};
enum class PauseOption {
    Resume,
    Restart,
    QuitToMenu
};
// GameSpeed lives in Constants.hpp (near the top) so the GameTiming accessors can reference it; the include
// below pulls in the rest of the constants. Keeping it here means every header that includes Game.hpp still
// sees both, in order.
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
    ~Game();

    void run();

    // Click the count-style label ("Count (Hi-Lo)") to toggle between counting systems. Both are always
    // counted in the background; this swaps which buffer drives the active count and flips the style.
    void toggleCountingStyle();
    std::string gameMessage;
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

    int initalBet = 0;
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

    PlayerStats &getStats()
    {
        return players[0].getStats();
    }

    // ---------------- Setup ----------------

    void startNewGame();

    void loadAssets();

    void nextRound();

    void deal();

    // ---------------- Game Loop ----------------

    void update(float deltaTime);

    void updateHUDState();

    // Shared HUD timer countdown for summary / after-hand / after-shoe timers.
    void tickTimer(float &timerField, float deltaTime);
    // Advance + wrap activePlayerIndex (and evaluate Blackjack) at insurance-loop boundaries.
    void advanceToNextInsurancePlayer();

    // Removed: void draw(); - now in GameRenderer
    // ---------------- Betting ----------------

    void confirmBet();
    // Kelly-based optimal bet calculation (uses kellyFraction)
    int calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const;

    // ---------------- Player Actions ----------------

    void hit();

    void stand();

    bool doubleDown();

    bool split();

    bool surrender();

    void confirmInsurance();

    void declineInsurance();

    // ---------------- Dealer ----------------

    // Central helper: update running count and check cut card after each draw
    void afterDraw(Card card);

    // Collect all cards currently on the table (for reshuffle fallback)
    std::vector<Card> collectTableCards() const;

    // Handle rare case where deck runs out before cut card
    void handleEmptyDeck();

    void dealCardToPlayer(Hand &hand);
    void dealCardToDealer();

    void dealerTurn();

    void offerInsurance();

    char giveBasicStrategyAdvice(Hand &playerHand, Hand &dealerHand);
    void logStrategyDeviation(char actionTaken);

    // NPC decision making
    void npcTakeTurn();

    // Compute global hand index for handActions (flat list across all players) 
    size_t getGlobalHandIndexForPlayer(size_t playerIdx, size_t handIdx); 
    size_t incrementGlobalHandIndex(size_t current);

    // Accessors for the seat whose turn it is, so action methods don't re-index every time.
    Player& activePlayer() { return players[activePlayerIndex]; }
    Hand& activeHand() { return activePlayer().hands[activePlayer().currentHand]; }
};
