#include "Game.hpp"
#include "BasicStrategy.hpp"
#include "RoundEngine.hpp"
#include "SideBet.hpp"

#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "FontManager.hpp"
#include "Logger.hpp"
#include "Replay.hpp"
#include "StrategyTrainer.hpp"
#include "Tournament.hpp"
// included fully here so Game's out-of-line destructor destroys the unique_ptr with a
// complete type, and startStatsSession() can seed the dashboard from the player's history + career stats. Pure-logic + no
// new include cycle (pulls only GameRecord/SFML, already in this chain).
#include "StatsDashboard.hpp"
// Betting core is pure-logic; included fully here
// complete type, and both ctors can construct it. No new include cycle (pulls only Deck/BasicStrategy, already in chain).
#include "BettingHandler.hpp"

// Defined here (Replay.hpp is included in this translation unit)
// destruction runs with a complete Replay type regardless of who destroys the Game.
Game::~Game() {}

// --- Refresh-rate ("Gamer") detection ---------------------------------------------------------------
// This SFML build's sf::VideoMode exposes no public refresh-rate reader
// the OS at startup (Linux/XRandr). Best-effort: falls back to GameTiming::DEFAULT_REFRESH_HZ when there's no display or
// XRandr isn't available. The value is injected into Game
#if defined(__linux__) && !defined(__ANDROID__)
namespace
{
    // Parse the refresh rate (Hz) from a single XRandr output line, taking the last floating-point number on it as the
    // rate (modern xrandr appends it to the current-mode token, e.g. "1920x1080_144.00" or "@144").
    float refreshHzFromCurrentLine(const std::string& line)
    {
        float last = 0.0f;
        bool inNumber = false;
        size_t start = 0;
        for (size_t i = 0; i <= line.size(); ++i)
        {
            char c = static_cast<char>((i < line.size()) ? line[i] : ' ');
            const bool digitOrDot = (c == '.') || (c >= '0' && c <= '9');
            if (digitOrDot)
            {
                if (!inNumber)
                {
                    inNumber = true;
                    start = i;
                }
            }
            else if (inNumber)
            {
                inNumber = false;
                last = std::strtof(line.c_str() + start, nullptr);
            }
        }
        return last;
    }

    float detectMonitorRefreshHz()
    {
        FILE* pipe = popen("xrandr --query 2>/dev/null", "r");
        if (!pipe)
            return GameTiming::DEFAULT_REFRESH_HZ;

        std::string output;
        char buffer[512];
        size_t read = 0;
        while ((read = fread(buffer, 1, sizeof(buffer) - 1, pipe)) > 0)
        {
            buffer[read] = '\0';
            output += buffer;
        }
        pclose(pipe);

        // The active output's mode line is marked with 'current'; its refresh rate is the last float on that line.
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.find("current") != std::string::npos)
                return refreshHzFromCurrentLine(line);
        }
        return GameTiming::DEFAULT_REFRESH_HZ;
    }
}
#else
static float detectMonitorRefreshHz() { return GameTiming::DEFAULT_REFRESH_HZ; }
#endif

Game::Game()
    : window(
          sf::VideoMode({Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT}), Constants::WINDOW_TITLE),
      gameView(
          sf::FloatRect(
              {0.f, 0.f},
              {static_cast<float>(Constants::WINDOW_WIDTH),
               static_cast<float>(Constants::WINDOW_HEIGHT)})),
      inputHandler(*this),
      evaluator(*this),
      roundDealer_(*this),
      sessionLifecycle_(*this),
      achievementFeeder_(*this)
{
    window.setView(gameView);
    players.push_back(Player(Constants::DEFAULT_PLAYER_NAME));
    // Open the main menu on the settings this player actually uses instead of the compile-time defaults.
    applySavedTableSettings();
    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;
    // Gamer mode reads the monitor's refresh rate from hardware here (headless tests inject it via setRefreshRateForTest).
    refreshRateHz = detectMonitorRefreshHz();
    loadAssets();

    // wire up sound + music and restore the player's saved volumes/mute from the profile. Buffers are only
    // decoded here (full mode); the headless constructor skips loadAssets()
    sound_.registerAssetPaths(Constants::Sound::SOUND_DIR, Constants::Sound::MUSIC_DIR);
    sound_.loadAssets();
    {
        const PlayerProfile sprof = players[0].getProfile();
        SoundManager::Settings ss;
        ss.soundVolume = static_cast<float>(sprof.preferredSoundVolume);
        ss.musicVolume = static_cast<float>(sprof.preferredMusicVolume);
        ss.muted = sprof.preferredMuted;
        sound_.setSettings(ss);
    }

    replay = std::make_unique<Replay>(window, cardTexture, FontManager::get());
    // the mode's state machine. Built in full mode only; headless tests drive it directly.
    trainer = std::make_unique<StrategyTrainer>();
    // its own AppMode state machine, built in full mode only; headless tests drive
    // Tournament directly (see test_tournament_mode.cpp)
    tournament = std::make_unique<Tournament>();
    // its own AppMode state machine, built in full mode only; headless tests drive it
    // directly (see test_stats_dashboard.cpp). Data is seeded on entry via startStatsSession().
    statsDashboard = std::make_unique<StatsDashboard>();
    // Betting core (Feature): pure-logic money-in math; built in both ctors like AchievementManager
    // can drive betting decisions through Game's delegators. No SFML dependency.
    betting_ = std::make_unique<BettingHandler>();
    auto profile = players[0].getProfile();
    // All three recorded histories, not just career: Daily and tournament shoes live in their own
    // vectors, so a browser that reads longHistory alone can never show them.
    replay->loadHistory(profile.getLongHistory(), profile.getDailyShoes(), profile.getTournamentShoes());
    replay->setStartingBankroll(players[0].getBankroll());
    // restore previously-unlocked achievements from the profile (additive: empty for new/legacy profiles).
    achievementManager_.loadUnlockedFrom(profile.unlockedAchievementIds);
    Logger::info(startupControlsBanner());

}

// B11 -- startup "PLAYER CONTROLS" banner. Single source of truth shared by the production print site
// (Game::Game) and headless tests: B11 corrected its Left/Right labels so they match the InputHandler.cpp
// PlaceBets bindings (Left = auto Kelly bet, Right = confirm). Pure -- no SFML or mutable Game state -- so a
// Game(true) instance reads it directly. Not suffixed *ForTest() because Game::Game() calls it in production
// too; mirrors how calculateOptimalBet()/activeTrueCount() are plain public helpers.
std::string Game::startupControlsBanner() const
{
    static const std::string banner = R"(
=========================================================
                    ♠ BLACKJACK ♣
                     PLAYER CONTROLS
=========================================================

BETTING PHASE
-------------
  ↑  Up Arrow      Increase Bet
  ↓  Down Arrow    Decrease Bet
  <-  Left Arrow   Auto Kelly Bet
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
    return banner;
}

std::string Game::trueCountStatusLabel() const
{
    // Under Hi-Lo the box shows a real true count (running count normalized by decks remaining); under Reko it shows the
    // running count, which is the betting count ("the true count is the running count" per the roadmap), so calling that "True
    // Count" reads as a broken negative on a fresh shoe. Label accordingly. Pure -- mirror of activeTrueCount(); HUD.cpp applies
    // it to the true-count box title via StatBox::setTitle().
    return (countingStyle == BasicStrategy::CountingStyle::REKO) ? std::string("Betting Count (Reko)") : std::string("True Count");
}

std::string Game::bettingControlsHint() const
{
    // Betting-phase control hint, the single source of truth for the line drawn in the play band during PlaceBets. One
    // consistent "<key> <action>" format (no "Left: optimal" vagueness) so it can't drift against startupControlsBanner(); every
    // key is a real PlaceBets binding (InputHandler.cpp). Pure -- shared with headless tests, no SFML or mutable state.
    // Betting-phase control hint, the single source of truth for the line drawn in the play band during PlaceBets. One
    // consistent "<key> <action>" format (no "Left: optimal" vagueness) so it can't drift against startupControlsBanner(); every
    // key is a real PlaceBets binding (InputHandler.cpp). Pure -- shared with headless tests, no SFML or mutable state.
    // ASCII-only, deliberately: this string goes straight into sf::Text, and (a) sf::String decodes std::string as ANSI
    // through std::locale, so a UTF-8 arrow decodes to three code points, and (b) the shipped UI font has no U+2191/U+2193
    // glyph at all (verified with fontTools against assets/fonts/noto/NotoSans-Regular.ttf). The arrows this replaces
    // rendered as tofu boxes on screen. Locked by the ASCII test in tests/test_presentation_layout.cpp.
    return "Up Increase Bet   Down Decrease Bet   Left Auto Kelly   Right Confirm";
}

