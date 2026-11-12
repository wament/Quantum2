#include "InputHandler.hpp"
#include "Game.hpp"
#include "Constants.hpp"
#include "BasicStrategy.hpp"
#include "Replay.hpp"
#include "FontManager.hpp"
#include "Logger.hpp"
#include "StrategyTrainer.hpp"
#include "Tournament.hpp"
// included here so activateMainMenuOption can start it and processStatsDashboardEvents can
// drive its tab switching. Pure-logic + no new include cycle.
#include "StatsDashboard.hpp"
// processRulesEvents edits the in-game rules-config panel's selected row via applyGameRuleEdit,
// so it needs these helpers. Pure-logic header, no include cycle.
#include "GameRules.hpp"

InputHandler::InputHandler(Game& game) : game(game) {}

// Shared "Escape returns to main menu" collapse of the five duplicated blocks across processTrainer/Tournament/StatsDashboard/
// Achievement/Rules events. One log line naming the source mode (kept distinct per caller via `fromMode`) plus a single
// AppMode/selection reset, so every mode's escape path is one call instead of three repeated statements.
void InputHandler::returnToMainMenu(Game& game, const char* fromMode)
{
    Logger::info() << "Returned to main menu from the " << fromMode << ".";
    // B7 — persist on exit via Game::persistOnExit() (the single choke point) so every Escape->menu route keeps this
    // session's bankroll / shoe record / settings, matching the window-close save that already happened. Runs before the
    // mode reset so the profile is durable even if the following state changes are later extended.
    game.persistOnExit();
    game.appMode = AppMode::MainMenu;
    game.menuSelection = MenuOption::PlayGame;
}

// Single code path for keyboard (Enter) and mouse (left-click) menu activation
// behaves identically. Reads Game's private members directly: hence the friendship access that
// lets the other process* methods touch them too. This is the source of truth for selection.
void InputHandler::activateMainMenuOption(Game &game, MenuOption option)
{
    switch (option)
    {
    case MenuOption::PlayGame:
        Logger::info() << "Starting game from menu.";
        game.startNewGame();
        break;
    case MenuOption::DailyChallenge:
    {
        // The one-attempt gate. The DECISION is Daily::gateFor(), a pure function that is asserted
        // directly; this is the dispatch that turns a decision into a session. startDailySession()
        // sets AppMode::Playing itself, mirroring the other entry points, and marks the seat ephemeral
        // so the run cannot reach the career file.
        const std::string today = Daily::todayDateKey();
        const PlayerProfile profile =
            game.players.empty() ? PlayerProfile(std::string{}) : game.players[0].getProfile();

        switch (Daily::gateFor(today, profile.dailyHasActiveShoe(), profile.dailyActiveDateKey,
                               profile.dailyLastPlayed))
        {
        case Daily::GateAction::ResumeActive:
            // Resume THE SHOE THAT WAS STARTED, whatever day it belongs to.
            Logger::info() << "Resuming the Daily shoe started on " << profile.dailyActiveDateKey << ".";
            game.startDailySession(profile.dailyActiveDateKey, /*practice=*/false);
            break;
        case Daily::GateAction::PracticeYesterday:
        {
            // Today is scored: no second scored attempt, so offer yesterday's shoe as PRACTICE --
            // labelled, recorded in history, and it never moves the streak. Without an escape hatch a
            // loss is punitive and players stop playing the Daily.
            const std::string yesterday = Daily::previousDateKey(today);
            Logger::info() << "Daily already scored today; starting " << yesterday << " as practice.";
            game.startDailySession(yesterday, /*practice=*/true);
            break;
        }
        case Daily::GateAction::StartToday:
            Logger::info() << "Entering daily challenge.";
            game.startDailySession(today, /*practice=*/false);
            break;
        case Daily::GateAction::Nothing:
            Logger::info() << "Daily already scored today; nothing available to replay.";
            break;
        }
        break;
    }
    case MenuOption::PlayerCount:
    case MenuOption::GameSpeed:
        // Left/Right adjust these values; nothing to activate on Enter/click.
        break;
    case MenuOption::ReviewHistory:
    {
        Logger::info() << "Entering replay history.";
        auto profile = game.players[0].getProfile();
        game.replay->loadHistory(profile.getLongHistory(), profile.getDailyShoes(),
                                 profile.getTournamentShoes());
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
    case MenuOption::Achievements:
    {
        Logger::info() << "Entering achievements.";
        game.appMode = AppMode::Achievements;
        break;
    }
    case MenuOption::GameRules:
    {
        Logger::info() << "Entering custom game rules.";
        game.appMode = AppMode::GameRules;
        game.gameRulesSelectedRow = 0;
        break;
    }
    case MenuOption::Statistics:
    {
        Logger::info() << "Entering statistics dashboard.";
        game.startStatsSession();
        game.appMode = AppMode::StatsDashboard;
        break;
    }
    }
}

// Pause-menu activation for Enter/click: see activateMainMenuOption(). Only runs while paused
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
        // Stamp the current table settings first: this is the last save on the way out
        // the player count / speed / counting style survive to the next launch.
        game.rememberTableSettings();

        PlayerProfile profile = game.players[0].getProfile();
        profile.addShoeRecord(game.currentShoeRecord);
        // stamp the current unlocked-achievement ids onto the profile too
        game.achievementManager_.persistUnlockedTo(profile.unlockedAchievementIds);
        game.players[0].updateProfile(profile);
        game.players[0].saveProfile();
        game.appMode = AppMode::MainMenu;
        game.menuSelection = MenuOption::PlayGame;
        break;
    }
    }
}

