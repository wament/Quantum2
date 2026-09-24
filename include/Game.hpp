#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "Deck.hpp"
#include "Hand.hpp"
#include "Player.hpp"
#include "GameRecord.hpp"
#include "HUD.hpp"
#include "HUDState.hpp"
#include "Replay.hpp"
#include "GameRenderer.hpp"

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

enum class GameSpeed {
    Fast,
    Normal,
    Slow
};

class Game
{
public:
    Game();

    void run();
    std::string gameMessage;
    float deltaTime;

    friend class GameRenderer;

private: 
    AppMode appMode = AppMode::MainMenu;
    std::unique_ptr<Replay> replay;
    GameRenderer renderer;
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

    // NPC action timing
    float npcActionTimer = 0.0f;
    float npcActionDelay = 1.0f;

    // ---------------- Kelly / Risk of Ruin Slider ----------------

    float kellyFraction = 0.33f; // 1/3 Kelly ≈ 3% RoR

    int playerCount = 1;

    void setPlayerCount(int count);

    struct SliderState
    {
        bool dragging = false;
        sf::FloatRect bounds; // x, y, w, h (w is track width)
        float value = 0.33f;  // 0.0..1.0 = Kelly fraction
    };

    SliderState riskSlider;
    // ---------------- Blackjack ----------------

    Deck deck;

    Hand dealer;

    std::vector<Player> players;
    int activePlayerIndex = 0;

    RoundState roundState = RoundState::PlayerTurn;

    GameState gameState = GameState::Started;

    int initalBet = 0;
    int runningCount = 0;
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

    void processEvents();
    void processMenuEvents(const sf::Event &event);
    void processGameEvents(const sf::Event &event);
    void processPauseEvents(const sf::Event &event);

    void update(float deltaTime);

    void updateHUDState();

    // Removed: void draw(); - now in GameRenderer
    // ---------------- Betting ----------------

    void increaseBet();
    void decreaseBet();
    void confirmBet();
    // Kelly-based optimal bet calculation (uses kellyFraction)
    int calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const;

    // ---------------- Player Actions ----------------

    void hit();

    void stand();

    void doubleDown();

    void split();

    void surrender();

    void increaseInsurance();

    void decreaseInsurance();

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

    void evaluateHands();

    void evaluateBlackjack();

    const int getMaxInsurance();

    char giveBasicStrategyAdvice(Hand &playerHand, Hand &dealerHand);
    void logStrategyDeviation(char actionTaken);

    // NPC decision making
    void npcTakeTurn();

    // Compute global hand index for handActions (flat list across all players) 
    size_t getGlobalHandIndexForPlayer(size_t playerIdx, size_t handIdx); 
    size_t incrementGlobalHandIndex(size_t current);
};

