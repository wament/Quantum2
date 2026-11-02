#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

// GameRecord.hpp is pure logic (no SFML anywhere) and defines PlayerStats / RoundRecord
// the ComputedStats/feed() signatures to see those types. Kept first because this header stays headless-safe.
#include "GameRecord.hpp"

// GameRecord.hpp is pure logic (no SFML anywhere)
// runs over the Graphics-only test binary, exactly like Tournament / StrategyTrainer. The dashboard reads existing career
// stats (PlayerStats) + round history (RoundRecord) and derives analytics; presentation lives in draw() only (no game
// state mutation), so it is safe to call from GameRenderer while another thread owns nothing here.
class StatsDashboard
{
  public:
    // The three views of the dashboard. Overview = career totals, Hands = per-round result log, Profits = charts.
    enum class Tab
    {
        Overview,
        Hands,
        Profits
    };

    // Everything the dashboard computes from career stats + round history. Pure data
    // the *ForTest() accessors before any UI is written (logic-first).
    struct ComputedStats
    {
        int handsFinished = 0;   // number of individual hands played, summed across every round
        int wins = 0;            // decisive wins (won and not surrendered)
        int losses = 0;          // decisive losses (not won and not surrendered)
        int pushes = 0;          // surrendered hands, treated here as a neutral outcome
        int blackjacks = 0;      // naturals (two-card 21)
        int busts = 0;           // hands that went over 21

        double winRate = 0.0;         // wins / decisive (decisive = wins + losses); 0 when nothing was decisive
        double bjFrequency = 0.0;     // blackjacks / handsFinished
        double bustFrequency = 0.0;   // busts / handsFinished
        double strategyAccuracy = 0.0;// career strategyHits / strategyTotal (0 when no data)

        int shoesFinished = 0;
        int splits = 0;
        int doubles = 0;

        std::vector<int> perRoundProfit;    // net profit per round -> line/area chart data
        std::vector<long long> bankrollSeries; // cumulative bankroll progression across rounds -> curve chart
        int peakBankroll = 0;               // highest cumulative value reached
        int maxDrawdown = 0;                // worst peak-to-current decline (magnitude, >= 0)

        int greatestHandProfit = 0; // best single-hand net profit vs its stake
        int worstHandLoss = 0;      // worst single-hand net loss vs its stake (<= 0)
    };

    StatsDashboard() = default;

    // Feed the data source of truth: career stats + a flat list of round history (flattened from PlayerProfile's shoe
    // records). Pure logic: drives every *ForTest() accessor, no SFML. Called when entering the mode in full gameplay.
    void feed(const PlayerStats& career, const std::vector<RoundRecord>& rounds);

    const ComputedStats& computed() const { return computed_; }

    // Exporters (nlohmann/json is header-only and has no SFML dependency
    // toJson mirrors the codebase's Serialization.hpp approach (vendored nlohmann), toCsv is a flat analytics dump.
    std::string toJson() const;
    std::string toCsv() const;

    // Frame update for AppMode parity (mirrors Tournament/StrategyTrainer wiring). Pure-logic tests drive feed()+computed()
    // directly and never rely on this timer
    // InputHandler (advanceTab), not by update().
    void update(float deltaTime);

    // --- Test accessors (mirror the *ForTest() convention) ---
    Tab currentTabForTest() const { return tab_; }
    void nextTabForTest() { advanceTab(true); }
    void prevTabForTest() { advanceTab(false); }
    int handsFinishedForTest() const { return computed_.handsFinished; }
    double winRateForTest() const { return computed_.winRate; }
    int blackjacksForTest() const { return computed_.blackjacks; }
    int bustsForTest() const { return computed_.busts; }
    double bjFrequencyForTest() const { return computed_.bjFrequency; }
    double bustFrequencyForTest() const { return computed_.bustFrequency; }
    double strategyAccuracyForTest() const { return computed_.strategyAccuracy; }
    int greatestHandProfitForTest() const { return computed_.greatestHandProfit; }
    int worstHandLossForTest() const { return computed_.worstHandLoss; }
    int peakBankrollForTest() const { return computed_.peakBankroll; }
    int maxDrawdownForTest() const { return computed_.maxDrawdown; }
    const std::vector<int>& perRoundProfitForTest() const { return computed_.perRoundProfit; }
    const std::vector<long long>& bankrollSeriesForTest() const { return computed_.bankrollSeries; }

    // --- Presentation (graphics-only, no game-state mutation) ---
    void draw(sf::RenderWindow& window) const;

  private:
    Tab tab_ = Tab::Overview;
    ComputedStats computed_{};
    float frameTimer_ = 0.0f;

    void advanceTab(bool forward);
};