#include "Game.hpp"
#include <memory>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <filesystem>

#include "Constants.hpp"
#include "Graphics.hpp"
#include "Player.hpp"
#include "FontManager.hpp"

Game::Game()
    : window(
          sf::VideoMode({Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT}), Constants::WINDOW_TITLE),
      gameView(
          sf::FloatRect(
              {0.f, 0.f},
              {static_cast<float>(Constants::WINDOW_WIDTH),
               static_cast<float>(Constants::WINDOW_HEIGHT)})),
      player("William")
{


    window.setView(gameView);
    currentShoeRecord = {};
    loadAssets();
    replay = std::make_unique<Replay>(window, cardTexture, FontManager::get());
    replay = std::make_unique<Replay>(window, cardTexture, FontManager::get());
    deck.create(Constants::NUM_DECKS);
    deck.shuffle();
    currentShoeRecord.shuffleSeed = deck.getLastShuffleSeed();
    currentShoeRecord.shuffleSeed = deck.getLastShuffleSeed();
    currentShoeRecord.initialDeck = deck.getCards();
    nextRound();
#pragma region
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
  <-  Right Arrow   Auto Kelly Bet 
  ->            Confirm Bet

PLAYER TURN
-----------
  H                Hit
  S                Stand
  D                Double Down
      P                Split
      Y                Surrender

ROUND / GAME
------------
  R                Next Round (after round ends)
  R                Restart Game (when Game Over)

Close the window at any time to quit.

=========================================================

)";
#pragma endregion
#pragma endregion
}

void Game::run()
{
    sf::Clock clock;
    while (window.isOpen())
    sf::Clock clock;
    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        float dt = clock.restart().asSeconds();

        processEvents();

        if (inReplayMode)
        {
            replay->update(dt);
            replay->draw();
            if (replay->getMode() == Replay::Mode::HistoryList)
                inReplayMode = false;
        }
        else
        {
            update();
            draw();
        }
        if (inReplayMode)
        {
            replay->update(dt);
            replay->draw();
            if (replay->getMode() == Replay::Mode::HistoryList)
                inReplayMode = false;
        }
        else
        {
            update();
            draw();
        }
    }
}

// --------------------------------------------------
// Setup
// --------------------------------------------------

