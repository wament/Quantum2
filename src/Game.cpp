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
// Feature 4 (Statistics Dashboard): included fully here so Game's out-of-line destructor destroys the unique_ptr with a
// complete type, and startStatsSession() can seed the dashboard from the player's history + career stats. Pure-logic + no
// new include cycle (pulls only GameRecord/SFML, already in this chain).
#include "StatsDashboard.hpp"
// Betting core is pure-logic; included fully here so Game's out-of-line destructor (below) destroys its unique_ptr with a
// complete type, and both ctors can construct it. No new include cycle (pulls only Deck/BasicStrategy, already in chain).
#include "BettingHandler.hpp"

// Defined here (Replay.hpp is included in this translation unit) so unique_ptr<Replay>'s member
// destruction runs with a complete Replay type regardless of who destroys the Game.
Game::~Game() {}

// --- Refresh-rate ("Gamer") detection ---------------------------------------------------------------
// This SFML build's sf::VideoMode exposes no public refresh-rate reader, so the monitor's rate is read straight from
// the OS at startup (Linux/XRandr). Best-effort: falls back to GameTiming::DEFAULT_REFRESH_HZ when there's no display or
// XRandr isn't available. The value is injected into Game so pure-logic Gamer timing stays headless-safe and testable.
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

namespace
{
    // Feature 5 (Achievement System): given a list of just-unlocked achievement ids, set the HUD popup to the
    // highest-priority one (first in definition order). Uses only the public AchievementManager API + Achievement fields,
    // so it needs no friendship and is safe at either call site.
    void setPendingUnlocked(const AchievementManager& mgr, const std::vector<Achievement::Id>& newly, std::string& pending)
    {
        if (newly.empty())
            return;
        for (const auto& def : mgr.all())
            for (auto id : newly)
                if (def.id == id)
                {
                    pending = def.name;
                    return;
                }
    }
}

Game::Game()
    : window(
          sf::VideoMode({Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT}), Constants::WINDOW_TITLE),
      gameView(
          sf::FloatRect(
              {0.f, 0.f},
              {static_cast<float>(Constants::WINDOW_WIDTH),
               static_cast<float>(Constants::WINDOW_HEIGHT)})),
      inputHandler(*this),
      evaluator(*this)
{
    window.setView(gameView);
    players.push_back(Player("William"));
    // Open the main menu on the settings this player actually uses instead of the compile-time defaults.
    applySavedTableSettings();
    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;
    // Gamer mode reads the monitor's refresh rate from hardware here (headless tests inject it via setRefreshRateForTest).
    refreshRateHz = detectMonitorRefreshHz();
    loadAssets();

    // Feature 7: wire up sound + music and restore the player's saved volumes/mute from the profile. Buffers are only
    // decoded here (full mode); the headless constructor skips loadAssets() so unit tests never open an audio device.
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
    // Feature 2 (Strategy Trainer): the mode's state machine. Built in full mode only; headless tests drive it directly.
    trainer = std::make_unique<StrategyTrainer>();
    // Feature 9 (Tournament Mode): its own AppMode state machine, built in full mode only; headless tests drive
    // Tournament directly (see test_tournament_mode.cpp), so the window never needs one for them.
    tournament = std::make_unique<Tournament>();
    // Feature 4 (Statistics Dashboard): its own AppMode state machine, built in full mode only; headless tests drive it
    // directly (see test_stats_dashboard.cpp). Data is seeded on entry via startStatsSession().
    statsDashboard = std::make_unique<StatsDashboard>();
    // Betting core (Feature): pure-logic money-in math; built in both ctors like AchievementManager so headless tests
    // can drive betting decisions through Game's delegators. No SFML dependency.
    betting_ = std::make_unique<BettingHandler>();
    auto profile = players[0].getProfile();
    replay->loadHistory(profile.getLongHistory());
    replay->setStartingBankroll(players[0].getBankroll());
    // Feature 5: restore previously-unlocked achievements from the profile (additive — empty for new/legacy profiles).
    achievementManager_.loadUnlockedFrom(profile.unlockedAchievementIds);
    Logger::info(R"(

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

)");
}

void Game::run()
{
    sf::Clock clock;

    while (window.isOpen())
    {
        deltaTime = clock.restart().asSeconds();
        inputHandler.processEvents();
        update(deltaTime);
        renderer.draw(*this);
    }
}

// Feature 2 (Strategy Trainer): begin/restart a training session. Guards against nullptr so headless construction,
// which never builds the trainer, can't crash when the menu calls this.
void Game::startTrainerSession(int deckCount, int secondsPerSession)
{
    if (!trainer)
        trainer = std::make_unique<StrategyTrainer>();
    trainer->startSession(deckCount, secondsPerSession);
}

