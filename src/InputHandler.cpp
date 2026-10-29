#include "InputHandler.hpp"
#include "Game.hpp"
#include "Constants.hpp"
#include "BasicStrategy.hpp"
#include "Replay.hpp"
#include "FontManager.hpp"
#include "Logger.hpp"
#include "StrategyTrainer.hpp"
#include "Tournament.hpp"

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
    case MenuOption::StrategyTrainer:
    {
        Logger::info() << "Entering strategy trainer.";
        game.startTrainerSession();
        game.appMode = AppMode::StrategyTrainer;
        break;
    }
    case MenuOption::Tournament:
    {
        Logger::info() << "Entering tournament mode.";
        game.startTournamentSession();
        game.appMode = AppMode::Tournament;
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
        // Stamp the current table settings first: this is the last save on the way out, so it is what makes
        // the player count / speed / counting style survive to the next launch.
        game.rememberTableSettings();

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
        case MenuOption::ReviewHistory:
            game.menuSelection = MenuOption::StrategyTrainer;
            break;
        case MenuOption::StrategyTrainer:
            game.menuSelection = MenuOption::Tournament;
            break;
        default: // Tournament wraps to the top.
            game.menuSelection = MenuOption::PlayGame;
            break;
        }
    }
    else
    {
        switch (game.menuSelection)
        {
        case MenuOption::PlayGame:
            game.menuSelection = MenuOption::Tournament; // wrap to the last option
            break;
        default: // PlayerCount/GameSpeed/ReviewHistory/StrategyTrainer/Tournament step one option back.
            game.menuSelection = static_cast<MenuOption>(static_cast<int>(game.menuSelection) - 1);
            break;
        }
    }
}