void Game::loadAssets()
{
    std::cout << "Current working directory: "
              << std::filesystem::current_path() << '\n';

              << std::filesystem::current_path() << '\n';

    if (!FontManager::load("assets/fonts/roboto/RobotoFlex-VariableFont.ttf"))
    {
        std::cout << "Failed to load font!\n";
        return;
    }

    hud.initialize(FontManager::get());

    if (!backgroundTexture.loadFromFile(
            "assets/images/felt.png"))
    {
    if (!backgroundTexture.loadFromFile(
            "assets/images/felt.png"))
    {
        throw std::runtime_error(
            "Failed to load felt texture");
    }

    backgroundSprite = std::make_unique<sf::Sprite>(backgroundTexture);

    sf::Vector2u textureSize = backgroundTexture.getSize();
    sf::Vector2u windowSize = window.getSize();
    backgroundSprite->setScale(
        sf::Vector2f(
            static_cast<float>(windowSize.x) / textureSize.x,
            static_cast<float>(windowSize.y) / textureSize.y));

    if (!cardTexture.loadFromFile(
            "assets/images/cardsLarge_tilemap_packed.png"))
    {
        throw std::runtime_error(
            "Failed to load card texture");
    }
}

void Game::nextRound()
{
    player.resetRound(initalBet); // also clear bets
    dealer.clear();
    player.resetRound(initalBet); // also clear bets
    dealer.clear();
    currentRoundRecord = {};
    handActions.clear();
    handActions.resize(1); // start with one hand's actions
    roundState = RoundState::PlaceBets;
}

// --------------------------------------------------
// Events
// --------------------------------------------------

void Game::processEvents()
{
    while (const auto event = window.pollEvent())
    {
        // Closed
        if (event->is<sf::Event::Closed>())
        {
            // Save current shoe and profile before exiting
            if (!currentShoeRecord.rounds.empty())
            {
                currentShoeRecord.completed = false; // quit mid-shoe
            }
            PlayerProfile profile = player.getProfile();
            profile.addShoeRecord(currentShoeRecord);
            player.updateProfile(profile);
            player.saveProfile();
            window.close();
        }

        // Resized
        if (event->is<sf::Event::Resized>())
        {
            resizeView(window, gameView);
        }

        // Mouse button pressed (slider)
        if (auto pressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button == sf::Mouse::Button::Left)
            {
                sf::FloatRect slider = hud.getRiskSliderBounds();
                if (slider.contains(sf::Vector2f(pressed->position.x, pressed->position.y)))
                {
                    hudState.riskSliderDragging = true;
                }
            }
        }

        // Mouse button released (slider)
        if (auto released = event->getIf<sf::Event::MouseButtonReleased>())
        {
            if (released->button == sf::Mouse::Button::Left && hudState.riskSliderDragging)
            {
                hudState.riskSliderDragging = false;
            }
        }

        // Mouse moved (slider)
        if (auto moved = event->getIf<sf::Event::MouseMoved>())
        {
            if (hudState.riskSliderDragging)
            {
                sf::FloatRect slider = hud.getRiskSliderBounds();
                float frac = (moved->position.x - slider.position.x) / slider.size.x;
                frac = std::max(0.0f, std::min(1.0f, frac));
                hudState.kellyFraction = frac;
                kellyFraction = frac;
            }
        }

        // Key pressed
        if (auto key = event->getIf<sf::Event::KeyPressed>())
        {
            if (roundState == RoundState::PlaceBets)
            {
                switch (key->code)
                {
                case sf::Keyboard::Key::Up:
                    increaseBet();
                    break;

                case sf::Keyboard::Key::Down:
                    decreaseBet();
                    break;

                case sf::Keyboard::Key::Left:
                {
                    float tc = findTrueCount();
                    int optimal = calculateOptimalBet(tc, kellyFraction, player.getBankroll() + player.hands[0].getBet(),
                                                      Constants::MINIMUM_BET, Constants::MAXIMUM_BET);
                    player.hands[0].setBet(optimal);
                    std::cout << "Optimal bet (TC=" << tc << ") set to "
                              << optimal << ".\n";
                    break;
                }

                case sf::Keyboard::Key::Right:
                    confirmBet();
                    break;

                default:
                    break;
                }

                continue;
            }

            if (roundState == RoundState::OfferInsurance)
            {
                switch (key->code)
                {
                case sf::Keyboard::Key::Up:
                    increaseInsurance();
                    break;

                case sf::Keyboard::Key::Down:
                    decreaseInsurance();
                    break;

                case sf::Keyboard::Key::Right:
                    confirmInsurance();
                    break;

                case sf::Keyboard::Key::Left:
                    declineInsurance();
                    break;
                }

                continue;
            }

            if (roundState == RoundState::PlayerTurn)
            {
                switch (key->code)
                {
                case sf::Keyboard::Key::H:
                    hit();
                    break;

                case sf::Keyboard::Key::S:
                    stand();
                    break;

                case sf::Keyboard::Key::D:
                    doubleDown();
                    break;

                case sf::Keyboard::Key::P:
                    split();
                    break;

                case sf::Keyboard::Key::I:
                {
                    char choice = giveBasicStrategyAdvice(player.hands[player.currentHand], dealer);
                    std::cout << "Book says: " << choice << std::endl;
                    break;
                }
                }
            }

            if (key->code == sf::Keyboard::Key::R)
            {
                if (gameState == GameState::GameOver)
                {
                    deck.create(Constants::NUM_DECKS);
                    deck.shuffle();
                    currentShoeRecord.initialDeck = deck.getCards();

                    gameState = GameState::Started;
                }
                else if (roundState == RoundState::Cleanup &&
                         gameState != GameState::LastHand)
                {
                    // Don't start a new round if cut card was reached -
                    // the shoe ends after this hand
                    nextRound();
                }
            }

            if (key->code == sf::Keyboard::Key::Y)
            {
                if (roundState == RoundState::PlayerTurn)
                {
                    surrender();
                }
            }
        }
    }
}

// --------------------------------------------------
// Betting
// --------------------------------------------------

void Game::update()
{
    if (roundState == RoundState::PlayerTurn)
    {
        // Advance to the next unfinished hand.
        while (player.currentHand < player.hands.size() &&
               player.hands[player.currentHand].isFinished())
               player.hands[player.currentHand].isFinished())
        {
            ++player.currentHand;
        }

        // If there are no unfinished hands left, the player's turn is over.
        if (player.currentHand >= player.hands.size())
        {
            roundState = RoundState::DealerTurn;
        }
        else
        {
            // After advancing, ensure the current hand has at least two cards.
            // This handles split hands: each split hand must receive its second
            // card before the player can take an action on it.
            if (player.hands[player.currentHand].size() == 1)
            {
                dealCardToPlayer(player.hands[player.currentHand]);
            }
        }
    }

    if (roundState ==
        RoundState::DealerTurn)
    if (roundState ==
        RoundState::DealerTurn)
    {
        dealerTurn();
    }
}


