#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>

enum class AppMode
{
    MainMenu,
    Playing,
    Paused,
    ReplayHistory,
    ReplayPlaying,
    StrategyTrainer,
    Tournament,
    Achievements,
    GameRules,
    StatsDashboard,
    NameEntry
};
enum class MenuOption
{
    PlayGame,
    PlayerCount,
    GameSpeed,
    ReviewHistory,
    StrategyTrainer,
    Tournament,
    Achievements,
    GameRules,
    Statistics
};
enum class PauseOption
{
    Resume,
    Restart,
    QuitToMenu
};
#include "Constants.hpp"
#include "BasicStrategy.hpp"
#include "Deck.hpp"
#include "Hand.hpp"
#include "SideBet.hpp"
#include "Player.hpp"
#include "GameRecord.hpp"
#include "HUD.hpp"
// Forward-declared: unique_ptr<Replay> needs only an incomplete type here. Game's destructor is
// defined in Game.cpp (which includes Replay.hpp) where the type is complete.
class Replay;
class StrategyTrainer;
// like Replay / StrategyTrainer, its state machine is built in full mode only; the
// forward declaration lets Game hold it as a unique_ptr with an incomplete type (destroyed out-of-line in Game.cpp).
class StatsDashboard;

// Betting core (Feature): pure-logic betting sizing + true-count derivation, held like the other composed helpers as a
// unique_ptr with an incomplete type and destroyed out-of-line in Game.cpp. Built in both ctors (no SFML dependency),
// mirroring AchievementManager.
class BettingHandler;
// included fully (not just forward-declared) because Game.hpp exposes a public method
// that takes `Tournament::TournamentConfig`; the header therefore needs the complete type. Tournament.hpp only pulls in
// Deck/Hand/Player, which are already part of this include chain, so there is no new cycle.
#include "Tournament.hpp"
#include "GameRenderer.hpp"
#include "InputHandler.hpp"
#include "HandEvaluator.hpp"
#include "RoundDealer.hpp"
#include "SessionLifecycle.hpp"
// Follow-up #8 (settlement/aftermath): AchievementFeeder drives settlement bookkeeping off game.* members; its own header only
// pulls Achievement/GameRecord (no Game dependency)
#include "AchievementFeeder.hpp"
#include "SoundManager.hpp"
// pure-logic achievement engine. Included fully (value member, not forward-declared)
// because Game owns one by value and exposes it via accessors; Achievement.hpp pulls only <string>/<vector>/<map>
// new include cycle is introduced.
#include "Achievement.hpp"

// the per-table rules struct is pure logic and lightweight; included fully here because
// Game owns a value member and exposes it through *ForTest() accessors. It pulls in Constants only, no new cycle.
#include "GameRules.hpp"

enum class RoundState
{
    PlaceBets,
    OfferInsurance,
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
    explicit Game(bool headless); // test-only: no SFML window/assets/replay/HUD, pure game state only
    ~Game();

    // ---- Test support: headless construction + deterministic deck injection ----
    // Replace the shoe with an exact card order so tests can deal a fully determined sequence.
    void setDeck(const std::vector<Card>& cards);
    // Set up a single in-memory player with a minimum bet and begin the round (deals the initial two
    // cards plus the dealer up-card from the injected shoe), leaving roundState at PlayerTurn when the
    // dealer's up-card is not an ace. Used to exercise split/hit/stand end-to-end without SFML.
    void startTestRound();
    // Same, but seats up `seats - 1` extra in-memory NPC seats (Alice, Bob, ... from NPC_CONFIGS) behind the human.
    // Used by the analytics tests to prove the HUD readouts follow the human seat alone and ignore the NPCs.
    void startTestRound(int seats);