float Game::trainerElapsedSecondsForTest() const { return trainer->elapsedSecondsForTest(); }

// Feature 9 (Tournament Mode): begin/restart a tournament. Guards against nullptr so headless construction - which
// never builds the tournament - can't crash when the menu calls this; on first entry it allocates the state machine.
void Game::startTournamentSession(const Tournament::TournamentConfig& config)
{
    if (!tournament)
        tournament = std::make_unique<Tournament>();
    tournament->start(config);
}

// Feature 4 (Statistics Dashboard): begin/restart the dashboard from the main menu. Guards against nullptr so headless
// construction (which never builds the dashboard) can't crash when the menu calls this; on first entry it seeds the
// dashboard by flattening every shoe's rounds into one history list and pairing it with the player's career stats.
void Game::startStatsSession()
{
    if (!statsDashboard)
        statsDashboard = std::make_unique<StatsDashboard>();

    // Flatten completed-shoe history from the profile plus any rounds played in the current (in-progress) shoe, into one
    // round-history list — exactly what the dashboard feeds on. Reading through getProfile().getLongHistory() keeps this
    // additive to existing persistence; nothing is rewritten here. getProfile() returns a copy by value, so we own that
    // copy for the whole function: binding const& longHistory to it (rather than to getProfile()'s temporary) keeps the
    // history reference valid and avoids a stack-use-after-scope once startStatsSession runs against a populated history.
    PlayerProfile profile = players[0].getProfile();

    std::vector<RoundRecord> rounds;
    const auto& longHistory = profile.getLongHistory();
    for (const auto& shoe : longHistory)
        for (const auto& round : shoe.rounds)
            rounds.push_back(round);
    // A round settles into BOTH profile.currentHistory (addRoundRecord) and game.currentShoeRecord.rounds. On a later
    // QuitToMenu/Restart, addShoeRecord() moves currentHistory -> longHistory but leaves the live mirror in
    // currentShoeRecord.rounds in place, so reopening stats after quitting-to-menu would otherwise feed that round twice
    // (once from longHistory, once from the stale current record). Skip any current-record round whose content already
    // appears in history — dealerCards + playerHands fully fingerprint a round's consumed outcome — so every round feeds
    // exactly once regardless of how many times it was stored. RoundRecord has no operator==, so compare members directly.
    auto cardsMatch = [](const Card& a, const Card& b) { return a.value == b.value && a.suit == b.suit; };
    auto roundOverlap = [&](const RoundRecord& candidate) -> bool
    {
        for (const auto& existing : rounds)
        {
            if (existing.dealerCards.size() != candidate.dealerCards.size())
                continue;
            bool dealerMatch = true;
            for (int i = 0; i < static_cast<int>(existing.dealerCards.size()); ++i)
            {
                if (!cardsMatch(existing.dealerCards[i], candidate.dealerCards[i]))
                {
                    dealerMatch = false;
                    break;
                }
            }
            if (!dealerMatch)
                continue;

            if (existing.playerHands.size() != candidate.playerHands.size())
                continue;
            bool handsMatch = true;
            for (int i = 0; i < static_cast<int>(existing.playerHands.size()); ++i)
            {
                const HandOutcome& e = existing.playerHands[i];
                const HandOutcome& c = candidate.playerHands[i];
                if (e.wager != c.wager || e.payout != c.payout || e.won != c.won ||
                    e.surrendered != c.surrendered || e.insuranceBet != c.insuranceBet ||
                    e.insuranceTaken != c.insuranceTaken || e.playerIndex != c.playerIndex ||
                    e.playerName != c.playerName || e.sideBetKind != c.sideBetKind ||
                    e.sideBetWager != c.sideBetWager || e.sideBetPayout != c.sideBetPayout)
                {
                    handsMatch = false;
                    break;
                }
                if (e.cards.size() != c.cards.size())
                {
                    handsMatch = false;
                    break;
                }
                for (int k = 0; k < static_cast<int>(e.cards.size()); ++k)
                {
                    if (!cardsMatch(e.cards[k], c.cards[k]))
                    {
                        handsMatch = false;
                        break;
                    }
                }
                if (!handsMatch)
                    break;
            }
            if (handsMatch)
                return true;
        }
        return false;
    };
    for (const auto& round : currentShoeRecord.rounds)
        if (!roundOverlap(round))
            rounds.push_back(round);

    statsDashboard->feed(players[0].getStats(), rounds);
}

int Game::statsPerRoundCountForTest() const
{
    // nullptr-guarded: headless construction never builds the dashboard, so a test can call this before any session.
    return statsDashboard ? static_cast<int>(statsDashboard->computed().perRoundProfit.size()) : 0;
}

