#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <utility>

#include "Panel.hpp"
#include "HUDState.hpp"
#include "StatBox.hpp"

class HUD
{
public:
    HUD();

    ~HUD();

    void initialize(sf::Font &font);

    void update(const HUDState &hudState);

    void draw(sf::RenderWindow &window);

    // World-space bounds of the Counting Style toggle box (label + value), used for click hit-testing.
    sf::FloatRect countTitleBounds() const;

    // Screen-space bounds of the three graph panes, in draw order (top to bottom). The panes are display-only —
    // they are no longer clickable — but the bounds are exposed so the layout tests can assert they tile the left
    // panel evenly.
    std::vector<sf::Rect<float>> chartPaneBounds() const;

    // Bets Placed histogram bucketing. The raw distribution holds one column per distinct wager, which renders as a
    // bar per unique bet size and is unreadable. Bets are folded into `bucketCount` evenly spaced ranges spanning
    // [0, maxBet], so the chart always shows the same number of evenly spaced bars no matter which sizes were
    // wagered. Pure and static so it can be unit-tested directly.
    static std::vector<int> bucketBetDistribution(const std::map<int, int>& distribution, int bucketCount, int maxBet);

    // Screen-space bounds of the three HUD panels. Exposed so hit-testing and the layout tests can measure the real
    // panels (asserting the side panels stay symmetric and that content stays inside its panel) rather than
    // re-deriving coordinates the way hard-coded geometry used to.
    sf::Rect<float> leftPanelBounds() const;
    sf::Rect<float> rightPanelBounds() const;
    sf::Rect<float> bottomPanelBounds() const;

    // Read-only views of the boxes each panel owns (see Panel::statBoxes). Used by the layout tests to measure real
    // rendered box bounds and assert the distribution stays even; the panels keep ownership.
    const std::vector<StatBox*>& rightPanelStatBoxes() const;
    const std::vector<StatBox*>& bottomPanelStatBoxes() const;

private:
    bool m_showLeft = true;
    bool m_showRight = true;

    Panel m_leftPanel;
    Panel m_rightPanel;
    Panel m_bottomPanel;

    StatBox *m_bankrollBox = nullptr;
    StatBox *m_betBox = nullptr;

    // Counting/stat boxes — Counting Style sits at the top of the right panel and is clickable to toggle.
    StatBox *m_countingStyleBox = nullptr;
    StatBox *m_countBox = nullptr;
    StatBox *m_trueCountBox = nullptr;
    StatBox *m_cardsRemainingBox = nullptr;
    StatBox *m_streakBox = nullptr;
    StatBox *m_strategyBox = nullptr;

    // Book advisor: advises basic strategy for the current hand
    StatBox *m_bookBox = nullptr;

    // Feature 3 analytics stat boxes (moved to the RIGHT panel so the LEFT panel is dedicated to graphs) plus the
    // chart backing data. update() repopulates these every frame from HUDState; drawCharts() renders them on the
    // left panel so they never depend on box geometry at draw time.
    StatBox *m_stdDevBox = nullptr;   // standard deviation of per-hand net profits
    StatBox *m_longWinBox = nullptr;  // longest winning streak (session)
    StatBox *m_longLossBox = nullptr; // longest losing streak (session)
    StatBox *m_edgeBox = nullptr;     // latest estimated player edge (%)

    std::vector<float> m_bankrollSeries;   // bankroll curve: cumulative net profit per round
    std::vector<double> m_edgeSeries;      // edge chart: estimated edge sampled once per round (a level)
    double m_analyticsStdDev = 0.0;        // standard deviation of per-hand net profits
    int m_longestWinStreak = 0;            // longest winning streak, session-long
    int m_longestLoseStreak = 0;           // longest losing streak, session-long
    double m_edgePercent = 0.0;            // current estimated edge (%), live off the count

    // Bets Placed histogram, pre-bucketed into Constants::Hud::BET_BUCKET_COUNT evenly spaced wager ranges by
    // bucketBetDistribution(). m_betBucketWidth is the width of one range in chips, used for the x-axis labels.
    std::vector<int> m_betHistogram;
    int m_betBucketWidth = 0;

    // Per-shoe graph reset is owned by Game (Game::resetShoeAnalytics zeroes the whole ShoeAnalytics struct at
    // every shoe boundary), so the series copied in above are already per-shoe and the HUD keeps no shoe
    // bookkeeping of its own.

    // Per-pane graph geometry, recomputed each frame from the left panel bounds so it tracks the panel size.
    sf::Rect<float> m_bankrollChartRect{};
    sf::Rect<float> m_edgeChartRect{};
    sf::Rect<float> m_betsChartRect{};

    // Controls display
    sf::Text *m_controlsText = nullptr;

    void updateControls(sf::Font &font);

    // Recomputes the three graph-pane rects from the left panel's actual bounds, so geometry tracks the panel
    // size instead of being hard-coded. Called on init and every frame (before draw + input hit-testing).
    void layoutLeftPanels();

    // Feature 3: draw the bankroll-curve, edge-tracking and bet-distribution charts on the left panel.
    void drawCharts(sf::RenderWindow &window) const;
};

