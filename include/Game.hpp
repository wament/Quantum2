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
private:

    // ---------------- SFML ----------------

    sf::RenderWindow window;

    sf::Texture backgroundTexture;
    sf::Sprite backgroundSprite;

    sf::Texture cardTexture;


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

    void dealCardToPlayer(Hand& hand);
    
    void dealCardToDealer();

    void finishRound();

    void evaluateHands();

    
    // ---------------- Player Actions ----------------
    void hit();

    void stand();

    void doubleDown();

    void split();

    void surrender();


    // ---------------- Dealer ----------------

    void dealerTurn();

    std::string giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand);

    // ---------------- Drawing ----------------

    void drawDealer();

    void drawPlayers();

    void drawChips();

    void drawDeck();
};