// Shared by keyboard arrows and the mouse wheel. `forward` = "advance" (like Down/Right): step once
// around the main-menu cycle.
//
// Index-driven off mainMenuItems(), which is already the single source of truth for rows, hover bands
// and click selection. The previous body was a hand-written switch over every option in both
// directions, and it is the reason three options (Achievements / Custom Game Rules / Statistics) were
// unreachable by arrow keys -- a new row had to be added to two switches, and was not. Navigation now
// cannot drift from the table: adding a row here is the only edit a new menu option needs.
void InputHandler::navigateMenu(bool forward)
{
    const auto& items = mainMenuItems();
    const int n = static_cast<int>(items.size());
    if (n == 0)
        return;

    int index = -1;
    for (int i = 0; i < n; ++i)
    {
        if (items[static_cast<std::size_t>(i)].option == game.menuSelection)
        {
            index = i;
            break;
        }
    }

    // A selection that is not in the table (a value option, or a stale value) snaps to the top row
    // rather than stepping through an index that does not exist.
    if (index < 0)
    {
        game.menuSelection = items[0].option;
        return;
    }

    index = (index + (forward ? 1 : -1) + n) % n;
    game.menuSelection = items[static_cast<std::size_t>(index)].option;
}

// Single source of truth for cycling the main menu's Speed row (Fast -> Normal -> Slow -> AutoPlay -> Fast).
// Shared by arrow keys, the mouse wheel, and Left/Right
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
// `option` is whichever row is under the cursor when called from scroll handling
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
        // Cycle Fast -> Normal -> Slow -> AutoPlay -> Fast (and reverse) with wraparound: see cycleGameSpeed().
        cycleGameSpeed(game, forward ? 1 : -1);
        break;
    }
}

