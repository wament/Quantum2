#include "Game.hpp"

#include <iostream>
#include <stdexcept>
#include <filesystem> 

#include "Constants.hpp"
#include "Graphics.hpp"
#include "Player.hpp"
 

Game::Game():window(sf::VideoMode(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT), Constants::WINDOW_TITLE), player("William")
{
    currentShoeRecord = {};
    loadAssets();
    deck.create(2);
    deck.shuffle();
    currentShoeRecord.deck = deck;
    player.resetRound();
    nextRound();
    #pragma region 
    std::cout << R"(

=========================================================
                    ♠ BLACKJACK ♣
                     PLAYER CONTROLS
=========================================================

BETTING PHASE
-------------
  ↑  Up Arrow      Increase Bet
  ↓  Down Arrow    Decrease Bet
  Enter            Confirm Bet

PLAYER TURN
-----------
  H                Hit
  S                Stand
  D                Double Down
  P                Split
  R                Surrender

ROUND / GAME
------------
  R                Next Round (after round ends)
  R                Restart Game (when Game Over)

Close the window at any time to quit.

=========================================================

)";
    #pragma endregion


}
    

void Game::run()
{
    while(window.isOpen())
    {
        processEvents();

        update();

        draw();
    }
}


// --------------------------------------------------
// Setup
// --------------------------------------------------

void Game::loadAssets()
{

std::cout << "Current working directory: "
          << std::filesystem::current_path() << '\n';
          
std::cout << std::filesystem::exists("assets/felt.png") << '\n';


    if(!backgroundTexture.loadFromFile(
        "assets/felt.png"))
    { 
        throw std::runtime_error(
            "Failed to load felt texture");
    }


    backgroundSprite.setTexture(
        backgroundTexture);


    sf::Vector2u textureSize =
        backgroundTexture.getSize();

    sf::Vector2u windowSize =
        window.getSize();


    backgroundSprite.setScale(
        static_cast<float>(windowSize.x)
            / textureSize.x,

        static_cast<float>(windowSize.y)
            / textureSize.y
    );


    if(!cardTexture.loadFromFile(
        "assets/cardsLarge_tilemap_packed.png"))
    {
        throw std::runtime_error(
            "Failed to load card texture");
    }
}



void Game::nextRound()
{
    player.resetRound(); //also clear bets
    dealer.clear();
    currentRoundRecord = {};
    roundState = RoundState::PlaceBets;
}


// --------------------------------------------------
// Events
// --------------------------------------------------

void Game::processEvents()
{
    sf::Event event;


    while(window.pollEvent(event))
    {
        if(event.type ==
           sf::Event::Closed)
        {
            window.close();
        }


        if(event.type !=
           sf::Event::KeyPressed)
        {
            continue;
        }

        if(roundState == RoundState::PlaceBets)
        {
            switch(event.key.code)
            {
                case sf::Keyboard::Up:
                    increaseBet();
                    break;

                case sf::Keyboard::Down:
                    decreaseBet();
                    break;

                case sf::Keyboard::Enter:
                    confirmBet();
                    break;

                default:
                    break;
            }

            continue;
        }



        if(roundState ==
           RoundState::PlayerTurn)
        {
            switch(event.key.code)
            {
                case sf::Keyboard::H:
                    hit();
                    break;


                case sf::Keyboard::S:
                    stand();
                    break;


                case sf::Keyboard::D:
                    doubleDown();
                    break;


                case sf::Keyboard::P:
                    split();
                    break;


                default:
                    break;
            }
        }


        if(event.key.code ==
           sf::Keyboard::R)
        {
            if(gameState ==
               GameState::GameOver)
            {
                deck.create(2);
                deck.shuffle();
                currentShoeRecord.deck = deck;

                gameState =
                    GameState::Started;
            } 
             
            if(roundState == RoundState::PlayerTurn){ 
                surrender();
            }else if(roundState == RoundState::Cleanup)
            {
                nextRound();
            }
        }
    }
}




// --------------------------------------------------
// Update
// --------------------------------------------------

void Game::update()
{
    if (roundState == RoundState::PlayerTurn)
    {
        // Advance to the next unfinished hand.
        while (player.currentHand < player.hands.size() &&
            player.hands[player.currentHand].finished)
        {
            ++player.currentHand;
        }

        // If there are no unfinished hands left, the player's turn is over.
        if (player.currentHand >= player.hands.size())
        {
            roundState = RoundState::DealerTurn;
        }
    }




    if(roundState ==
       RoundState::DealerTurn)
    {
        dealerTurn();
    }
}



