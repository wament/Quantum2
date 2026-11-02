#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <utility>

#include <string>

#include "HUDState.hpp"
#include "StatBox.hpp"

// Folded out of Panel: the HUD now owns its three panels directly as HudPanel units (a background rectangle plus the
// StatBoxes it lays out). The old separate Panel class was a one-level-of-indirection that only HUD ever used, so
// removing it drops two files and brings HUD's widgets under HUD itself. HudPanel mirrors Panel's exact API
// per-panel call site in HUD.cpp is unchanged
struct HudPanel {
    sf::RectangleShape background;      // dim rounded backing behind the boxes

    std::vector<StatBox*> boxes;        // StatBoxes owned by this panel; HUD keeps ownership through them

    void initialize(sf::Font& font)
    {
        background.setFillColor(sf::Color(40, 40, 40));
        background.setOutlineThickness(2.f);
        background.setOutlineColor(sf::Color(80, 80, 80));
        for (auto* box : boxes)
            box->initialize(font, box->getTitle());
    }

    void setBounds(sf::Vector2f position, sf::Vector2f size)
    {
        background.setPosition(position);
        background.setSize(size);
    }

    // World-space rect of this panel's backing (position + size)
    sf::Rect<float> bounds() const
    {
        return sf::Rect<float>(background.getPosition(), background.getSize());
    }

    const std::vector<StatBox*>& statBoxes() const { return boxes; }

    StatBox* addStatBox(const std::string& title)
    {
        auto* box = new StatBox();
        box->setTitle(title);
        boxes.push_back(box);
        return box;
    }

    void draw(sf::RenderWindow& window) const
    {
        window.draw(background);
        for (auto* box : boxes)
            box->draw(window);
    }
};

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

    // Screen-space bounds of the three graph panes, in draw order (top to bottom). The panes are display-only:
    // they are no longer clickable: but the bounds are exposed
    // panel evenly.
    std::vector<sf::Rect<float>> chartPaneBounds() const;

    // Bets Placed histogram bucketing. The raw distribution holds one column per distinct wager
    // bar per unique bet size and is unreadable. Bets are folded into `bucketCount` evenly spaced ranges spanning
    // [0, maxBet], so the chart always shows the same number of evenly spaced bars no matter which sizes were
    // wagered. Pure and static so it can be unit-tested directly.
    static std::vector<int> bucketBetDistribution(const std::map<int, int>& distribution, int bucketCount, int maxBet);

    // Screen-space bounds of the three HUD panels. Exposed
    // panels (asserting the side panels stay symmetric and that content stays inside its panel) rather than
    // re-deriving coordinates the way hard-coded geometry used to.
    sf::Rect<float> leftPanelBounds() const;
    sf::Rect<float> rightPanelBounds() const;
    sf::Rect<float> bottomPanelBounds() const;

    // Screen-space bounds of all three HUD panels in one lookup
    // geometry lives in a single definition (see HUD::computePanels). Exposed for tests; production reads m_XPanel.bounds()
    // directly where it needs a specific panel.
    struct Panels { sf::Rect<float> left, right, bottom; };
    Panels computePanels() const;

    // Read-only views of the boxes each panel owns (see Panel::statBoxes). Used by the layout tests to measure real
    // rendered box bounds and assert the distribution stays even; the panels keep ownership.
    const std::vector<StatBox*>& rightPanelStatBoxes() const;
    const std::vector<StatBox*>& bottomPanelStatBoxes() const;

private:
    bool m_showLeft = true;
    bool m_showRight = true;

    HudPanel m_leftPanel;
    HudPanel m_rightPanel;
    HudPanel m_bottomPanel;

    StatBox *m_bankrollBox = nullptr;
    StatBox *m_betBox = nullptr;

    // Counting/stat boxes: Counting Style sits at the top of the right panel and is clickable to toggle.
    StatBox *m_countingStyleBox = nullptr;
    StatBox *m_countBox = nullptr;
    StatBox *m_trueCountBox = nullptr;
    StatBox *m_cardsRemainingBox = nullptr;
    StatBox *m_streakBox = nullptr;
    StatBox *m_strategyBox = nullptr;

    // Book advisor: advises basic strategy for the current hand
    StatBox *m_bookBox = nullptr;

    // analytics stat boxes (moved to the RIGHT panel
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

    // Per-pane graph geometry, recomputed each frame from the left panel bounds
    sf::Rect<float> m_bankrollChartRect{};
    sf::Rect<float> m_edgeChartRect{};
    sf::Rect<float> m_betsChartRect{};

    // Controls display
    sf::Text *m_controlsText = nullptr;

    void updateControls(sf::Font &font);

    // Recomputes the three graph-pane rects from the left panel's actual bounds
    // size instead of being hard-coded. Called on init and every frame (before draw + input hit-testing).
    void layoutLeftPanels();

    // draw the bankroll-curve, edge-tracking and bet-distribution charts on the left panel.
    void drawCharts(sf::RenderWindow &window) const;
};