// Single source of truth for cycling the main menu's Speed row (Fast -> Normal -> Slow -> AutoPlay -> Fast).
// Shared by arrow keys, the mouse wheel, and Left/Right so every input wraps around identically instead of
// stopping at either end. `direction` is +1 to advance or -1 to step back; +4 keeps the index positive
// before modulo so wraparound works in both directions.
void InputHandler::cycleGameSpeed(Game &game, int direction)
{
    constexpr const GameSpeed order[6] = {
        GameSpeed::Fast, GameSpeed::Normal, GameSpeed::Slow, GameSpeed::AutoPlay, GameSpeed::Hyper, GameSpeed::Gamer};

    int idx = 0;
    for (int i = 0; i < 6; ++i)
        if (order[i] == game.gameSpeed)
        {
            idx = i;
            break;
        }

    idx = (idx + direction + 6) % 6;

    GameSpeed next = order[idx];
    if (game.gameSpeed != next)
    {
        game.gameSpeed = next;
        switch (next)
        {
        case GameSpeed::Fast:      Logger::info() << "Game speed set to Fast."; break;
        case GameSpeed::Normal:    Logger::info() << "Game speed set to Normal."; break;
        case GameSpeed::Slow:      Logger::info() << "Game speed set to Slow."; break;
        case GameSpeed::AutoPlay:  Logger::info() << "Game speed set to AutoPlay."; break;
        case GameSpeed::Hyper:     Logger::info() << "Game speed set to Hyper."; break;
        case GameSpeed::Gamer:     Logger::info() << "Game speed set to Gamer."; break;
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
        // Cycle Fast -> Normal -> Slow -> AutoPlay -> Fast (and reverse) with wraparound — see cycleGameSpeed().
        cycleGameSpeed(game, forward ? 1 : -1);
        break;
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
        case AppMode::StrategyTrainer:
            processTrainerEvents(event.value());
            break;
        case AppMode::Tournament:
            processTournamentEvents(event.value());
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

// Feature 2 (Strategy Trainer): keyboard drives the trainer directly. H/S/D/P/Y/I choose the play to grade; Enter/Space starts or restarts a fresh shoe; Escape returns to the main menu. Grading uses submitAction(char) so no extra state-machine method is needed, and game.startTrainerSession() begins a new shoe.
void InputHandler::processTrainerEvents(const sf::Event& event)
{
    auto pressed = event.getIf<sf::Event::KeyPressed>();
    if (!pressed)
        return;

    switch (pressed->code)
    {
    case sf::Keyboard::Key::H: game.trainer->submitAction('H'); break;
    case sf::Keyboard::Key::S: game.trainer->submitAction('S'); break;
    case sf::Keyboard::Key::D: game.trainer->submitAction('D'); break;
    case sf::Keyboard::Key::P: game.trainer->submitAction('P'); break;
    case sf::Keyboard::Key::Y:
    case sf::Keyboard::Key::R: game.trainer->submitAction('R'); break;
    case sf::Keyboard::Key::Space:
    case sf::Keyboard::Key::Enter:
        game.startTrainerSession();
        break;
    case sf::Keyboard::Key::Escape:
        Logger::info() << "Returned to main menu from the strategy trainer.";
        game.appMode = AppMode::MainMenu;
        game.menuSelection = MenuOption::PlayGame;
        break;
    }
}

// Feature 9 (Tournament Mode): keyboard drives the tournament. Enter/Space steps to the next round immediately (the mode
// also auto-advances hands via Game::update); Escape returns to the main menu. Guards the shared tournament so headless
// games - which never build it - can't crash when input is processed.
void InputHandler::processTournamentEvents(const sf::Event& event)
{
    if (auto pressed = event.getIf<sf::Event::KeyPressed>())
    {
        switch (pressed->code)
        {
        case sf::Keyboard::Key::Enter:
        case sf::Keyboard::Key::Space:
            if (game.tournament)
                game.tournament->playHand();
            break;
        case sf::Keyboard::Key::Escape:
            Logger::info() << "Returned to main menu from tournament mode.";
            game.appMode = AppMode::MainMenu;
            game.menuSelection = MenuOption::PlayGame;
            break;
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
            cycleGameSpeed(game, -1);  // Left steps back around the Speed cycle
        break;
    case sf::Keyboard::Key::Right:
        if (game.menuSelection == MenuOption::PlayerCount && game.playerCount < 6)
            game.setPlayerCount(game.playerCount + 1);
        else if (game.menuSelection == MenuOption::GameSpeed)
            cycleGameSpeed(game, 1);  // Right advances around the Speed cycle
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
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        // Clicking the count-style label ("Count (Hi-Lo)") toggles which counting system is shown. Both
        // systems are counted in the background, so this just swaps which buffer drives the active count.
        //
        // This is the only clickable HUD element. The left-panel charts are display-only: their click-to-swap
        // secondary "overlay" views are gone (each one hid the primary series it was drawn over without adding
        // anything legible), so the graph panes no longer take clicks and need no hit-testing here.
        if (clicked->button == sf::Mouse::Button::Left)
        {
            sf::Vector2f world = game.window.mapPixelToCoords(sf::Mouse::getPosition(game.window));
            if (game.hud.countTitleBounds().contains(world))
            {
                game.toggleCountingStyle();
            }
        }
    }

    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if (key->code == sf::Keyboard::Key::R)
        {
            if (game.gameState == GameState::GameOver)
            {
                // Fresh shoe. startNewShoe() is the single shoe-boundary path shared by auto-play, the mid-shoe
                // reshuffle and a new game: it rebuilds the shoe AND resets the per-shoe analytics (the
                // shoeGeneration recalibration signal, the bankroll/edge series and the running count), so the
                // Edge % readout and the Edge Over Time curve fall back to their baseline here instead of carrying
                // the previous shoe's drift forward. The player's bankroll is deliberately kept: this continues the
                // same session rather than restarting the game.
                Logger::info() << "Starting new shoe.";
                game.startNewShoe();
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

        // In auto-play mode (AutoPlay or refresh-rate Gamer), player cannot take actions (except R and Escape handled above)
        if (GameTiming::isAutoPlaying(game.gameSpeed))
            return;

        if (game.activePlayerIndex != 0)
            return;

        if (game.roundState == RoundState::PlaceBets)
        {
            switch (key->code)
            {
            case sf::Keyboard::Key::Up:
                game.players[0].adjustHandBet(5);
                break;
            case sf::Keyboard::Key::Down:
                game.players[0].adjustHandBet(-5);
                break;
            case sf::Keyboard::Key::Left:
            {
                float tc = game.activeTrueCount();
                int optimal = game.calculateOptimalBet(tc, GameConfig::KELLY_FRACTION, game.players[0].getBankroll() + game.players[0].hands[0].getBet(),
                                                      Constants::MINIMUM_BET, Constants::MAXIMUM_BET);
                game.players[0].hands[0].setBet(optimal);
                Logger::info() << "Optimal bet (TC=" << tc << ") set to " << optimal << ".";
                break;
            }
            case sf::Keyboard::Key::Right:
                game.confirmBet();
                break;
            case sf::Keyboard::Key::Z:
            {
                // Cycle the side-bet kind: Off -> Perfect Pairs -> 21+3 -> Off. Reads the live hand state that placeSideBet
                // keeps updated, so the HUD/renderer reflect each change instantly during PlaceBets.
                auto& p = game.players[0];
                SideBetKind next;
                switch (p.hands[0].getSideBetKind())
                {
                case SideBetKind::None:          next = SideBetKind::PerfectPairs; break;
                case SideBetKind::PerfectPairs:  next = SideBetKind::TwentyOneThree; break;
                default:                         next = SideBetKind::None; break; // 21+3 -> off
                }
                game.placeSideBet(next, (next == SideBetKind::None) ? 0 : p.hands[0].getSideBetWager());
                Logger::info() << "Side bet type set to "
                               << (next == SideBetKind::None ? std::string("off")
                                   : (next == SideBetKind::PerfectPairs ? std::string("Perfect Pairs") : std::string("21+3")))
                               << ".";
                break;
            }
            case sf::Keyboard::Key::X: // increase the side-bet wager by a chip
            {
                auto& p = game.players[0];
                int nextWager = p.hands[0].getSideBetWager() + 10;
                if (p.getBankroll() < nextWager)
                    nextWager = p.getBankroll(); // clamp to what the seat can afford
                game.placeSideBet(p.hands[0].getSideBetKind(), nextWager);
                break;
            }
            case sf::Keyboard::Key::V: // decrease the side-bet wager by a chip
            {
                auto& p = game.players[0];
                int nextWager = p.hands[0].getSideBetWager() - 10;
                if (nextWager < 0)
                    nextWager = 0;
                game.placeSideBet(p.hands[0].getSideBetKind(), nextWager);
                break;
            }
            default:
                break;
            }
        }
        else if (game.roundState == RoundState::OfferInsurance)
        {
            switch (key->code)
            {
            case sf::Keyboard::Key::Up:
                game.players[game.activePlayerIndex].raiseInsurance();
                break;
            case sf::Keyboard::Key::Down:
                game.players[game.activePlayerIndex].lowerInsurance();
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