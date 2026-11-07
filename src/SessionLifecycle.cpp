#include "SessionLifecycle.hpp"

#include <algorithm>
#include "Game.hpp"            // complete Game type: all moved methods read/write its members (friend).
#include "Logger.hpp"          // Logger::info used by startNewGame() and applySavedTableSettings().
#include "StatsDashboard.hpp"   // make_unique<StatsDashboard>() + feed(): forward-declared in Game.hpp, complete here.
#include "StrategyTrainer.hpp"  // make_unique<StrategyTrainer>() + startSession(): incomplete type in Game.hpp, complete here.
#include "Tournament.hpp"       // make_unique<Tournament>() + start(config): incomplete type in Game.hpp, complete here.

SessionLifecycle::SessionLifecycle(Game& game) : game(game) {}

// begin/restart a training session. Delegates the exact nullptr-guarded build from before the
// split; all trainer state stays in Game's `trainer` member
void SessionLifecycle::startTrainerSession(int deckCount, int secondsPerSession)
{
    if (!game.trainer)
        game.trainer = std::make_unique<StrategyTrainer>();
    game.trainer->startSession(deckCount, secondsPerSession);
}

// begin/restart a tournament. Builds the table like an ordinary game (human first, then NPC opponents up to playerCount), but
// gives every seat equal starting chips so the run is decided purely by who ends with the most across the stages. It then runs
// under tournament rules: each stage is its own four-deck shoe, the minimum bet escalates per stage, and the AI opponents play
// the modified strategy -- all handled in Game's shared play path via tournamentActive_. Guards against nullptr so headless
// construction can't crash when the menu calls this.
void SessionLifecycle::startTournamentSession(const Tournament::TournamentConfig& config)
{
    if (!game.tournament)
        game.tournament = std::make_unique<Tournament>();
    game.tournament->start(config);

    // Build the seats exactly like startNewGame: the human at index 0, then up to playerCount - 1 NPC opponents. Every seat
    // starts with equal chips so a tournament is decided purely by who finishes richest -- not by how many chips you were dealt.
    game.players.clear();
    // Load the REAL career profile into seat 0 (loading constructor), then override only the in-memory chips. This preserves the
    // player's progress while a tournament is a competition, not career play -- contrast startNewGame(), which also loads the real
    // profile rather than fabricating an ephemeral one. markPersistable(false) stops every saveProfile() during the run from writing
    // these throwaway chips/stats over the on-disk career file (the B1 bug).
    game.players.push_back(Player("William"));
    game.players[0].setPersistable(false);
    game.players[0].setBankroll(config.startingChips);

    const int opponentCount = std::max(0, game.playerCount - 1);
    for (int i = 0; i < opponentCount && i < GameConfig::NPC_COUNT; ++i)
    {
        const auto& cfg = GameConfig::NPC_CONFIGS[i];
        game.players.push_back(Player(cfg.name, config.startingChips));
        game.players.back().setBettingStrategy(static_cast<BettingStrategy>(cfg.strategy), cfg.baseBet);
    }
    // At least one opponent so there is someone to beat.
    if (game.players.size() < 2)
    {
        game.players.push_back(Player(GameConfig::NPC_CONFIGS[0].name, config.startingChips));
        game.players.back().setBettingStrategy(BettingStrategy::Flat, 10);
    }

    // A tournament's stages are self-contained four-deck shoes regardless of the table's normal deck count. Mirror startNewGame:
    // a fresh + shuffled shoe, per-shoe analytics reset (bankrolls persist), and enter at PlaceBets so the human places a bet to deal.
    game.deck.create(game.tournament->decksPerShoe());
    game.deck.shuffle();
    game.currentShoeRecord.initialDeck = game.deck.getCards();
    game.currentShoeRecord.rounds.clear();

    game.resetShoeAnalytics();
    game.sessionCumulativeWinLoss = 0;
    game.handActions.clear();
    game.handActions.resize(game.players.size());
    game.handDeviations_.clear();
    game.handDeviations_.resize(game.players.size());
    game.advisorFeedback = AdvisorFeedback::None;
    game.advisorAction = 0;
    game.dealer.clear();
    for (auto& p : game.players)
        p.resetRound(config.minBetStage1);
    game.initialBet = config.minBetStage1;
    game.gameState = GameState::Started;
    game.roundState = RoundState::PlaceBets;
    game.activePlayerIndex = 0;

    game.tournamentActive_ = true;

    Logger::info() << "Tournament started: " << game.tournament->stages() << " stages, "
                   << game.tournament->decksPerShoe() << "-deck shoes, stage min bet " << config.minBetStage1 << ".";
    game.appMode = AppMode::Tournament;
}

