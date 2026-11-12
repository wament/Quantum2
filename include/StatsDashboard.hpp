#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

// GameRecord.hpp is pure logic (no SFML anywhere) and defines PlayerStats / RoundRecord
// the ComputedStats/feed() signatures to see those types. Kept first because this header stays headless-safe.
#include "GameRecord.hpp"

// The Progress tab's source switch IS the Replay browser's three-way split, reused rather than
// re-invented: one model for "which recorded history am I reading", shared by the browser, the chart
// and the tests. Inventing a second enum is how two surfaces start meaning different things.
#include "HistoryBrowser.hpp"

// GameRecord.hpp is pure logic (no SFML anywhere)
// runs over the Graphics-only test binary, exactly like Tournament / StrategyTrainer. The dashboard reads existing career
// stats (PlayerStats) + round history (RoundRecord) and derives analytics; presentation lives in draw() only (no game
// state mutation), so it is safe to call from GameRenderer while another thread owns nothing here.
class StatsDashboard
{
  public:
    // The four views of the dashboard. Overview = career totals, Hands = per-round result log,
    // Profits = charts, Progress = the bankroll progression charts.
    // Progress is APPENDED, not inserted: Tab's numeric order drives advanceTab()'s cycle and the
    // renderer's label table, so inserting mid-list would silently renumber the existing tabs (the same
    // rule mainMenuItems() follows for MenuOption).
    enum class Tab
    {
        Overview,
        Hands,
        Profits,
        Progress
    };

    // The Progress tab's data. Two SEPARATE axes, kept in separate fields on purpose -- see the
    // vocabulary decision in CONTEXT.md §5 (IDEAS-REVIEWED §G #6):
    //
    //   SOURCE = which recorded history the top chart reads: Career, Daily Challenge or Tournament.
    //          That axis is "across shoes, across play windows", and it is historybrowser::Source --
    //          the same three-way split the Replay browser uses, so a chart and the browser can never
    //          disagree about what "Daily" means.
    //   SCOPE  = the bottom chart's U12 axis: "this shoe" or "this session". That axis is "within one
    //          open play window" and it is NOT a sibling of the source axis. A control that offered
    //          "session" and "Daily" as two options on one switch would be offering two
    //          incommensurable things, which is exactly what §G #6 was opened to prevent.
    struct ProgressData
    {
        // Recorded shoes per source, oldest first (the profile stores them that way).
        std::vector<ShoeRecord> career;
        std::vector<ShoeRecord> daily;
        std::vector<ShoeRecord> tournament;

        // The live play-window curves: cumulative net per round, per shoe and per session.
        std::vector<double> shoeBankroll;
        std::vector<double> sessionBankroll;
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

    // ---- Progress tab ------------------------------------------------------
    // Fed alongside feed() when the mode is entered. Kept separate because the Progress tab reads
    // SHOE records (the across-shoes axis) while feed() takes flattened ROUND records (the per-round
    // axis); merging them would make every existing feed() caller pass data it does not have.
    void setProgressData(const ProgressData& data);

    // Which recorded history the top chart is reading. Defaults to Career, and opening lands on the
    // first NON-EMPTY source -- the same rule the Replay browser uses (historybrowser::preferredOpen),
    // so a chart that opens on an empty list reads as a broken feature in both places.
    historybrowser::Source progressSource() const { return progressSource_; }
    void cycleProgressSource();

    // The bottom chart's scope: false = this shoe, true = this session (the U12 axis).
    bool progressScopeIsSession() const { return progressSession_; }
    void toggleProgressScope() { progressSession_ = !progressSession_; }

    // The series the two charts draw, through the same accessors drawProgressTab() reads. Exposed so a
    // headless test asserts the SELECTION (which dataset a click/keypress lands on), not just the maths.
    std::vector<double> progressCareerSeries() const;
    std::vector<double> progressLiveSeries() const;
    // The label the bottom chart prints, so the scope is always named on screen next to the curve.
    const char* progressLiveLabel() const;
    // The label the top chart prints, naming the source -- the "say which mode it reads" rule.
    const char* progressSourceLabel() const;
    // Which sources currently have shoes in them, indexed by historybrowser::Source.
    historybrowser::Populated progressPopulated() const;


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

    // Progress-tab accessors (same *ForTest() convention): the two axes are asserted through the exact
    // selectors the renderer reads, so a test that mutates a selection proves the chart follows it.
    historybrowser::Source progressSourceForTest() const { return progressSource_; }
    bool progressScopeIsSessionForTest() const { return progressSession_; }
    std::vector<double> progressCareerSeriesForTest() const { return progressCareerSeries(); }
    std::vector<double> progressLiveSeriesForTest() const { return progressLiveSeries(); }

    // --- Presentation (graphics-only, no game-state mutation) ---
    void draw(sf::RenderWindow& window) const;

  private:
    Tab tab_ = Tab::Overview;
    ComputedStats computed_{};
    float frameTimer_ = 0.0f;

    ProgressData progress_;
    historybrowser::Source progressSource_ = historybrowser::Source::Career;
    bool progressSession_ = false;

    void advanceTab(bool forward);

    // The Progress tab's two charts, drawn from the accessors above. Split out of draw() so the tab's
    // layout is one function a screenshot check can reason about.
    void drawProgressTab(sf::RenderWindow& window) const;
};