std::string Game::sideBetControlsHint() const
{
    // Side-bet control hint, shown only when the human seat can place bets interactively (Game::canPlaceBets). Same single source
    // of truth + format as bettingControlsHint(); Z cycles the kind, X/V adjust the wager (InputHandler.cpp PlaceBets bindings).
    return "Z Switch Side-Bet Type   X Raise Wager   V Lower Wager";
}

void Game::run()
{
    sf::Clock clock;

    // U7 — if no name has ever been persisted, prompt for it before ever showing the main menu. One-time startup gate: a
    // committed name makes needsNameEntry()==false and we fall straight through to MainMenu. Real mode only; headless tests
    // set the AppMode directly.
    if (needsNameEntry())
        appMode = AppMode::NameEntry;

    while (window.isOpen())
    {
        deltaTime = clock.restart().asSeconds();
        inputHandler.processEvents();
        update(deltaTime);
        renderer.draw(*this);
    }
}

// B7 — persist-on-exit choke point. Stamps shoe-record bookkeeping into career stats, the current table settings and the
// unlocked-achievement ids onto the profile exactly once, then saves. Deliberately identical to what the pause-menu
// Quit-to-Menu path did before B7; centralising it here means no exit route (Escape from trainer/tournament/rules/etc.,
// replay history) can silently drop a session's bankroll / record / settings -- the defect observed was that window-close
// saved while an Escape quit did not. Pure Game state, so headless-testable: no SFML window is required. Guards against an
// empty seat vector (no game started yet), which QuitToMenu/Event::Closed assumed away and persistOnExit cannot assume.
void Game::persistOnExit()
{
    if (players.empty())
        return;

    // Stamp the current table settings first: this is the last save on the way out -- the player count / speed / counting
    // style survive to the next launch.
    rememberTableSettings();

    PlayerProfile profile = players[0].getProfile();
    profile.addShoeRecord(currentShoeRecord);
    // stamp the current unlocked-achievement ids onto the profile too
    achievementManager_.persistUnlockedTo(profile.unlockedAchievementIds);
    players[0].updateProfile(profile);
    players[0].saveProfile();
}

// /4/9 + table-settings persistence: the session entry points and profile-driven table settings moved into the
// composed SessionLifecycle helper (Follow-up #8). Game keeps one thin delegator per method below; all mutable state stays in
// Game, so every entry point behaves byte-for-byte identically to before the split. Test accessors are retained unchanged.
float Game::trainerElapsedSecondsForTest() const { return trainer->elapsedSecondsForTest(); }

int Game::statsPerRoundCountForTest() const
{
    // nullptr-guarded: headless construction never builds the dashboard
    return statsDashboard ? static_cast<int>(statsDashboard->computed().perRoundProfit.size()) : 0;
}

void Game::startTrainerSession(int deckCount, int secondsPerSession)
{
    sessionLifecycle_.startTrainerSession(deckCount, secondsPerSession);
}

void Game::startTournamentSession(const Tournament::TournamentConfig& config)
{
    sessionLifecycle_.startTournamentSession(config);
}

void Game::startStatsSession()
{
    sessionLifecycle_.startStatsSession();
}

// ---- Daily Challenge -------------------------------------------------------
void Game::startDailySession(const std::string& dateKey, bool practice)
{
    sessionLifecycle_.startDailySession(dateKey, practice);
}

void Game::commitDailyResult(const Daily::DailyResult& result, const ShoeRecord& shoe)
{
    sessionLifecycle_.commitDailyResult(result, shoe);
}

bool Game::dailyActive() const { return dailyActive_; }
bool Game::dailyComplete() const { return dailyComplete_; }
bool Game::dailyPractice() const { return dailyPractice_; }
const std::string& Game::dailyDateKey() const { return dailyDateKey_; }
int Game::dailyNet() const { return dailyNet_; }

// The Daily shoe boundary: book the run and STOP. Ordinary play opens a new shoe at this point; the
// Daily must not, because one shoe per day is the feature. The result and the shoe record are written
// to the career file by commitDailyResult, which is the Daily's only durable write; the router sends
// the record to the Daily history, so it never touches career stats.
void Game::finishDailyShoe(const ShoeRecord& finishedShoe)
{
    dailyNet_ = players.empty() ? 0 : players[0].getBankroll() - Daily::kStartingBankroll;

    Daily::DailyResult result;
    result.dateKey = dailyDateKey_;
    result.net = dailyNet_;
    result.isContinuation = finishedShoe.isContinuation;
    result.practice = dailyPractice_;

    commitDailyResult(result, finishedShoe);

    // Sync the seat from the career file. The seat's in-memory profile was loaded BEFORE this booking,
    // and the main-menu row reads the streak off the seat -- without this the menu shows the streak
    // from before the run, which is exactly B23's shape (a consumer reading a value nothing syncs).
    // Replacing the whole profile is right here, not merely tolerable: the seat is ephemeral, and the
    // stats/history it accumulated during the Daily are precisely what must be discarded.
    if (!players.empty())
        players[0].updateProfile(PlayerProfile::load(Player::savePath()));

    dailyComplete_ = true;
    Logger::info() << "Daily " << dailyDateKey_ << " complete: net " << dailyNet_
                   << (finishedShoe.isContinuation ? " (continued shoe)" : "") << ". No further shoe.";
}

void Game::startNewGame()
{
    sessionLifecycle_.startNewGame();
}

void Game::nameEntryAppendChar(char c)
{
    // Hard clamp: ignore characters past the max so m_nameBuffer_ can never exceed Constants::MAX_PLAYER_NAME_LENGTH.
    if (m_nameBuffer_.size() < Constants::MAX_PLAYER_NAME_LENGTH)
        m_nameBuffer_.push_back(c);
}

void Game::nameEntryBackspace()
{
    if (!m_nameBuffer_.empty())
        m_nameBuffer_.pop_back();
}

const std::string& Game::nameEntryText() const
{
    return m_nameBuffer_;
}

bool Game::needsNameEntry()
{
    // A name has never been persisted (fresh/legacy profile) -> prompt before the main menu. Guard against an empty seat
    // vector so a bare headless Game can't crash; real startup always has at least seat 0.
    return !players.empty() && players[0].getProfile().getName().empty();
}

void Game::commitPlayerName(const std::string& name)
{
    if (players.empty())
        return;
    // Update the live display and the persisted profile, then write to disk so it round-trips across sessions. The human
    // seat persists by default, so this is a real save; an ephemeral seat would no-op via saveProfile(). Fixes U7: nothing
    // previously ever set profile.name.
    players[0].setName(name);
    PlayerProfile profile = players[0].getProfile();
    profile.setName(name);
    players[0].updateProfile(profile);
    players[0].saveProfile();
}

void Game::applySavedTableSettings()
{
    sessionLifecycle_.applySavedTableSettings();
}

void Game::rememberTableSettings()
{
    sessionLifecycle_.rememberTableSettings();
}

// U12 session view: switch the side-panel charts between per-shoe and per-session accumulation. The choice persists for
// the whole play window until cycled again; neither view is persisted to the profile.
void Game::cycleGraphView()
{
    hudState.graphView = (hudState.graphView == HUDState::GraphViewMode::Session)
        ? HUDState::GraphViewMode::Shoe
        : HUDState::GraphViewMode::Session;
    // U12 part 1: the global 'G' view switch also applies this whole-window scope to every side-panel graph at once. The
    // graphs are still individually refinable with a click, and pressing 'G' again re-applies the window's view.
    hud.setAllGraphScope(hudState.graphView == HUDState::GraphViewMode::Session);
}

void Game::resetShoeAnalytics()
{
    // The single shoe-boundary reset. Bumping shoeGeneration is the HUD's recalibration signal (HUDState::
    // currentShoeId); zeroing shoe_ is what actually puts the readouts back at baseline: the two graph series, the
    // std-dev accumulators, the hand-to-hand and round-to-round streaks, the strategy counts and the bet
    // distribution all restart here. That is the whole point of the per-shoe struct: every stat box except
    // Bankroll reports THIS shoe, so nothing drifts upward across shoes or leaks in from previous sessions.
    shoeGeneration++;
    shoe_ = ShoeAnalytics{};

    // Re-seed BOTH counting systems, not just the active one. `backgroundRunningCount` is what the other style
    // swaps in when the player clicks the Count box
    // True Count boxes jump to stale numbers the first time the style was toggled after a reshuffle.
    // Reko is unbalanced and compensated by starting the count at its per-deck offset (per the roadmap table);
    // Hi-Lo starts flat. rekoStartingOffset maps the active deck count to -1/-5/-12/-20/-27 for 1/2/4/6/8 decks.
    const int rekoSeed = BasicStrategy::rekoStartingOffset(gameRules_.numDecks);
    const bool rekoActive = (countingStyle == BasicStrategy::CountingStyle::REKO);
    runningCount = rekoActive ? rekoSeed : 0;
    backgroundRunningCount = rekoActive ? 0 : rekoSeed;
    trueCount = 0.f;

    shoeCumulativeWinLoss = 0;
    gameState = GameState::Started;

    // a new shoe begins: resolve the finished shoe's end-of-shoe achievements (Deviation Free / Comeback King /
    // Perfect Shoe / Kelly Master) via onNewShoe()
    achievementFeeder_.surfaceNewlyUnlocked(achievementManager_.onNewShoe());
}

