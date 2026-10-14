#pragma once

#include <SFML/Graphics.hpp>
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

    // Clicking a graph pane toggles its secondary overlay view (see toggleGraphSecondary()). Pane identity is
    // exposed so InputHandler can map a click to the correct chart; the display state itself lives in the HUD.
    enum class GraphPane { Bankroll, Edge, Bets };

    // Click-to-change views: toggles which secondary overlay (if any) is drawn on a graph pane. Called from
    // InputHandler when a left-click lands inside a pane's bounds. Display-only state — no game logic changes.
    void toggleGraphSecondary(GraphPane pane);

    // Screen-space bounds of each graph pane, in draw order, for input hit-testing (see InputHandler).
    std::vector<std::pair<sf::Rect<float>, GraphPane>> graphPaneBounds() const;

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
    std::vector<double> m_edgeSeries;      // edge-tracking series: cumulative estimated edge per round
    double m_analyticsStdDev = 0.0;        // standard deviation of per-hand net profits
    int m_longestWinStreak = 0;            // longest winning streak, session-long
    int m_longestLoseStreak = 0;           // longest losing streak, session-long
    double m_edgePercent = 0.0;            // latest cumulative estimated edge (%)
    std::vector<std::pair<int, int>> m_betDistribution; // (bet size, frequency) for the histogram

    // Per-shoe graph recalibration: remembers the last HUDState::currentShoeId seen so we can reset the chart
    // series exactly when a new shoe begins. -1 = never initialised yet.
    int m_lastShoeId = -1;

    // Per-pane graph geometry + secondary-overlay toggles for click-to-change views. Geometry is recomputed each
    // frame from the left panel bounds; the flags are display-only and owned here (no game-logic feedback).
    sf::Rect<float> m_bankrollChartRect{};
    sf::Rect<float> m_edgeChartRect{};
    sf::Rect<float> m_betsChartRect{};
    bool m_bankrollShowSecondary = false;  // per-round net-profit swings overlay
    bool m_edgeShowSecondary = false;      // raw true-count signal overlay
    bool m_betsShowSecondary = false;      // cumulative-distribution overlay
    std::vector<double> m_trueCountPerRound_;   // raw true count per round (backs the Edge chart secondary)

    // Controls display
    sf::Text *m_controlsText = nullptr;

    void updateControls(sf::Font &font);

    // Recomputes the three graph-pane rects from the left panel's actual bounds, so geometry tracks the panel
    // size instead of being hard-coded. Called on init and every frame (before draw + input hit-testing).
    void layoutLeftPanels();

    // Feature 3: draw the bankroll-curve, edge-tracking and bet-distribution charts on the left panel.
    void drawCharts(sf::RenderWindow &window) const;
};