    const Hand& testHand(size_t playerIndex, size_t handIndex) const; // read a player hand back
    Player& testPlayer(int index = 0) { return players[index]; }      // mutate/read a player (bankroll etc.)
    std::vector<Player>& playersForTest() { return players; }          // mutate the seat vector for deterministic setup
    const Hand& testDealer() const { return dealer; }
    // read the current shoe's card count so a configurable deck count can be asserted
    // headlessly after startNewShoeForTest() rebuilds the shoe with gameRules_.numDecks.
    int deckSizeForTest() const { return static_cast<int>(deck.size()); }
    RoundState roundStateForTest() const { return roundState; }
    // Test-only: drive the round-state machine directly
    // without running a full deal/turn/settle cycle. Mirrors setGameSpeedForTest / setGameRulesForTest.
    void setRoundStateForTest(RoundState rs) { roundState = rs; }
    GameState gameStateForTest() const { return gameState; }
    size_t activePlayerIndexForTest() const { return activePlayerIndex; }
    // expose the advisor verdict set by logStrategyDeviation (the source of the Book box color).
    AdvisorFeedback advisorFeedbackForTest() const { return advisorFeedback; }
    char advisorActionForTest() const { return advisorAction; }

    // the per-shoe analytics + the shoe-recalibration counter, exposed
    // (edge readout, Edge Over Time curve, streaks, std dev, strategy %, bet histogram) actually returns to
    // baseline at a shoe boundary: and that nothing else clears them mid-shoe.
    // updateHUDStateForTest/edgePercentForTest/hudStateForTest read back what the stat boxes show.
    const std::vector<double>& edgeSeriesForTest() const { return shoe_.edgeSeries; }
    const std::vector<float>& bankrollSeriesForTest() const { return shoe_.bankrollSeries; }
    const ShoeAnalytics& shoeAnalyticsForTest() const { return shoe_; }
    int runningCountForTest() const { return runningCount; }
    int backgroundCountForTest() const { return backgroundRunningCount; }
    float hiLoTrueCountForTest() const { return hiLoTrueCount(); }
    const HUDState& hudStateForTest() const { return hudState; }
    // the round currently being built, so integration tests can assert each hand's side-bet
    // kind/wager/payout without reaching into private state.
    const RoundRecord& currentRoundRecordForTest() const { return currentRoundRecord; }
    int shoeGenerationForTest() const { return shoeGeneration; }
    // Drive the real shoe boundary (the same path the R key and auto-play advance take) from a test.
    void startNewShoeForTest() { startNewShoe(); }
    // Deal the next round at the same table without touching the shoe (what R does between rounds).
    void nextRoundForTest() { nextRound(); }
    void updateHUDStateForTest() { updateHUDState(); }
    double edgePercentForTest() const { return hudState.edgePercent; }

    // Table settings as they stand after the profile was applied at launch.
    int playerCountForTest() const { return playerCount; }
    int gameSpeedForTest() const { return static_cast<int>(gameSpeed); }
    // Test-only: select the speed (AutoPlay makes every seat act on basic strategy via npcTakeTurn).
    void setGameSpeedForTest(GameSpeed speed) { gameSpeed = speed; }
    // Refresh rate in Hz used by Gamer speed mode. Headless-safe default; detected from the monitor at full-mode startup
    // and injectable here so pure-logic tests can assert refresh-rate derivation without an SFML window.
    float refreshRateForTest() const { return refreshRateHz; }
    void setRefreshRateForTest(float hz) { refreshRateHz = hz; }
    int countingStyleForTest() const { return static_cast<int>(countingStyle); }
    // read the live ruleset, and in tests override it before startTestRound() so logic can
    // be driven headlessly with any rule set (soft-17, DAS, resplit, deck count, surrender) without touching disk.
    const GameRules& getGameRulesForTest() const { return gameRules_; }
    void setGameRulesForTest(const GameRules& rules) { gameRules_ = rules; }
    // Main-menu highlighted row: read/select it
    // InputHandler::navigateMenu) without an SFML window or event loop. menuSelection is otherwise private on Game.
    MenuOption menuSelectionForTest() const { return menuSelection; }
    void setMenuSelectionForTest(MenuOption option) { menuSelection = option; }
    // AppMode is otherwise private on Game. Read it
    // and assert the Escape -> main-menu path switched modes (see InputHandler::returnToMainMenu) without an event loop.
    AppMode getAppModeForTest() const { return appMode; }
    void setAppModeForTest(AppMode mode) { appMode = mode; }

