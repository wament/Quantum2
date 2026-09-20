#include "Game.hpp"

#include <iostream>
#include <stdexcept>
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
               static_cast<float>(Constants::WINDOW_HEIGHT)}))
{
    window.setView(gameView);
    players.push_back(Player("William"));
    loadAssets();
    
    // Create replay BEFORE startNewGame()
    replay = std::make_unique<Replay>(window, cardTexture, FontManager::get());
    auto profile = players[0].getProfile();
    replay->loadHistory(profile.getLongHistory());
    replay->setStartingBankroll(players[0].getBankroll());
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
}

void Game::run()
{
    sf::Clock clock;

    while (window.isOpen())
    {
        deltaTime = clock.restart().asSeconds(); // time since last frame
        processEvents();
        update(deltaTime); // ← change: pass deltaTime
        draw();
    }
}

// --------------------------------------------------
// Setup
// --------------------------------------------------

void Game::startNewGame()
{
    // Load fresh bankroll from profile
    auto profile = players[0].getProfile();

    // Create fresh shoe
    deck.create(Constants::NUM_DECKS);
    deck.shuffle();
    currentShoeRecord.initialDeck = deck.getCards();
    currentShoeRecord.rounds.clear();

    // Reset game state
    runningCount = 0;
    shoeCumulativeWinLoss = 0;
    sessionCumulativeWinLoss = 0;
    handActions.clear();
    handActions.resize(1);
    dealer.clear();
    players[0].resetRound(0);
    nextRound();

    // Switch mode
    appMode = AppMode::Playing;
}

void Game::loadAssets()
{
    std::cout << "Current working directory: "
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
    // Reset all players for new round
    for (auto& p : players)
    {
        p.resetRound(initalBet);
    }
    dealer.clear();
    currentRoundRecord = {};
    handActions.clear();
    handActions.resize(1); // start with one hand's actions
    roundState = RoundState::PlaceBets;
    activePlayerIndex = 0;
}

// --------------------------------------------------
// Events
// --------------------------------------------------

void Game::processEvents()
{
    while (true)
    {
        auto event = window.pollEvent();
        
        if (!event.has_value())
            break;

        if (event->is<sf::Event::Closed>())
        {
            players[0].updateProfile(players[0].getProfile());
            players[0].saveProfile();
            window.close();
            return;
        }

        if (event->is<sf::Event::Resized>())
        {
            resizeView(window, gameView);
            continue;
        }

        switch (appMode)
        {
        case AppMode::MainMenu:
            processMenuEvents(event.value());
            break;
        case AppMode::Playing:
            processGameEvents(event.value());
            break;
        case AppMode::Paused:
            processPauseEvents(event.value());
            break;
        case AppMode::ReplayHistory:
        case AppMode::ReplayPlaying:
        {
            sf::Event replayEvent = event.value();
            replay->processEvents(replayEvent);
            if (appMode == AppMode::ReplayHistory &&
                replayEvent.is<sf::Event::KeyPressed>() &&
                replayEvent.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape)
            {
                appMode = AppMode::MainMenu;
                menuSelection = MenuOption::PlayGame;
            }
            break;
        }
        }
    }
}

void Game::processMenuEvents(const sf::Event &event)
{
    // Mouse click support
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (clicked->button == sf::Mouse::Button::Left)
        {
            float cx = static_cast<float>(window.getSize().x) / 2.0f;
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(clicked->position.x, clicked->position.y));

            // Check Play Game (y=250)
            if (std::abs(mousePos.y - 250.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                menuSelection = MenuOption::PlayGame;
                startNewGame();
                return;
            }
            // Check Review History (y=300)
            if (std::abs(mousePos.y - 300.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                menuSelection = MenuOption::ReviewHistory;
                appMode = AppMode::ReplayHistory;
                return;
            }
        }
    }

    auto pressed = event.getIf<sf::Event::KeyPressed>();
    if (!pressed)
        return;

    switch (pressed->code)
    {
    case sf::Keyboard::Key::Up:
        menuSelection = MenuOption::PlayGame;
        break;
    case sf::Keyboard::Key::Down:
        menuSelection = MenuOption::ReviewHistory;
        break;
    case sf::Keyboard::Key::Enter:
        switch (menuSelection)
        {
        case MenuOption::PlayGame:
            startNewGame(); // creates deck, resets, deals first hand
            break;
        case MenuOption::ReviewHistory:
        {
            auto profile = players[0].getProfile();
            replay->loadHistory(profile.getLongHistory());
            replay->setStartingBankroll(players[0].getBankroll());
            appMode = AppMode::ReplayHistory;
            break;
        }
        }
        break;
    case sf::Keyboard::Key::Escape:
        window.close();
        break;
    }
}