void Game::startNewShoe()
{
    startNewShoeWith(std::nullopt);
}

// Shared body of startNewShoe() and startNewShoeWithSeedForTest(); the seed is the only thing that differs between
// them, so the production path (OS seed) and the deterministic test path cannot drift apart.
void Game::startNewShoeWith(std::optional<unsigned long long> seed)
{
    // A fresh, full shoe plus the shared analytics reset. Used by the R key at end of shoe and by auto-play's
    // auto-advance; the mid-shoe reshuffle (handleEmptyDeck) reuses resetShoeAnalytics() but keeps its partially
    // dealt cards, and startNewGame() resets a whole session rather than just a shoe.
    deck.create(gameRules_.numDecks);
    if (seed)
        deck.shuffleWithSeed(*seed);
    else
        deck.shuffle();
    currentShoeRecord = {};
    currentShoeRecord.shoeDecks = deck.shoeDecks();
    currentShoeRecord.shuffleSeed = deck.shuffleSeed();
    currentShoeRecord.shoeCardCount = static_cast<int>(deck.size());
    // Stamped explicitly rather than left to the struct default: this is the shared shoe boundary for
    // ordinary play, and PlayerProfile::addShoeRecord() routes the record on this value into the
    // career history. A default that silently does the routing is a default that will one day mis-route.
    currentShoeRecord.mode = ShoeMode::Regular;
    resetShoeAnalytics();
}

// thin delegator over the composed AchievementFeeder helper
// (Follow-up #8 settlement/aftermath stage). Aggregates the human seat outcome into one HandEvent and feeds
// the achievement engine; all mutable state stays in Game.
void Game::recordHandAchievements(const RoundRecord& round)
{
    achievementFeeder_.recordHandAchievements(round);
}

void Game::setPlayerCount(int count)
{
    playerCount = std::max(1, std::min(6, count));
    Logger::debug() << "Player count set to " << playerCount;
}

void Game::loadAssets()
{
    Logger::debug("Current working directory: " + std::filesystem::current_path().string());

    if (!FontManager::load("assets/fonts/noto/NotoSans-Regular.ttf",
                           "assets/fonts/roboto/RobotoFlex-VariableFont.ttf"))
    {
        Logger::error("Failed to load font!");
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
    Logger::info("New round");
    for (auto& p : players)
    {
        p.resetRound(initialBet);
    }
    dealer.clear();
    currentRoundRecord = {};
    handActions.clear();
    handActions.resize(players.size()); // one entry per player initially
    handDeviations_.clear();
    handDeviations_.resize(players.size());
    advisorFeedback = AdvisorFeedback::None;
    advisorAction = 0;
    roundState = RoundState::PlaceBets;
    activePlayerIndex = 0;
    // a fresh round starts with no deviations yet recorded for it.
    deviationsThisRound_ = 0;
}
// Events
void Game::setRulesKeyHeldForTest(bool left, bool right)
{
    m_rulesLeftHeld_ = left;
    m_rulesRightHeld_ = right;
    m_rulesKeyOverride_ = true;
}

void Game::resetRulesKeyOverrideForTest()
{
    m_rulesKeyOverride_ = false;
}

void Game::update(float deltaTime)
{
    // Each AppMode routes through its own case here instead of a wall of `if (appMode == ...)` guards (adding a mode is one new
    // case, not a new guard); non-Playing modes return before RoundEngine and Playing advances it. Enum-driven dispatch.
    switch (appMode)
    {
    case AppMode::MainMenu:
        return;
    case AppMode::NameEntry:
        return;   // static screen: no per-frame state advance. InputHandler edits m_nameBuffer_, GameRenderer draws it.
    case AppMode::ReplayPlaying:
    case AppMode::ReplayHistory:
        replay->update(deltaTime);
        return;
    // / 9 / 4 advance their own state machine, then hand off to rendering (GameRenderer.draw).
    case AppMode::StrategyTrainer:
        trainer->update(deltaTime);
        return;
    case AppMode::StatsDashboard:
        statsDashboard->update(deltaTime);
        return;
    // rules-config panel: arrow keys move the highlight, Left/Right edits the selected rule via applyGameRuleEdit.
    case AppMode::GameRules:
    {
        const int rowCount = static_cast<int>(gameRuleRows().size());
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
        {
            if (gameRulesSelectedRow > 0) --gameRulesSelectedRow;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
        {
            if (gameRulesSelectedRow < rowCount - 1) ++gameRulesSelectedRow;
        }
        const bool rightHeld = m_rulesKeyOverride_ ? m_rulesRightHeld_ : sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
        const bool leftHeld = m_rulesKeyOverride_ ? m_rulesLeftHeld_ : sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        // B6: edge-triggered so one tap is exactly one step -- applyGameRuleEdit fires only on a false->true press, never once
        // per frame while the key is held (the pre-fix `if (rightHeld || leftHeld)` re-edited every frame at uncapped fps).
        if (rightHeld && !rulesRightPrev_)
        {
            if (gameRulesSelectedRow >= 0 && gameRulesSelectedRow < rowCount)
                applyGameRuleEdit(gameRules_, gameRuleRows()[gameRulesSelectedRow].id, true);
        }
        if (leftHeld && !rulesLeftPrev_)
        {
            if (gameRulesSelectedRow >= 0 && gameRulesSelectedRow < rowCount)
                applyGameRuleEdit(gameRules_, gameRuleRows()[gameRulesSelectedRow].id, false);
        }
        rulesRightPrev_ = rightHeld;
        rulesLeftPrev_ = leftHeld;
        break;   // falls through to the Playing guard below (never reaches RoundEngine).
    }
    case AppMode::Playing:
    case AppMode::Tournament:
        break;   // both advance the turn/round state machine via RoundEngine (tournament reuses ordinary play end-to-end).
    }

    // Non-Playing modes return before RoundEngine here. Tournament is treated like Playing above so it plays exactly an
    // ordinary shoe under tournament rules; Paused / Achievements are driven elsewhere and only hold AppMode parity.
    if (appMode != AppMode::Playing && appMode != AppMode::Tournament)
        return;

    // Delegate the turn/round state machine to RoundEngine.
    RoundEngine engine;
    engine.tick(deltaTime, *this);
}

void Game::advanceTurnState(float deltaTime)
{
    // Advance one frame of the round/turn state machine without any AppMode guard, so tests drive PlayerTurn -> DealerTurn ->
    // Cleanup directly (no window or input). Instantiates RoundEngine locally exactly like update() under Playing — no game data
    // member is needed (keeps Game.hpp's shape unchanged); reveal pacing reads deltaTime per dealer turn.
    RoundEngine engine;
    engine.tick(deltaTime, *this);
}

// Shared HUD timer countdown used by update().
void Game::tickTimer(float& timerField, float deltaTime)
{
    if (timerField > 0.0f)
    {
        timerField -= deltaTime;
        if (timerField < 0.0f)
            timerField = 0.0f;
    }
}

void Game::updateHUDState()
{
    // Each readout is one self-contained concern; composition reads as an index of concerns
    // target any single concern (test_hud_layout.cpp locks the values through hudStateForTest()). No field changes.
    syncBettingAndSideBet();
    syncPlayerBankrolls();
    syncCounting();
    syncSoundState();
    syncBookAdvisor();
    syncShoeAnalytics();
}

// Betting + side bet: single data source for the HUD + betting-area renderer. Reads the live human hand;
// defaults stay "no side bet" when there is no seat or hand yet.
void Game::syncBettingAndSideBet()
{
    hudState.bankroll = players[0].getBankroll();

    hudState.currentBet = players[0].hands[0].getBet();

    if (!players.empty() && !players[0].hands.empty())
    {
        hudState.sideBetKind = players[0].hands[0].getSideBetKind();
        hudState.sideBetWager = players[0].hands[0].getSideBetWager();
        hudState.sideBetPayout = players[0].hands[0].getSideBetPayout();
        hudState.hasSideBet = (hudState.sideBetKind != SideBetKind::None);
    }
    else
    {
        hudState.sideBetKind = SideBetKind::None;
        hudState.sideBetWager = 0;
        hudState.sideBetPayout = 0;
        hudState.hasSideBet = false;
    }
}

// All player bankrolls, refreshed each frame so the readout follows betting changes.
void Game::syncPlayerBankrolls()
{
    // Populate all player bankrolls
    hudState.playerBankrolls.clear();
    for (auto& p : players)
    {
        hudState.playerBankrolls.push_back(p.getBankroll());
    }
}

// Counting readouts: running count, the active style's name, and its true count. Hi-Lo normalizes by decks remaining;
// Reko is unbalanced so "the true count is the running count" (per the roadmap), highlighted in yellow when active.
// Both counts stay tracked in the background for instant swap. Cards remaining comes off the live shoe.
void Game::syncCounting()
{
    hudState.runningCount = runningCount;
    hudState.countingStyleName = BasicStrategy::countingStyleName(countingStyle);

    if (countingStyle == BasicStrategy::CountingStyle::REKO)
        hudState.trueCount = static_cast<float>(runningCount);
    else
        hudState.trueCount = BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size()));
    hudState.trueCountHighlighted = (countingStyle == BasicStrategy::CountingStyle::REKO);
    // Keep the true-count box title in sync so it reflects the active system live (U5): Hi-Lo -> "True Count", Reko -> "Betting Count (Reko)".
    hudState.countingBoxLabel = trueCountStatusLabel();

    hudState.cardsRemaining = deck.size();
}