    // B6 headless seam for the rules-panel keyboard edit edge detection: inject held-key state so update() can be driven with a
    // key logically down (held for N frames) without real SFML hardware keys. When override is off (default) update() reads
    // isKeyPressed() unchanged, so gameplay behaviour is identical. resetRulesKeyOverrideForTest() restores that default.
    void setRulesKeyHeldForTest(bool left, bool right);
    void resetRulesKeyOverrideForTest();

    // Tournament hooks (see startTournamentSession): whether the shared play path is running under tournament rules, plus the
    // current stage / completion for tests that drive tournament play headlessly through Game's delegating methods.
    bool tournamentActiveForTest() const { return tournamentActive_; }
    void setTournamentActiveForTest(bool active) { tournamentActive_ = active; }
    int currentTournamentStageForTest() const { return tournament ? tournament->currentStage() : 0; }
    bool isTournamentCompleteForTest() const { return tournament && tournament->isComplete(); }
    // The StrategyTrainer is otherwise private (built only in the full constructor). Exposed out-of-line
    // assert the StrategyTrainer AppMode routes to trainer->update() and advances session time (see Game::trainerElapsedSecondsForTest).
    float trainerElapsedSecondsForTest() const;

    // expose SoundManager state so headless tests can assert the event->queue path without SFML.
    size_t soundPendingForTest() const { return sound_.pendingSounds().size(); }
    const std::vector<std::string>& soundQueueForTest() const { return sound_.pendingSounds(); }
    int soundTotalPlayedForTest() const { return sound_.totalSoundsPlayed(); }
    float soundVolumeForTest() const { return sound_.settings().soundVolume; }
    float musicVolumeForTest() const { return sound_.settings().musicVolume; }
    bool soundMutedForTest() const { return sound_.isMuted(); }
    void setSoundVolumeForTest(float v) { sound_.setSoundVolume(v); }
    void setMusicVolumeForTest(float v) { sound_.setMusicVolume(v); }
    void setMutedForTest(bool m) { sound_.setMuted(m); }

    // ---------------- Player Actions (public

    void hit();
    void stand();
    bool doubleDown();
    bool split();
    bool surrender();

    // place an optional side bet on the human seat's main hand before the deal. The payout is
    // resolved later, inside deal() via resolveSideBets(): BEFORE any main-hand settlement
    // choice (guarded by bankroll). Public + headless-safe: tests drive a full betting->deal cycle with it; the UI calls
    // it from PlaceBets. A zero wager opts out regardless of kind.
    void placeSideBet(SideBetKind kind, int wager);

    // Whether the human seat can currently place bets interactively: round in PlaceBets, active seat is the human
    // (index 0) and betting isn't auto-advanced by AutoPlay/Hyper/Gamer. Mirrors InputHandler's input gate
    // hints use one shared predicate; returns false outside PlaceBets. Used to decide whether to draw the side-bet control line.
    bool canPlaceBets() const;

    // Minimum bet the human seat must reach before confirming: a tournament's escalating stage minimum when the shared play
    // path is running under tournament rules, otherwise the ordinary table MINIMUM_BET. Consulted by betting so both the UI
    // and auto-play honour the same floor without repeating it.
    int activeMinBet() const;

    void run();

    // B7 — single persist-on-exit choke point. Every route that leaves play back to the main menu funnels through it so a
    // session's bankroll / shoe record / current table settings / unlocked achievements are stamped and saved exactly once,
    // instead of only on the window-close path (before B7 an Escape exit dropped everything). Pure Game state: no SFML
    // window required, which is what makes it headless-testable. Called from InputHandler::returnToMainMenu().
    void persistOnExit();