void Game::processPauseEvents(const sf::Event &event)
{
    // Mouse click support
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (clicked->button == sf::Mouse::Button::Left)
        {
            float cx = static_cast<float>(window.getSize().x) / 2.0f;
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Vector2i(clicked->position.x, clicked->position.y));

            // Check Resume (y=200)
            if (std::abs(mousePos.y - 200.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                appMode = AppMode::Playing;
                return;
            }
            // Check Restart (y=250)
            if (std::abs(mousePos.y - 250.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                {
                    PlayerProfile profile = players[0].getProfile();
                    profile.addShoeRecord(currentShoeRecord);
                    players[0].updateProfile(profile);
                    players[0].saveProfile();
                }
                startNewGame();
                return;
            }
            // Check Quit to Menu (y=300)
            if (std::abs(mousePos.y - 300.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                {
                    PlayerProfile profile = players[0].getProfile();
                    profile.addShoeRecord(currentShoeRecord);
                    players[0].updateProfile(profile);
                    players[0].saveProfile();
                }
                appMode = AppMode::MainMenu;
                menuSelection = MenuOption::PlayGame;
                return;
            }
        }
    }

    auto pressed = event.getIf<sf::Event::KeyPressed>();
    if (!pressed)
        return;

    switch (pressed->code)
    {
    case sf::Keyboard::Key::Up:
    case sf::Keyboard::Key::W:
        if (pauseSelection == PauseOption::Resume)
            pauseSelection = PauseOption::QuitToMenu;
        else if (pauseSelection == PauseOption::Restart)
            pauseSelection = PauseOption::Resume;
        else
            pauseSelection = PauseOption::Restart;
        break;
    case sf::Keyboard::Key::Down:
    case sf::Keyboard::Key::S:
        if (pauseSelection == PauseOption::Resume)
            pauseSelection = PauseOption::Restart;
        else if (pauseSelection == PauseOption::Restart)
            pauseSelection = PauseOption::QuitToMenu;
        else
            pauseSelection = PauseOption::Resume;
        break;
    case sf::Keyboard::Key::Enter:
    case sf::Keyboard::Key::Space:
        switch (pauseSelection)
        {
        case PauseOption::Resume:
            appMode = AppMode::Playing;
            break;
        case PauseOption::Restart:
        {
            PlayerProfile profile = players[0].getProfile();
            profile.addShoeRecord(currentShoeRecord);
            players[0].updateProfile(profile);
            players[0].saveProfile();
            startNewGame();
            break;
        }
        case PauseOption::QuitToMenu:
        {
            PlayerProfile profile = players[0].getProfile();
            profile.addShoeRecord(currentShoeRecord);
            players[0].updateProfile(profile);
            players[0].saveProfile();
            appMode = AppMode::MainMenu;
            menuSelection = MenuOption::PlayGame;
            break;
        }
        }
        break;
    case sf::Keyboard::Key::Escape:
        appMode = AppMode::Playing;
        break;
    }
}

void Game::processGameEvents(const sf::Event &event)
{
    // Mouse button pressed (slider)
    if (auto pressed = event.getIf<sf::Event::MouseButtonPressed>())
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
    if (auto released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (released->button == sf::Mouse::Button::Left && hudState.riskSliderDragging)
        {
            hudState.riskSliderDragging = false;
        }
    }

    // Mouse moved (slider)
    if (auto moved = event.getIf<sf::Event::MouseMoved>())
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

    // Key pressed - only process for human player (index 0)
    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        // Only human player responds to input
        if (activePlayerIndex != 0)
            return;

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
                float tc = BasicStrategy::computeTrueCount(runningCount, deck.size());
                int optimal = calculateOptimalBet(tc, kellyFraction, players[0].getBankroll() + players[0].hands[0].getBet(),
                                                  Constants::MINIMUM_BET, Constants::MAXIMUM_BET);
                players[0].hands[0].setBet(optimal);
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
        }
        else if (roundState == RoundState::OfferInsurance)
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
        }
        else if (roundState == RoundState::PlayerTurn)
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
                char choice = giveBasicStrategyAdvice(players[activePlayerIndex].hands[players[activePlayerIndex].currentHand], dealer);
                hudState.basicStrategyHint = "Book says: " + std::string(1, choice);
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

        if (key->code == sf::Keyboard::Key::Escape)
        {
            appMode = AppMode::Paused;
            pauseSelection = PauseOption::Resume;
        }
    }
}