// begin/restart the dashboard from the main menu. Delegates verbatim: flattening completed-shoe
// history plus any current-shoe rounds into one round-history list and feeding it with the player's career stats, double-feeding
// guards intact. All state (statsDashboard, players[], currentShoeRecord) stays in Game; behavior is unchanged.
void SessionLifecycle::startStatsSession()
{
    if (!game.statsDashboard)
        game.statsDashboard = std::make_unique<StatsDashboard>();

    // Flatten completed-shoe history from the profile plus any rounds played in the current (in-progress) shoe, into one
    // round-history list: exactly what the dashboard feeds on. Reading through getProfile().getLongHistory() keeps this
    // additive to existing persistence; nothing is rewritten here. getProfile() returns a copy by value
    // copy for the whole function: binding const& longHistory to it (rather than to getProfile()'s temporary) keeps the
    // history reference valid and avoids a stack-use-after-scope once startStatsSession runs against a populated history.
    PlayerProfile profile = game.players[0].getProfile();

    std::vector<RoundRecord> rounds;
    const auto& longHistory = profile.getLongHistory();
    for (const auto& shoe : longHistory)
        for (const auto& round : shoe.rounds)
            rounds.push_back(round);
    // A round settles into BOTH profile.currentHistory (addRoundRecord) and game.currentShoeRecord.rounds. On a later
    // QuitToMenu/Restart, addShoeRecord() moves currentHistory -> longHistory but leaves the live mirror in
    // currentShoeRecord.rounds in place
    // (once from longHistory, once from the stale current record). Skip any current-record round whose content already
    // appears in history: dealerCards + playerHands fully fingerprint a round's consumed outcome
    // exactly once regardless of how many times it was stored. RoundRecord has no operator==
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
    for (const auto& round : game.currentShoeRecord.rounds)
        if (!roundOverlap(round))
            rounds.push_back(round);

    game.statsDashboard->feed(game.players[0].getStats(), rounds);
}

// begin/restart a whole session. Delegates verbatim: fresh seats + shuffled shoe, per-shoe readouts reset to zero (the
// bankroll is the one value that persists), counting/strategies cleared, and enter Playing. All state stays in Game.
void SessionLifecycle::startNewGame()
{
    Logger::info("Starting new game");

    // Remember what was just chosen before the profile is re-read below
    game.rememberTableSettings();

    game.players.clear();

    // Human player (William) - always first
    game.players.push_back(Player("William"));

    for (int i = 0; i < GameConfig::NPC_COUNT && i + 1 < game.playerCount; i++)
    {
        const auto& cfg = GameConfig::NPC_CONFIGS[i];
        // Fresh in-memory NPC: no disk read, no JSON parse, no persistence. The human player above loads the real profile.
        game.players.push_back(Player(cfg.name, cfg.bankroll));
        game.players[i + 1].setBettingStrategy(static_cast<BettingStrategy>(cfg.strategy), cfg.baseBet);
        Logger::info() << game.players[i + 1].getName() << " starts with $" << cfg.bankroll
                       << " using " << cfg.strategy << " strategy";
    }

    auto profile = game.players[0].getProfile();

    game.deck.create(game.gameRules_.numDecks);
    game.deck.shuffle();
    game.currentShoeRecord.initialDeck = game.deck.getCards();
    game.currentShoeRecord.rounds.clear();

    // A brand-new shoe: every per-shoe readout starts from zero
    // reflect only this shoe. (The bankroll itself is deliberately kept: it is the one value that persists.)
    game.resetShoeAnalytics();
    game.sessionCumulativeWinLoss = 0;
    game.handActions.clear();
    game.handActions.resize(1);
    game.handDeviations_.clear();
    game.handDeviations_.resize(1);
    game.advisorFeedback = AdvisorFeedback::None;
    game.advisorAction = 0;
    game.dealer.clear();
    game.players[0].resetRound(0);
    game.nextRound();

    game.appMode = AppMode::Playing;
}

