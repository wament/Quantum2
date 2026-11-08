#pragma once
#include "GameRecord.hpp"
#include "SideBet.hpp"
#include <string>
#include <vector>

struct HUDState
{
    // Bottom Bar
    int bankroll = 0;
    int currentBet = 0;
    std::vector<int> playerBankrolls;

    int streak = 0;
    bool streakIsWin = false;

    int runningCount = 0;
    // True count of the HI_LO system regardless of which style is currently active (see Game::updateHUDState).
    float trueCount = 0.f;

    // Yellow when Reko is active and we surface the Hi-Lo true count as a cross-check; white otherwise.
    bool trueCountHighlighted = false;
 
    float kellyFraction = 0.33f;
    bool riskSliderDragging = false;

    // The per-shoe analytics snapshot (Game::shoe_): backs the Std Dev / Long Win / Long Loss / Edge % / Strategy
    // boxes and the Bets Placed histogram. Game zeroes it at every shoe boundary
    // Bankroll restarts with the new shoe.
    const ShoeAnalytics* shoeAnalytics = nullptr;

    // Right Panel
    const ShoeRecord* currentShoe = nullptr;

    // Bumped by the game whenever a new shoe begins (fresh deal or a deck reshuffle). The HUD uses it as the
    // graph-recalibration signal: when it changes every accumulated chart series is cleared
    // this shoe's data instead of drifting persistently upward across shoes/sessions.
    int currentShoeId = 0;

    // Internal
    int cardsRemaining = 0;
    int decksRemaining = 0;

    // Basic Strategy Advisor
    std::string basicStrategyHint;
    // Verdict on the Book box after a completed action: Correct -> green, Deviated -> red. None while advising live.
    AdvisorFeedback advisorFeedback = AdvisorFeedback::None;

    // Advanced Analytics dashboard values, all derived from ShoeAnalytics and therefore all per-shoe:
    // they return to their baseline the moment a new shoe starts.
    double analyticsStdDev = 0.0;   // standard deviation of this shoe's per-hand net profits
    int longestWinningStreak = 0;   // longest winning run this shoe
    int longestLosingStreak = 0;    // longest losing run this shoe
    double edgePercent = 0.0;       // live estimated player edge (%), read from the current count
    std::vector<float> bankrollSeries; // your cumulative net profit per round this shoe (bankroll curve)
    std::vector<double> edgeSeries;    // estimated edge sampled once per round of the current shoe (a level)

    // Human-readable name of the active counting style (e.g. "Hi-Lo"/"Reko"), used to title the Count box.
    std::string countingStyleName;
    // Title for the true-count box, recomputed each frame from Game::trueCountStatusLabel() so it stays correct as the active
    // system flips (Hi-Lo -> "True Count", Reko -> "Betting Count (Reko)"). HUD.cpp applies it via StatBox::setTitle().
    std::string countingBoxLabel;

    // True while it's the dealer's turn. The book advisor hides its action during this window.
    bool isDealerTurn = false;

    // Sound & music status, populated from Game::sound_ in updateHUDState()
    // a single data source of truth to read. Volumes are in SFML's 0..100 units, matching the profile.
    float soundVolume = 100.f;
    float musicVolume = 60.f;
    bool soundMuted = false;

    // Side bets: the human seat's current-round side bet, read off the live hand in Game::updateHUDState so
    // the HUD and betting-area renderer share one source of truth. Defaults mean "no side bet this round".
    bool hasSideBet = false;
    SideBetKind sideBetKind = SideBetKind::None;
    int sideBetWager = 0;
    int sideBetPayout = 0; // gross returned on a win (stake + winnings), 0 on a loss

    // End-of-round summary overlay
    std::vector<std::string> playerSummaries;
    float summaryTimer = 0.0f;

    // name of the most recently unlocked achievement, set by Game when an unlock fires so
    // HUDState/GameRenderer can surface a brief popup. Empty means "no notification pending"; cleared once its
    // lifetime (achievementToastTimer) elapses via RoundEngine::tick — never left on screen forever.
    std::string pendingAchievementName;
    float achievementToastTimer = 0.0f;   // seconds remaining for the popup; 0 means no toast is armed

    // Auto-play timers (0 means no wait)
    float afterHandTimer = 0.0f;
    float afterShoeTimer = 0.0f;
};