// --------------------------------------------------
// Betting
// --------------------------------------------------

void Game::update(float deltaTime)
{
    if (appMode == AppMode::MainMenu)
        return;

    if (appMode == AppMode::ReplayPlaying || appMode == AppMode::ReplayHistory)
    {
        replay->update(deltaTime);
        return;
    }
    if (appMode != AppMode::Playing)
        return;

    if (roundState == RoundState::PlayerTurn)
    {
        // Check if all players are done
        bool allDone = true;
        for (auto& p : players)
        {
            if (p.currentHand < p.hands.size() && !p.hands[p.currentHand].isFinished())
            {
                allDone = false;
                break;
            }
        }
        if (allDone)
        {
            roundState = RoundState::DealerTurn;
            return;
        }

        // Advance active player to next unfinished hand or next player
        while (activePlayerIndex < players.size())
        {
            auto& p = players[activePlayerIndex];
            // Skip finished hands
            while (p.currentHand < p.hands.size() && p.hands[p.currentHand].isFinished())
            {
                ++p.currentHand;
            }
            // If this player has unfinished hands, process them
            if (p.currentHand < p.hands.size())
            {
                break;
            }
            // Move to next player
            ++activePlayerIndex;
        }

        if (activePlayerIndex >= players.size())
        {
            roundState = RoundState::DealerTurn;
            return;
        }

        auto& p = players[activePlayerIndex];
        
        // Ensure current hand has at least two cards (for split hands)
        if (p.hands[p.currentHand].size() == 1)
        {
            dealCardToPlayer(p.hands[p.currentHand]);
            // Split ace rule: one card only, auto-stand
            if (p.hands[p.currentHand].isSplitAce())
            {
                p.hands[p.currentHand].setFinished(true);
            }
        }

        // If NPC player, auto-play
        if (activePlayerIndex > 0)
        {
            npcTakeTurn();
        }
    }

    if (roundState == RoundState::DealerTurn)
    {
        dealerTurn();
    }
}

void Game::updateHUDState()
{
    hudState.bankroll = players[0].getBankroll();

    hudState.currentBet = players[0].hands[0].getBet();

    hudState.runningCount = runningCount;

    hudState.trueCount = BasicStrategy::computeTrueCount(runningCount, deck.size());

    hudState.cardsRemaining = deck.size();

    hudState.kellyFraction = kellyFraction;

    hudState.statusMessage = gameMessage;

    hudState.playerStats = &getStats();

    hudState.currentShoe = &currentShoeRecord;
}

// --------------------------------------------------
// Player actions
// --------------------------------------------------

void Game::hit()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];

    if (hand.isFinished())
        return;

    logStrategyDeviation('H');
    dealCardToPlayer(hand);

    std::cout
        << "Hit! You have "
        << hand.getValue()
        << ". " << std::endl;

    if (hand.isBust())
    {
        std::cout
            << "Bust!\n"
            << std::endl;

        hand.setFinished(true);
    }

    handActions[p.currentHand].push_back("H");
    hudState.basicStrategyHint.clear();
}

void Game::stand()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];
    std::cout
        << "Stand on "
        << hand.getValue()
        << ".\n";
    logStrategyDeviation('S');
    hand.setFinished(true);

    handActions[p.currentHand].push_back("S");
    hudState.basicStrategyHint.clear();
}

