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