// Feature: restore saved table settings (player count / speed / counting style + custom rules) from the human seat's profile onto
// live state. The profile's fields already carry defaults for legacy files
// changed a rule and it was persisted additively. Delegates verbatim; only reads/writes Game state.
void SessionLifecycle::applySavedTableSettings()
{
    if (game.players.empty())
        return;

    const PlayerProfile profile = game.players[0].getProfile();

    // setPlayerCount clamps to [1,6]
    game.setPlayerCount(profile.preferredPlayerCount);

    // An int read off disk can be anything
    switch (static_cast<GameSpeed>(profile.preferredSpeed))
    {
    case GameSpeed::Fast:
    case GameSpeed::Normal:
    case GameSpeed::Slow:
    case GameSpeed::AutoPlay:
    case GameSpeed::Hyper:
    case GameSpeed::Gamer:
        game.gameSpeed = static_cast<GameSpeed>(profile.preferredSpeed);
        break;
    default:
        game.gameSpeed = GameSpeed::Normal;
        break;
    }

    switch (static_cast<BasicStrategy::CountingStyle>(profile.preferredCountingStyle))
    {
    case BasicStrategy::CountingStyle::HI_LO:
    case BasicStrategy::CountingStyle::REKO:
        game.countingStyle = static_cast<BasicStrategy::CountingStyle>(profile.preferredCountingStyle);
        break;
    default:
        game.countingStyle = BasicStrategy::CountingStyle::REKO;
        break;
    }

    // restore the saved table rules. The profile's GameRules already carries defaults for
    // legacy files, so this is a clean no-op unless the player
    game.gameRules_ = profile.gameRules;

    Logger::info() << "Restored table settings: " << game.playerCount << " players, "
                   << BasicStrategy::countingStyleName(game.countingStyle) << " counting.";
}

// Feature: stamp the current table choices (player count / speed / counting style + sound prefs + custom rules) back into the human
// seat's profile so they survive the session. Called at every existing save point, so no extra I/O. Written as SFML-unit ints to
// mirror preferredSpeed; additive schema unchanged for profiles saved before custom rules. Delegates verbatim.
void SessionLifecycle::rememberTableSettings()
{
    if (game.players.empty())
        return;

    PlayerProfile profile = game.players[0].getProfile();
    profile.preferredPlayerCount = game.playerCount;
    profile.preferredSpeed = static_cast<int>(game.gameSpeed);
    profile.preferredCountingStyle = static_cast<int>(game.countingStyle);
    // persist sound + music prefs alongside the other table settings (called at every existing save point,
    // so no extra I/O). Written as SFML-unit ints to mirror preferredSpeed.
    profile.preferredSoundVolume = static_cast<int>(game.sound_.settings().soundVolume);
    profile.preferredMusicVolume = static_cast<int>(game.sound_.settings().musicVolume);
    profile.preferredMuted = game.sound_.isMuted();
    // stamp the live table rules onto the profile so they survive the session, written on
    // every existing save point (no extra I/O). Additive schema: unchanged for profiles saved before custom rules.
    profile.gameRules = game.gameRules_;
    game.players[0].updateProfile(profile);
    game.players[0].saveProfile();
}