// --------------------------------------------------
// Player actions
// --------------------------------------------------

void Game::hit()
{
    Hand& hand =
        player.hands[player.currentHand];


    if(hand.finished)
        return;

    dealCardToPlayer(hand);

    std::cout 
        << "Hit! You have "
        << hand.getValue()
        <<". ";

    if(deck.reachedCutCard())
    {
        gameState =
            GameState::LastHand;
    }
 
    if(hand.isBust())
    {
        std::cout
            << "Bust!\n";

        hand.finished = true;
    }

    currentRoundRecord.actionSequence.push_back("H");
}



void Game::stand()
{
    Hand& hand = player.hands[player.currentHand];
    std::cout 
        << "Stand on "
        << hand.getValue()
        <<".\n";
    hand.finished = true;
    
    currentRoundRecord.actionSequence.push_back("S");    
}



void Game::doubleDown()
{
    Hand& hand = player.hands[player.currentHand];


    if(!hand.canDouble())
        return;


    hand.doubledDown = true;


    
    dealCardToPlayer(hand);
    std::cout 
        << "Double trouble... Oh! It's a "
        << hand.getValue()
        <<"!\n";

    hand.finished = true;
    
    currentRoundRecord.actionSequence.push_back("D");
}



void Game::split()
{
    Hand& hand = player.hands[player.currentHand];


    if(player.hands.size()
       >= Constants::MAX_SPLIT_HANDS)
    {
        return;
    }


    if(!hand.canSplit())
        return;


    Hand newHand;
    Card splitCard = hand.takeLastCard();
    std::cout 
        << "Splitting "
        << splitCard.getBlackjackValueToString()
        <<"s.\n";
    newHand.addCard(splitCard);

    currentRoundRecord.actionSequence.push_back("P");

    dealCardToPlayer(hand);
    player.hands.push_back(newHand);
    hand.split = true;
    newHand.split = true;
}

void Game::surrender()
{
    Hand& hand = player.hands[player.currentHand];

    // Cannot surrender a finished hand
    if(hand.finished)
        return;

    // Only allowed on the first two cards
    if(!hand.canDouble())
        return;

    std::cout
        << "Surrender. You lose half your bet.\n";

    hand.surrendered = true;
    hand.finished = true;

    currentRoundRecord.actionSequence.push_back("R");
}



// --------------------------------------------------
// Betting
// --------------------------------------------------

void Game::increaseBet()
{
    std::cout << "Increased bet to "
        << player.hands[player.currentHand].getBet()
        << ".\n";
    player.hands[0].setBet(player.hands[0].getBet() + 5);
}

void Game::decreaseBet()
{
    std::cout << "Decreased bet to "
        << player.hands[player.currentHand].getBet()
        << ".\n";
    if (player.hands[0].getBet() >= 5)
        player.hands[0].setBet(player.hands[0].getBet() - 5);
}

void Game::confirmBet()
{
    std::cout << "You bet "
        << player.hands[player.currentHand].getBet()
        << ". All bets are down.\n";
    if(player.hands[0].getBet() <= Constants::MINIMUM_BET)
        return;

    deal();
}


// --------------------------------------------------
// Deck / Deal
// --------------------------------------------------

void Game::deal()
{
    dealCardToPlayer(player.hands[0]);
    dealCardToDealer();

    dealCardToPlayer(player.hands[0]);
    dealCardToDealer();

    currentRoundRecord.player = player.hands[player.currentHand];
    currentRoundRecord.dealer = dealer;

    roundState =
        RoundState::PlayerTurn;


    if(player.hands[0].isBlackjack())
    {
        std::cout
            << "Blackjack!\n";
        roundState = RoundState::DealerTurn;
    }else{
        std::cout
            << "You have a "
            << player.hands[0].getValue()
            << " against dealers "
            << dealer.getDealerUpCardToString()
            << "." << std::endl;


        
    }
    
}

void Game::dealCardToPlayer(Hand& hand)
{
    hand.addCard(deck.draw());
}


void Game::dealCardToDealer()
{
    dealer.addCard(deck.draw());
}


// --------------------------------------------------
// Dealer
// --------------------------------------------------