// surface the current sound + music state
void Game::syncSoundState()
{
    hudState.soundVolume = sound_.settings().soundVolume;
    hudState.musicVolume = sound_.settings().musicVolume;
    hudState.soundMuted = sound_.isMuted();
}

// Advisor + turn awareness: the dealer-turn flag (so the book can hide its action) plus the Book box feedback. The Book
// has three states: a green/red letter while a verdict is pending after a completed action; the live move only during
// the player's turn when an active hand exists; otherwise nothing. advisorFeedback carries the raw verdict.
void Game::syncBookAdvisor()
{
    // Turn awareness so the book advisor can hide its action during the dealer's turn.
    hudState.isDealerTurn = (roundState == RoundState::DealerTurn);

    if (advisorFeedback != AdvisorFeedback::None)
    {
        hudState.basicStrategyHint = std::string(1, advisorAction);
    }
    else if (!hudState.isDealerTurn && roundState == RoundState::PlayerTurn &&
             !players.empty() &&
             activePlayerIndex < static_cast<int>(players.size()) &&
             !players[static_cast<size_t>(activePlayerIndex)].hands.empty())
    {
        hudState.basicStrategyHint = std::string(1, giveBasicStrategyAdvice(activeHand(), dealer));
    }
    else
    {
        hudState.basicStrategyHint.clear();
    }
    hudState.advisorFeedback = advisorFeedback;
}

// Advanced Analytics: every readout comes from this shoe's trackers (shoe_)
// at each boundary: so Std Dev, Long Win, Long Loss, Edge %, Streak and Strategy % all restart with the shoe while the
// Bankroll box keeps running. Variance = E[x^2] - (E[x])^2 over per-hand net profits; std dev is its non-negative root.
void Game::syncShoeAnalytics()
{
    hudState.shoeAnalytics = &shoe_;
    hudState.longestWinningStreak = shoe_.longestWinningStreak;
    hudState.longestLosingStreak = shoe_.longestLosingStreak;
    if (shoe_.handProfitCount > 0)
    {
        double n = static_cast<double>(shoe_.handProfitCount);
        double mean = static_cast<double>(shoe_.handProfitSum) / n;
        double variance = static_cast<double>(shoe_.handProfitSumSq) / n - mean * mean;
        hudState.analyticsStdDev = (variance > 0.0) ? std::sqrt(variance) : 0.0;
    }
    else
    {
        hudState.analyticsStdDev = 0.0;
    }

    // Bottom-bar Streak box: the human's round-to-round streak, restarted with the shoe.
    hudState.streak = shoe_.roundStreak;
    hudState.streakIsWin = shoe_.roundStreakIsWin;

    // Feed the bankroll curve and edge-tracking series to the HUD charts. Both hold the CURRENT shoe only: and
    // the curve is the human seat's net profit alone, never the table's
    // moment a new shoe begins.
    hudState.bankrollSeries = shoe_.bankrollSeries;
    hudState.edgeSeries = shoe_.edgeSeries;
    // The Edge % box is the live figure: the same estimate the chart samples once per settled round, read from
    // the Hi-Lo count as it stands this instant. On a fresh shoe that is the baseline (about -0.50% for basic
    // strategy), not 0 - a new shoe is not an edge of nothing, it is the house's usual half percent.
    hudState.edgePercent = BasicStrategy::estimateEdgePercent(hiLoTrueCount());

    hudState.currentShoe = &currentShoeRecord;
    hudState.currentShoeId = shoeGeneration;
    // U12 session view: expose the live-session accumulator so HUD can render per-shoe or per-session. The active view
    // (hudState.graphView) persists across shoes for the whole play window until the user cycles it.
    hudState.sessionAnalytics = &session_;
}

// Test support: headless construction + deterministic deck injection.

Game::Game(bool headless)
    : window(),
      gameView(),
      inputHandler(*this),
      evaluator(*this),
      roundDealer_(*this),
      sessionLifecycle_(*this),
      achievementFeeder_(*this)
{
    // Headless mode (unit tests): no SFML window, no asset loading, no replay, no HUD. Only the pure
    // round/state machine is initialized
    if (!headless)
        return;

    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;
    runningCount = 0;
    backgroundRunningCount = 0;
    trueCount = 0.f;
    gameState = GameState::Started;
    roundState = RoundState::PlayerTurn;
    activePlayerIndex = 0;
    initialBet = 0;
    shoeCumulativeWinLoss = 0;
    sessionCumulativeWinLoss = 0;
    // Betting core (Feature): pure-logic money-in math, built in both ctors like AchievementManager (no SFML dependency)
    // so headless tests can drive betting decisions through Game's delegators.
    betting_ = std::make_unique<BettingHandler>();
}

// Replace the shoe with an exact card order
// natural DEAL order (first drawn first); Deck::draw() pops from the back
void Game::setDeck(const std::vector<Card>& cards)
{
    deck.setCards(std::vector<Card>(cards.rbegin(), cards.rend()));
}

const Hand& Game::testHand(size_t playerIndex, size_t handIndex) const
{
    return players[playerIndex].hands[handIndex];
}

// Set up a single in-memory player with a minimum bet and begin the round (deals the initial two cards
// plus the dealer up-card from the injected shoe). Leaves roundState at PlayerTurn when the dealer's
// up-card is not an ace, so split/hit/stand can be exercised end-to-end without SFML or disk profiles.
void Game::startTestRound()
{
    startTestRound(1);
}

// Same round setup, but seats up `seats - 1` extra in-memory NPC seats behind the human
// prove the readouts follow the human seat alone. Bets are still placed by confirmBet(), exactly as a human would.
void Game::startTestRound(int seats)
{
    // In-memory seats (NPC constructor: no disk read/write), so tests stay hermetic.
    players.clear();
    players.emplace_back(Constants::DEFAULT_PLAYER_NAME, GameConfig::STARTING_BANKROLL);

    for (int i = 0; i + 1 < seats; ++i)
    {
        const auto& cfg = GameConfig::NPC_CONFIGS[i];
        players.push_back(Player(cfg.name, cfg.bankroll));
        players[i + 1].setBettingStrategy(static_cast<BettingStrategy>(cfg.strategy), cfg.baseBet);
    }

    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;

    dealer.clear();
    runningCount = 0;
    backgroundRunningCount = 0;
    trueCount = 0.f;
    gameState = GameState::Started;
    roundState = RoundState::PlaceBets;
    activePlayerIndex = 0;
    initialBet = Constants::MINIMUM_BET;

    handActions.clear();
    handActions.resize(players.size());
    // handDeviations_ mirrors handActions' per-hand indexing (nextRound()/startNewGame() size both together);
    // sizing it here keeps a test-driven round from reading past the end in evaluateHands.
    handDeviations_.clear();
    handDeviations_.resize(players.size());

    for (auto& p : players)
        p.resetRound(Constants::MINIMUM_BET); // one hand each, minimum bet (>0) so every seat gets dealt
    deal();                                   // initial two cards + dealer up card from the injected shoe
}