void Game::doubleDown()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];
    int initalbet = hand.getBet();

    if (!hand.canDouble())
        return;

    logStrategyDeviation('D');
    hand.setDoubledDown(true);
    if (!p.placeBet(initalbet))
        return;

    hand.setBet(initalbet * 2);
    dealCardToPlayer(hand);

    // Track double stats
    PlayerStats &stats = getStats();
    stats.doubles++;
    stats.betsPlaced++;
    stats.betSum += initalbet;

    std::cout
        << "Doubling "
        << initalbet << " to " << hand.getBet()
        << " on a "
        << hand.getValue()
        << "!\n";

    hand.setFinished(true);

    handActions[p.currentHand].push_back("D");
    hudState.basicStrategyHint.clear();
}

void Game::split()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];

    if (p.hands.size() >= Constants::MAX_SPLIT_HANDS)
    {
        return;
    }

    if (!hand.canSplit())
        return;

    logStrategyDeviation('P');
    Hand newHand;
    Card splitCard = hand.takeLastCard();
    std::cout
        << "Splitting "
        << splitCard.getBlackjackValueToString()
        << "s.\n";
    newHand.addCard(splitCard); 

    handActions[p.currentHand].push_back("P");
    hudState.basicStrategyHint.clear();

    dealCardToPlayer(hand);

    p.hands.push_back(newHand);
    handActions.push_back({}); // new action vector for the split hand
    hand.setSplit(true, hand.getCard(0).getBlackjackValue());
    newHand.setSplit(true, hand.getCard(0).getBlackjackValue());

    // Track split stats
    PlayerStats &stats = getStats();
    stats.splits++;
    stats.betsPlaced++;
    stats.betSum += hand.getBet(); // new hand inherits bet amount
}

void Game::surrender()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];

    // Cannot surrender a finished hand
    if (hand.isFinished())
        return;

    // Only allowed on the first two cards
    if (!hand.canDouble())
        return;

    logStrategyDeviation('R');
    std::cout
        << "Surrender. You lose half your bet.\n";

    hand.setSurrendered(true);
    hand.setFinished(true);

    handActions[p.currentHand].push_back("R");
    hudState.basicStrategyHint.clear();
}

// --------------------------------------------------
// NPC Decision Making
// --------------------------------------------------

void Game::npcTakeTurn()
{
    auto& p = players[activePlayerIndex];
    Hand& hand = p.hands[p.currentHand];

    if (hand.isFinished())
        return;

    // Get basic strategy recommendation
    char action = giveBasicStrategyAdvice(hand, dealer);

    switch (action)
    {
    case 'H':
        hit();
        break;
    case 'S':
        stand();
        break;
    case 'D':
        doubleDown();
        break;
    case 'P':
        split();
        break;
    case 'R':
        surrender();
        break;
    default:
        stand();
        break;
    }
}

// --------------------------------------------------
// Betting
// --------------------------------------------------

void Game::increaseBet()
{
    if (players[0].hands[0].getBet() < Constants::MAXIMUM_BET)
    {
        players[0].hands[0].setBet(players[0].hands[0].getBet() + 5);
        std::cout << "Increased bet to "
                  << players[0].hands[players[0].currentHand].getBet()
                  << ".\n";
    }
}

void Game::decreaseBet()
{
    if (players[0].hands[0].getBet() >= Constants::MINIMUM_BET)
    {
        players[0].hands[0].setBet(players[0].hands[0].getBet() - 5);
        std::cout << "Decreased bet to "
                  << players[0].hands[players[0].currentHand].getBet()
                  << ".\n";
    }
}

int Game::calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const
{
    return BasicStrategy::calculateKellyBet(trueCount, kellyFraction, bankroll, minBet, maxBet);
}

