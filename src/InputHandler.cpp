#include "InputHandler.hpp"
#include "Game.hpp"
#include "Constants.hpp" 
#include "FontManager.hpp"
#include "Logger.hpp"

InputHandler::InputHandler(Game& game) : game(game) {}

void InputHandler::processEvents()
{
    while (true)
    {
        auto event = game.window.pollEvent();
        
        if (!event.has_value())
            break;

        if (event->is<sf::Event::Closed>())
        {
            Logger::info("=== Closing game ===");
            game.players[0].updateProfile(game.players[0].getProfile());
            game.players[0].saveProfile();
            game.window.close();
            return;
        }

        if (event->is<sf::Event::Resized>())
        {
            game.renderer.resizeView(game.window, game.gameView);
            continue;
        }

        switch (game.appMode)
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
            game.replay->processEvents(replayEvent);
            if (game.appMode == AppMode::ReplayHistory &&
                replayEvent.is<sf::Event::KeyPressed>() &&
                replayEvent.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape)
            {
                game.appMode = AppMode::MainMenu;
                game.menuSelection = MenuOption::PlayGame;
            }
            break;
        }
        }
    }
}

void InputHandler::processMenuEvents(const sf::Event& event)
{
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (clicked->button == sf::Mouse::Button::Left)
        {
            float cx = static_cast<float>(game.window.getSize().x) / 2.0f;
            sf::Vector2f mousePos = game.window.mapPixelToCoords(sf::Vector2i(clicked->position.x, clicked->position.y));

            if (std::abs(mousePos.y - 220.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                game.menuSelection = MenuOption::PlayGame;
                game.startNewGame();
                return;
            }
            if (std::abs(mousePos.y - 340.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                game.menuSelection = MenuOption::ReviewHistory;
                game.appMode = AppMode::ReplayHistory;
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
        if (game.menuSelection == MenuOption::PlayGame)
            game.menuSelection = MenuOption::ReviewHistory;
        else if (game.menuSelection == MenuOption::PlayerCount)
            game.menuSelection = MenuOption::PlayGame;
        else if (game.menuSelection == MenuOption::GameSpeed)
            game.menuSelection = MenuOption::PlayerCount;
        else
            game.menuSelection = MenuOption::GameSpeed;
        break;
    case sf::Keyboard::Key::Down:
        if (game.menuSelection == MenuOption::PlayGame)
            game.menuSelection = MenuOption::PlayerCount;
        else if (game.menuSelection == MenuOption::PlayerCount)
            game.menuSelection = MenuOption::GameSpeed;
        else if (game.menuSelection == MenuOption::GameSpeed)
            game.menuSelection = MenuOption::ReviewHistory;
        else
            game.menuSelection = MenuOption::PlayGame;
        break;
    case sf::Keyboard::Key::Left:
        if (game.menuSelection == MenuOption::PlayerCount && game.playerCount > 1)
            game.setPlayerCount(game.playerCount - 1);
        else if (game.menuSelection == MenuOption::GameSpeed)
        {
            if (game.gameSpeed == GameSpeed::Normal)
            {
                game.gameSpeed = GameSpeed::Fast;
                Logger::info() << "Game speed set to Fast.";
            }
            else if (game.gameSpeed == GameSpeed::Slow)
            {
                game.gameSpeed = GameSpeed::Normal;
                Logger::info() << "Game speed set to Normal.";
            }
            else if (game.gameSpeed == GameSpeed::AutoPlay)
            {
                game.gameSpeed = GameSpeed::Slow;
                Logger::info() << "Game speed set to Slow.";
            }
        }
        break;
    case sf::Keyboard::Key::Right:
        if (game.menuSelection == MenuOption::PlayerCount && game.playerCount < 6)
            game.setPlayerCount(game.playerCount + 1);
        else if (game.menuSelection == MenuOption::GameSpeed)
        {
            if (game.gameSpeed == GameSpeed::Fast)
            {
                game.gameSpeed = GameSpeed::Normal;
                Logger::info() << "Game speed set to Normal.";
            }
            else if (game.gameSpeed == GameSpeed::Normal)
            {
                game.gameSpeed = GameSpeed::Slow;
                Logger::info() << "Game speed set to Slow.";
            }
            else if (game.gameSpeed == GameSpeed::Slow)
            {
                game.gameSpeed = GameSpeed::AutoPlay;
                Logger::info() << "Game speed set to AutoPlay.";
            }
        }
        break;
    case sf::Keyboard::Key::Enter:
        switch (game.menuSelection)
        {
        case MenuOption::PlayGame:
            Logger::info() << "Starting game from menu.";
            game.startNewGame();
            break;
        case MenuOption::PlayerCount:
            break;
        case MenuOption::GameSpeed:
            break;
        case MenuOption::ReviewHistory:
        {
            Logger::info() << "Entering replay history.";
            auto profile = game.players[0].getProfile();
            game.replay->loadHistory(profile.getLongHistory());
            game.replay->setStartingBankroll(game.players[0].getBankroll());
            game.appMode = AppMode::ReplayHistory;
            break;
        }
        }
        break;
    case sf::Keyboard::Key::Escape:
        game.window.close();
        break;
    }
}

void InputHandler::processGameEvents(const sf::Event& event)
{
    if (auto pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button == sf::Mouse::Button::Left)
        {
            sf::FloatRect slider = game.hud.getRiskSliderBounds();
            if (slider.contains(sf::Vector2f(pressed->position.x, pressed->position.y)))
            {
                game.hudState.riskSliderDragging = true;
            }
        }
    }

    if (auto released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (released->button == sf::Mouse::Button::Left && game.hudState.riskSliderDragging)
        {
            game.hudState.riskSliderDragging = false;
            Logger::info() << "Kelly fraction set to " << game.kellyFraction << ".";
        }
    }

    if (auto moved = event.getIf<sf::Event::MouseMoved>())
    {
        if (game.hudState.riskSliderDragging)
        {
            sf::FloatRect slider = game.hud.getRiskSliderBounds();
            float frac = (moved->position.x - slider.position.x) / slider.size.x;
            frac = std::max(0.0f, std::min(1.0f, frac));
            game.hudState.kellyFraction = frac;
            game.kellyFraction = frac;
        }
    }

    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if (key->code == sf::Keyboard::Key::R)
        {
            if (game.gameState == GameState::GameOver)
            {
                Logger::info() << "Starting new shoe.";
                game.deck.create(Constants::NUM_DECKS);
                game.deck.shuffle();
                game.currentShoeRecord.initialDeck = game.deck.getCards();
                game.gameState = GameState::Started;
            }
            else if (game.roundState == RoundState::Cleanup && game.gameState != GameState::LastHand)
            {
                Logger::info() << "Starting next round.";
                game.nextRound();
            }
        }
        if (key->code == sf::Keyboard::Key::Escape)
        {
            Logger::info() << "Game paused.";
            game.appMode = AppMode::Paused;
            game.pauseSelection = PauseOption::Resume;
        }

        // In auto-play mode, player cannot take actions (except R and Escape handled above)
        if (game.gameSpeed == GameSpeed::AutoPlay)
            return;

        if (game.activePlayerIndex != 0)
            return;

        if (game.roundState == RoundState::PlaceBets)
        {
            switch (key->code)
            {
            case sf::Keyboard::Key::Up:
                game.increaseBet();
                break;
            case sf::Keyboard::Key::Down:
                game.decreaseBet();
                break;
            case sf::Keyboard::Key::Left:
            {
                float tc = BasicStrategy::computeTrueCount(game.runningCount, game.deck.size());
                int optimal = game.calculateOptimalBet(tc, game.kellyFraction, game.players[0].getBankroll() + game.players[0].hands[0].getBet(),
                                                      Constants::MINIMUM_BET, Constants::MAXIMUM_BET);
                game.players[0].hands[0].setBet(optimal);
                Logger::info() << "Optimal bet (TC=" << tc << ") set to " << optimal << ".";
                break;
            }
            case sf::Keyboard::Key::Right:
                game.confirmBet();
                break;
            default:
                break;
            }
        }
        else if (game.roundState == RoundState::OfferInsurance)
        {
            switch (key->code)
            {
            case sf::Keyboard::Key::Up:
                game.increaseInsurance();
                break;
            case sf::Keyboard::Key::Down:
                game.decreaseInsurance();
                break;
            case sf::Keyboard::Key::Right:
                game.confirmInsurance();
                break;
            case sf::Keyboard::Key::Left:
                game.declineInsurance();
                break;
            }
        }
        else if (game.roundState == RoundState::PlayerTurn)
        {
            switch (key->code)
            {
            case sf::Keyboard::Key::H:
                game.hit();
                break;
            case sf::Keyboard::Key::S:
                game.stand();
                break;
            case sf::Keyboard::Key::D:
                game.doubleDown();
                break;
            case sf::Keyboard::Key::P:
                game.split();
                break;
            case sf::Keyboard::Key::I:
            {
                char choice = game.giveBasicStrategyAdvice(game.players[game.activePlayerIndex].hands[game.players[game.activePlayerIndex].currentHand], game.dealer);
                game.hudState.basicStrategyHint = "Book says: " + std::string(1, choice);
                Logger::info() << "Basic strategy advice: " << choice;
                break;
            }
            }
        }

        if (key->code == sf::Keyboard::Key::Y)
        {
            if (game.roundState == RoundState::PlayerTurn)
            {
                game.surrender();
            }
        }
    }
}

void InputHandler::processPauseEvents(const sf::Event& event)
{
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (clicked->button == sf::Mouse::Button::Left)
        {
            float cx = static_cast<float>(game.window.getSize().x) / 2.0f;
            sf::Vector2f mousePos = game.window.mapPixelToCoords(sf::Vector2i(clicked->position.x, clicked->position.y));

            if (std::abs(mousePos.y - 200.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                game.appMode = AppMode::Playing;
                return;
            }
            if (std::abs(mousePos.y - 250.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                {
                    PlayerProfile profile = game.players[0].getProfile();
                    profile.addShoeRecord(game.currentShoeRecord);
                    game.players[0].updateProfile(profile);
                    game.players[0].saveProfile();
                }
                game.startNewGame();
                return;
            }
            if (std::abs(mousePos.y - 300.0f) < 20.0f && std::abs(mousePos.x - cx) < 100.0f)
            {
                {
                    PlayerProfile profile = game.players[0].getProfile();
                    profile.addShoeRecord(game.currentShoeRecord);
                    game.players[0].updateProfile(profile);
                    game.players[0].saveProfile();
                }
                game.appMode = AppMode::MainMenu;
                game.menuSelection = MenuOption::PlayGame;
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
        if (game.pauseSelection == PauseOption::Resume)
            game.pauseSelection = PauseOption::QuitToMenu;
        else if (game.pauseSelection == PauseOption::Restart)
            game.pauseSelection = PauseOption::Resume;
        else
            game.pauseSelection = PauseOption::Restart;
        break;
    case sf::Keyboard::Key::Down:
    case sf::Keyboard::Key::S:
        if (game.pauseSelection == PauseOption::Resume)
            game.pauseSelection = PauseOption::Restart;
        else if (game.pauseSelection == PauseOption::Restart)
            game.pauseSelection = PauseOption::QuitToMenu;
        else
            game.pauseSelection = PauseOption::Resume;
        break;
    case sf::Keyboard::Key::Enter:
    case sf::Keyboard::Key::Space:
        switch (game.pauseSelection)
        {
        case PauseOption::Resume:
            Logger::info() << "Game resumed.";
            game.appMode = AppMode::Playing;
            break;
        case PauseOption::Restart:
        {
            Logger::info() << "Restarting game.";
            PlayerProfile profile = game.players[0].getProfile();
            profile.addShoeRecord(game.currentShoeRecord);
            game.players[0].updateProfile(profile);
            game.players[0].saveProfile();
            game.startNewGame();
            break;
        }
        case PauseOption::QuitToMenu:
        {
            Logger::info() << "Returning to main menu.";
            PlayerProfile profile = game.players[0].getProfile();
            profile.addShoeRecord(game.currentShoeRecord);
            game.players[0].updateProfile(profile);
            game.players[0].saveProfile();
            game.appMode = AppMode::MainMenu;
            game.menuSelection = MenuOption::PlayGame;
            break;
        }
        }
        break;
    case sf::Keyboard::Key::Escape:
        game.appMode = AppMode::Playing;
        break;
    }
}