void Game::updateHUDState()
{
    hudState.bankroll = player.getBankroll();

    hudState.currentBet = player.hands[0].getBet();

    hudState.runningCount = runningCount;

    hudState.trueCount = findTrueCount();

    hudState.cardsRemaining = deck.size();

    hudState.kellyFraction = kellyFraction;


    hudState.statusMessage = gameMessage;

    hudState.playerStats = &getStats();

    hudState.currentShoe = &currentShoeRecord;
    if (roundState == RoundState::Cleanup)
    {
        gameMessage += " [Tab: Review History]";
    }
    if (roundState == RoundState::Cleanup)
    {
        gameMessage += " [Tab: Review History]";
    }
}

// --------------------------------------------------
// Player actions
// --------------------------------------------------

void Game::hit()
{
    Hand &hand =
    Hand &hand =
        player.hands[player.currentHand];

    if (hand.isFinished())
    if (hand.isFinished())
        return;

    dealCardToPlayer(hand);

    std::cout
    std::cout
        << "Hit! You have "
        << hand.getValue()
        << ". " << std::endl;

    if (hand.isBust())
        << ". " << std::endl;

    if (hand.isBust())
    {
        std::cout
            << "Bust!\n"
            << std::endl;
            << "Bust!\n"
            << std::endl;

        hand.setFinished(true);
    }

    handActions[player.currentHand].push_back("H");
}

void Game::stand()
{
    Hand &hand = player.hands[player.currentHand];
    std::cout
    Hand &hand = player.hands[player.currentHand];
    std::cout
        << "Stand on "
        << hand.getValue()
        << ".\n";
        << ".\n";
    hand.setFinished(true);

    handActions[player.currentHand].push_back("S");

    handActions[player.currentHand].push_back("S");
}

void Game::doubleDown()
{
    Hand &hand = player.hands[player.currentHand];
    Hand &hand = player.hands[player.currentHand];
    int initalbet = hand.getBet();

    if (!hand.canDouble())
    if (!hand.canDouble())
        return;

    hand.setDoubledDown(true);
    if (!player.placeBet(initalbet))
    if (!player.placeBet(initalbet))
        return;

    hand.setBet(initalbet * 2);
    dealCardToPlayer(hand);

    // Track double stats
    PlayerStats &stats = getStats();
    PlayerStats &stats = getStats();
    stats.doubles++;
    stats.betsPlaced++;
    stats.betSum += initalbet;

    std::cout
    std::cout
        << "Doubling "
        << initalbet << " to " << hand.getBet()
        << " on a "
        << hand.getValue()
        << "!\n";
        << "!\n";

    hand.setFinished(true);

    handActions[player.currentHand].push_back("D");
}

    handActions[player.currentHand].push_back("D");
}

void Game::split()
{
    Hand &hand = player.hands[player.currentHand];
    Hand &hand = player.hands[player.currentHand];

    if (player.hands.size() >= Constants::MAX_SPLIT_HANDS)
    if (player.hands.size() >= Constants::MAX_SPLIT_HANDS)
    {
        return;
    }

    if (!hand.canSplit())
    if (!hand.canSplit())
        return;

    Hand newHand;
    Card splitCard = hand.takeLastCard();
    std::cout
    std::cout
        << "Splitting "
        << splitCard.getBlackjackValueToString()
        << "s.\n";
        << "s.\n";
    newHand.addCard(splitCard);

    handActions[player.currentHand].push_back("P");

    dealCardToPlayer(hand);

    player.hands.push_back(newHand);
    handActions.push_back({}); // new action vector for the split hand
    hand.setSplit(true);
    newHand.setSplit(true);

    // Track split stats
    PlayerStats &stats = getStats();
    PlayerStats &stats = getStats();
    stats.splits++;
    stats.betsPlaced++;
    stats.betSum += hand.getBet(); // new hand inherits bet amount
}

void Game::surrender()
{
    Hand &hand = player.hands[player.currentHand];
    Hand &hand = player.hands[player.currentHand];

    // Cannot surrender a finished hand
    if (hand.isFinished())
    if (hand.isFinished())
        return;

    // Only allowed on the first two cards
    if (!hand.canDouble())
    if (!hand.canDouble())
        return;

    std::cout
        << "Surrender. You lose half your bet.\n";

    hand.setSurrendered(true);
    hand.setFinished(true);

    handActions[player.currentHand].push_back("R");
}

// --------------------------------------------------
// Betting
// --------------------------------------------------

void Game::increaseBet()
{


    if (player.hands[0].getBet() < Constants::MAXIMUM_BET)
    {
        player.hands[0].setBet(player.hands[0].getBet() + 5);
        std::cout << "Increased bet to "
                  << player.hands[player.currentHand].getBet()
                  << ".\n";
                  << player.hands[player.currentHand].getBet()
                  << ".\n";
    }
}