void Game::startNewGame()
{
    Logger::info("Starting new game");

    // Remember what was just chosen before the profile is re-read below, so the next launch opens on it.
    rememberTableSettings();

    players.clear();

    // Human player (William) - always first
    players.push_back(Player("William"));

    for (int i = 0; i < GameConfig::NPC_COUNT && i + 1 < playerCount; i++)
    {
        const auto& cfg = GameConfig::NPC_CONFIGS[i];
        // Fresh in-memory NPC: no disk read, no JSON parse, no persistence. The human player above loads the real profile.
        players.push_back(Player(cfg.name, cfg.bankroll));
        players[i + 1].setBettingStrategy(static_cast<BettingStrategy>(cfg.strategy), cfg.baseBet);
        Logger::info() << players[i + 1].getName() << " starts with $" << cfg.bankroll
                       << " using " << cfg.strategy << " strategy";
    }

    auto profile = players[0].getProfile();

    deck.create(gameRules_.numDecks);
    deck.shuffle();
    currentShoeRecord.initialDeck = deck.getCards();
    currentShoeRecord.rounds.clear();

    // A brand-new shoe: every per-shoe readout starts from zero, so the stat boxes and the bankroll/edge charts
    // reflect only this shoe. (The bankroll itself is deliberately kept — it is the one value that persists.)
    resetShoeAnalytics();
    sessionCumulativeWinLoss = 0;
    handActions.clear();
    handActions.resize(1);
    handDeviations_.clear();
    handDeviations_.resize(1);
    advisorFeedback = AdvisorFeedback::None;
    advisorAction = 0;
    dealer.clear();
    players[0].resetRound(0);
    nextRound();

    appMode = AppMode::Playing;
}

void Game::applySavedTableSettings()
{
    if (players.empty())
        return;

    const PlayerProfile profile = players[0].getProfile();

    // setPlayerCount clamps to [1,6], so a stale or hand-edited value can never ask for a nonexistent seat.
    setPlayerCount(profile.preferredPlayerCount);

    // An int read off disk can be anything, so validate against the enumerators before trusting it.
    switch (static_cast<GameSpeed>(profile.preferredSpeed))
    {
    case GameSpeed::Fast:
    case GameSpeed::Normal:
    case GameSpeed::Slow:
    case GameSpeed::AutoPlay:
    case GameSpeed::Hyper:
    case GameSpeed::Gamer:
        gameSpeed = static_cast<GameSpeed>(profile.preferredSpeed);
        break;
    default:
        gameSpeed = GameSpeed::Normal;
        break;
    }

    switch (static_cast<BasicStrategy::CountingStyle>(profile.preferredCountingStyle))
    {
    case BasicStrategy::CountingStyle::HI_LO:
    case BasicStrategy::CountingStyle::REKO:
        countingStyle = static_cast<BasicStrategy::CountingStyle>(profile.preferredCountingStyle);
        break;
    default:
        countingStyle = BasicStrategy::CountingStyle::REKO;
        break;
    }

    // Feature 3 (Custom Game Rules): restore the saved table rules. The profile's GameRules already carries defaults for
    // legacy files, so this is a clean no-op unless the player previously changed a rule and it was persisted additively.
    gameRules_ = profile.gameRules;

    Logger::info() << "Restored table settings: " << playerCount << " players, "
                   << BasicStrategy::countingStyleName(countingStyle) << " counting.";
}

void Game::rememberTableSettings()
{
    if (players.empty())
        return;

    PlayerProfile profile = players[0].getProfile();
    profile.preferredPlayerCount = playerCount;
    profile.preferredSpeed = static_cast<int>(gameSpeed);
    profile.preferredCountingStyle = static_cast<int>(countingStyle);
    // Feature 7: persist sound + music prefs alongside the other table settings (called at every existing save point,
    // so no extra I/O). Written as SFML-unit ints to mirror preferredSpeed.
    profile.preferredSoundVolume = static_cast<int>(sound_.settings().soundVolume);
    profile.preferredMusicVolume = static_cast<int>(sound_.settings().musicVolume);
    profile.preferredMuted = sound_.isMuted();
    // Feature 3 (Custom Game Rules): stamp the live table rules onto the profile so they survive the session, written on
    // every existing save point (no extra I/O). Additive schema: unchanged for profiles saved before custom rules.
    profile.gameRules = gameRules_;
    players[0].updateProfile(profile);
    players[0].saveProfile();
}