    // Click the count-style label ("Count (Hi-Lo)") to toggle between counting systems. Both are always
    // counted in the background; this swaps which buffer drives the active count and flips the style.
    void toggleCountingStyle();
    // begin/restart a training session from the main menu. Guards against nullptr
    // so headless construction (which skips building the trainer) never crashes.
    void startTrainerSession(int deckCount = 6, int secondsPerSession = 0);
    // begin/restart a tournament from the main menu. Guards against nullptr so headless
    // construction (which never builds the tournament) can't crash when the menu calls this; takes config or defaults.
    void startTournamentSession(const Tournament::TournamentConfig& config = {});
    // begin/restart the statistics dashboard from the main menu. Guards against nullptr so
    // headless construction (which never builds the dashboard) can't crash when the menu calls this; on first entry it seeds
    // the dashboard from the player's career stats + flattened round history.
    void startStatsSession();
    // Name-entry screen (U7): editing state + commit/gate. m_nameBuffer_ holds the text currently being typed; append/backspace
    // mutate it with a hard max-length clamp so both headless tests and InputHandler share one source of truth, and GameRenderer
    // reads nameEntryText() to draw it. needsNameEntry() gates the automatic "enter your name before the main menu" prompt on
    // whether the profile has ever stored a name; commitPlayerName() writes that name into seat 0's display + persisted profile.
    void nameEntryAppendChar(char c);
    void nameEntryBackspace();
    const std::string& nameEntryText() const;
    bool needsNameEntry();
    void commitPlayerName(const std::string& name);
    // headless count of rounds fed to the dashboard through the real startStatsSession ->
    // feed path (mirrors the *ForTest() convention). Exposed as a plain int
    // exactly once without needing the incomplete StatsDashboard type in Game.hpp. nullptr-guarded for headless builds.
    int statsPerRoundCountForTest() const;
    float deltaTime;

    friend class GameRenderer;
    friend class InputHandler;
    friend class HandEvaluator;
    friend class RoundDealer;
    // Follow-up #8 (settlement/aftermath): AchievementFeeder drives settlement bookkeeping off game.* members.
    friend class AchievementFeeder;
    friend class SessionLifecycle;
    friend class RoundEngine;

  private:
    AppMode appMode = AppMode::MainMenu;
    // Text currently being typed on the NameEntry screen (U7). Appended/backspaced by InputHandler via nameEntryAppendChar/
    // nameEntryBackspace and read by GameRenderer via nameEntryText(); clamped to Constants::MAX_PLAYER_NAME_LENGTH.
    std::string m_nameBuffer_;
    std::unique_ptr<Replay> replay;
    // its own AppMode state machine. Constructed in the full mode only; headless
    // tests drive the trainer directly. unique_ptr with an incomplete type is fine here (like Replay) because ~Game
    // is defined out-of-line in Game.cpp, where StrategyTrainer.hpp is included.
    std::unique_ptr<StrategyTrainer> trainer;
    // its own AppMode state machine, built in full mode only. unique_ptr with an incomplete
    // type is fine here (like Replay/StrategyTrainer) because ~Game is defined out-of-line where Tournament.hpp loads.
    std::unique_ptr<Tournament> tournament;
    // its own AppMode state machine, built in full mode only; headless tests drive the
    // dashboard directly (see test_stats_dashboard.cpp). unique_ptr with an incomplete type is fine here (like
    // Replay/StrategyTrainer) because ~Game is defined out-of-line where StatsDashboard.hpp loads.
    std::unique_ptr<StatsDashboard> statsDashboard;
    // Betting core (Feature): the money-in math (true-count derivation, Kelly/Reko optimal-bet sizing). Pure logic with
    // no SFML dependency, so Game owns it as a unique_ptr and constructs it in both ctors: like AchievementManager.
    std::unique_ptr<BettingHandler> betting_;
    GameRenderer renderer;
    InputHandler inputHandler;
    HandEvaluator evaluator;
    // Follow-up #8 (Settlement/deal()): deals the round's cards + resolves side-bet settlement, composed like the other helpers.
    RoundDealer roundDealer_;
    // Follow-up #8 (Session lifecycle): session entry points + table-settings persistence, composed like the other helpers.
    SessionLifecycle sessionLifecycle_;
    // Follow-up #8 (settlement/aftermath): feeds a settled round's human-seat outcome into the achievement engine and surfaces
    // any just-unlocked popup; composed like the other helpers.
    AchievementFeeder achievementFeeder_;
    // ---------------- SFML ----------------