void Game::confirmBet()
{
    int currentBet = players[0].hands[0].getBet();
    if (currentBet < Constants::MINIMUM_BET || currentBet > Constants::MAXIMUM_BET)
        return;

    if (!players[0].placeBet(currentBet))
    {
        std::cout << "Not enough bankroll. " << players[0].getBankroll() << " is your max. Or "
                  << players[0].getBankroll() / 2 << " if you want to double.\n";
        return;
    }

    initalBet = currentBet;

    // Track bet stats
    PlayerStats &stats = getStats();
    stats.betsPlaced++;
    stats.betSum += currentBet;

    std::cout << "You bet "
              << players[0].hands[players[0].currentHand].getBet()
              << ". Your remaining bankroll is: "
              << players[0].getBankroll()
              << "\n"
              << "The running count is: "
              << runningCount
              << ".\n";

    // NPCs auto-bet minimum
    for (size_t i = 1; i < players.size(); i++)
    {
        int npcBet = Constants::MINIMUM_BET;
        if (players[i].placeBet(npcBet))
        {
            players[i].hands[0].setBet(npcBet);
            std::cout << players[i].getName() << " bets " << npcBet << ".\n";
        }
    }

    deal();
}

// --------------------------------------------------
// Insurance
// --------------------------------------------------

void Game::increaseInsurance()
{
    auto& p = players[activePlayerIndex];
    if (p.hands[p.currentHand].getInsuranceBet() < getMaxInsurance())
    {
        p.hands[p.currentHand].setInsuranceBet(
            p.hands[p.currentHand].getInsuranceBet() + 1);
        std::cout << "Increased insurance to "
                  << p.hands[p.currentHand].getInsuranceBet()
                  << ".\n";
    }
}

void Game::decreaseInsurance()
{
    auto& p = players[activePlayerIndex];
    if (p.hands[p.currentHand].getInsuranceBet() > 0)
    {
        p.hands[p.currentHand].setInsuranceBet(
            p.hands[p.currentHand].getInsuranceBet() - 1);
        std::cout << "Decreased insurance to "
                  << p.hands[p.currentHand].getInsuranceBet()
                  << ".\n";
    }
}

void Game::confirmInsurance()
{
    auto& p = players[activePlayerIndex];
    int insBet = p.hands[p.currentHand].getInsuranceBet();
    if (insBet > 0)
    {
        p.placeBet(insBet);
        std::cout << "Placed insurance bet of"
                  << insBet
                  << ".\n";
    }

    evaluateBlackjack();
}

void Game::declineInsurance()
{
    auto& p = players[activePlayerIndex];
    p.hands[p.currentHand].setInsuranceBet(0);
    std::cout << "Insurance Denied" << std::endl;
    evaluateBlackjack();
}

const int Game::getMaxInsurance()
{
    auto& p = players[activePlayerIndex];
    return p.hands[p.currentHand].getBet() / 2;
}

// --------------------------------------------------
// Deck / Deal
// --------------------------------------------------

void Game::deal()
{
    // Deal to all players
    for (auto& p : players)
    {
        dealCardToPlayer(p.hands[0]);
        dealCardToDealer();
    }
    for (auto& p : players)
    {
        dealCardToPlayer(p.hands[0]);
        dealCardToDealer();
    }

    roundState = RoundState::PlayerTurn;

    if (dealer.getDealerUpCard().isAce())
    {
        std::cout << "offered insurance" << std::endl;
        roundState = RoundState::OfferInsurance;
    }
    else
        evaluateBlackjack();

    std::cout
        << "You have a "
        << players[0].hands[0].getValue()
        << " against dealers "
        << dealer.getDealerUpCardToString()
        << ".\n";
}

void Game::evaluateBlackjack()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];

    int bet = hand.getBet();

    if (dealer.isBlackjack())
    {
        if (hand.getInsuranceBet() > 0)
        {
            // Pays 2:1, plus the original insurance bet back.
            p.winBet(hand.getInsuranceBet() * 3);

            std::cout << "Insurance wins!\n";
        }

        if (hand.isBlackjack())
        {
            p.pushBet(bet);
        }
        else
        {
            p.loseBet();
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
        roundState = RoundState::PlayerTurn;
    }

    // Only natural (non-split) blackjack pays 3:2
    if (hand.isBlackjack() && !hand.isSplit())
    {
        p.winBet(bet * Constants::BLACKJACK_PAYOUT);

        std::cout << "Blackjack!\n";

        // Track blackjack (non-split only)
        getStats().blackjacks++;

        roundState = RoundState::DealerTurn;
    }
}

