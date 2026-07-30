#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

#include "Deck.hpp"
#include "Hand.hpp"
#include "Player.hpp"
#include "GameRecord.hpp"

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

    void increaseBet();
    void decreaseBet();
    void confirmBet();
    Player player;
 
    int initalBet = 0;
    int runningCount = 0;
private:

    // ---------------- SFML ----------------

    sf::RenderWindow window;

    sf::Texture backgroundTexture;
    sf::Sprite backgroundSprite;

    sf::Texture cardTexture;
    sf::Font font;

    // ---------------- Blackjack ----------------

    Deck deck;


    Hand dealer; 


    RoundState roundState =
        RoundState::PlayerTurn;

    GameState gameState =
        GameState::Started;

    RoundRecord currentRoundRecord;
    ShoeRecord currentShoeRecord;


    // ---------------- Setup ----------------

    void loadAssets();

    void nextRound();

    void deal();

    // ---------------- Game Loop ----------------

    void processEvents();

    void update();

    void draw();

    void finishRound();
    
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

    void drawPlayers();

    void drawChips();

    void drawDeck();

    void drawInsurance();
};