void Game::dealerTurn()
{
    bool doNotDraw = false;
    int numBusted = 0;
    for(Hand hand: player.hands){
        if((hand.isBlackjack() && hand.split == false)
            || (hand.surrendered && hand.split == false)
        ){
            doNotDraw = true;
        }

        if(hand.isBust()){
            numBusted++;
        }
    }
    std::cout << numBusted << " " << player.currentHand << std::endl;
    if(numBusted == player.hands.size())
        doNotDraw = true;

    
    while(dealer.getValue() < Constants::DEALER_STAND && !doNotDraw)
    {
        std::cout
        << "Dealer has "
        << dealer.getValue()
        << "\n";

        dealer.addCard(
            deck.draw());
    }

    if (dealer.isBust()){
        std::cout
        << "Dealer busted with "
        << dealer.getValue()
        << ". Too many!\n";
    }else{
        
        switch (dealer.getValue())
        {
        case 17:
            std::cout
            << "Dealer stands on the edge of "
            << dealer.getValue()
            << "." << std::endl;
            break;
        case 18:
            std::cout
            << "Dealer stands on "
            << dealer.getValue()
            << "." << std::endl;
            break;
        case 19:
            std::cout
            << "Dealer stands on "
            << dealer.getValue()
            << ". It's bigger than average." << std::endl;
            break;
        case 20:
            std::cout
            << "Dealer stands on "
            << dealer.getValue()
            << ". Dealer always has 20." << std::endl;
            break;
        case 21:
            std::cout
            << "Dealer stands on "
            << dealer.getValue()
            << ". That's the name of the game folks." << std::endl;
            break;
        default:
            break;
        }
    }

    evaluateHands();
    

    if(gameState ==
       GameState::LastHand)
    {
        gameState =
            GameState::GameOver;

        //do end game saving here

        PlayerProfile profile = player.getProfile();
 
        profile.addShoeRecord(currentShoeRecord);
        player.updateProfile(profile);
        player.saveProfile();

        currentShoeRecord = {};
        std::cout
            << "New shoe required. Press R.\n";
    }
}
 

void Game::evaluateHands()
{
    for (Hand hand : player.hands){
        currentRoundRecord.wager += hand.getBet();
        if (hand.isBust()){
            player.loseBet(hand.getBet());
        }else
        if (hand.surrendered){
            player.winBet(hand.getBet() / 2);
            currentRoundRecord.payout += (hand.getBet() / 2);
        }else
        if (hand.isBlackjack()){
            player.winBet(hand.getBet() * Constants::BLACKJACK_PAYOUT);
            currentRoundRecord.payout += (hand.getBet() * Constants::BLACKJACK_PAYOUT);
        }else


        if(hand.getValue() > dealer.getValue()){
            currentRoundRecord.payout += hand.getBet();
            player.winBet(hand.getBet());
        }else 
        if (hand.getValue() < dealer.getValue()){
            player.loseBet(hand.getBet());
        }else{
            player.pushBet(hand.getBet());
        }
    }
    
    currentRoundRecord.won = currentRoundRecord.isWin();
    PlayerProfile profile = player.getProfile();
    profile.addRoundRecord(currentRoundRecord);
    player.updateProfile(profile);

    currentShoeRecord.rounds.push_back(currentRoundRecord);
    roundState = RoundState::Cleanup;
}



std::string Game::giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand){

    return playerHand.getDealerUpCardToString() + dealerHand.getDealerUpCardToString();
}


// --------------------------------------------------
// Drawing
// --------------------------------------------------

void Game::draw()
{
    window.clear();

    window.draw(backgroundSprite);

    drawDealer();
    drawPlayers();
    drawChips();
    drawDeck();

    window.display();
}



void Game::drawDealer()
{
    if(roundState ==
       RoundState::PlayerTurn)
    {
        auto back =
            Graphics::makeCardBackSprite(
                cardTexture);

        back.setPosition(
            Constants::DEALER_X,
            Constants::DEALER_Y);


        window.draw(back);


        auto up =
            Graphics::makeCardSprite(
                dealer.getCard(0),
                cardTexture);


        up.setPosition(
            Constants::DEALER_X - 10,
            Constants::DEALER_Y);


        window.draw(up);

        return;
    }



    for(std::size_t i = 0;
        i < dealer.size();
        i++)
    {
        Graphics::drawCard(
            window,
            cardTexture,
            dealer.getCard(i),
            Constants::DEALER_X +
                i * 60,
            Constants::DEALER_Y);
    }
}



void Game::drawPlayers()
{
    for(std::size_t handIndex = 0;
        handIndex < player.hands.size();
        handIndex++)
    {
        const Hand& hand =
            player.hands[handIndex];


        for(std::size_t i = 0;
            i < hand.size();
            i++)
        {
            Graphics::drawCard(
                window,
                cardTexture,
                hand.getCard(i),
                Constants::PLAYER_X +
                    handIndex * 142 +
                    i * 16,

                Constants::PLAYER_Y -
                    i * 16);
        }
    }
}