void Game::resetShoeAnalytics()
{
    // The single shoe-boundary reset. Bumping shoeGeneration is the HUD's recalibration signal (HUDState::
    // currentShoeId); zeroing shoe_ is what actually puts the readouts back at baseline — the two graph series, the
    // std-dev accumulators, the hand-to-hand and round-to-round streaks, the strategy counts and the bet
    // distribution all restart here. That is the whole point of the per-shoe struct: every stat box except
    // Bankroll reports THIS shoe, so nothing drifts upward across shoes or leaks in from previous sessions.
    shoeGeneration++;
    shoe_ = ShoeAnalytics{};

    // Re-seed BOTH counting systems, not just the active one. `backgroundRunningCount` is what the other style
    // swaps in when the player clicks the Count box, so leaving it at the previous shoe's value made the Count and
    // True Count boxes jump to stale numbers the first time the style was toggled after a reshuffle.
    // Reko is unbalanced and compensated by starting the count at its per-deck offset (per the roadmap table);
    // Hi-Lo starts flat. rekoStartingOffset maps NUM_DECKS to -1/-5/-12/-20/-27 for 1/2/4/6/8 decks.
    const int rekoSeed = BasicStrategy::rekoStartingOffset(Constants::NUM_DECKS);
    const bool rekoActive = (countingStyle == BasicStrategy::CountingStyle::REKO);
    runningCount = rekoActive ? rekoSeed : 0;
    backgroundRunningCount = rekoActive ? 0 : rekoSeed;
    trueCount = 0.f;

    shoeCumulativeWinLoss = 0;
    gameState = GameState::Started;

    // Feature 5: a new shoe begins — resolve the finished shoe's end-of-shoe achievements (Deviation Free / Comeback King /
    // Perfect Shoe / Kelly Master) via onNewShoe(), which then resets every per-shoe counter for this fresh shoe.
    setPendingUnlocked(achievementManager_, achievementManager_.onNewShoe(), hudState.pendingAchievementName);
}

void Game::startNewShoe()
{
    // A fresh, full shoe plus the shared analytics reset. Used by the R key at end of shoe and by auto-play's
    // auto-advance; the mid-shoe reshuffle (handleEmptyDeck) reuses resetShoeAnalytics() but keeps its partially
    // dealt cards, and startNewGame() resets a whole session rather than just a shoe.
    deck.create(gameRules_.numDecks);
    deck.shuffle();
    currentShoeRecord = {};
    currentShoeRecord.initialDeck = deck.getCards();
    resetShoeAnalytics();
}

// Feature 5 (Achievement System): feed one settled round's human-seat outcome into the achievement engine and surface any
// just-unlocked popup. The human seat is index 0; a split yields multiple seat-0 outcomes on the record, so their signals
// are OR'd. True count comes straight from the settled round; bankroll is read live. Shoe-end achievements resolve in
// resetShoeAnalytics() via onNewShoe(), not here.
void Game::recordHandAchievements(const RoundRecord& round)
{
    AchievementManager::HandEvent e;
    int bet = 0;
    int seat0Outcomes = 0;
    for (const auto& h : round.playerHands)
    {
        if (h.playerIndex != 0)
            continue;
        ++seat0Outcomes;
        const Hand hand(h.cards);
        e.isBlackjack = e.isBlackjack || hand.isBlackjack();
        e.busted = e.busted || hand.getValue() > 21;
        e.wonAny = e.wonAny || h.won;
        bet += h.wager;
    }
    e.splitHand = seat0Outcomes > 1;                 // a split produces more than one seat-0 outcome on the record
    e.followedStrategy = (deviationsThisRound_ == 0); // whole round matched basic strategy if no deviation was logged
    e.bet = bet;
    e.bankrollAfter = players.empty() ? 0.0f : static_cast<float>(players[0].getBankroll());
    e.positiveTrueCount = round.trueCount > 0.0f;

    std::vector<Achievement::Id> newly = achievementManager_.recordHand(e);
    setPendingUnlocked(achievementManager_, newly, hudState.pendingAchievementName);
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
    // Feature 5: a fresh round starts with no deviations yet recorded for it.
    deviationsThisRound_ = 0;
}
// Events
void Game::update(float deltaTime)
{
    // Each AppMode routes through its own case here instead of a wall of `if (appMode == ...)` guards (adding a mode is one new
    // case, not a new guard); non-Playing modes return before RoundEngine and Playing advances it. Enum-driven dispatch.
    switch (appMode)
    {
    case AppMode::MainMenu:
        return;
    case AppMode::ReplayPlaying:
    case AppMode::ReplayHistory:
        replay->update(deltaTime);
        return;
    // Feature 2 / 9 / 4 advance their own state machine, then hand off to rendering (GameRenderer.draw).
    case AppMode::StrategyTrainer:
        trainer->update(deltaTime);
        return;
    case AppMode::Tournament:
        tournament->update(deltaTime);
        return;
    case AppMode::StatsDashboard:
        statsDashboard->update(deltaTime);
        return;
    // Feature 3: rules-config panel — arrow keys move the highlight, Left/Right edits the selected rule via applyGameRuleEdit.
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
        const bool rightHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
        const bool leftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        if (rightHeld || leftHeld)
        {
            if (gameRulesSelectedRow >= 0 && gameRulesSelectedRow < rowCount)
                applyGameRuleEdit(gameRules_, gameRuleRows()[gameRulesSelectedRow].id, rightHeld);
        }
        break;   // falls through to the Playing guard below (never reaches RoundEngine).
    }
    case AppMode::Playing:
        break;   // sole mode that advances the turn/round state machine via RoundEngine.
    }

    // Non-Playing modes return before RoundEngine here (identical to the original guards). Paused / Achievements are driven
    // elsewhere and only hold AppMode parity — they fall through this same guard, exactly as they did before the refactor.
    if (appMode != AppMode::Playing)
        return;

    // Delegate the turn/round state machine to RoundEngine.
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
    // Each readout is one self-contained concern; composition reads as an index of concerns, so a reviewer or test can
    // target any single concern (test_hud_layout.cpp locks the values through hudStateForTest()). No field changes.
    syncBettingAndSideBet();
    syncPlayerBankrolls();
    syncCounting();
    syncSoundState();
    syncBookAdvisor();
    syncShoeAnalytics();
}