void Game::offerInsurance()
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];

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

    p.placeBet(insurance);

    std::cout << "Insurance bet of $"
              << insurance
              << " placed.\n";
}

// Central helper: apply Hi-Lo count update and check for cut card after drawing
void Game::afterDraw(Card card)
{
    runningCount = BasicStrategy::updateHiLo(runningCount, card.getBlackjackValue());

    if (deck.reachedCutCard())
    {
        gameState = GameState::LastHand;
    }
}

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
    for (auto& p : players)
    {
        for (Hand hand : p.hands)
        {
            if (hand.isBlackjack() && !hand.isSplit())
            {
                doNotDraw = true;
            }

            if (hand.isBust())
            {
                numBusted++;
            }
        }
    }
    
    int totalHands = 0;
    for (auto& p : players)
    {
        totalHands += p.hands.size();
    }
    
    if (numBusted == totalHands)
        doNotDraw = true;

    // Hit soft 17 (H17 rule): dealer must draw when total is 17 and soft.
    while ((dealer.getValue() < Constants::DEALER_STAND ||
            (dealer.getValue() == Constants::DEALER_STAND && dealer.isSoft())) &&
           !doNotDraw)
    {
        std::cout
            << "Dealer has "
            << dealer.getValue()
            << "\n";

        dealCardToDealer();
    }

    if (dealer.isBust())
    {
        std::cout
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

    if (gameState ==
        GameState::LastHand)
    {
        gameState =
            GameState::GameOver;

        // End of shoe stats
        getStats().shoesFinished++;
        shoeCumulativeWinLoss = 0; // reset for next shoe

        // do end game saving here

        PlayerProfile profile = players[0].getProfile();

        profile.addShoeRecord(currentShoeRecord);
        players[0].updateProfile(profile);
        players[0].saveProfile();

        currentShoeRecord = {};
        std::cout
            << "New shoe required. Press R.\n";
    }
}