void Game::decreaseBet()
{


    if (player.hands[0].getBet() >= Constants::MINIMUM_BET)
    {
    {
        player.hands[0].setBet(player.hands[0].getBet() - 5);
        std::cout << "Decreased bet to "
                  << player.hands[player.currentHand].getBet()
                  << ".\n";
                  << player.hands[player.currentHand].getBet()
                  << ".\n";
    }
}

int Game::calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const
{
    // Only use positive edge (TC >= 2) TC = 1 is fair. TC < 1 the house has the advantage.
    if (trueCount < 2.0f)
    {
        return minBet;
    }

    // Calculate player edge for H17, 3:2 conditions
    // Player Edge = 0.005 * (TC - 1) - 0.0005
    float playerEdge = 0.005f * (trueCount - 1.0f) - 0.0005f;

    // Raw bet = KellyFraction * Bankroll * PlayerEdge
    float rawBet = kellyFraction * static_cast<float>(bankroll) * playerEdge;

    int wager = static_cast<int>(rawBet);
    wager = std::max(minBet, std::min(maxBet, wager));

    return wager;
}

void Game::confirmBet()
{
    int currentBet = player.hands[0].getBet();
    if (currentBet < Constants::MINIMUM_BET || currentBet > Constants::MAXIMUM_BET)
    if (currentBet < Constants::MINIMUM_BET || currentBet > Constants::MAXIMUM_BET)
        return;

    if (!player.placeBet(currentBet))
    if (!player.placeBet(currentBet))
    {
        std::cout << "Not enough bankroll. " << player.getBankroll() << " is your max. Or "
                  << player.getBankroll() / 2 << " if you want to double.\n";
                  << player.getBankroll() / 2 << " if you want to double.\n";
        return;
    }

    initalBet = currentBet;

    // Track bet stats
    PlayerStats &stats = getStats();
    PlayerStats &stats = getStats();
    stats.betsPlaced++;
    stats.betSum += currentBet;

    std::cout << "You bet "
              << player.hands[player.currentHand].getBet()
              << ". Your remaining bankroll is: "
              << player.getBankroll()
              << "\n"
              << "The running count is: "
              << runningCount
              << ".\n";
              << player.hands[player.currentHand].getBet()
              << ". Your remaining bankroll is: "
              << player.getBankroll()
              << "\n"
              << "The running count is: "
              << runningCount
              << ".\n";
    deal();
}

// --------------------------------------------------
// Insurance
// --------------------------------------------------

void Game::increaseInsurance()
{
    if (player.hands[player.currentHand].getInsuranceBet() < getMaxInsurance())
    {
    if (player.hands[player.currentHand].getInsuranceBet() < getMaxInsurance())
    {

        player.hands[player.currentHand].setInsuranceBet(
            player.hands[player.currentHand].getInsuranceBet() + 1);
        std::cout << "Increased insurance to "
                  << player.hands[player.currentHand].getInsuranceBet()
                  << ".\n";
                  << player.hands[player.currentHand].getInsuranceBet()
                  << ".\n";
    }
}

void Game::decreaseInsurance()
{
    if (player.hands[player.currentHand].getInsuranceBet() > 0)
    {
    if (player.hands[player.currentHand].getInsuranceBet() > 0)
    {
        player.hands[player.currentHand].setInsuranceBet(
            player.hands[player.currentHand].getInsuranceBet() - 1);
        std::cout << "Decreased insurance to "
                  << player.hands[player.currentHand].getInsuranceBet()
                  << ".\n";
                  << player.hands[player.currentHand].getInsuranceBet()
                  << ".\n";
    }
}

void Game::confirmInsurance()
{
    int insBet = player.hands[player.currentHand].getInsuranceBet();
    if (insBet > 0)
    {
        player.placeBet(insBet);
        std::cout << "Placed insurance bet of"
                  << insBet
                  << ".\n";
                  << insBet
                  << ".\n";
    }

    evaluateBlackjack();
}

void Game::declineInsurance()
{
    player.hands[player.currentHand].setInsuranceBet(0);
    std::cout << "Insurance Denied" << std::endl;
    evaluateBlackjack();
}

const int Game::getMaxInsurance()
const int Game::getMaxInsurance()
{
    return player.hands[player.currentHand].getBet() / 2;
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

    roundState = RoundState::PlayerTurn;

    if (dealer.getDealerUpCard().isAce())
    {
    if (dealer.getDealerUpCard().isAce())
    {
        std::cout << "offered insurance" << std::endl;
        roundState = RoundState::OfferInsurance;
        roundState = RoundState::OfferInsurance;
    }
    else
        evaluateBlackjack();
    else
        evaluateBlackjack();

    std::cout
        << "You have a "
        << player.hands[0].getValue()
        << " against dealers "
        << dealer.getDealerUpCardToString()
        << ".\n";
}

void Game::evaluateBlackjack()
{
    Hand &hand = player.hands[player.currentHand];
    Hand &hand = player.hands[player.currentHand];

    int bet = hand.getBet();

    if (dealer.isBlackjack())
    {
        if (hand.getInsuranceBet() > 0)
        {
            // Pays 2:1, plus the original insurance bet back.
            player.winBet(hand.getInsuranceBet() * 3);

            std::cout << "Insurance wins!\n";
        }

        if (hand.isBlackjack())
        {
            player.pushBet(bet);
        }
        else
        {
            player.loseBet();
        }

        roundState = RoundState::DealerTurn;
        return;
    }

    // Dealer does not have blackjack.
    if (hand.getInsuranceBet() > 0)
    {
        std::cout << "Insurance lost.\n";
        hand.setInsuranceBet(0);
    }
    else
    {
    }
    else
    {
        roundState = RoundState::PlayerTurn;
    }

    // Only natural (non-split) blackjack pays 3:2
    if (hand.isBlackjack() && !hand.isSplit())
    {
        player.winBet(bet * Constants::BLACKJACK_PAYOUT);

        std::cout << "Blackjack!\n";

        // Track blackjack (non-split only)
        getStats().blackjacks++;

        roundState = RoundState::DealerTurn;
    }
}

void Game::offerInsurance()
{
    Hand &hand = player.hands[player.currentHand];
    Hand &hand = player.hands[player.currentHand];

    int maxInsurance = hand.getBet() / 2;

    std::cout << "\nDealer shows an Ace.\n";
    std::cout << "Insurance? (y/n): ";

    char choice;
    std::cin >> choice;

    if (tolower(choice) != 'y')
        return;

    int insurance;

    do
    {
        std::cout << "Enter insurance bet (0 - "
                  << maxInsurance
                  << "): ";

        std::cin >> insurance;

    } while (insurance < 0 || insurance > maxInsurance);

    hand.setInsuranceBet(insurance);

    player.placeBet(insurance);

    std::cout << "Insurance bet of $"
              << insurance
              << " placed.\n";
}

// Central helper: apply Hi-Lo count update and check for cut card after drawing
void Game::afterDraw(Card card)
{
    runningCount += card.getHiLoValue();
    runningCount += card.getHiLoValue();

    if (deck.reachedCutCard())
    {
        gameState = GameState::LastHand;
    }
}

void Game::dealCardToPlayer(Hand &hand)
void Game::dealCardToPlayer(Hand &hand)
{
    Card card = deck.draw();
    afterDraw(card);
    hand.addCard(card);
}

void Game::dealCardToDealer()
{
    Card card = deck.draw();
    afterDraw(card);
    dealer.addCard(card);
}

// --------------------------------------------------
// Dealer
// --------------------------------------------------

void Game::dealerTurn()
{
    bool doNotDraw = false;
    int numBusted = 0;
    for (Hand hand : player.hands)
    {
        if (hand.isBlackjack() && !hand.isSplit())
        {
    for (Hand hand : player.hands)
    {
        if (hand.isBlackjack() && !hand.isSplit())
        {
            doNotDraw = true;
        }

        if (hand.isBust())
        {
        if (hand.isBust())
        {
            numBusted++;
        }
    }
    if (numBusted == player.hands.size())
    }
    if (numBusted == player.hands.size())
        doNotDraw = true;


    // Hit soft 17 (H17 rule): dealer must draw when total is 17 and soft.
    while ((dealer.getValue() < Constants::DEALER_STAND ||
            (dealer.getValue() == Constants::DEALER_STAND && dealer.isSoft())) &&
    while ((dealer.getValue() < Constants::DEALER_STAND ||
            (dealer.getValue() == Constants::DEALER_STAND && dealer.isSoft())) &&
           !doNotDraw)
    {
        std::cout
            << "Dealer has "
            << dealer.getValue()
            << "\n";
            << "Dealer has "
            << dealer.getValue()
            << "\n";

        dealCardToDealer();
    }

    if (dealer.isBust())
    {
    if (dealer.isBust())
    {
        std::cout
            << "Dealer busted with "
            << dealer.getValue()
            << ". Too many!\n";
    }
    else
    {

            << "Dealer busted with "
            << dealer.getValue()
            << ". Too many!\n";
    }
    else
    {

        switch (dealer.getValue())
        {
        case 17:
            std::cout
                << "Dealer stands on the edge of "
                << dealer.getValue()
                << "." << std::endl;
                << "Dealer stands on the edge of "
                << dealer.getValue()
                << "." << std::endl;
            break;
        case 18:
            std::cout
                << "Dealer stands on "
                << dealer.getValue()
                << "." << std::endl;
                << "Dealer stands on "
                << dealer.getValue()
                << "." << std::endl;
            break;
        case 19:
            std::cout
                << "Dealer stands on "
                << dealer.getValue()
                << ". It's bigger than average." << std::endl;
                << "Dealer stands on "
                << dealer.getValue()
                << ". It's bigger than average." << std::endl;
            break;
        case 20:
            std::cout
                << "Dealer stands on "
                << dealer.getValue()
                << ". Dealer always has 20." << std::endl;
                << "Dealer stands on "
                << dealer.getValue()
                << ". Dealer always has 20." << std::endl;
            break;
        case 21:
            std::cout
                << "Dealer stands on "
                << dealer.getValue()
                << ". That's the name of the game folks." << std::endl;
                << "Dealer stands on "
                << dealer.getValue()
                << ". That's the name of the game folks." << std::endl;
            break;
        default:
            break;
        }
    }

    evaluateHands();

    if (gameState ==
        GameState::LastHand)

    if (gameState ==
        GameState::LastHand)
    {
        gameState =
            GameState::GameOver;

        // End of shoe stats
        getStats().shoesFinished++;
        shoeCumulativeWinLoss = 0; // reset for next shoe

        // do end game saving here
        // do end game saving here

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
    PlayerStats &stats = getStats();
    PlayerStats &stats = getStats();

    // Build the new RoundRecord
    currentRoundRecord.dealerCards.clear();
    for (size_t i = 0; i < dealer.size(); i++)
        currentRoundRecord.dealerCards.push_back(dealer.getCard(i));

    currentRoundRecord.playerHands.clear();
    currentRoundRecord.runningCount = runningCount;
    currentRoundRecord.trueCount = findTrueCount();
    currentRoundRecord.cardsRemaining = deck.size();
    currentRoundRecord.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

    // Evaluate each hand
    for (size_t handIndex = 0; handIndex < player.hands.size(); handIndex++)
    {
        const Hand &hand = player.hands[handIndex];
        const Hand &hand = player.hands[handIndex];

        std::cout << "Entering evaluateHands() on hand " << (handIndex + 1)
        std::cout << "Entering evaluateHands() on hand " << (handIndex + 1)
                  << ". Bankroll = " << player.getBankroll() << std::endl;

        HandOutcome outcome;
        outcome.wager = hand.getBet();
        outcome.actions = handActions[handIndex];

        if (handIndex == 0)
        {
            outcome.insuranceBet = hand.getInsuranceBet();
            outcome.insuranceTaken = (outcome.insuranceBet > 0);
        }

        if (handIndex == 0)
        {
            outcome.insuranceBet = hand.getInsuranceBet();
            outcome.insuranceTaken = (outcome.insuranceBet > 0);
        }

        int netProfitOnHand = 0;

        if (hand.isSurrendered())
        {
            player.winBet(hand.getBet() / 2);
            outcome.payout = hand.getBet() / 2;
            outcome.won = false;
            stats.losses++;
            outcome.surrendered = true;
            outcome.surrendered = true;
            netProfitOnHand = hand.getBet() / 2 - hand.getBet();
        }
        else if (hand.isBust() || (hand.getValue() < dealer.getValue() && !dealer.isBust()))
        {
            player.loseBet();
            outcome.payout = 0;
            outcome.won = false;
            stats.losses++;
            netProfitOnHand = -hand.getBet();
        }
        else if (hand.getValue() > dealer.getValue() || dealer.isBust())
        {
            player.winBet(hand.getBet() * 2);
            outcome.payout = hand.getBet();
            outcome.won = true;
            stats.wins++;
            netProfitOnHand = hand.getBet();
        }
        else
        {
            player.pushBet(hand.getBet());
            outcome.payout = hand.getBet();
            outcome.won = true; // push counts as non-loss
            stats.pushes++;
            netProfitOnHand = 0;
        }

        // Capture the cards for this hand
        for (size_t c = 0; c < hand.size(); c++)
            outcome.cards.push_back(hand.getCard(c));

        currentRoundRecord.playerHands.push_back(outcome);

        // Track hands finished
        stats.handsFinished++;

        // Track greatest single hand win
        if (netProfitOnHand > stats.greatestHand)
            stats.greatestHand = netProfitOnHand;

        // Update cumulative trackers
        shoeCumulativeWinLoss += netProfitOnHand;
        sessionCumulativeWinLoss += netProfitOnHand;

        // Update shoe-level extremes
        if (shoeCumulativeWinLoss > stats.greatestWin)
            stats.greatestWin = shoeCumulativeWinLoss;
        if (shoeCumulativeWinLoss < stats.greatestLoss)
            stats.greatestLoss = shoeCumulativeWinLoss;

        // Update session-level extremes
        if (sessionCumulativeWinLoss > stats.greatestSessionWin)
            stats.greatestSessionWin = sessionCumulativeWinLoss;
        if (sessionCumulativeWinLoss < stats.greatestSessionLoss)
            stats.greatestSessionLoss = sessionCumulativeWinLoss;
    }

    std::cout << "Exiting evaluation with " << player.getBankroll() << std::endl;

    // Save to profile and shoe record
    PlayerProfile profile = player.getProfile();
    profile.addRoundRecord(currentRoundRecord);
    player.updateProfile(profile);

    currentShoeRecord.rounds.push_back(currentRoundRecord);

    handActions.clear();
    roundState = RoundState::Cleanup;
}

char Game::giveBasicStrategyAdvice(Hand &playerHand, Hand &dealerHand)
{
    Card firstCard = playerHand.getCard(0);
    Card lastCard = playerHand.getCard(1);
    char choice = ' ';
    if (firstCard.isAce() || lastCard.isAce())
    {
        // soft hands
        if (firstCard.isAce())
        {
            choice = BasicStrategy::SOFT[lastCard.getBlackjackValue() - 2][dealerHand.getDealerUpCard().getBlackjackValue() - 2];
        }
        else
        {
            choice = BasicStrategy::SOFT[firstCard.getBlackjackValue() - 2][dealerHand.getDealerUpCard().getBlackjackValue() - 2];
        }
    }
    else if (firstCard.getBlackjackValue() == lastCard.getBlackjackValue())
    {
        // split hands
        choice = BasicStrategy::PAIRS[firstCard.getBlackjackValue() - 2][dealerHand.getDealerUpCard().getBlackjackValue() - 2];
    }
    else
    {
        // hard hands
        if (playerHand.getValue() == 15)
        {
            choice = BasicStrategy::SURRENDER[0][dealerHand.getDealerUpCard().getBlackjackValue() - 2];
        }
        else if (playerHand.getValue() == 16)
        {
            choice = BasicStrategy::SURRENDER[1][dealerHand.getDealerUpCard().getBlackjackValue() - 2];
        }

        if (choice == ' ')
        {
            if (playerHand.getValue() > 17)
                choice = 'S';
            else if (playerHand.getValue() < 8)
                choice = 'H';
            else
                choice = BasicStrategy::HARD[playerHand.getValue() - 8][dealerHand.getDealerUpCard().getBlackjackValue() - 2];
        }
    }

    return choice;
}

// --------------------------------------------------
// Drawing
// --------------------------------------------------

void Game::draw()
{
    window.clear();

    window.draw(*backgroundSprite);

    drawDealer();
    drawPlayers();
    drawChips();
    // drawDeck();
    if (roundState == RoundState::OfferInsurance)
    // drawDeck();
    if (roundState == RoundState::OfferInsurance)
        drawInsurance();

    updateHUDState();
    hud.update(hudState);
    hud.draw(window);


    window.display();
}

void Game::resizeView(sf::RenderWindow &window, sf::View &view)
void Game::resizeView(sf::RenderWindow &window, sf::View &view)
{
    float windowRatio = static_cast<float>(window.getSize().x) /
                        static_cast<float>(window.getSize().y);

    float viewRatio = view.getSize().x / view.getSize().y;

    if (windowRatio > viewRatio)
    {
        // Window is wider: add horizontal space
        view.setViewport(sf::FloatRect(
            {(1.f - viewRatio / windowRatio) / 2.f, 0.f},
            {viewRatio / windowRatio, 1.f}));
    }
    else
    {
        // Window is taller: add vertical space
        view.setViewport(sf::FloatRect(
            {0.f, (1.f - windowRatio / viewRatio) / 2.f},
            {1.f, windowRatio / viewRatio}));
    }

    window.setView(view);
}

void Game::drawDealer()
{
    if (roundState == RoundState::PlayerTurn || roundState == RoundState::OfferInsurance)
    if (roundState == RoundState::PlayerTurn || roundState == RoundState::OfferInsurance)
    {
        auto back =
            Graphics::makeCardBackSprite(
                cardTexture);

        back.setPosition(sf::Vector2f(Constants::DEALER_X, Constants::DEALER_Y));

        window.draw(back);

        auto up =
            Graphics::makeCardSprite(
                dealer.getCard(0),
                cardTexture);

        up.setPosition(sf::Vector2f(Constants::DEALER_X - 10, Constants::DEALER_Y));

        window.draw(up);

        return;
    }

    for (std::size_t i = 0; i < dealer.size(); i++)
    for (std::size_t i = 0; i < dealer.size(); i++)
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
    for (std::size_t handIndex = 0;
         handIndex < player.hands.size();
         handIndex++)
    for (std::size_t handIndex = 0;
         handIndex < player.hands.size();
         handIndex++)
    {
        const Hand &hand =
        const Hand &hand =
            player.hands[handIndex];

        for (std::size_t i = 0;
             i < hand.size();
             i++)
        for (std::size_t i = 0;
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


void Game::drawChips()
{
    struct Chip
    {
        int value;
        sf::Color color;
    };

    const std::vector<Chip> denominations =
        {
            {50000, sf::Color(128, 0, 128)}, // $500
            {10000, sf::Color::Black},       // $100
            {2500, sf::Color(0, 180, 0)},    // $25
            {500, sf::Color::Red},           // $5
            {250, sf::Color(255, 105, 180)}, // $2.50
            {100, sf::Color::White}          // $1
        };
        {
            {50000, sf::Color(128, 0, 128)}, // $500
            {10000, sf::Color::Black},       // $100
            {2500, sf::Color(0, 180, 0)},    // $25
            {500, sf::Color::Red},           // $5
            {250, sf::Color(255, 105, 180)}, // $2.50
            {100, sf::Color::White}          // $1
        };

    int handIndex = 0;
    for (Hand &hand : player.hands)
    {
    for (Hand &hand : player.hands)
    {
        int totalBet = hand.getBet();
        int stackBet = hand.isDoubledDown() ? totalBet / 2 : totalBet;

        if (totalBet <= 0)
        if (totalBet <= 0)
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
            45.f;

        //
        // Convert dollars to cents
        //
        int remaining =
            stackBet * 100;

        std::vector<Chip> stack;

        for (const auto &chip : denominations)
        for (const auto &chip : denominations)
        {
            while (remaining >= chip.value)
            while (remaining >= chip.value)
            {
                stack.push_back(chip);
                remaining -= chip.value;
            }
        }

        //
        // Draw stack
        //
        for (std::size_t i = 0;
             i < stack.size();
             i++)
        for (std::size_t i = 0;
             i < stack.size();
             i++)
        {
            sf::CircleShape chip(Constants::CHIP_RADIUS);

            chip.setFillColor(
                stack[i].color);

            chip.setOutlineColor(
                sf::Color::White);

            chip.setOutlineThickness(1.f);

            chip.setOrigin(sf::Vector2f(Constants::CHIP_RADIUS, Constants::CHIP_RADIUS));

            chip.setPosition(sf::Vector2f(chipX, chipY - i * 3.f));

            window.draw(chip);
        }

        //
        // Draw total bet
        //

        sf::Text text(FontManager::get(), "$" + std::to_string(totalBet), 16.f);
        text.setFillColor(sf::Color::White);

        sf::FloatRect bounds =
            text.getLocalBounds();

        text.setOrigin(sf::Vector2f(bounds.size.x / 2.f, bounds.size.y / 2.f));

        text.setPosition(sf::Vector2f(chipX, chipY - stack.size() * 3.f));

        window.draw(text);

        //
        // Draw second stack when doubled
        //
        if (hand.isDoubledDown())
        if (hand.isDoubledDown())
        {
            float secondX =
                chipX + 35.f;

            for (std::size_t i = 0;
                 i < stack.size();
                 i++)
            for (std::size_t i = 0;
                 i < stack.size();
                 i++)
            {
                sf::CircleShape chip(Constants::CHIP_RADIUS);

                chip.setFillColor(
                    stack[i].color);

                chip.setOutlineColor(
                    sf::Color::White);

                chip.setOutlineThickness(1.f);

                chip.setOrigin(sf::Vector2f(Constants::CHIP_RADIUS, Constants::CHIP_RADIUS));

                chip.setPosition(sf::Vector2f(secondX, chipY - i * 3.f));

                window.draw(chip);
            }
        }
        handIndex++;
    }
}

void Game::drawDeck()
{
    for (std::size_t i = 0;
         i < deck.size();
         i++)
    for (std::size_t i = 0;
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

void Game::drawInsurance()
{
    sf::Text text(FontManager::get(),
                  "Dealer shows an Ace\n\n"
                  "Insurance Bet: $" +
                      std::to_string(player.hands[player.currentHand].getInsuranceBet()) +
                      "\n\n"
                      "Up/Down : Change\n"
                      "Enter : Confirm\n"
                      "Esc : Decline",
                  12.f);
    text.setFillColor(sf::Color::White);

    text.setPosition(sf::Vector2f(400.f, 500.f));

    window.draw(text);
}