#include "InputHandler.hpp"
#include "Game.hpp"
#include "Constants.hpp" 
#include "FontManager.hpp"
#include "Logger.hpp"

InputHandler::InputHandler(Game& game) : game(game) {}

// Single code path for keyboard (Enter) and mouse (left-click) menu activation, so every option
// behaves identically. Reads Game's private members directly — hence the friendship access that
// lets the other process* methods touch them too. This is the source of truth for selection.
void InputHandler::activateMainMenuOption(Game &game, MenuOption option)
{
    switch (option)
    {
    case MenuOption::PlayGame:
        Logger::info() << "Starting game from menu.";
        game.startNewGame();
        break;
    case MenuOption::PlayerCount:
    case MenuOption::GameSpeed:
        // Left/Right adjust these values; nothing to activate on Enter/click.
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
}

// Pause-menu activation for Enter/click — see activateMainMenuOption(). Only runs while paused
// (AppMode::Paused); Restart/QuitToMenu persist the finished shoe record before changing mode.
void InputHandler::activatePauseOption(Game &game, PauseOption option)
{
    switch (option)
    {
    case PauseOption::Resume:
        game.appMode = AppMode::Playing;
        break;
    case PauseOption::Restart:
    {
        PlayerProfile profile = game.players[0].getProfile();
        profile.addShoeRecord(game.currentShoeRecord);
        game.players[0].updateProfile(profile);
        game.players[0].saveProfile();
        game.startNewGame();
        break;
    }
    case PauseOption::QuitToMenu:
    {
        PlayerProfile profile = game.players[0].getProfile();
        profile.addShoeRecord(game.currentShoeRecord);
        game.players[0].updateProfile(profile);
        game.players[0].saveProfile();
        game.appMode = AppMode::MainMenu;
        game.menuSelection = MenuOption::PlayGame;
        break;
    }
    }
}

// Shared by keyboard arrows and the mouse wheel. `forward` = "advance" (like Down/Right):
// step once in the main-menu cycle, or bump a value option's speed up one level.
void InputHandler::navigateMenu(bool forward)
{
    if (forward)
    {
        switch (game.menuSelection)
        {
        case MenuOption::PlayGame:
            game.menuSelection = MenuOption::PlayerCount;
            break;
        case MenuOption::PlayerCount:
            game.menuSelection = MenuOption::GameSpeed;
            break;
        case MenuOption::GameSpeed:
            game.menuSelection = MenuOption::ReviewHistory;
            break;
        default: // ReviewHistory wraps to the top.
            game.menuSelection = MenuOption::PlayGame;
            break;
        }
    }
    else
    {
        switch (game.menuSelection)
        {
        case MenuOption::PlayGame:
            game.menuSelection = MenuOption::ReviewHistory;
            break;
        default: // PlayerCount/GameSpeed/ReviewHistory step one option back.
            game.menuSelection = static_cast<MenuOption>(static_cast<int>(game.menuSelection) - 1);
            break;
        }
    }
}

// Advances a value option (player count or game speed) in the given direction, matching the
// Left/Right handling so mouse-wheel scrolling changes values exactly like the arrow keys do.
// `option` is whichever row is under the cursor when called from scroll handling, so scrolling
// changes a value without that row having to be keyboard-selected first (game.menuSelection).
void InputHandler::adjustValue(bool forward, MenuOption option)
{
    switch (option)
    {
    case MenuOption::PlayerCount:
        if (forward && game.playerCount < 6)
            game.setPlayerCount(game.playerCount + 1);
        else if (!forward && game.playerCount > 1)
            game.setPlayerCount(game.playerCount - 1);
        break;
    case MenuOption::GameSpeed:
    {
        constexpr const GameSpeed order[4] = {
            GameSpeed::Fast, GameSpeed::Normal, GameSpeed::Slow, GameSpeed::AutoPlay};
        int idx = 0;
        for (int i = 0; i < 4; ++i)
            if (order[i] == game.gameSpeed)
            {
                idx = i;
                break;
            }

        if (forward && idx < 3)
            idx++;
        else if (!forward && idx > 0)
            idx--;

        GameSpeed next = order[idx];
        if (game.gameSpeed != next)
        {
            game.gameSpeed = next;
            switch (next)
            {
            case GameSpeed::Normal:
                Logger::info() << "Game speed set to Normal.";
                break;
            case GameSpeed::Fast:
                Logger::info() << "Game speed set to Fast.";
                break;
            case GameSpeed::Slow:
                Logger::info() << "Game speed set to Slow.";
                break;
            case GameSpeed::AutoPlay:
                Logger::info() << "Game speed set to AutoPlay.";
                break;
            }
        }
        break;
    }
    }
}

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
            // Left-click activates whichever row the cursor hovers over — see activateMainMenuOption().
            int hoveredIndex = game.renderer.getHoveredMenuItemIndex(game);
            if (hoveredIndex >= 0 && hoveredIndex < static_cast<int>(mainMenuItems().size()))
            {
                MenuOption option = mainMenuItems()[hoveredIndex].option;
                game.menuSelection = option;
                activateMainMenuOption(game, option);
                return;
            }
        }
    }

    // Mouse wheel scrubs PlayerCount / GameSpeed rows via hover authority (hovered row if under the
    // cursor, else game.menuSelection) — same row-selection rule as click in activateMainMenuOption().
    // Positive delta scrolls up and increases the value, mirroring the Left/Right keys.
    auto scrolled = event.getIf<sf::Event::MouseWheelScrolled>();
    if (scrolled && game.appMode == AppMode::MainMenu)
    {
        MenuOption target = game.menuSelection;
        int hoveredIndex = game.renderer.getHoveredMenuItemIndex(game);
        if (hoveredIndex >= 0 && hoveredIndex < static_cast<int>(mainMenuItems().size()))
            target = mainMenuItems()[hoveredIndex].option;

        if (target == MenuOption::PlayerCount || target == MenuOption::GameSpeed)
            adjustValue(scrolled->delta > 0.f, target);
    }

    auto pressed = event.getIf<sf::Event::KeyPressed>();
    if (!pressed)
        return;

    switch (pressed->code)
    {
    case sf::Keyboard::Key::Up:
        navigateMenu(false);
        break;
    case sf::Keyboard::Key::Down:
        navigateMenu(true);
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
        activateMainMenuOption(game, game.menuSelection);
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
        // Hover-highlight drives the paused row; left-click activates — see activateMainMenuOption().
        int hoveredIndex = game.renderer.getHoveredPauseMenuItemIndex(game);
        if (clicked->button == sf::Mouse::Button::Left && hoveredIndex >= 0)
        {
            const PauseMenuItem &item = pauseMenuItems()[hoveredIndex];
            activatePauseOption(game, item.option);
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
        activatePauseOption(game, game.pauseSelection);
        break;
    case sf::Keyboard::Key::Escape:
        game.appMode = AppMode::Playing;
        break;
    }
}