    sf::RenderWindow window;

    sf::View gameView;

    sf::Texture backgroundTexture;
    std::unique_ptr<sf::Sprite> backgroundSprite;

    sf::Texture cardTexture;
    sf::Font font;

    // ---------------- UI ----------------

    HUD hud;
    HUDState hudState;

    // Sound & music. Default-constructed in every mode; buffers are only decoded by loadAssets(), which
    // the headless constructor skips
    // when assets aren't loaded, so Game event hooks can call it unconditionally from both full and headless paths.
    SoundManager sound_;

    MenuOption menuSelection = MenuOption::PlayGame;
    PauseOption pauseSelection = PauseOption::Resume;
    GameSpeed gameSpeed = GameSpeed::Normal;
    // Refresh rate in Hz for Gamer speed mode. Headless-safe default; detected from the monitor at full-mode startup and
    // injectable via setRefreshRateForTest
    float refreshRateHz = GameTiming::DEFAULT_REFRESH_HZ;
    BasicStrategy::CountingStyle countingStyle = BasicStrategy::CountingStyle::REKO;
    // the live table rules. Default-constructed values mirror the compile-time Constants
    // exactly, so headless tests that never set rules behave identically to before. Restored from the profile in
    // applySavedTableSettings() and written back via rememberTableSettings(). The in-game rules-config panel edits this
    // struct directly (see update()); `gameRulesSelectedRow` is the panel's highlighted row while editing it.
    GameRules gameRules_;
    int gameRulesSelectedRow = 0;

    // B6 rules-panel keyboard edit edge detection. rulesRightPrev_ / rulesLeftPrev_ hold the previous frame's held state so
    // applyGameRuleEdit fires only on a false->true press (one step per tap, no repeat-storm while a key is held). The m_rules*
    // members are a headless test seam: when override is off (default) update() reads real SFML hardware keys via isKeyPressed();
    // when on, it reads the injected values instead so held-key frames can be simulated without a window.
    bool rulesRightPrev_ = false;   // last frame's Right-held state (B6 edge detection)
    bool rulesLeftPrev_  = false;   // last frame's Left-held state (B6 edge detection)
    bool m_rulesKeyOverride_ = false;
    bool m_rulesRightHeld_ = false;
    bool m_rulesLeftHeld_  = false;

    // NPC action timing
    float npcActionTimer = 0.0f;
    float npcActionDelay = 1.0f;

    float dealerStartDelayTimer = 0.0f;
    bool dealerTurnStarted = false;

    // When true, the shared play path runs under tournament rules (staged 4-deck shoes, escalating stage minimum bet, the AI
    // opponents' modified basic strategy); consulted at each betting/NPC/shoe-boundary seam in Game.cpp so ordinary play is
    // byte-identical when false. The Tournament controller holds all the rule state; Game owns it as a unique_ptr like Replay.
    bool tournamentActive_ = false;

    int playerCount = GameConfig::DEFAULT_PLAYER_COUNT;

    void setPlayerCount(int count);

    // ---------------- Blackjack ----------------

    Deck deck;

    Hand dealer;

    std::vector<Player> players;
    int activePlayerIndex = 0;

    RoundState roundState = RoundState::PlayerTurn;

    GameState gameState = GameState::Started;

    int initialBet = 0;
    int runningCount = 0;
    // how many human-seat plays in the CURRENT round deviated from basic strategy. Read at
    // settlement to feed strategy-adherence achievements, then reset each round by nextRound(). Kept per-round rather than on
    // shoe_ because Textbook Player needs a continuous career streak while Deviation Free only needs "did this round match".
    int deviationsThisRound_ = 0;