void Game::evaluateHands()
{
    PlayerStats &stats = getStats();

    // Track win/loss streak
    bool roundWon = false;
    bool roundLost = false;

    // Build the new RoundRecord
    currentRoundRecord.dealerCards.clear();
    for (size_t i = 0; i < dealer.size(); i++)
        currentRoundRecord.dealerCards.push_back(dealer.getCard(i));

    currentRoundRecord.playerHands.clear();
    currentRoundRecord.runningCount = runningCount;
    currentRoundRecord.trueCount = BasicStrategy::computeTrueCount(runningCount, deck.size());
    currentRoundRecord.cardsRemaining = deck.size();
    currentRoundRecord.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

    // Evaluate each hand for all players
    for (auto& p : players)
    {
        for (size_t handIndex = 0; handIndex < p.hands.size(); handIndex++)
        {
            const Hand &hand = p.hands[handIndex];

            std::cout << "Entering evaluateHands() on hand " << (handIndex + 1)
                      << ". Bankroll = " << p.getBankroll() << std::endl;

            HandOutcome outcome;
            outcome.wager = hand.getBet();
            outcome.actions = handActions[handIndex];

            int netProfitOnHand = 0;

            if (hand.isSurrendered())
            {
                p.winBet(hand.getBet() / 2);
                outcome.payout = hand.getBet() / 2;
                outcome.won = false;
                stats.losses++;
                netProfitOnHand = hand.getBet() / 2 - hand.getBet();
            }
            else if (hand.isBust() || (hand.getValue() < dealer.getValue() && !dealer.isBust()))
            {
                p.loseBet();
                outcome.payout = 0;
                outcome.won = false;
                stats.losses++;
                netProfitOnHand = -hand.getBet();
                roundLost = true;
            }
            else if (hand.getValue() > dealer.getValue() || dealer.isBust())
            {
                p.winBet(hand.getBet() * 2);
                outcome.payout = hand.getBet();
                outcome.won = true;
                stats.wins++;
                netProfitOnHand = hand.getBet();
                roundWon = true;
            }
            else
            {
                p.pushBet(hand.getBet());
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
    }

    // Update streak based on round result
    if (roundWon && !roundLost)
    {
        if (hudState.streakIsWin)
            hudState.streak++;
        else
        {
            hudState.streak = 1;
            hudState.streakIsWin = true;
        }
    }
    else if (roundLost && !roundWon)
    {
        if (!hudState.streakIsWin)
            hudState.streak++;
        else
        {
            hudState.streak = 1;
            hudState.streakIsWin = false;
        }
    }
    else
    {
        // Mixed result (some wins, some losses) or all pushes - reset streak
        hudState.streak = 0;
    }

    std::cout << "Exiting evaluation with " << players[0].getBankroll() << std::endl;

    // Save to profile and shoe record
    PlayerProfile profile = players[0].getProfile();
    profile.addRoundRecord(currentRoundRecord);
    players[0].updateProfile(profile);
    players[0].saveProfile();

    currentShoeRecord.rounds.push_back(currentRoundRecord);

    handActions.clear();
    roundState = RoundState::Cleanup;
}

void Game::logStrategyDeviation(char actionTaken)
{
    auto& p = players[activePlayerIndex];
    Hand &hand = p.hands[p.currentHand];
    int playerValue = hand.getValue();
    int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
    bool hasAce = hand.isSoft();
    bool isPair = hand.size() == 2 &&
                  hand.getCard(0).getBlackjackValue() ==
                      hand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? hand.getCard(0).getBlackjackValue() : 0;

    char correct = BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);

    PlayerStats &stats = getStats();
    stats.strategyTotal++;

    if (actionTaken == correct)
    {
        stats.strategyHits++;
    }
    else
    {
        stats.strategyDeviations++;
        DeviationRecord record;
        record.playerValue = playerValue;
        record.dealerUpValue = dealerUpValue;
        record.wasSoft = hasAce;
        record.wasPair = isPair;
        record.pairValue = pairValue;
        record.actionTaken = actionTaken;
        record.correctAction = correct;
        record.bet = hand.getBet();
        record.won = false; // filled in later at evaluation time
        stats.recentDeviations.push_back(record);
        if (stats.recentDeviations.size() > 10)
            stats.recentDeviations.erase(stats.recentDeviations.begin());
    }
}

char Game::giveBasicStrategyAdvice(Hand &playerHand, Hand &dealerHand)
{
    int playerValue = playerHand.getValue();
    int dealerUpValue = dealerHand.getDealerUpCard().getBlackjackValue();
    bool hasAce = playerHand.isSoft();
    bool isPair = playerHand.size() == 2 &&
                  playerHand.getCard(0).getBlackjackValue() ==
                      playerHand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? playerHand.getCard(0).getBlackjackValue() : 0;

    return BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);
}

// --------------------------------------------------
// Drawing
// --------------------------------------------------

