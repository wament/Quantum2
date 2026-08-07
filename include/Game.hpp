#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "Deck.hpp"
#include "Hand.hpp"
#include "Player.hpp"
#include "GameRecord.hpp"
#include "HUD.hpp"
#include "HUDState.hpp" 


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

    void run();
    std::string gameMessage;
private:
    float findTrueCount() const
    {
        return static_cast<float>(runningCount) / static_cast<float>((deck.size() / 52.f));
    }

    // ---------------- SFML ----------------

    sf::RenderWindow window;

    sf::View gameView;

    sf::Texture backgroundTexture;
    sf::Sprite backgroundSprite;

    sf::Texture cardTexture;
    sf::Font font;

    // ---------------- UI ----------------

    HUD hud;
    HUDState hudState;

    // ---------------- Kelly / Risk of Ruin Slider ----------------

    float kellyFraction = 0.33f; // 1/3 Kelly ≈ 3% RoR

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

    Player player;

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

    int shoeCumulativeWinLoss = 0;   // running win/loss for current shoe
    int sessionCumulativeWinLoss = 0; // running win/loss for entire session

    PlayerStats& getStats()
    {
        return player.getStats();
    }

    // ---------------- Setup ----------------

    void loadAssets();

    void nextRound();

    void deal();

    // ---------------- Game Loop ----------------

    void processEvents();

    void update();

    void updateHUDState();

    void draw();

    void finishRound();
    
    // ---------------- Betting ----------------

    void increaseBet();
    void decreaseBet();
    void confirmBet();

    // Kelly-based optimal bet calculation (uses kellyFraction)
    int calculateOptimalBet(float trueCount, int bankroll, int minBet, int maxBet) const;

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

    void dealCardToPlayer(Hand& hand);
    
    void dealCardToDealer();

    void dealerTurn();

    void offerInsurance();

    void evaluateHands();

    void evaluateBlackjack();

    const int getMaxInsurance();

    char giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand);

    // ---------------- Drawing ----------------

    void drawDealer();

    void resizeView(sf::RenderWindow& window, sf::View& view);

    void drawPlayers();

    void drawChips();

    void drawDeck();

    void drawInsurance();
};