    // the human seat's pending side-bet choice, consumed once by deal() when it applies + resolves
    // every placed side bet. Mirrors how insurance is chosen then confirmed: this records intent until the round deals.
    SideBetKind sideBetIntentKind_ = SideBetKind::None;
    int sideBetIntentWager_ = 0;
    // Other counting system's running count, kept current every draw
    int backgroundRunningCount = 0;
    float trueCount = 0;

    RoundRecord currentRoundRecord;
    ShoeRecord currentShoeRecord;

    // Per-hand action tracking (parallel to player.hands)
    std::vector<std::vector<std::string>> handActions;

    // ---------------- Stats Tracking ----------------

    int shoeCumulativeWinLoss = 0;    // running win/loss for current shoe
    int sessionCumulativeWinLoss = 0; // running win/loss for entire session

    PlayerStats& getStats()
    {
        return players[0].getStats();
    }

    // Basic Strategy advisor feedback shown on the Book box after each completed player action.
    AdvisorFeedback advisorFeedback = AdvisorFeedback::None;
    char advisorAction = 0; // book's advice letter for the just-completed hand (the green/red target)

    // Per global-hand-index deviation records: source of truth during a round, mirroring handActions' indexing.
    // Popped into by logStrategyDeviation
    std::vector<std::vector<DeviationRecord>> handDeviations_;

    // Everything the HUD stat boxes + left-panel graphs report for the CURRENT shoe only: std-dev accumulators,
    // the hand-to-hand and round-to-round streaks, strategy accuracy, the bet distribution and both graph series.
    // resetShoeAnalytics() zeroes the whole struct
    ShoeAnalytics shoe_;

    // Bumped every time a new shoe begins (new game, R at end of shoe, auto-play, mid-shoe reshuffle). Exposed to
    // the HUD via HUDState::currentShoeId as the per-shoe recalibration signal. Every boundary that bumps it also
    // zeroes shoe_ and re-seeds both running counts (see resetShoeAnalytics).
    int shoeGeneration = 0;

  public:
    // owns the pure-logic achievement engine. Constructed as a plain value in both the
    // full and headless ctors because it has no SFML dependency and must be checkable through *ForTest() after driving a
    // real game: unlike trainer/tournament
    // via recordHandAchievements()/onNewShoe(); the unlocked set is loaded from the profile at launch and stamped back on save.
    AchievementManager achievementManager_;

    // read/write accessors so tests (headless) can drive the manager directly and inspect the unlocked set; the
    // UI reads achievementListForTest() to render the list view.
    AchievementManager& achievementsForTest() { return achievementManager_; }
    const std::vector<Achievement>& achievementListForTest() const { return achievementManager_.all(); }

    // feed one settled round's human-seat outcome into the achievement engine (called by HandEvaluator after
    // settlement) and set a HUDState popup for any just-unlocked achievement. Resolving shoe-end achievements happens in
    // resetShoeAnalytics() via onNewShoe().
    void recordHandAchievements(const RoundRecord& round);

    // ---------------- Setup ----------------

    void startNewGame();

    // Table settings persistence. applySavedTableSettings() seeds playerCount / gameSpeed / countingStyle
    // from the profile loaded at launch
    // rememberTableSettings() writes the current choices back into the profile; it runs at every point the
    // profile is already being saved
    void applySavedTableSettings();
    void rememberTableSettings();

    // Shoe boundaries. resetShoeAnalytics() is the ONE place a shoe boundary is handled: it bumps the recalibration
    // signal, clears the per-shoe analytics curves (so the edge readout + Edge Over Time chart return to baseline)
    // and re-seeds the running count for the new shoe. startNewShoe() layers a freshly created + shuffled shoe and
    // a new shoe record on top of it. Every path that begins a shoe (new game, R at end of shoe, auto-play, the
    // mid-shoe reshuffle) goes through these, so no boundary can silently skip the reset.
    void resetShoeAnalytics();
    void startNewShoe();

    void loadAssets();

    void nextRound();