// Player actions

void Game::hit()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    if (hand.isFinished())
        return;

    // Split aces receive exactly one extra card and then stand: they may not be hit again.
    if (hand.isSplitAce() && !hand.splitAceNeedsMoreCards())
    {
        Logger::info() << p.getName() << " stands on " << hand.getValue() << " after splitting aces.";
        hand.setFinished(true);
        handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("S");
        hudState.basicStrategyHint.clear();
        return;
    }

    logStrategyDeviation('H');
    dealCardToPlayer(hand);

    Logger::info() << p.getName() << " hits. Has " << hand.getValue();

    if (hand.isBust())
    {
        Logger::info() << p.getName() << " busts!";
        hand.setFinished(true);
    }
    else if (hand.getValue() == 21)
    {
        Logger::info() << p.getName() << " has 21!";
        hand.setFinished(true);
    }

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("H");
    hudState.basicStrategyHint.clear();
}

void Game::stand()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();
    Logger::info() << p.getName() << " stands on " << hand.getValue() << ".";
    logStrategyDeviation('S');
    hand.setFinished(true);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("S");
    hudState.basicStrategyHint.clear();
}

bool Game::doubleDown()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();
    int initialBet = hand.getBet();

    if (hand.isFinished())
    {
        logInvalidAction(p.getName(), "double down but hand is finished");
        return false;
    }

    // A split ace receives exactly one card and then stands
    // double-down consistent with the forced-stand behavior of hit()/stand() on split aces.
    if (hand.isSplitAce())
    {
        logInvalidAction(p.getName(), "double down but cannot double a split ace");
        return false;
    }

    // double-after-split. A hand produced by splitting may not be doubled when the rule
    // disallows it. Default rules keep DAS on
    if (hand.isSplit() && !gameRules_.doublesAfterSplitAllowed)
    {
        logInvalidAction(p.getName(), "double down but double-after-split is off");
        return false;
    }

    if (!hand.canDouble())
    {
        logInvalidAction(p.getName(), "double down but hand cannot double (not 2 cards)");
        return false;
    }

    logStrategyDeviation('D');
    hand.setDoubledDown(true);
    if (!p.placeBet(initialBet))
    {
        logInvalidAction(p.getName(), "double down but cannot afford the bet");
        return false;
    }

    hand.setBet(initialBet * 2);
    dealCardToPlayer(hand);

    // Attribute to the seat that actually doubled (an NPC doubling must not land in the human's record).
    PlayerStats& stats = p.getStats();
    stats.doubles++;
    stats.betsPlaced++;
    stats.betSum += initialBet;

    Logger::info() << p.getName() << " doubles " << initialBet << " to " << hand.getBet() << " on a " << hand.getValue() << ".";

    hand.setFinished(true);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("D");
    hudState.basicStrategyHint.clear();
    return true;
}

bool Game::split()
{
    auto& p = activePlayer();
    int handIndex = p.currentHand;

    if (p.hands[handIndex].isFinished())
    {
        logInvalidAction(p.getName(), "split but hand is finished");
        return false;
    }

    if (gameRules_.maxSplitHands > 0 && p.hands.size() >= gameRules_.maxSplitHands)
    {
        logInvalidAction(p.getName(), "split but has reached max split hands (" + std::to_string(gameRules_.maxSplitHands) + ").");
        return false;
    }

    if (!p.hands[handIndex].canSplit())
    {
        logInvalidAction(p.getName(), "split but hand cannot split (not a pair or already hit)");
        return false;
    }

    // gameRules_.resplitTensAllowed gates re-splitting ten-value pairs only: an ordinary initial split of tens is always
    // permitted. A hand that was itself produced by splitting a previous one (isSplit()) is being re-split here;
    // an untouched deal hand has isSplit() == false and splits freely regardless of the flag.
    if (p.hands[handIndex].isSplit() &&
        p.hands[handIndex].getCard(0).getBlackjackValue() == 10 && !gameRules_.resplitTensAllowed)
    {
        logInvalidAction(p.getName(), "re-split tens but resplit-ten is off");
        return false;
    }

    logStrategyDeviation('P');
    Hand newHand;
    Card splitCard = p.hands[handIndex].takeLastCard();
    // Value of the card left in the first hand (the remaining ace for a pair of aces -> 11). Computed once
    // so both hands are flagged identically.
    const int splitValue = p.hands[handIndex].getCard(0).getBlackjackValue();
    Logger::info() << p.getName() << " splits " << splitCard.getBlackjackValueToString() << "s.";
    newHand.addCard(splitCard);
    newHand.setBet(p.hands[handIndex].getBet());
    // Flag BEFORE pushing: push_back copies newHand into the vector
    // afterwards would leave the stored hand unflagged. That exact ordering bug meant the second split
    // hand never became a split-ace
    newHand.setSplit(true, splitValue);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("P");
    hudState.basicStrategyHint.clear();

    // Each split hand is left with exactly one card here. RoundEngine then deals the single permitted
    // card to every split hand uniformly (auto-finish / re-split for aces; first card then act otherwise).
    // Pre-dealing into only this hand broke that symmetry: the first split-ace hand ended up size 2 but
    // unfinished, so it was left for the player to hit and even double down.
    p.hands.push_back(newHand);
    handActions.push_back({}); // new action vector for the split hand
    handDeviations_.push_back({}); // parallel per-hand index; deviations on this new slot start empty
    p.hands[handIndex].setSplit(true, splitValue);

    // Attribute to the seat that actually split (an NPC splitting must not land in the human's record).
    PlayerStats& stats = p.getStats();
    stats.splits++;
    stats.betsPlaced++;
    stats.betSum += p.hands[handIndex].getBet(); // new hand inherits bet amount
    return true;
}

bool Game::surrender()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    // Cannot surrender a finished hand
    if (hand.isFinished())
    {
        logInvalidAction(p.getName(), "surrender but hand is finished");
        return false;
    }

    // Only allowed on the first two cards
    if (!hand.canDouble())
    {
        logInvalidAction(p.getName(), "surrender but hand cannot surrender (already hit or not 2 cards)");
        return false;
    }

    // surrender is governed by the active surrender type. None disables it entirely; Early
    // offers it only when the dealer's up-card is an Ace or ten-value card (the classic early-surrender scenario);
    // Late / Hit-or-Stand keep the historical "first two cards" behaviour. Reads the live rule
    if (!gameRules_.surrenderEnabled())
    {
        logInvalidAction(p.getName(), "surrender but surrender is off");
        return false;
    }
    int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
    bool dealerUpIsAceOrTen = (dealerUpValue == 1 || dealerUpValue >= 10);
    if (gameRules_.surrenderType == SurrenderType::Early && !dealerUpIsAceOrTen)
    {
        logInvalidAction(p.getName(), "surrender but early surrender is only offered vs an Ace or ten");
        return false;
    }

    logStrategyDeviation('R');
    Logger::info() << p.getName() << " surrenders. Loses half their bet.";

    hand.setSurrendered(true);
    hand.setFinished(true);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("R");
    hudState.basicStrategyHint.clear();
    return true;
}

void Game::logInvalidAction(const std::string& player, const std::string& reason)
{
    // U10c: action-validation feedback ("tried to double down but ...") used to warn for every active seat. During auto-play the
    // AI re-hits these dead branches constantly, spamming the console. Log only for the human seat (activePlayerIndex == 0); an
    // NPC mis-attempt still returns false and never writes, so the log stays readable. The emitted text is byte-identical to before.
    if (activePlayerIndex == 0)
        Logger::warn() << player << " tried to " << reason;
}