// Betting + side bet (Feature 1): single data source for the HUD + betting-area renderer. Reads the live human hand;
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

    hudState.cardsRemaining = deck.size();
}

// Feature 7: surface the current sound + music state so any HUD/renderer layer can read it from a single source.
void Game::syncSoundState()
{
    hudState.soundVolume = sound_.settings().soundVolume;
    hudState.musicVolume = sound_.settings().musicVolume;
    hudState.soundMuted = sound_.isMuted();
}

// Advisor + turn awareness: the dealer-turn flag (so the book can hide its action) plus the Book box feedback. The Book
// has three states — a green/red letter while a verdict is pending after a completed action; the live move only during
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

// Advanced Analytics (Feature 3): every readout comes from this shoe's trackers (shoe_), which resetShoeAnalytics() zeroes
// at each boundary — so Std Dev, Long Win, Long Loss, Edge %, Streak and Strategy % all restart with the shoe while the
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

    // Feed the bankroll curve and edge-tracking series to the HUD charts. Both hold the CURRENT shoe only — and
    // the curve is the human seat's net profit alone, never the table's — so they return to their baseline the
    // moment a new shoe begins.
    hudState.bankrollSeries = shoe_.bankrollSeries;
    hudState.edgeSeries = shoe_.edgeSeries;
    // The Edge % box is the live figure: the same estimate the chart samples once per settled round, read from
    // the Hi-Lo count as it stands this instant. On a fresh shoe that is the baseline (about -0.50% for basic
    // strategy), not 0 - a new shoe is not an edge of nothing, it is the house's usual half percent.
    hudState.edgePercent = BasicStrategy::estimateEdgePercent(hiLoTrueCount());

    hudState.currentShoe = &currentShoeRecord;
    hudState.currentShoeId = shoeGeneration;
}

// Test support: headless construction + deterministic deck injection.