void InputHandler::processEvents()
{
    // Decay the rules-config mouse-wheel rate-limit cooldown by this frame's real elapsed time (once per frame)
    // scrolling is throttled to at most one action per SCROLL_RATE_LIMIT_SECONDS regardless of frame rate. See processRulesEvents().
    scrollCooldown_ = scrollCooldown_ > game.deltaTime ? scrollCooldown_ - game.deltaTime : 0.f;

    while (true)
    {
        auto event = game.window.pollEvent();
        
        if (!event.has_value())
            break;

        if (event->is<sf::Event::Closed>())
        {
            Logger::info("=== Closing game ===");
            PlayerProfile closeProfile = game.players[0].getProfile();
            // stamp the current unlocked-achievement ids before saving on exit.
            game.achievementManager_.persistUnlockedTo(closeProfile.unlockedAchievementIds);
            game.players[0].updateProfile(closeProfile);
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
        case AppMode::StatsDashboard:
            processStatsDashboardEvents(event.value());
            break;
        case AppMode::Achievements:
            processAchievementEvents(event.value());
            break;
        case AppMode::GameRules:
            processRulesEvents(event.value());
            break;
        case AppMode::NameEntry:
            processNameEntryEvents(event.value());
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

// keyboard drives the trainer directly. H/S/D/P/Y/I choose the play to grade; Enter/Space starts or restarts a fresh shoe; Escape returns to the main menu. Grading uses submitAction(char)
void InputHandler::processTrainerEvents(const sf::Event& event)
{
    // Mouse clicks on the action buttons (drawn by the trainer). The five plays remain available via keyboard below; this lets
    // either input grade an answer. buttonIndexAt() recomputes the same button geometry draw() renders
    // exactly. submitAction() is a no-op while no question is live
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (clicked->button == sf::Mouse::Button::Left)
        {
            auto pos = sf::Mouse::getPosition(game.window);
            int idx = game.trainer->buttonIndexAt(pos.x, pos.y,
                                                  static_cast<float>(game.window.getSize().x),
                                                  static_cast<float>(game.window.getSize().y));
            if (idx >= 0)
                game.trainer->submitAction(game.trainer->buttonActions()[idx]);
        }
        return; // a left-click is handled here; everything else falls through to the key handling below
    }

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
        returnToMainMenu(game, "strategy trainer");
        break;
    }
}

// keyboard drives the tournament. Enter/Space steps to the next round immediately (the mode
// also auto-advances hands via Game::update); Escape returns to the main menu. Guards the shared tournament
// games - which never build it - can't crash when input is processed.
// Drives tournament play through the SAME input path as an ordinary game (betting arrows + H/S/D/P/Y/side-bets), so a tournament
// really plays like a normal shoe with the human seat making decisions -- only the rules differ (see Tournament). While the
// tournament is running, processGameEvents handles everything including Escape-to-pause; once it is complete, only Escape does
// anything (it leaves straight back to the main menu) so the leaderboard holds until the player quits.
void InputHandler::processTournamentEvents(const sf::Event& event)
{
    // Escape always returns to the main menu -- exactly like the other four per-mode handlers (a single shared call instead of
    // a duplicated block), so test_input_return_to_main_menu.cpp's shared assertion still holds. Live betting arrows + H/S/D/P/Y
    // and side-bets below share ordinary play through processGameEvents, so the human seat makes real decisions on the same path.
    if (auto pressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (pressed->code == sf::Keyboard::Key::Escape)
            returnToMainMenu(game, "tournament mode");
        else if (!game.tournament || !game.tournament->isComplete())
            processGameEvents(event);   // a live stage: ordinary betting/decision input
        // once the final stage has run out, Escape is the only key handled here so the leaderboard holds.
        return;
    }

    // Mouse clicks likewise share ordinary play while a stage is live; after completion nothing but Escape is handled.
    if (!game.tournament || !game.tournament->isComplete())
        processGameEvents(event);
}

// keyboard drives the tabbed browse view. Left/Right cycle tabs (advanceTab); Escape
// returns to the main menu. Guards the shared dashboard
// is processed, mirroring processTournamentEvents().
void InputHandler::processStatsDashboardEvents(const sf::Event& event)
{
    if (auto pressed = event.getIf<sf::Event::KeyPressed>())
    {
        switch (pressed->code)
        {
        case sf::Keyboard::Key::Left:
            if (game.statsDashboard)
                game.statsDashboard->prevTabForTest();
            break;
        case sf::Keyboard::Key::Right:
            if (game.statsDashboard)
                game.statsDashboard->nextTabForTest();
            break;
        case sf::Keyboard::Key::Escape:
            returnToMainMenu(game, "statistics dashboard");
            break;
        }
    }
}

// U7 name-entry screen input: character capture via TextEntered (enabled on the window), plus Enter/Space to commit + save,
// Escape to cancel and clear, and Backspace to edit. Editing goes through Game::nameEntryAppendChar/backspace so the same
// clamped buffer both draws (GameRenderer) and commits; committing calls Game::commitPlayerName which persists the name before
// returning to the main menu.
void InputHandler::processNameEntryEvents(const sf::Event& event)
{
    if (auto text = event.getIf<sf::Event::TextEntered>())
    {
        // Only printable ASCII advances the buffer; control characters are ignored and nameEntryAppendChar also clamps length.
        if (text->unicode >= 32 && text->unicode < 127)
            game.nameEntryAppendChar(static_cast<char>(text->unicode));
        return;
    }

    if (auto pressed = event.getIf<sf::Event::KeyPressed>())
    {
        switch (pressed->code)
        {
        case sf::Keyboard::Key::Enter:
        case sf::Keyboard::Key::Space:
            game.commitPlayerName(game.nameEntryText());
            returnToMainMenu(game, "name entry");
            break;
        case sf::Keyboard::Key::Escape:
            while (!game.nameEntryText().empty())   // cancel: drop any in-progress text (no save) before leaving
                game.nameEntryBackspace();
            returnToMainMenu(game, "name entry");
            break;
        case sf::Keyboard::Key::Backspace:
            game.nameEntryBackspace();
            break;
        default:
            break;
        }
    }
}

// the achievements list is a read-only view. There are no live controls beyond returning to
// the main menu with Escape, so this mirrors processTrainerEvents()'s escape path only.
void InputHandler::processAchievementEvents(const sf::Event& event)
{
    if (auto pressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (pressed->code == sf::Keyboard::Key::Escape)
        {
            returnToMainMenu(game, "achievements list");
        }
    }
}

// drives the rules-config panel using the SAME baseline as the main menu
// selects it (applyMenuRowSelection + getHoveredGameRulesRowIndex) and the wheel edits that row's value forward/back, exactly like
// Left/Right. Esc persists changes and returns to the main menu as an edge trigger. Up/Down/Left/Right navigation stay in
// Game::update() because they are held-key state (not edge-triggered). Sharing applyMenuRowSelection() is why future menus inherit
// consistent hover+wheel behavior without re-implementing it.
constexpr float gameRulesScrollRateLimitSeconds = 0.15f; // ~6 edits/sec cap

void InputHandler::processRulesEvents(const sf::Event& event)
{
    // Hover selects the rule under the cursor, exactly like the main menu updates selection on mouse move.
    if (event.getIf<sf::Event::MouseMoved>())
        applyMenuRowSelection(game.renderer.getHoveredGameRulesRowIndex(game), game.gameRulesSelectedRow);

    // Mouse wheel edits whichever rule is under the cursor, else the arrow-selected one
    // resolution as processMenuEvents' wheel. Rate-limited
    // land in a frame or the wheel spins fast (cooldown decays by real time once per frame in processEvents()).
    if (auto wheel = event.getIf<sf::Event::MouseWheelScrolled>())
    {
        if (scrollCooldown_ <= 0.f)
        {
            int targetRow = game.gameRulesSelectedRow; // fall back to the arrow-selected row when nothing is hovered
            int hoveredIndex = game.renderer.getHoveredGameRulesRowIndex(game);
            if (hoveredIndex >= 0 && hoveredIndex < static_cast<int>(gameRuleRows().size()))
                targetRow = hoveredIndex;

            if (targetRow >= 0 && targetRow < static_cast<int>(gameRuleRows().size()))
                applyGameRuleEdit(game.gameRules_, gameRuleRows()[targetRow].id, wheel->delta > 0.f);
            scrollCooldown_ = gameRulesScrollRateLimitSeconds;
        }
    }

    if (auto pressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (pressed->code == sf::Keyboard::Key::Escape)
        {
            game.rememberTableSettings(); // persist any rule changes before leaving the panel
            returnToMainMenu(game, "the custom game rules");
        }
    }
}

void InputHandler::processMenuEvents(const sf::Event& event)
{
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (clicked->button == sf::Mouse::Button::Left)
        {
            // Left-click activates whichever row the cursor hovers over: see activateMainMenuOption().
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

    // U1: hover is a VISUAL affordance only -- it must not move the keyboard selection. It used to, via
    // applyMenuRowSelection() on every MouseMoved, which made "the row under the cursor" and "the row Enter
    // will activate" the same variable. That is the real root cause of two rows looking selected at once: the
    // renderer could not distinguish the two states because the game had only one. Gold now marks the keyboard
    // selection (what Enter activates) and the hover pill + chevron mark what a click activates, so the player
    // can see exactly what each input will do. No MouseMoved handler is needed at all: drawMainMenu() already
    // asks the shared hit-test for the hovered row every frame, clicking activates the hovered row directly
    // (above), and the wheel below keeps its own hover authority -- so no pointer path loses functionality.

    // Mouse wheel scrubs PlayerCount / GameSpeed rows via hover authority (hovered row if under the
    // cursor, else game.menuSelection): same row-selection rule as click in activateMainMenuOption().
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
        // systems are counted in the background.
        //
        // The left-panel charts are clickable again, but now per-graph rather than as an all-at-once overlay: clicking a
        // graph toggles ONLY that graph between its per-shoe view and its session view (U12 part 1). A click anywhere else
        // on the table lands outside every chart pane, so it is ignored. Handled before the key auto-play gate below, so
        // viewing/toggling never has to coincide with taking a hand action.
        if (clicked->button == sf::Mouse::Button::Left)
        {
            sf::Vector2f world = game.window.mapPixelToCoords(sf::Mouse::getPosition(game.window));
            if (game.hud.countTitleBounds().contains(world))
            {
                game.toggleCountingStyle();
            }

            // chartPaneBounds() returns the three panes in draw order: bankroll-curve, edge-over-time, bets-placed.
            // Taken once, not per iteration: it builds a fresh vector on every call.
            const std::vector<sf::Rect<float>> panes = game.hud.chartPaneBounds();
            for (std::size_t i = 0; i < panes.size(); ++i)
            {
                if (panes[i].contains(world))
                {
                    game.hud.cycleGraphScope(i);
                    Logger::info() << "Side-panel graph view toggled: graph " << i << ".";
                    break;   // a click lands in exactly one pane
                }
            }
        }
    }

    if (auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if (key->code == sf::Keyboard::Key::R)
        {
            if (game.gameState == GameState::GameOver)
            {
                if (game.dailyComplete())
                {
                    // A finished Daily has no next shoe: one shoe per day is the feature. R returns to
                    // the menu, where the streak is shown, instead of silently dealing a second shoe.
                    Logger::info() << "Daily complete: returning to the main menu.";
                    returnToMainMenu(game, "daily challenge");
                }
                else
                {
                    // Fresh shoe. startNewShoe() is the single shoe-boundary path shared by auto-play, the mid-shoe
                    // reshuffle and a new game: it rebuilds the shoe AND resets the per-shoe analytics (the
                    // shoeGeneration recalibration signal, the bankroll/edge series and the running count)
                    // Edge % readout and the Edge Over Time curve fall back to their baseline here instead of carrying
                    // the previous shoe's drift forward. The player's bankroll is deliberately kept: this continues the
                    // same session rather than restarting the game.
                    Logger::info() << "Starting new shoe.";
                    game.startNewShoe();
                }
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

        // U12 session view: 'G' cycles the side-panel graphs between per-shoe and per-session accumulation. Handled here,
        // before the auto-play / non-human returns below, so it works at any round state (including auto-play) -- viewing a
        // chart never needs to be an action you take with the hand.
        if (key->code == sf::Keyboard::Key::G)
        {
            game.cycleGraphView();
            Logger::info() << "Side-panel graph view: "
                           << (game.hudState.graphView == HUDState::GraphViewMode::Session ? "session" : "per-shoe") << ".";
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
        // Hover-highlight drives the paused row; left-click activates: see activateMainMenuOption().
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