void Game::logStrategyDeviation(char actionTaken)
{
    auto& p = players[activePlayerIndex];
    Hand& hand = p.hands[p.currentHand];
    int playerValue = hand.getValue();
    int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
    bool hasAce = hand.isSoft();
    bool isPair = hand.size() == 2 &&
                  hand.getCard(0).getBlackjackValue() ==
                      hand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? hand.getCard(0).getBlackjackValue() : 0;

    char correct = BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);

    // Career counts go on the seat that took the action; the Strategy box counts only the human's plays, and only
    // for this shoe (shoe_ is zeroed at every boundary). NPC play used to be tallied into the human's accuracy.
    PlayerStats& stats = p.getStats();
    stats.strategyTotal++;
    // count this round's human-seat deviations
    if (activePlayerIndex == 0 && actionTaken != correct)
        deviationsThisRound_++;
    if (activePlayerIndex == 0)
        shoe_.strategyTotal++;

    // Set the verdict shown on the Book box after the action completes: green when it matched the book, red when
    // the player deviated. advisorAction holds the book's letter
    advisorFeedback = (actionTaken == correct) ? AdvisorFeedback::Correct : AdvisorFeedback::Deviated;
    advisorAction = correct;

    if (actionTaken == correct)
    {
        stats.strategyHits++;
        if (activePlayerIndex == 0)
            shoe_.strategyHits++;
    }
    else
    {
        stats.strategyDeviations++;
        if (activePlayerIndex == 0)
            shoe_.strategyDeviations++;
        Logger::info() << "Strategy deviation: took " << actionTaken << " but book says " << correct << ".";
        DeviationRecord record;
        record.playerValue = playerValue;
        record.dealerUpValue = dealerUpValue;
        record.wasSoft = hasAce;
        record.wasPair = isPair;
        record.pairValue = pairValue;
        record.actionTaken = actionTaken;
        record.correctAction = correct;
        record.bet = hand.getBet();
        record.won = false; // stamped at evaluation time (see HandEvaluator::evaluateHands)

        // Store per global-hand index so evaluation can attribute the deviation to its hand and stamp won. This
        // mirrors handActions' indexing exactly (see split() / nextRound()).
        size_t gidx = getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand);
        while (handDeviations_.size() <= gidx)
            handDeviations_.emplace_back();
        handDeviations_[gidx].push_back(record);

        stats.recentDeviations.push_back(record);
        if (stats.recentDeviations.size() > 10)
            stats.recentDeviations.erase(stats.recentDeviations.begin());
    }
}

char Game::giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand)
{
    int playerValue = playerHand.getValue();
    int dealerUpValue = dealerHand.getDealerUpCard().getBlackjackValue();
    bool hasAce = playerHand.isSoft();
    bool isPair = playerHand.size() == 2 &&
                  playerHand.getCard(0).getBlackjackValue() ==
                      playerHand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? playerHand.getCard(0).getBlackjackValue() : 0;

    return BasicStrategy::getRecommendedAction(gameRules_, playerValue, dealerUpValue, hasAce, isPair, pairValue);
}

size_t Game::getGlobalHandIndexForPlayer(size_t playerIdx, size_t handIdx)
{
    size_t globalIndex = 0;
    for (size_t i = 0; i < playerIdx; i++)
    {
        globalIndex += players[i].hands.size();
    }
    globalIndex += handIdx;
    return globalIndex;
}

float Game::activeTrueCount() const
{
    // Delegates to the composed BettingHandler; shared game state (counting style + running count + shoe depth) is
    // passed in so each system's pattern still sees its own true count. See BettingHandler::activeTrueCount.
    return betting_->activeTrueCount(deck, runningCount, countingStyle);
}

float Game::hiLoTrueCount() const
{
    // Delegates to BettingHandler: Hi-Lo calibration uses the Hi-Lo buffer when active, else the background Reko buffer.
    return betting_->hiLoTrueCount(deck, runningCount, backgroundRunningCount, countingStyle);
}

int Game::calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const
{
    // Delegates to BettingHandler: Reko steps by table-minimum units; Hi-Lo uses the Kelly criterion (one unit = min).
    return betting_->calculateOptimalBet(trueCount, kellyFraction, bankroll, minBet, maxBet, countingStyle);
}

int Game::activeMinBet() const
{
    // A tournament escalates the minimum every stage; ordinary play keeps the table MINIMUM_BET. Centralised so betting,
    // auto-play and the UI all honour the same floor without repeating it.
    if (tournamentActive_ && tournament)
        return tournament->minBetForStage(tournament->currentStage());
    return Constants::MINIMUM_BET;
}

void Game::confirmBet()
{
    int currentBet = players[0].hands[0].getBet();
    int floorBet = activeMinBet();
    if (currentBet < floorBet || currentBet > Constants::MAXIMUM_BET)
        return;

    // Escrow the main bet at commit: reject a hand bet that exceeds the *available* balance (bankroll minus any side bet already
    // committed this round). The stake is still settled later through winBet/loseBet, exactly as before -- here we only record what
    // is on the table so getAvailableBankroll() reads one predicate for every clamp and hint (B5).
    if (currentBet > players[0].getAvailableBankroll())
    {
        Logger::warn() << "Not enough bankroll. Available: " << players[0].getAvailableBankroll() << ".";
        return;
    }

    initialBet = currentBet;

    // Record the total active-hand stakes as committed this round (covers split hands, each of which carries a stake).
    int totalMain = 0;
    for (const auto& h : players[0].hands)
        totalMain += h.getBet();
    players[0].commitMainBet(totalMain);

    // a placed bet triggers the "bet" effect. Safe in headless mode (just enqueues when buffers are absent).
    // Hyper/Gamer deliver no sound effects at all (GameTiming::animationDisabled)
    // instant-play pace; every other speed keeps the event.
    if (!GameTiming::animationDisabled(gameSpeed))
        sound_.playSound(Constants::Sound::EVENT_BET);

    // Track bet stats
    PlayerStats& stats = getStats();
    stats.betsPlaced++;
    stats.betSum += currentBet;
    // record this hand's wager into the bet-distribution histogram. The career copy lives in
    // the saved profile; shoe_ is what the Bets Placed chart reads and is zeroed at every shoe boundary
    // histogram shows this shoe's wagering spread rather than everything ever bet.
    stats.betDistribution[currentBet]++;
    shoe_.betDistribution[currentBet]++;
    // U12 session view: also record this wager into the live-session distribution so the per-session Bets Placed chart
    // spans every shoe in this play window (unlike shoe_, which resets at each shoe boundary).
    session_.betDistribution[currentBet]++;

    Logger::info() << players[0].getName() << " bets " << players[0].hands[players[0].currentHand].getBet() << ". Remaining bankroll: " << players[0].getBankroll() << ". Running count: " << runningCount << ".";

    // NPCs bet according to their strategy
    for (size_t i = 1; i < players.size(); i++)
    {
        // Skip NPCs who can't afford this stage's minimum bet (which rises each tournament stage)
        if (players[i].getBankroll() < activeMinBet())
        {
            Logger::info() << players[i].getName() << " is out of chips and sits out this round.";
            players[i].hands[0].setBet(0);
            continue;
        }

        int npcBet = players[i].determineRoundBet(BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size())));

        // Apply table limits, then cap at bankroll. The floor is the stage minimum (escalating in a tournament); ordinary
        // play uses the same helper and keeps it at the table MINIMUM_BET.
        npcBet = std::max(npcBet, activeMinBet());
        npcBet = std::min(npcBet, Constants::MAXIMUM_BET);
        npcBet = std::min(npcBet, players[i].getBankroll());

        if (players[i].placeBet(npcBet))
        {
            players[i].hands[0].setBet(npcBet);
            Logger::info() << players[i].getName() << " bets " << npcBet << ".";
        }
        else
        {
            // Shouldn't happen after the checks above, but be safe
            Logger::warn() << players[i].getName() << " cannot place bet of " << npcBet << ".";
            players[i].hands[0].setBet(0);
        }
    }

    deal();
}

// Advance + wrap active player index at insurance-loop boundaries.
void Game::advanceToNextInsurancePlayer()
{
    ++activePlayerIndex;
    if (activePlayerIndex >= players.size())
    {
        activePlayerIndex = 0;
        evaluator.evaluateBlackjack();
    }
}

// Insurance
void Game::offerInsurance()
{
    auto& p = activePlayer();
    int bet = p.hands[p.currentHand].getBet();

    // Skip players who didn't bet
    if (bet == 0)
    {
        advanceToNextInsurancePlayer();
        return;
    }

    // Auto-decide for NPCs and auto-play human
    if (activePlayerIndex > 0 || GameTiming::isAutoPlaying(gameSpeed))
    {
        float tc = BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size()));
        int maxIns = bet / 2;

        if (p.shouldTakeInsurance(bet, tc, countingStyle))
            p.takeInsurance(maxIns);
        else
            Logger::info() << p.getName() << " declines insurance (TC=" << tc << ")";

        advanceToNextInsurancePlayer();
    }
    // else: human player in manual mode - wait for input (handled in InputHandler)
}

void Game::confirmInsurance()
{
    auto& p = players[activePlayerIndex];
    int insBet = p.hands[p.currentHand].getInsuranceBet();
    if (insBet > 0)
    {
        p.placeBet(insBet);
        Logger::info() << p.getName() << " places insurance bet of " << insBet << ".";
    }

    advanceToNextInsurancePlayer();
}