    // Tournament-only shoe boundary: advance to the next stage (reshuffling a fresh four-deck shoe and re-opening betting at
    // the higher stage minimum) when another remains, or mark the tournament complete once the final stage has run out. Called
    // from dealerTurn() after the last hand of a stage's shoe while tournamentActive_. Private; Game.cpp owns the logic.
    void finishTournamentShoe();

    void deal();

    // ---------------- Game Loop ----------------

    void update(float deltaTime);

    // Advance the round/turn state machine one frame regardless of AppMode — update()'s Playing path minus the guard, so tests
    // can drive PlayerTurn -> DealerTurn -> Cleanup directly (no window or input) through RoundEngine::tick().
    void advanceTurnState(float deltaTime);
    void updateHUDState();

    // Per-concern HUD sync helpers; updateHUDState composes them
    // counting, sound, advisor/turn, shoe analytics) is one self-contained block. Pure refactor: every hudState field's
    // value and type are unchanged; see test_hud_layout.cpp which locks the values through hudStateForTest().
    void syncBettingAndSideBet();

    void syncPlayerBankrolls();

    void syncCounting();

    void syncSoundState();

    void syncBookAdvisor();

    void syncShoeAnalytics();

    // Shared HUD timer countdown for summary / after-hand / after-shoe timers.
    void tickTimer(float& timerField, float deltaTime);
    // Advance + wrap activePlayerIndex (and evaluate Blackjack) at insurance-loop boundaries.
    void advanceToNextInsurancePlayer();

    // Removed: void draw(); - now in GameRenderer
    // ---------------- Betting ----------------

  public:
    // Place the human's current hand bet and record it for the session bet-distribution histogram. Public (with
    // activeTrueCount() below) so tests can drive a full betting round without SFML.
    void confirmBet();

    // Accept or decline insurance on the active seat's current hand. These are public (rather than private like their
    // InputHandler-facing callers assume)
    // headlessly -- most importantly the dealer-natural regression that fires after a player declines insurance against
    // a natural dealer (see test_dealer_blackjack.cpp).
    void confirmInsurance();
    void declineInsurance();

    // Kelly-based optimal bet calculation (uses kellyFraction)
    int calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const;

    // The true count the betting logic should act on for the active counting system: Reko uses the raw running
    // count ("the true count is the running count"), Hi-Lo normalizes by decks remaining. Drives the human's
    // auto-bet paths (manual Left-arrow bet and AutoPlay)
    float activeTrueCount() const;

    // The Hi-Lo true count regardless of which style is on screen: `runningCount` holds the active system's count
    // and `backgroundRunningCount` the other one (see afterDraw). The edge estimate is calibrated against Hi-Lo
    // true counts, so the Edge % box and the Edge Over Time chart read from this rather than from the displayed
    // count - feeding Reko's raw (unbalanced, offset) count into that calibration would put the readout wildly
    // out of range on the first round of every shoe.
    float hiLoTrueCount() const;

  private:
    // evaluate + bankroll every seat's side bet once all cards are known (called from deal()),
    // BEFORE any main-hand settlement via evaluateHands(). Side bets pay out / lose independently of the main hand.
    void resolveSideBets();

    // ---------------- Dealer ----------------

    // Central helper: update running count and check cut card after each draw
    void afterDraw(Card card);

    // Collect all cards currently on the table (for reshuffle fallback)
    std::vector<Card> collectTableCards() const;

    // Handle rare case where deck runs out before cut card
    void handleEmptyDeck();

    void dealCardToPlayer(Hand& hand);
    void dealCardToDealer();

    void dealerTurn();

    void offerInsurance();

    char giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand);
    void logStrategyDeviation(char actionTaken);

    // NPC decision making
    void npcTakeTurn();

    // Compute global hand index for handActions (flat list across all players)
    size_t getGlobalHandIndexForPlayer(size_t playerIdx, size_t handIdx);

    // Accessors for the seat whose turn it is, so action methods don't re-index every time.
    Player& activePlayer() { return players[activePlayerIndex]; }
    Hand& activeHand() { return activePlayer().hands[activePlayer().currentHand]; }
};