void Game::draw()
{
    window.clear();

    window.draw(*backgroundSprite);

    switch (appMode)
    {
    case AppMode::MainMenu:
        drawMainMenu();
        break;
    case AppMode::Playing:
        drawDealer();
        drawPlayers();
        drawChips();
        // drawDeck();
        if (roundState == RoundState::OfferInsurance)
            drawInsurance();

        updateHUDState();
        hud.update(hudState);
        hud.draw(window);
        break;
    case AppMode::Paused:
        // Draw the game underneath
        drawDealer();
        drawPlayers();
        drawChips();
        updateHUDState();
        hud.update(hudState);
        hud.draw(window);
        // Draw pause overlay
        drawPauseMenu();
        break;
    case AppMode::ReplayHistory:
    case AppMode::ReplayPlaying:
        replay->draw();
        break;
    }

    window.display();
}

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
    // Draw each player's hands
    for (size_t playerIndex = 0; playerIndex < players.size(); playerIndex++)
    {
        auto& p = players[playerIndex];
        float xOffset = playerIndex * 200.f; // offset each player's hands

        for (std::size_t handIndex = 0;
             handIndex < p.hands.size();
             handIndex++)
        {
            const Hand &hand = p.hands[handIndex];

            for (std::size_t i = 0;
                 i < hand.size();
                 i++)
            {
                Graphics::drawCard(
                    window,
                    cardTexture,
                    hand.getCard(i),
                    Constants::PLAYER_X +
                        xOffset +
                        handIndex * 142 +
                        i * 16,

                    Constants::PLAYER_Y -
                        i * 16);
            }
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

    // Draw chips for all players
    for (size_t playerIndex = 0; playerIndex < players.size(); playerIndex++)
    {
        auto& p = players[playerIndex];
        float xOffset = playerIndex * 200.f;

        int handIndex = 0;
        for (Hand &hand : p.hands)
        {
            int totalBet = hand.getBet();
            int stackBet = hand.isDoubledDown() ? totalBet / 2 : totalBet;

            if (totalBet <= 0)
            {
                handIndex++;
                continue;
            }

            //
            // Position based on the same math as cards
            //
            float handX =
                Constants::PLAYER_X +
                xOffset +
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
            {
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
            {
                float secondX =
                    chipX + 35.f;

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
}

void Game::drawDeck()
{
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
    auto& p = players[activePlayerIndex];
    sf::Text text(FontManager::get(),
                  "Dealer shows an Ace\n\n"
                  "Insurance Bet: $" +
                      std::to_string(p.hands[p.currentHand].getInsuranceBet()) +
                      "\n\n"
                      "Up/Down : Change\n"
                      "Enter : Confirm\n"
                      "Esc : Decline",
                  12.f);
    text.setFillColor(sf::Color::White);

    text.setPosition(sf::Vector2f(400.f, 500.f));

    window.draw(text);
}

void Game::drawMenuItem(const std::string &label, bool selected, float y)
{
    sf::Text text(FontManager::get(), label, 24);
    text.setFillColor(selected ? sf::Color(255, 215, 0) : sf::Color(200, 200, 200));

    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, bounds.size.y / 2.0f));
    text.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) / 2.0f, y));

    window.draw(text);
}


void Game::drawMainMenu()
{
    window.clear(sf::Color(20, 20, 30));

    float cx = static_cast<float>(window.getSize().x) / 2.0f;

    sf::Text title(FontManager::get(), "♠ BLACKJACK ♣", 36);
    title.setFillColor(sf::Color(255, 215, 0));
    sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.size.x / 2.0f, 0));
    title.setPosition(sf::Vector2f(cx, 150.0f));
    window.draw(title);

    drawMenuItem("Play Game", menuSelection == MenuOption::PlayGame, 250.0f);
    drawMenuItem("Review History", menuSelection == MenuOption::ReviewHistory, 300.0f);

    sf::Text instr(FontManager::get(), "↑/↓ navigate  •  Enter: select  •  Esc: quit", 14);
    instr.setFillColor(sf::Color(140, 140, 140));
    instr.setPosition(sf::Vector2f(cx, 400.0f));
    window.draw(instr);
}

void Game::drawPauseMenu()
{
    float w = static_cast<float>(window.getSize().x);
    float h = static_cast<float>(window.getSize().y);
    float cx = w / 2.0f;

    // Semi-transparent overlay (50% opacity)
    sf::RectangleShape overlay(sf::Vector2f(w, h));
    overlay.setFillColor(sf::Color(0, 0, 0, 128));
    window.draw(overlay);

    // Title
    sf::Text title(FontManager::get(), "PAUSED", 36);
    title.setFillColor(sf::Color(255, 215, 0));
    sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.size.x / 2.0f, 0));
    title.setPosition(sf::Vector2f(cx, h * 0.25f));
    window.draw(title);

    // Menu items
    drawMenuItem("Resume", pauseSelection == PauseOption::Resume, h * 0.40f);
    drawMenuItem("Restart", pauseSelection == PauseOption::Restart, h * 0.48f);
    drawMenuItem("Quit to Menu", pauseSelection == PauseOption::QuitToMenu, h * 0.56f);

    // Instructions
    sf::Text instr(FontManager::get(), "↑/↓ navigate  •  Enter: select  •  Esc: resume", 14);
    instr.setFillColor(sf::Color(140, 140, 140));
    sf::FloatRect ib = instr.getLocalBounds();
    instr.setOrigin(sf::Vector2f(ib.size.x / 2.0f, 0));
    instr.setPosition(sf::Vector2f(cx, h * 0.68f));
    window.draw(instr);
}