void Game::declineInsurance()
{
    auto& p = players[activePlayerIndex];
    p.hands[p.currentHand].setInsuranceBet(0);
    Logger::info() << p.getName() << " declines insurance.";

    advanceToNextInsurancePlayer();
}

// place an optional side bet on the human seat's main hand before the deal. The payout is resolved
// inside deal() (see resolveSideBets): BEFORE any main-hand settlement
// human seat's bankroll; a zero wager opts out regardless of kind. When no player exists yet (tests set this before
// startTestRound builds the seats), it just stores the intent
void Game::placeSideBet(SideBetKind kind, int wager)
{
    if (wager < 0)
        return;
    // Affordability uses the *available* balance so a main bet already committed this round is not double-counted against the same
    // bankroll -- that is what previously let a $1000 main + $1000 side both clear and settle to a negative bankroll (B5).
    if (!players.empty() && players[0].getAvailableBankroll() < wager)
    {
        Logger::warn() << "Cannot place side bet of " << wager << "; available bankroll is " << players[0].getAvailableBankroll() << ".";
        return;
    }
    // A side bet is defined by its KIND, not just its amount. Selecting a non-None kind at $0 keeps that kind
    // selection shows up while betting and can be funded later (the "cycle before adding" path); only an explicit None
    // kind opts out. When a positive wager is placed with no kind chosen yet, default to the first side bet
    // X/V makes the selection appear on screen immediately instead of staying invisible at kind=None. A $0 amount never
    // becomes a real bet -- deal() only applies an intent when its wager is > 0 and the kind is not None.
    if (kind == SideBetKind::None && wager > 0)
        kind = SideBetKind::PerfectPairs;
    sideBetIntentKind_ = kind;
    sideBetIntentWager_ = wager;
    if (!players.empty())
        players[0].commitSideBet(wager);   // committed this round (a 0 amount opts the side bet out); available() excludes it
    // Reflect on the live main hand when it exists
    if (!players.empty() && !players[0].hands.empty())
        players[0].hands[0].setSideBet(kind, wager);

    // placing a bet triggers the "bet" effect. Safe in headless mode (just enqueues when buffers are absent).
    // Hyper/Gamer deliver no sound effects at all (GameTiming::animationDisabled)
    if (!GameTiming::animationDisabled(gameSpeed))
        sound_.playSound(Constants::Sound::EVENT_BET);
}

// whether the human seat can currently place bets interactively. The round must be in PlaceBets,
// the active seat must be the human (index 0), and betting must not be auto-advanced (AutoPlay/Hyper/Gamer). Mirrors the
// input gate in InputHandler so rendering hints share one predicate instead of duplicating it; safe to call from any state
// (returns false outside PlaceBets). Used by GameRenderer to decide whether to draw the side-bet control line.
bool Game::canPlaceBets() const
{
    return roundState == RoundState::PlaceBets && activePlayerIndex == 0 && !GameTiming::isAutoPlaying(gameSpeed);
}

SideBetKind Game::activeSideBetKind() const
{
    // Live main-hand kind when a hand exists; otherwise the pending intent set by placeSideBet()/Z-cycling during PlaceBets so a
    // selection shows its paytable before the deal. Thin accessor mirroring canPlaceBets(); pure for headless tests (U9).
    if (!players.empty() && !players[0].hands.empty())
        return players[0].hands[players[0].currentHand].getSideBetKind();
    return sideBetIntentKind_;
}

// delegate to RoundDealer (Follow-up #8): evaluates + bankrolls every seat's side bet once all cards
// are known, BEFORE main-hand settlement. The logic moved out of the god class; this stays a thin delegating entry point.
void Game::resolveSideBets()
{
    roundDealer_.resolveSideBets();
}

// Deck / Deal: delegate card dealing + side-bet application/resolution to RoundDealer (Follow-up #8). Thin delegator over the
// extracted logic; game.dealCardToPlayer/dealCardToDealer and evaluator.evaluateBlackjack() are reached through it.
void Game::deal()
{
    roundDealer_.deal();
}

void Game::npcTakeTurn()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    if (hand.isFinished())
        return;

    Logger::debug(p.getName() + " is thinking...");
    // Under tournament rules the AI opponents play a deliberately more aggressive strategy (split tens, double any hard eight)
    // to grow their bankroll in the short window; ordinary play keeps ordinary basic strategy unchanged.
    char action = tournamentActive_ ? tournament->aiAdvice(hand, dealer.getDealerUpCard()) : giveBasicStrategyAdvice(hand, dealer);

    // If strategy says split but we've reached max split hands,
    // fall back to hard hand strategy
    if (action == 'P' && p.hands.size() >= Constants::MAX_SPLIT_HANDS)
    {
        int playerValue = hand.getValue();
        int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
        bool hasAce = hand.isSoft();
        action = BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, false, 0);
    }

    switch (action)
    {
    case 'H':
        hit();
        break;
    case 'S':
        stand();
        break;
    case 'D':
        if (!doubleDown())
            hit(); // Can't double, hit instead
        break;
    case 'P':
        if (!split())
            hit(); // Can't split, hit instead
        break;
    case 'R':
        if (!surrender())
            hit();
        break;
    default:
        stand();
        break;
    }
}
// Tournament shoe boundary (called from dealerTurn() after the last hand of a stage's shoe). Advances to the next stage by
// reshuffling a fresh four-deck shoe and re-opening betting at the higher stage minimum, or marks the tournament complete once
// the final stage has run out. Bankrolls carry across stages so the run is decided purely by who ends with the most chips.
void Game::finishTournamentShoe(const ShoeRecord& finishedShoe)
{
    // Surface the live leaderboard ordering (by bankroll) so the HUD can rank seats and name this stage's leader.
    std::vector<std::string> names;
    std::vector<int> chips;
    names.reserve(players.size());
    chips.reserve(players.size());
    for (const auto& p : players)
    {
        names.push_back(p.getName());
        chips.push_back(p.getBankroll());
    }

    const int finishedStage = tournament ? tournament->currentStage() : 0;
    bool anotherStage = tournament->advanceStage();
    const int stage = tournament->currentStage();

    // Record this stage's shoe on the career file. The tournament seat is ephemeral, so this narrow
    // commit is the only durable write the run makes, and the router puts the record in the tournament
    // history -- career stats are derived from longHistory and stay untouched by a tournament.
    sessionLifecycle_.commitTournamentShoe(finishedShoe, finishedStage, /*completed=*/!anotherStage);

    if (!anotherStage)
    {
        // Final stage has finished: the tournament is over. GameOver stays set (assigned by dealerTurn's LastHand branch) so no
        // further hands are dealt; GameRenderer draws the leaderboard + winner and Escape returns to the menu.
        const auto order = tournament->rankOrder(names, chips);
        Logger::info() << "Tournament complete after " << tournament->shoesPlayed() << " stage(s). Winner: "
                       << names[order.front()] << " with " << chips[order.front()] << " chips.";
        return;
    }

    // Advance to the next stage: a fresh four-deck shoe (regardless of the table's normal deck count), analytics reset for
    // recalibration, and betting re-opened at the higher stage minimum. resetShoeAnalytics() also restores GameState::Started,
    // which lets play continue into the new stage rather than being parked at GameOver.
    Logger::info() << "Tournament -> Stage " << stage << " of " << tournament->stages()
                   << " (min bet " << activeMinBet() << "). Fresh " << tournament->decksPerShoe() << "-deck shoe.";
    resetShoeAnalytics();
    deck.create(tournament->decksPerShoe());
    deck.shuffle();
    currentShoeRecord = {};
    currentShoeRecord.mode = ShoeMode::Tournament;   // every stage shoe routes to the tournament history
    nextRound(); // begins the new betting round at the stage minimum bet
}

void Game::afterDraw(Card card)
{
    // Advance BOTH counting systems every draw
    // lost or recomputed when the user switches. `runningCount` stays mapped to the active style;
    // `backgroundRunningCount` tracks the other one. See toggleCountingStyle().
    runningCount = BasicStrategy::updateCount(runningCount, card.getBlackjackValue(), countingStyle);

    const BasicStrategy::CountingStyle other = (countingStyle == BasicStrategy::CountingStyle::REKO)
                                                   ? BasicStrategy::CountingStyle::HI_LO
                                                   : BasicStrategy::CountingStyle::REKO;
    backgroundRunningCount = BasicStrategy::updateCount(backgroundRunningCount, card.getBlackjackValue(), other);

    if (deck.reachedCutCard())
    {
        Logger::info("Cut card reached. Last hand of shoe.");
        gameState = GameState::LastHand;
    }
}