Game::Game(bool headless)
    : window(),
      gameView(),
      inputHandler(*this),
      evaluator(*this)
{
    // Headless mode (unit tests): no SFML window, no asset loading, no replay, no HUD. Only the pure
    // round/state machine is initialized so a full game can be driven deterministically from tests.
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

// Replace the shoe with an exact card order so tests can deal a fully determined sequence. `cards` is in
// natural DEAL order (first drawn first); Deck::draw() pops from the back, so we reverse before storing.
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

// Same round setup, but seats up `seats - 1` extra in-memory NPC seats behind the human so the analytics tests can
// prove the readouts follow the human seat alone. Bets are still placed by confirmBet(), exactly as a human would.
void Game::startTestRound(int seats)
{
    // In-memory seats (NPC constructor: no disk read/write), so tests stay hermetic.
    players.clear();
    players.emplace_back("William", GameConfig::STARTING_BANKROLL);

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

    // Split aces receive exactly one extra card and then stand — they may not be hit again.
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
        Logger::warn() << p.getName() << " tried to double down but hand is finished.";
        return false;
    }

    // A split ace receives exactly one card and then stands, so it may never be doubled. This guard makes
    // double-down consistent with the forced-stand behavior of hit()/stand() on split aces.
    if (hand.isSplitAce())
    {
        Logger::warn() << p.getName() << " tried to double down but cannot double a split ace.";
        return false;
    }

    // Feature 3 (Custom Game Rules): double-after-split. A hand produced by splitting may not be doubled when the rule
    // disallows it. Default rules keep DAS on, so existing play is unchanged; this only refuses under "DAS off".
    if (hand.isSplit() && !gameRules_.doublesAfterSplitAllowed)
    {
        Logger::warn() << p.getName() << " tried to double down but double-after-split is off.";
        return false;
    }

    if (!hand.canDouble())
    {
        Logger::warn() << p.getName() << " tried to double down but hand cannot double (not 2 cards).";
        return false;
    }

    logStrategyDeviation('D');
    hand.setDoubledDown(true);
    if (!p.placeBet(initialBet))
    {
        Logger::warn() << p.getName() << " tried to double down but cannot afford the bet.";
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
        Logger::warn() << p.getName() << " tried to split but hand is finished.";
        return false;
    }

    if (p.hands.size() >= gameRules_.maxSplitHands)
    {
        Logger::warn() << p.getName() << " tried to split but has reached max split hands (" << gameRules_.maxSplitHands << ").";
        return false;
    }

    if (!p.hands[handIndex].canSplit())
    {
        Logger::warn() << p.getName() << " tried to split but hand cannot split (not a pair or already hit).";
        return false;
    }

    // gameRules_.resplitTentsAllowed gates re-splitting ten-value pairs only: an ordinary initial split of tens is always
    // permitted. A hand that was itself produced by splitting a previous one (isSplit()) is being re-split here;
    // an untouched deal hand has isSplit() == false and splits freely regardless of the flag.
    if (p.hands[handIndex].isSplit() &&
        p.hands[handIndex].getCard(0).getBlackjackValue() == 10 && !gameRules_.resplitTentsAllowed)
    {
        Logger::warn() << p.getName() << " tried to re-split tens but resplit-ten is off.";
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
    // Flag BEFORE pushing: push_back copies newHand into the vector, so setting flags on the local copy
    // afterwards would leave the stored hand unflagged. That exact ordering bug meant the second split
    // hand never became a split-ace, so RoundEngine could not auto-finish it and the round stalled.
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
        Logger::warn() << p.getName() << " tried to surrender but hand is finished.";
        return false;
    }

    // Only allowed on the first two cards
    if (!hand.canDouble())
    {
        Logger::warn() << p.getName() << " tried to surrender but hand cannot surrender (already hit or not 2 cards).";
        return false;
    }

    // Feature 3 (Custom Game Rules): surrender is governed by the active surrender type. None disables it entirely; Early
    // offers it only when the dealer's up-card is an Ace or ten-value card (the classic early-surrender scenario);
    // Late / Hit-or-Stand keep the historical "first two cards" behaviour. Reads the live rule so in-game edits apply now.
    if (!gameRules_.surrenderEnabled())
    {
        Logger::warn() << p.getName() << " tried to surrender but surrender is off.";
        return false;
    }
    int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
    bool dealerUpIsAceOrTen = (dealerUpValue == 1 || dealerUpValue >= 10);
    if (gameRules_.surrenderType == SurrenderType::Early && !dealerUpIsAceOrTen)
    {
        Logger::warn() << p.getName() << " tried to surrender but early surrender is only offered vs an Ace or ten.";
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
    // Feature 5: count this round's human-seat deviations so recordHandAchievements() can feed strategy-adherence checks.
    if (activePlayerIndex == 0 && actionTaken != correct)
        deviationsThisRound_++;
    if (activePlayerIndex == 0)
        shoe_.strategyTotal++;

    // Set the verdict shown on the Book box after the action completes: green when it matched the book, red when
    // the player deviated. advisorAction holds the book's letter so green/red reads as "book said X, you were right/wrong".
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

void Game::confirmBet()
{
    int currentBet = players[0].hands[0].getBet();
    if (currentBet < Constants::MINIMUM_BET || currentBet > Constants::MAXIMUM_BET)
        return;

    if (!players[0].placeBet(currentBet))
    {
        Logger::warn() << "Not enough bankroll. " << players[0].getBankroll() << " is your max. Or " << players[0].getBankroll() / 2 << " if you want to double.";
        return;
    }

    initialBet = currentBet;

    // Feature 7: a placed bet triggers the "bet" effect. Safe in headless mode (just enqueues when buffers are absent).
    // Hyper/Gamer deliver no sound effects at all (GameTiming::animationDisabled), so skip it there to match that silent,
    // instant-play pace; every other speed keeps the event.
    if (!GameTiming::animationDisabled(gameSpeed))
        sound_.playSound(Constants::Sound::EVENT_BET);

    // Track bet stats
    PlayerStats& stats = getStats();
    stats.betsPlaced++;
    stats.betSum += currentBet;
    // Feature 3: record this hand's wager into the bet-distribution histogram (Feature 3). The career copy lives in
    // the saved profile; shoe_ is what the Bets Placed chart reads and is zeroed at every shoe boundary, so the
    // histogram shows this shoe's wagering spread rather than everything ever bet.
    stats.betDistribution[currentBet]++;
    shoe_.betDistribution[currentBet]++;

    Logger::info() << players[0].getName() << " bets " << players[0].hands[players[0].currentHand].getBet() << ". Remaining bankroll: " << players[0].getBankroll() << ". Running count: " << runningCount << ".";

    // NPCs bet according to their strategy
    for (size_t i = 1; i < players.size(); i++)
    {
        // Skip NPCs who can't afford the minimum bet
        if (players[i].getBankroll() < Constants::MINIMUM_BET)
        {
            Logger::info() << players[i].getName() << " is out of chips and sits out this round.";
            players[i].hands[0].setBet(0);
            continue;
        }

        int npcBet = players[i].determineRoundBet(BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size())));

        // Apply table limits, then cap at bankroll
        npcBet = std::max(npcBet, Constants::MINIMUM_BET);
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

// Feature 1 (Side Bets): place an optional side bet on the human seat's main hand before the deal. The payout is resolved
// inside deal() (see resolveSideBets) — BEFORE any main-hand settlement — so this only records the choice. Guarded by the
// human seat's bankroll; a zero wager opts out regardless of kind. When no player exists yet (tests set this before
// startTestRound builds the seats), it just stores the intent so deterministic tests can drive a full betting->deal cycle.
void Game::placeSideBet(SideBetKind kind, int wager)
{
    if (wager < 0)
        return;
    if (!players.empty() && players[0].getBankroll() < wager)
    {
        Logger::warn() << "Cannot place side bet of " << wager << "; bankroll is " << players[0].getBankroll() << ".";
        return;
    }
    // A side bet is defined by its KIND, not just its amount. Selecting a non-None kind at $0 keeps that kind so the
    // selection shows up while betting and can be funded later (the "cycle before adding" path); only an explicit None
    // kind opts out. When a positive wager is placed with no kind chosen yet, default to the first side bet so pressing
    // X/V makes the selection appear on screen immediately instead of staying invisible at kind=None. A $0 amount never
    // becomes a real bet -- deal() only applies an intent when its wager is > 0 and the kind is not None.
    if (kind == SideBetKind::None && wager > 0)
        kind = SideBetKind::PerfectPairs;
    sideBetIntentKind_ = kind;
    sideBetIntentWager_ = wager;
    // Reflect on the live main hand when it exists so HUD/renderer show the choice instantly while the player places it.
    if (!players.empty() && !players[0].hands.empty())
        players[0].hands[0].setSideBet(kind, wager);

    // Feature 7: placing a bet triggers the "bet" effect. Safe in headless mode (just enqueues when buffers are absent).
    // Hyper/Gamer deliver no sound effects at all (GameTiming::animationDisabled), so skip it there; every other speed keeps it.
    if (!GameTiming::animationDisabled(gameSpeed))
        sound_.playSound(Constants::Sound::EVENT_BET);
}

// Feature 1 (Side Bets): whether the human seat can currently place bets interactively. The round must be in PlaceBets,
// the active seat must be the human (index 0), and betting must not be auto-advanced (AutoPlay/Hyper/Gamer). Mirrors the
// input gate in InputHandler so rendering hints share one predicate instead of duplicating it; safe to call from any state
// (returns false outside PlaceBets). Used by GameRenderer to decide whether to draw the side-bet control line.
bool Game::canPlaceBets() const
{
    return roundState == RoundState::PlaceBets && activePlayerIndex == 0 && !GameTiming::isAutoPlaying(gameSpeed);
}

// Feature 1 (Side Bets): evaluate + bankroll every seat's side bet once all cards are known. Each side bet resolves against
// the seat's two hand cards plus the dealer's up-card, and it pays out / loses INDEPENDENTLY of the main hand, so this runs
// for all seats here — before evaluateHands() settles the main hands (deal() calls it right after the hole card).
void Game::resolveSideBets()
{
    if (players.empty() || dealer.size() < 1)
        return;

    const Card& dealerUp = dealer.getCard(0);
    for (size_t i = 0; i < players.size(); ++i)
    {
        auto& p = players[i];
        Hand& hand = p.hands[0]; // the main hand carries the side bet; split hands do not in this mode
        if (hand.getSideBetKind() == SideBetKind::None || hand.getSideBetWager() <= 0 || hand.size() < 2)
            continue;

        const Card& c0 = hand.getCard(0);
        const Card& c1 = hand.getCard(1);
        int wager = hand.getSideBetWager();
        SideBetResult result = (hand.getSideBetKind() == SideBetKind::PerfectPairs)
                                   ? SideBetEvaluator::perfectPairs(c0, c1)
                                   : SideBetEvaluator::twentyOneThree(c0, c1, dealerUp);

        PlayerStats& stats = p.getStats();
        if (result.won())
        {
            // Net gain is wager * ratio. The stake was never removed at placement (placeBet only guards), so winning adds
            // exactly the winnings here; gross payout for replay/analytics is stake returned + winnings.
            p.winBet(wager * result.multiplier);
            hand.setSideBetPayout(wager * (1 + result.multiplier));
            stats.sideBetWins++;
            stats.sideBetProfitSum += static_cast<long long>(wager) * result.multiplier;
            Logger::info() << p.getName() << "'s side bet wins " << result.multiplier << ":1 on $" << wager
                           << " (+" << (wager * result.multiplier) << ").";
        }
        else
        {
            // Nothing qualified: the whole side-bet wager is lost. Gross payout 0 => net -wager, matching the bankroll move.
            p.loseBet(wager);
            hand.setSideBetPayout(0);
            stats.sideBetLosses++;
            stats.sideBetProfitSum -= wager;
            Logger::info() << p.getName() << "'s side bet of $" << wager << " lost.";
        }
    }
}

// Deck / Deal
void Game::deal()
{
    for (auto& p : players)
    {
        if (p.hands[0].getBet() > 0)
        {
            dealCardToPlayer(p.hands[0]);
        }
    }
    dealCardToDealer(); // dealer's first card: index 0, the face-up UP card (see Hand::getDealerUpCard)
    for (auto& p : players)
    {
        if (p.hands[0].getBet() > 0)
        {
            dealCardToPlayer(p.hands[0]);
        }
    }
    dealCardToDealer(); // dealer's second card: the face-down hole card, revealed on the dealer's turn

    // Feature 1 (Side Bets): apply the human seat's pending side-bet choice to its main hand and resolve every placed side
    // bet now — after all cards are known but BEFORE any main-hand settlement. Side bets pay / lose independently of the main
    // hand, so a natural blackjack still resolves its side bet immediately (the round then flows through the BJ or dealer path
    // below). The intent is consumed here so it is never re-applied on the next round.
    if (sideBetIntentWager_ > 0 && sideBetIntentKind_ != SideBetKind::None)
    {
        players[0].hands[0].setSideBet(sideBetIntentKind_, sideBetIntentWager_);
        sideBetIntentKind_ = SideBetKind::None;
        sideBetIntentWager_ = 0;
    }
    resolveSideBets();

    roundState = RoundState::PlayerTurn;

    if (dealer.getDealerUpCard().isAce())
    {
        Logger::info("Dealer shows an Ace. Insurance offered.");
        roundState = RoundState::OfferInsurance;
    }
    else
        evaluator.evaluateBlackjack();

    Logger::info() << players[0].getName() << " has a " << players[0].hands[0].getValue() << " against dealer's " << dealer.getDealerUpCardToString() << ".";
}

void Game::npcTakeTurn()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    if (hand.isFinished())
        return;

    Logger::debug(p.getName() + " is thinking...");
    char action = giveBasicStrategyAdvice(hand, dealer);

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
void Game::afterDraw(Card card)
{
    // Advance BOTH counting systems every draw so toggling between them is instant — neither count is ever
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
// this swaps which buffer feeds the active count so switching between them is instant, and flips the active
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

    // Start fresh shoe record, marked as continuation
    currentShoeRecord = {};
    currentShoeRecord.initialDeck = deck.getCards();
    currentShoeRecord.isContinuation = true;

    // A reshuffled continuation is a new shoe: same reset the other boundaries use, so the graphs start from a
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

    // Feature 7: each dealt card triggers the "card" effect. Called for every draw in both full and headless modes;
    // headless-safe because playSound() only enqueues when buffers aren't decoded yet. Hyper/Gamer deliver no sound effects
    // (GameTiming::animationDisabled), so skip it there to match that silent, instant-play pace; every other speed keeps it.
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

    // Feature 7: each dealt card triggers the "card" effect (see dealCardToPlayer above for rationale). Hyper/Gamer deliver
    // no sound effects (GameTiming::animationDisabled), so skip it there; every other speed keeps it.
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
    // blackjack -- a two-card 21 is finished. Hand::isSoft() reports a two-card Ace+ten as "soft", so the
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
        // Turn just began: arm the anticipation beat before the first card, paced for the current speed so a fast
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

        // Arm the beat before the next card so cards come out one at a time, paced for speed (see dealerRevealInterval)
        // so a fast auto-play keeps every reveal as quick as the last - not stuck on the fixed 0.5s beat.
        dealerStartDelayTimer = GameTiming::dealerRevealInterval(gameSpeed, refreshRateHz);
        return;
    }

    // Dealer stands: no more cards will be drawn. Reveal is complete — settle the round now and reset the
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

        currentShoeRecord = {};
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