/*
void Game::drawChips()
{
    for (std::size_t handIndex = 0;
         handIndex < player.hands.size();
         ++handIndex)
    {
        const Hand& hand = player.hands[handIndex];

        float handX =
            Constants::PLAYER_X +
            handIndex * 80;

        float handWidth =
            Constants::CARD_WIDTH +
            (hand.size() - 1) * 16;

        float chipX =
            handX +
            handWidth / 2.f;

        float chipY =
            Constants::PLAYER_Y +
            Constants::CARD_HEIGHT +
            20.f;

        sf::Sprite chips(chipTexture);

        chips.setOrigin(
            chipTexture.getSize().x / 2.f,
            chipTexture.getSize().y / 2.f);

        chips.setPosition(chipX, chipY);

        window.draw(chips);

        // Draw a second stack if doubled down.
        if (hand.doubledDown)
        {
            sf::Sprite second(chipTexture);

            second.setOrigin(
                chipTexture.getSize().x / 2.f,
                chipTexture.getSize().y / 2.f);

            second.setPosition(
                chipX + 35.f,
                chipY);

            window.draw(second);
        }
    }
}


*/

void Game::drawChips()
{
    struct Chip
    {
        int value;
        sf::Color color;
    };


    const std::vector<Chip> denominations =
    {
        {50000, sf::Color(128, 0, 128)},        // $500
        {10000, sf::Color::Black},              // $100
        {2500,  sf::Color(0, 180, 0)},          // $25
        {500,   sf::Color::Red},                // $5
        {250,   sf::Color(255, 105, 180)},      // $2.50
        {100,   sf::Color::White}               // $1
    };

    int handIndex = 0;
    for(Hand& hand: player.hands)
    {  
        int bet = hand.getBet();


        if(bet <= 0)
            continue;


        //
        // Position based on the same math as cards
        //
        float handX =
            Constants::PLAYER_X +
            handIndex * 80;


        float handWidth =
            Constants::CARD_WIDTH +
            (hand.size() - 1) * 16;


        float chipX =
            handX +
            handWidth / 2.f;


        float chipY =
            Constants::PLAYER_Y +
            Constants::CARD_HEIGHT +
            30.f;



        //
        // Convert dollars to cents
        //
        int remaining =
            bet * 100;


        std::vector<Chip> stack;


        for(const auto& chip : denominations)
        {
            while(remaining >= chip.value)
            {
                stack.push_back(chip);
                remaining -= chip.value;
            }
        }



        //
        // Draw stack
        //
        for(std::size_t i = 0;
            i < stack.size();
            i++)
        {
            sf::CircleShape chip(15.f);


            chip.setFillColor(
                stack[i].color);


            chip.setOutlineColor(
                sf::Color::White);


            chip.setOutlineThickness(2.f);


            chip.setOrigin(
                15.f,
                15.f);


            chip.setPosition(
                chipX,
                chipY - i * 3.f);


            window.draw(chip);
        }



        //
        // Draw total bet
        //
        sf::Text text;

        text.setFont(font);

        text.setCharacterSize(16);

        text.setFillColor(
            sf::Color::White);


        text.setString(
            "$" + std::to_string(bet));


        sf::FloatRect bounds =
            text.getLocalBounds();


        text.setOrigin(
            bounds.width / 2.f,
            bounds.height / 2.f);


        text.setPosition(
            chipX,
            chipY -
            stack.size() * 3.f -
            20.f);


        window.draw(text);



        //
        // Draw second stack when doubled
        //
        if(hand.doubledDown)
        {
            float secondX =
                chipX + 35.f;


            for(std::size_t i = 0;
                i < stack.size();
                i++)
            {
                sf::CircleShape chip(15.f);

                chip.setFillColor(
                    stack[i].color);

                chip.setOutlineColor(
                    sf::Color::White);

                chip.setOutlineThickness(2.f);

                chip.setOrigin(
                    15.f,
                    15.f);

                chip.setPosition(
                    secondX,
                    chipY - i * 3.f);

                window.draw(chip);
            }
        }
        handIndex++;
    }
}


void Game::drawDeck()
{
    for(std::size_t i = 0;
        i < deck.size();
        i++)
    {
        Graphics::drawCard(
            window,
            cardTexture,
            deck.getCard(i),
            Constants::DECK_X +
                (i % 52) * 20,

            Constants::DECK_Y +
                (i / 52) * 80);
    }
}