// Toggle which counting system is shown. Both systems are always counted in the background (see afterDraw);
// this swaps which buffer feeds the active count
// style that drives display, deviations and betting.
void Game::toggleCountingStyle()
{
    const int tmp = runningCount;
    runningCount = backgroundRunningCount;
    backgroundRunningCount = tmp;

    countingStyle = (countingStyle == BasicStrategy::CountingStyle::REKO)
                        ? BasicStrategy::CountingStyle::HI_LO
                        : BasicStrategy::CountingStyle::REKO;
}

// Collect all cards currently on the table (dealer + all player hands)
std::vector<Card> Game::collectTableCards() const
{
    std::vector<Card> tableCards;

    // Dealer's cards
    for (size_t i = 0; i < dealer.size(); i++)
    {
        tableCards.push_back(dealer.getCard(i));
    }

    // All player hands
    for (const auto& p : players)
    {
        for (const auto& hand : p.hands)
        {
            for (size_t i = 0; i < hand.size(); i++)
            {
                tableCards.push_back(hand.getCard(i));
            }
        }
    }

    return tableCards;
}
// Handle the rare case where the deck runs out before the cut card
void Game::handleEmptyDeck()
{
    Logger::warn() << "Deck empty before cut card - reshuffling remaining cards.";

    // Collect all cards currently on the table (these are dealt from the new shoe)
    std::vector<Card> tableCards = collectTableCards();

    // Save the current (incomplete) shoe record to the profile before reshuffling
    currentShoeRecord.completed = false;
    currentShoeRecord.bankrollAtShoeEnd = players[0].getBankroll();
    PlayerProfile profile = players[0].getProfile();
    profile.addShoeRecord(currentShoeRecord);
    players[0].updateProfile(profile);
    players[0].saveProfile();

    // Rebuild the shoe (full NUM_DECKS set minus what's been dealt) and continue dealing from it.
    deck.reshuffleWithRemaining(tableCards);

    // The continuation belongs to the SAME session as the shoe it continues, so its mode has to be
    // carried across the reset -- a Daily that runs out mid-shoe must keep recording into the Daily's
    // history, not the career one. Captured before the reset because the reset erases it.
    const ShoeMode continuingMode = currentShoeRecord.mode;

    // Start fresh shoe record, marked as continuation
    currentShoeRecord = {};
    currentShoeRecord.shoeDecks = deck.shoeDecks();
    currentShoeRecord.shuffleSeed = deck.shuffleSeed();
    currentShoeRecord.shoeCardCount = static_cast<int>(deck.size());
    currentShoeRecord.isContinuation = true;
    currentShoeRecord.mode = continuingMode;

    // A reshuffled continuation is a new shoe: same reset the other boundaries use
    // flat baseline instead of accumulating drift across the reshuffle.
    resetShoeAnalytics();

    Logger::info() << "New shoe created with " << deck.size() << " cards.";
}

void Game::dealCardToPlayer(Hand& hand)
{
    if (deck.empty())
        handleEmptyDeck();

    Card card = deck.draw();
    afterDraw(card);
    hand.addCard(card);

    // each dealt card triggers the "card" effect. Called for every draw in both full and headless modes;
    // headless-safe because playSound() only enqueues when buffers aren't decoded yet. Hyper/Gamer deliver no sound effects
    // (GameTiming::animationDisabled)
    if (!GameTiming::animationDisabled(gameSpeed))
        sound_.playSound(Constants::Sound::EVENT_CARD);
}

void Game::dealCardToDealer()
{
    if (deck.empty())
        handleEmptyDeck();

    Card card = deck.draw();
    afterDraw(card);
    dealer.addCard(card);

    // each dealt card triggers the "card" effect (see dealCardToPlayer above for rationale). Hyper/Gamer deliver
    // no sound effects (GameTiming::animationDisabled)
    if (!GameTiming::animationDisabled(gameSpeed))
        sound_.playSound(Constants::Sound::EVENT_CARD);
}

// Dealer
void Game::dealerTurn()
{
    bool anyUnresolved = false;
    for (auto& p : players)
    {
        for (const Hand& hand : p.hands)
        {
            if (hand.isBust())
                continue;
            if (hand.isBlackjack() && !hand.isSplit())
                continue;
            anyUnresolved = true;
        }
    }

    bool doNotDraw = !anyUnresolved;

    // Should the dealer draw another card right now? Hit below soft-17 / hard-17, but never when every
    // player hand is already resolved (a standing-only dealer draws no cards) and never on a natural
    // blackjack -- a two-card 21 is finished. Hand::isSoft() reports a two-card Ace+ten as "soft"
    // default H17 rule would otherwise treat its own natural like a soft-17 and draw extra cards (the
    // insurance-decline bug: dealer with a natural drew to bust). !doNotDraw still avoids exposing the hole
    // card when every player hand has already resolved.
    bool standsAt17 = (dealer.getValue() >= Constants::DEALER_STAND);
    bool shouldHit;
    if (!standsAt17)
        shouldHit = true;                          // value < 17 -> always draw
    else if (dealer.isSoft())
        shouldHit = !gameRules_.dealerStandsOnSoft17;   // soft 17: stand only when the rule requires it
    else
        shouldHit = false;                         // hard >= 17 -> stand
    shouldHit = shouldHit && !doNotDraw && !dealer.isBlackjack();

    if (!dealerTurnStarted)
    {
        // Turn just began: arm the anticipation beat before the first card, paced for the current speed
        // auto-play (Gamer/Hyper) reveals cards at the same rate players act instead of waiting on a fixed 0.5s beat.
        dealerTurnStarted = true;
        dealerStartDelayTimer = GameTiming::dealerRevealInterval(gameSpeed, refreshRateHz);
        return;
    }
    if (shouldHit)
    {
        dealerStartDelayTimer -= deltaTime;
        if (dealerStartDelayTimer > 0.0f)
        {
            return; // wait before revealing this card
        }

        Logger::debug() << "Dealer has " << dealer.getValue();

        dealCardToDealer();

        // Arm the beat before the next card
        // so a fast auto-play keeps every reveal as quick as the last - not stuck on the fixed 0.5s beat.
        dealerStartDelayTimer = GameTiming::dealerRevealInterval(gameSpeed, refreshRateHz);
        return;
    }

    // Dealer stands: no more cards will be drawn. Reveal is complete: settle the round now and reset the
    // pacing flags for the next round.
    dealerTurnStarted = false;
    dealerStartDelayTimer = 0.0f;

    if (dealer.isBust())
    {
        Logger::info() << "Dealer busted with " << dealer.getValue() << ". Too many!";
    }
    else
    {
        switch (dealer.getValue())
        {
        case 17:
            Logger::info() << "Dealer stands on the edge of " << dealer.getValue() << ".";
            break;
        case 18:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ".";
            break;
        case 19:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ". It's bigger than average.";
            break;
        case 20:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ". Dealer always has 20.";
            break;
        case 21:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ". That's the name of the game folks.";
            break;
        default:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ".";
            break;
        }
    }

    evaluator.evaluateHands();

    if (gameState ==
        GameState::LastHand)
    {
        gameState =
            GameState::GameOver;

        // End of shoe stats
        getStats().shoesFinished++;
        shoeCumulativeWinLoss = 0; // reset for next shoe

        PlayerProfile profile = players[0].getProfile();

        currentShoeRecord.bankrollAtShoeEnd = players[0].getBankroll();
        profile.addShoeRecord(currentShoeRecord);
        players[0].updateProfile(profile);
        players[0].saveProfile();

        // Captured BEFORE the reset below erases it: the record carries the mode, the continuation flag
        // and the rounds, and both the Daily and the tournament boundary need all three.
        const ShoeRecord finishedShoe = currentShoeRecord;

        currentShoeRecord = {};

        if (dailyActive_)
        {
            // One shoe per day: book the run and stop. No new shoe and no auto-play continuation --
            // that is the whole feature, and it is why this branch precedes the ordinary-play path.
            finishDailyShoe(finishedShoe);
        }
        else if (tournamentActive_)
        {
            // A tournament stage's shoe has finished: advance to the next stage (fresh 4-deck shoe, higher minimum bet) or,
            // once the final stage has run out, mark the tournament complete so the leaderboard + winner are shown instead.
            finishTournamentShoe(finishedShoe);
        }
        else
        {
            Logger::info() << "Shoe completed. Final shoe profit: " << shoeCumulativeWinLoss << ".";

            if (GameTiming::isAutoPlaying(gameSpeed))
            {
                Logger::info() << "Auto-play: Starting a new shoe.";
                hudState.afterShoeTimer = GameTiming::afterShoeWait(gameSpeed);
            }
            else
            {
                Logger::info() << "New shoe required. Press R.";
            }
        }
    }
}