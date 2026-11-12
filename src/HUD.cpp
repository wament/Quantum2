#include "HUD.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <Constants.hpp>
#include "FontManager.hpp"

HUD::HUD()
{
}

HUD::~HUD()
{
    // StatBox pointers are owned by Panels and cleaned up there

    if (m_controlsText)
        delete m_controlsText;
}

void HUD::initialize(sf::Font& font)
{
    m_leftPanel.initialize(font);
    m_rightPanel.initialize(font);
    m_bottomPanel.initialize(font);

    // Panels are flush against screen edges (no external margin). Both side panels share Constants::Hud::SIDE_WIDTH,
    // so they are always the same width and the table stays centered in the band between them. Nothing here is
    // placed in absolute pixels: resizing a panel resizes its content with it.
    m_bottomPanel.setBounds(
        sf::Vector2f(0.f, static_cast<float>(Constants::WINDOW_HEIGHT) - Constants::Hud::BOTTOM_HEIGHT),
        sf::Vector2f(static_cast<float>(Constants::WINDOW_WIDTH), Constants::Hud::BOTTOM_HEIGHT));

    m_leftPanel.setBounds(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(Constants::Hud::SIDE_WIDTH, Constants::Hud::SIDE_HEIGHT));

    m_rightPanel.setBounds(
        sf::Vector2f(static_cast<float>(Constants::WINDOW_WIDTH) - Constants::Hud::SIDE_WIDTH, 0.f),
        sf::Vector2f(Constants::Hud::SIDE_WIDTH, Constants::Hud::SIDE_HEIGHT));

    // --- Bottom bar boxes ---
    m_bankrollBox = m_bottomPanel.addStatBox("Bankroll");
    m_betBox = m_bottomPanel.addStatBox("Bet");
    m_streakBox = m_bottomPanel.addStatBox("Streak");
    m_strategyBox = m_bottomPanel.addStatBox("Strategy");
    m_bookBox = m_bottomPanel.addStatBox("Book"); // Book advisor: advises basic strategy for the current hand

    // --- Right panel boxes ---
    // Counting Style is first so it sits at the very top; clicking it toggles between Hi-Lo and Reko.
    m_countingStyleBox = m_rightPanel.addStatBox("Counting Style");
    m_countBox = m_rightPanel.addStatBox("Count");
    m_trueCountBox = m_rightPanel.addStatBox("True Count");
    m_cardsRemainingBox = m_rightPanel.addStatBox("Cards Left");

    // analytics boxes live on the RIGHT panel so the LEFT panel is dedicated to graphs.
    m_stdDevBox = m_rightPanel.addStatBox("Std Dev");
    m_longWinBox = m_rightPanel.addStatBox("Long Win");
    m_longLossBox = m_rightPanel.addStatBox("Long Loss");
    m_edgeBox = m_rightPanel.addStatBox("Edge %");

    for (StatBox* box : {m_bankrollBox, m_betBox, m_streakBox, m_strategyBox, m_bookBox,
                         m_countingStyleBox, m_countBox, m_trueCountBox, m_cardsRemainingBox,
                         m_stdDevBox, m_longWinBox, m_longLossBox, m_edgeBox})
        box->initialize(font, box->getTitle());

    // Bottom bar: the five boxes are distributed evenly across the panel width (equal gaps AND equal end margins),
    // so the group is symmetric about the center of the play band
    // instead of clustering against the left edge at a fixed 140px pitch and leaving the right half empty.
    const sf::Rect<float> bottom = m_bottomPanel.bounds();
    const std::vector<StatBox*> bottomBoxes = {m_bankrollBox, m_betBox, m_streakBox, m_strategyBox, m_bookBox};
    const float bottomPitch = (bottom.size.x - 2.f * Constants::PANEL_MARGIN) / static_cast<float>(bottomBoxes.size());
    for (std::size_t i = 0; i < bottomBoxes.size(); ++i)
    {
        const float x = bottom.position.x + Constants::PANEL_MARGIN + static_cast<float>(i) * bottomPitch;
        const float y = bottom.position.y + (bottom.size.y - bottomBoxes[i]->blockHeight()) * 0.5f;
        bottomBoxes[i]->setPosition(sf::Vector2f(x, y));
    }

    // Right panel: the eight boxes are distributed evenly down the panel, each centered in its slot. The pitch comes
    // from the panel height, so the boxes fill it with equal gaps and equal top/bottom margins. The old fixed 46px
    // pitch both crammed them (a ~42px-tall box every 46px) and left the lower half of the panel empty.
    const sf::Rect<float> right = m_rightPanel.bounds();
    const std::vector<StatBox*> rightBoxes = {m_countingStyleBox, m_countBox, m_trueCountBox, m_cardsRemainingBox,
                                              m_stdDevBox, m_longWinBox, m_longLossBox, m_edgeBox};
    const float rightPitch = (right.size.y - 2.f * Constants::PANEL_MARGIN) / static_cast<float>(rightBoxes.size());
    const float rightX = right.position.x + Constants::PANEL_MARGIN;
    for (std::size_t i = 0; i < rightBoxes.size(); ++i)
    {
        const float slotTop = right.position.y + Constants::PANEL_MARGIN + static_cast<float>(i) * rightPitch;
        const float y = slotTop + (rightPitch - rightBoxes[i]->blockHeight()) * 0.5f;
        rightBoxes[i]->setPosition(sf::Vector2f(rightX, y));
    }

    // Layout the graph panes once so their rects are valid for the first frame's drawing and click hit-testing.
    layoutLeftPanels();

    updateControls(font);
}

void HUD::updateControls(sf::Font& font)
{

    m_controlsText = new sf::Text(font, "", static_cast<unsigned int>(Constants::Hud::CONTROLS_FONT_SIZE));
    m_controlsText->setFillColor(sf::Color(200, 200, 200));
    // Sits just above the bottom bar; derived from the bar's height
    m_controlsText->setPosition(sf::Vector2f(0.f, static_cast<float>(Constants::WINDOW_HEIGHT) - Constants::Hud::BOTTOM_HEIGHT - 24.f));

    // Build a single-line controls display
    std::string controls =
        " [Up/Down | Adjust Bet]   [ <- | Auto Kelly]   [ -> | Confirm]                     "
        "[H | Hit]    [S | Stand]    [D | Double]    [P | Split]    [Y | Surrender]                     "
        "[R | Next Round/Restart]";

    // The line must stay inside the central play band. It used to be centred at a fixed font size, and at this
    // length it was wider than the band
    // the bottom chart's axis labels. Shrink the type until it fits rather than letting it spill.
    const sf::Rect<float>& left = m_leftPanel.bounds();
    const sf::Rect<float>& right = m_rightPanel.bounds();
    const float bandLeft = left.position.x + left.size.x;
    const float bandRight = right.position.x;
    const float available = bandRight - bandLeft - 2.f * Constants::PANEL_MARGIN;

    int fontSize = Constants::Hud::CONTROLS_FONT_SIZE;
    m_controlsText->setString(controls);
    m_controlsText->setCharacterSize(static_cast<unsigned>(fontSize));
    while (fontSize > 10 && m_controlsText->getLocalBounds().size.x > available)
    {
        --fontSize;
        m_controlsText->setCharacterSize(static_cast<unsigned>(fontSize));
    }

    // Center horizontally within the central play band between the two side panels (not at absolute screen center)
    // so the controls stay balanced as panel sizes change. Uses real panel bounds, not hard-coded coordinates.
    sf::FloatRect bounds = m_controlsText->getLocalBounds();
    m_controlsText->setOrigin(sf::Vector2f(bounds.size.x / 2.f, 0.f));
    m_controlsText->setPosition(sf::Vector2f((bandLeft + bandRight) / 2.f, static_cast<float>(Constants::WINDOW_HEIGHT) - Constants::Hud::BOTTOM_HEIGHT - 24.f));
}

void HUD::update(const HUDState& hudState)
{
    // Recompute graph-pane geometry each frame
    layoutLeftPanels();

    if (m_bankrollBox)
        m_bankrollBox->setValue(std::to_string(hudState.bankroll));

    if (m_betBox)
        m_betBox->setValue(std::to_string(hudState.currentBet));

    if (m_countingStyleBox)
        m_countingStyleBox->setValue(hudState.countingStyleName);

    if (m_countBox)
    {
        m_countBox->setTitle("Count");
        m_countBox->setValue(std::to_string(hudState.runningCount));
    }

    if (m_trueCountBox)
    {
        // Title reflects the active counting system so Reko's displayed running count isn't mislabelled "True Count" (U5).
        m_trueCountBox->setTitle(hudState.countingBoxLabel);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << hudState.trueCount;
        m_trueCountBox->setValue(oss.str());

        // White when Hi-Lo is active (running + true count are the same system); yellow when Reko is active so
        // the surfaced Hi-Lo true count stands out as a cross-check. sf::Color(255, 215, 0) matches the game's
        // gold/yellow accent used for the strategy hint.
        m_trueCountBox->setValueColor(hudState.trueCountHighlighted ? sf::Color(255, 215, 0) : sf::Color::White);
    }

    if (m_cardsRemainingBox)
        m_cardsRemainingBox->setValue(std::to_string(hudState.cardsRemaining));

    if (m_streakBox)
    {
        std::string streakStr;
        if (hudState.streak == 0)
        {
            streakStr = "-";
        }
        else
        {
            streakStr = std::to_string(hudState.streak);
            streakStr += hudState.streakIsWin ? "W" : "L";
        }
        m_streakBox->setValue(streakStr);
    }

    if (m_strategyBox)
    {
        std::string strategyStr;
        // Per-shoe accuracy for the human seat only (Game::shoe_ is zeroed at every boundary and NPC plays are not
        // counted), so the box reads "how am I playing THIS shoe" instead of a career average that never resets.
        if (hudState.shoeAnalytics && hudState.shoeAnalytics->strategyTotal > 0)
        {
            int pct = (hudState.shoeAnalytics->strategyHits * 100) / hudState.shoeAnalytics->strategyTotal;
            strategyStr = std::to_string(pct) + "%";
        }
        else
        {
            strategyStr = "-";
        }
        m_strategyBox->setValue(strategyStr);
    }

    // Book advisor: show the advised action during the player's turn, and hide it (dash) on the
    // Book advisor feedback: after a completed action show the verdict: green when it matched basic
    // strategy, red when the player deviated. Otherwise advise the live move in white during the player's turn and
    // hide (dash) on the dealer's turn or with no active hand.
    if (m_bookBox)
    {
        const sf::Color correct = sf::Color(60, 200, 90);   // green: matched the book
        const sf::Color deviated = sf::Color(220, 50, 50);  // red: deviated from the book
        const sf::Color normal = sf::Color::White;

        if (hudState.isDealerTurn || hudState.basicStrategyHint.empty())
        {
            m_bookBox->setValue("-");
            m_bookBox->setValueColor(normal);
        }
        else if (hudState.advisorFeedback == AdvisorFeedback::Correct)
        {
            m_bookBox->setValue(hudState.basicStrategyHint);
            m_bookBox->setValueColor(correct);
        }
        else if (hudState.advisorFeedback == AdvisorFeedback::Deviated)
        {
            m_bookBox->setValue(hudState.basicStrategyHint);
            m_bookBox->setValueColor(deviated);
        }
        else
        {
            m_bookBox->setValue(hudState.basicStrategyHint);
            m_bookBox->setValueColor(normal);
        }
    }

    // analytics stat boxes + chart backing data (left-panel dashboard). updateHUDState() derives all of
    // these from Game::shoe_ each frame
    // when the shoe does. Dash anything not yet measured in this shoe.
    if (m_stdDevBox)
    {
        std::ostringstream devOss;
        devOss << std::fixed << std::setprecision(2) << hudState.analyticsStdDev;
        m_stdDevBox->setValue(hudState.analyticsStdDev != 0.0 ? devOss.str() : "-");
    }
    if (m_longWinBox)
        m_longWinBox->setValue(hudState.longestWinningStreak > 0 ? std::to_string(hudState.longestWinningStreak) : "-");
    if (m_longLossBox)
        m_longLossBox->setValue(hudState.longestLosingStreak > 0 ? std::to_string(hudState.longestLosingStreak) : "-");
    if (m_edgeBox)
    {
        std::ostringstream edgeOss;
        edgeOss << std::fixed << std::setprecision(2) << hudState.edgePercent;
        m_edgeBox->setValue(edgeOss.str() + "%");
    }

    // Chart backing data, copied from HUDState
    // The per-shoe reset is owned by Game: every shoe boundary runs resetShoeAnalytics()
    // ShoeAnalytics struct, so what arrives here already holds only the current shoe and the curves + Edge %
    // readout start at baseline. The bankroll curve is the human seat's net alone: NPC results never move it.
    // (An earlier HUD-side "clear on shoe change" guard here was dead code
    // assignments below on the very same frame, so it could never have reset anything.)
    m_bankrollSeries = hudState.bankrollSeries;
    m_edgeSeries = hudState.edgeSeries;
    m_analyticsStdDev = hudState.analyticsStdDev;
    m_longestWinStreak = hudState.longestWinningStreak;
    m_longestLoseStreak = hudState.longestLosingStreak;
    m_edgePercent = hudState.edgePercent;

    // Bets Placed chart: fold the raw per-wager distribution into evenly spaced ranges. One bar per distinct bet
    // size is unreadable as soon as a player varies their wager, and the ranges stay put as bets come in. The
    // distribution is this shoe's (Game::shoe_)
    m_betHistogram.clear();
    m_betBucketWidth = 0;
    if (hudState.shoeAnalytics && !hudState.shoeAnalytics->betDistribution.empty())
    {
        // Axis starts at the minimum bet and grows to the largest wager placed this shoe as it exceeds 100, capped
        // hard at MAXIMUM_BET. That spreads low wagers across the chart instead of collapsing them into bucket 0.
        const int maxWager = hudState.shoeAnalytics->betDistribution.rbegin()->first;
        m_betAxisLo = Constants::MINIMUM_BET;
        const int axisHi = HUD::computeAxisMax(maxWager);
        m_betHistogram = bucketBetDistribution(hudState.shoeAnalytics->betDistribution,
                                               Constants::BET_BUCKET_COUNT, m_betAxisLo, axisHi);
        m_betBucketWidth = (axisHi - m_betAxisLo) / Constants::BET_BUCKET_COUNT;
    }
}

void HUD::draw(sf::RenderWindow& window)
{
    if (m_showLeft)
        m_leftPanel.draw(window);

    if (m_showRight)
        m_rightPanel.draw(window);

    // Bottom panel always visible
    m_bottomPanel.draw(window);

    // bankroll-curve, edge-tracking and bet-distribution charts on the empty left panel.
    drawCharts(window);

    // Draw controls above bottom panel
    window.draw(*m_controlsText);
}

sf::FloatRect HUD::countTitleBounds() const
{
    // The Counting Style box (e.g. "Counting Style" / "Hi-Lo") is clickable: see InputHandler toggling the
    // counting style. Bounds span both its label and value lines

    return m_countingStyleBox ? m_countingStyleBox->getTitleBounds() : sf::FloatRect{};
}

namespace {

    // The left panel is now dedicated to graphs. These stacked plot rects tile the whole panel (geometry derived
    // from the actual panel bounds so they adapt if the panel size ever changes), reserving a top title strip and a
    // bottom x-axis strip for each chart with modest spacing between them.
    // Chart geometry and type sizes come from Constants::Hud
    // retuned by hand every time a panel is resized.
    constexpr float kTitleStrip = Constants::Hud::CHART_TITLE; // strip at a chart's top for its title label
    constexpr float kXAxisStrip = Constants::Hud::CHART_XAXIS; // strip at a chart's bottom for round-number ticks
    constexpr float kTick = Constants::Hud::CHART_TICK;        // x-axis tick length
    constexpr float kBgInset = Constants::Hud::CHART_BG_INSET; // chart background inset from its pane rect
    constexpr int kYGridlines = Constants::Hud::CHART_Y_GRIDLINES; // horizontal rules a plot gets before it turns to mush
    constexpr unsigned kLabelFont = static_cast<unsigned>(Constants::Hud::CHART_LABEL_FONT);
    constexpr unsigned kTitleFont = static_cast<unsigned>(Constants::Hud::CHART_TITLE_FONT);

    // One accent colour per chart, reused for the series, its fill and its title readout
    // identifies the chart without having to read the label.
    const sf::Color kBankrollColor(90, 200, 90);
    const sf::Color kEdgeColor(120, 170, 255);
    const sf::Color kBetsColor(255, 215, 0);

    std::vector<sf::Rect<float>> leftPanelCharts(const sf::Rect<float>& panel)
    {
        const int chartCount = 3;
        const float margin = Constants::Hud::CHART_MARGIN;
        const float gap = Constants::Hud::CHART_GAP;

        const float totalGap = static_cast<float>(chartCount - 1) * gap;
        const float perChart = (panel.size.y - 2.f * margin - totalGap) / static_cast<float>(chartCount);
        const float plotW = panel.size.x - 2.f * margin;

        std::vector<sf::Rect<float>> rects;
        rects.reserve(chartCount);
        for (int i = 0; i < chartCount; ++i)
        {
            const float top = panel.position.y + margin + static_cast<float>(i) * (perChart + gap);
            rects.emplace_back(sf::Vector2f(panel.position.x + margin, top), sf::Vector2f(plotW, perChart));
        }
        return rects;
    }

    // Inner plotting box of a chart rect: inset for the left y-axis gutter and the top/bottom title/x strips. The
    // extra 8px under the title strip keeps the topmost y label clear of the title text.
    sf::Rect<float> plotArea(const sf::Rect<float>& rect)
    {
        const float gutter = Constants::Hud::CHART_GUTTER;  // space on the left for right-aligned value labels
        return sf::Rect<float>(sf::Vector2f(rect.position.x + gutter, rect.position.y + kTitleStrip + 8.f),
                               sf::Vector2f(rect.size.x - gutter - 6.f,
                                            rect.size.y - kTitleStrip - 8.f - kXAxisStrip - 4.f));
    }

    // Chart chrome shared by all three panes: the plot background, the chart name on the left of the title strip and
    // a live value readout on the right. The readout is what makes a ~176px-wide plot worth looking at: the exact
    // number is legible even when the curve's own amplitude is only a few pixels tall.
    void drawChartFrame(sf::RenderWindow& window, const sf::Rect<float>& rect, const char* title,
                        const std::string& readout, sf::Color accent)
    {
        sf::RectangleShape bg(sf::Vector2f(rect.size.x - kBgInset, rect.size.y - kBgInset));
        bg.setPosition(sf::Vector2f(rect.position.x + kBgInset * 0.5f, rect.position.y + kBgInset * 0.5f));
        bg.setFillColor(sf::Color(20, 25, 40, 130));
        window.draw(bg);

        sf::Text value(FontManager::get(), readout, kTitleFont);
        value.setFillColor(accent);
        sf::FloatRect vb = value.getLocalBounds();
        value.setOrigin(sf::Vector2f(vb.size.x, 0.f));   // top-right anchor
        value.setPosition(sf::Vector2f(rect.position.x + rect.size.x - 8.f, rect.position.y + 4.f));
        window.draw(value);

        sf::Text name(FontManager::get(), title, kTitleFont);
        name.setFillColor(sf::Color(220, 220, 220));
        name.setPosition(sf::Vector2f(rect.position.x + 8.f, rect.position.y + 4.f));
        window.draw(name);
    }

    // Compact axis label: 1200 -> "1.2k", 1000 -> "1k", 40 -> "40". The gutter is only CHART_GUTTER px wide
    // full "-1,240" overflows it and lands on the panel edge
    // clipped garbage. k-notation keeps every label inside the gutter at any magnitude.
    std::string compactNumber(double v, int precision)
    {
        std::ostringstream oss;
        if (std::abs(v) >= 1000.0)
        {
            const double k = v / 1000.0;
            if (std::abs(k - std::round(k)) < 0.05)
                oss << std::fixed << std::setprecision(0) << std::round(k);   // "1k", not "1.0k"
            else
                oss << std::fixed << std::setprecision(1) << k;
            oss << 'k';
        }
        else
        {
            oss << std::fixed << std::setprecision(precision) << v;
        }
        return oss.str();
    }

    // Round a target spacing up to a "nice" step (1/2/5 x power of ten)
    double niceStep(double raw)
    {
        if (!(raw > 0.0))
            raw = 1.0;
        const int exp = static_cast<int>(std::floor(std::log10(raw)));
        const float frac = static_cast<float>(raw / std::pow(10.0, exp));
        double niceFrac;
        if (frac < 1.5)
            niceFrac = 1.0;
        else if (frac < 3.0)
            niceFrac = 2.0;
        else if (frac < 7.0)
            niceFrac = 5.0;
        else
            niceFrac = 10.0;
        return niceFrac * std::pow(10.0, exp);
    }

    // Snap [lo,hi] outward to whole "nice" steps
    // plot edge. The range always spans zero: for both the bankroll curve and the edge curve, "above or below the
    // baseline" is the first question the chart has to answer, and pinning the baseline in place also stops the plot
    // from rescaling (and the whole curve jumping) every time a new extreme comes in.
    void niceRange(double lo, double hi, int gridlines, double& outLo, double& outHi, double& outStep)
    {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
        if (hi <= lo)
            hi = lo + 1.0;

        outStep = niceStep((hi - lo) / static_cast<double>(gridlines));
        outLo = std::floor(lo / outStep) * outStep;
        outHi = std::ceil(hi / outStep) * outStep;
    }

    // Horizontal gridlines + right-aligned value labels across [lo,hi] at `step` increments (lo/hi/step come from
    // niceRange, so every label is a round number), the left axis line, and a brighter zero baseline. The zero rule
    // is drawn heavier than the others because it is the reference the whole chart is read against.
    void drawYAxis(sf::RenderWindow& window, const sf::Rect<float>& pbox, double lo, double hi, double step,
                   int precision)
    {
        const float axisX = pbox.position.x;
        const float plotRight = pbox.position.x + pbox.size.x;
        const float top = pbox.position.y;
        const float bottom = pbox.position.y + pbox.size.y;

        for (double v = lo; v <= hi + step * 0.5; v += step)
        {
            const float y = bottom + static_cast<float>((v - lo) / (hi - lo)) * (top - bottom);
            const bool isZero = (lo <= 0.0 && hi >= 0.0 && std::abs(v) < step * 0.5);

            sf::VertexArray grid(sf::PrimitiveType::Lines, 2);
            grid[0].position = sf::Vector2f(axisX, y);
            grid[1].position = sf::Vector2f(plotRight, y);
            grid[0].color = grid[1].color = isZero ? sf::Color(200, 200, 200, 210) : sf::Color(120, 120, 120, 110);
            window.draw(grid);

            sf::Text label(FontManager::get(), compactNumber(v, precision), kLabelFont);
            label.setFillColor(sf::Color(180, 180, 180));
            sf::FloatRect lb = label.getLocalBounds();
            label.setOrigin(sf::Vector2f(lb.size.x + 2.f, lb.size.y * 0.5f));  // right-center anchor
            label.setPosition(sf::Vector2f(axisX - 3.f, y));
            window.draw(label);
        }

        sf::VertexArray axisLine(sf::PrimitiveType::Lines, 2);
        axisLine[0].position = sf::Vector2f(axisX, top);
        axisLine[1].position = sf::Vector2f(axisX, bottom);
        axisLine[0].color = axisLine[1].color = sf::Color(150, 150, 150, 150);
        window.draw(axisLine);
    }

    // Evenly spaced round-number x ticks ("Round n") centered in the bottom strip of a chart. Three labels is as
    // many as a ~176px plot can carry without the numbers running into each other, and first / middle / last is
    // enough to judge how far through the shoe the curve has run.
    void drawXTicks(sf::RenderWindow& window, const sf::Rect<float>& pbox, std::size_t points)
    {
        if (points < 2)
            return;

        const int labelCount = 3;
        const float bottom = pbox.position.y + pbox.size.y;
        for (int i = 0; i < labelCount; ++i)
        {
            double frac = static_cast<double>(i) / static_cast<double>(labelCount - 1);
            float x = pbox.position.x + static_cast<float>(frac) * pbox.size.x;
            long roundNo = 1 + static_cast<long>(std::lround(frac * static_cast<double>(points - 1)));

            sf::VertexArray tick(sf::PrimitiveType::Lines, 2);
            tick[0].position = sf::Vector2f(x, bottom);
            tick[1].position = sf::Vector2f(x, bottom + kTick);
            tick[0].color = tick[1].color = sf::Color(150, 150, 150, 150);
            window.draw(tick);

            std::ostringstream oss;
            oss << roundNo;
            sf::Text label(FontManager::get(), oss.str(), kLabelFont);
            label.setFillColor(sf::Color(160, 160, 160));
            sf::FloatRect lb = label.getLocalBounds();
            // Opaque backing so the number sits in a clean cutout over the gridlines/background. Without it, centered
            // digits whose width changes each round (9->10->...->100) smear against whatever is behind them and read
            // as stale ghosting/residue rather than crisp labels.
            sf::RectangleShape backing(sf::Vector2f(lb.size.x + 4.f, lb.size.y + 2.f));
            backing.setPosition(sf::Vector2f(x - lb.size.x * 0.5f - 2.f, bottom + 3.f));
            backing.setFillColor(sf::Color(18, 22, 36));
            window.draw(backing);
            label.setOrigin(sf::Vector2f(lb.size.x * 0.5f, lb.size.y));  // bottom-center anchor
            label.setPosition(sf::Vector2f(x, bottom + kTick));
            window.draw(label);
        }
    }

    template <typename Series>
    void drawLineChart(sf::RenderWindow& window, const sf::Rect<float>& rect, const Series& data,
                       sf::Color lineColor, int precision, const char* title, const std::string& readout)
    {
        // The frame (background + title + readout) draws even with too little data to plot
        // labelled and in place instead of blanking out at the start of every shoe.
        drawChartFrame(window, rect, title, readout, lineColor);

        if (data.size() < 2)
            return;

        double lo = data.front(), hi = data.front();
        for (const auto& v : data) { lo = std::min(lo, static_cast<double>(v)); hi = std::max(hi, static_cast<double>(v)); }
        double axisLo, axisHi, step;
        niceRange(lo, hi, kYGridlines, axisLo, axisHi, step);

        const sf::Rect<float> pbox = plotArea(rect);
        drawYAxis(window, pbox, axisLo, axisHi, step, precision);
        drawXTicks(window, pbox, data.size());

        const float plotBottom = pbox.position.y + pbox.size.y;
        auto xAt = [&](std::size_t i) { return pbox.position.x + pbox.size.x * static_cast<float>(i) / static_cast<float>(data.size() - 1); };
        auto yAt = [&](double v) { return plotBottom - static_cast<float>((v - axisLo) / (axisHi - axisLo)) * pbox.size.y; };

        // Filled band between the curve and the zero baseline. In a plot this narrow the line's own amplitude is only
        // a few pixels, so the filled area above/below the baseline is what actually reads as "up" or "down"
        // line alone was most of why these charts conveyed nothing.
        const float zeroY = yAt(0.0);
        const sf::Color fillColor(lineColor.r, lineColor.g, lineColor.b, 60);
        sf::VertexArray fill(sf::PrimitiveType::Triangles, (data.size() - 1) * 6);
        std::size_t tri = 0;
        auto addVert = [&](float x, float y) {
            fill[tri].position = sf::Vector2f(x, y);
            fill[tri].color = fillColor;
            ++tri;
        };
        for (std::size_t i = 0; i + 1 < data.size(); ++i)
        {
            const float x0 = xAt(i), x1 = xAt(i + 1);
            const float y0 = yAt(static_cast<double>(data[i])), y1 = yAt(static_cast<double>(data[i + 1]));
            addVert(x0, zeroY); addVert(x0, y0); addVert(x1, y1);
            addVert(x0, zeroY); addVert(x1, y1); addVert(x1, zeroY);
        }
        // Every segment contributes exactly 6 vertices
        window.draw(fill);

        sf::VertexArray line(sf::PrimitiveType::LineStrip, data.size());
        for (std::size_t i = 0; i < data.size(); ++i) { line[i].position = sf::Vector2f(xAt(i), yAt(static_cast<double>(data[i]))); line[i].color = lineColor; }
        window.draw(line);

        // Marker on the newest point: the eye lands on "where am I now", and it lines up with the title readout.
        sf::CircleShape dot(3.f);
        dot.setOrigin(sf::Vector2f(3.f, 3.f));
        dot.setPosition(sf::Vector2f(xAt(data.size() - 1), yAt(static_cast<double>(data.back()))));
        dot.setFillColor(lineColor);
        window.draw(dot);
    }


    // Bets Placed: one bar per wager range (see HUD::bucketBetDistribution). The x-axis labels run from m_betAxisLo
    // (the minimum bet) up to the adaptive upper bound, spaced by bucketWidth chips.
    void drawHistogram(sf::RenderWindow& window, const sf::Rect<float>& rect, const std::vector<int>& buckets,
                       int bucketWidth, int axisBase, const std::string& readout)
    {
        drawChartFrame(window, rect, "Bets Placed", readout, kBetsColor);

        int maxFreq = 0;
        for (int count : buckets)
            maxFreq = std::max(maxFreq, count);
        if (maxFreq <= 0)
            return;

        double axisLo, axisHi, step;
        niceRange(0.0, static_cast<double>(maxFreq), kYGridlines, axisLo, axisHi, step);

        const sf::Rect<float> pbox = plotArea(rect);
        drawYAxis(window, pbox, axisLo, axisHi, step, 0);

        const float plotBottom = pbox.position.y + pbox.size.y;
        const float barW = pbox.size.x / static_cast<float>(buckets.size());

        for (std::size_t i = 0; i < buckets.size(); ++i)
        {
            if (buckets[i] <= 0)
                continue;   // an empty range still occupies its slot, keeping the bars evenly spaced
            const float barH = static_cast<float>(buckets[i]) / static_cast<float>(axisHi) * pbox.size.y;
            sf::RectangleShape bar(sf::Vector2f(std::max(1.f, barW - 2.f), barH));
            bar.setFillColor(kBetsColor);
            bar.setPosition(sf::Vector2f(pbox.position.x + static_cast<float>(i) * barW + 1.f, plotBottom - barH));
            window.draw(bar);
        }

        // X axis: a tick at every range boundary (axisBase, axisBase+bucketWidth, ... up to the adaptive upper bound)
        // evenly spaced bars read against the wager scale, but text only at 0 / the midpoint / the table maximum.
        // Ten ranges across a ~176px plot is ~17px per bar
        // top of each other -- the single most illegible thing about the old chart.
        const int rangeCount = static_cast<int>(buckets.size());
        for (int i = 0; i <= rangeCount; ++i)
        {
            const float x = pbox.position.x + static_cast<float>(i) * barW;
            const bool labelThis = (i == 0 || i == rangeCount || i * 2 == rangeCount);

            sf::VertexArray tick(sf::PrimitiveType::Lines, 2);
            tick[0].position = sf::Vector2f(x, plotBottom);
            tick[1].position = sf::Vector2f(x, plotBottom + (labelThis ? kTick : kTick * 0.5f));
            tick[0].color = tick[1].color = sf::Color(150, 150, 150, 150);
            window.draw(tick);

            if (!labelThis)
                continue;

            std::ostringstream oss;
            oss << axisBase + i * bucketWidth;
            sf::Text label(FontManager::get(), oss.str(), kLabelFont);
            label.setFillColor(sf::Color(160, 160, 160));
            sf::FloatRect lb = label.getLocalBounds();
            // Keep the labels inside the pane: a boundary label is centered on its tick
            // otherwise hang off the panel edge and the first would sit on top of the y axis' own bottom label.
            const float labelX = std::clamp(x,
                                            pbox.position.x + lb.size.x * 0.5f + 2.f,
                                            rect.position.x + rect.size.x - lb.size.x * 0.5f - 2.f);

            sf::RectangleShape backing(sf::Vector2f(lb.size.x + 4.f, lb.size.y + 2.f));
            backing.setPosition(sf::Vector2f(labelX - lb.size.x * 0.5f - 2.f, plotBottom + kTick - 1.f));
            backing.setFillColor(sf::Color(18, 22, 36));
            window.draw(backing);
            label.setOrigin(sf::Vector2f(lb.size.x * 0.5f, lb.size.y));  // bottom-center anchor
            label.setPosition(sf::Vector2f(labelX, plotBottom + kTick));
            window.draw(label);
        }
    }

} // namespace

void HUD::drawCharts(sf::RenderWindow& window) const
{
    // Bankroll Curve: net profit for the CURRENT shoe. Game clears the series at every shoe boundary
    // always restarts from the baseline and answers "how has this shoe gone?" rather than "how has my whole session
    // drifted?". The title readout carries the exact figure, which a 176px-wide curve cannot.
    {
        const double net = m_bankrollSeries.empty() ? 0.0 : static_cast<double>(m_bankrollSeries.back());
        std::ostringstream bankOss;
        if (net > 0.0)
            bankOss << '+';
        bankOss << compactNumber(net, 0);
        drawLineChart(window, m_bankrollChartRect, m_bankrollSeries, kBankrollColor, 0, "Bankroll Curve", bankOss.str());
    }

    // Edge Over Time: the player's estimated edge at each round of the current shoe. It is a LEVEL, not a running
    // total - every round re-reads the edge from the count as it stands
    // baseline (about -0.5%) and stays inside a few percent instead of ramping off the plot the longer the shoe
    // runs. resetShoeAnalytics() clears the series at every shoe boundary (new game, R at end of shoe, auto-play,
    // mid-shoe reshuffle), so the plot restarts with the shoe instead of carrying the old shoe's drift.
    {
        std::ostringstream edgeOss;
        edgeOss << std::fixed << std::setprecision(2) << m_edgePercent << '%';
        drawLineChart(window, m_edgeChartRect, m_edgeSeries, kEdgeColor, 1, "Edge Over Time", edgeOss.str());
    }

    // Bets Placed: wagers folded into evenly spaced ranges (see bucketBetDistribution). The readout is the number of
    // wagers the bars cover, so an all-empty histogram is visibly "no bets yet" rather than a rendering bug.
    {
        int total = 0;
        for (int count : m_betHistogram)
            total += count;
        drawHistogram(window, m_betsChartRect, m_betHistogram, m_betBucketWidth, m_betAxisLo, std::to_string(total) + " bets");
    }
}

std::vector<sf::Rect<float>> HUD::chartPaneBounds() const
{
    // Draw order (top to bottom): Bankroll, Edge, Bets.
    return {m_bankrollChartRect, m_edgeChartRect, m_betsChartRect};
}

// Folds the raw per-wager distribution into `bucketCount` evenly spaced ranges spanning [axisLo, axisHi]: bucket i
// covers [axisLo + i*width, axisLo + (i+1)*width). Wagers below axisLo land in the first bucket; wagers at or above
// axisHi clamp into the last. See computeAxisMax for how the upper bound grows with the largest placed wager.
std::vector<int> HUD::bucketBetDistribution(const std::map<int, int>& distribution, int bucketCount, int axisLo, int axisHi)
{
    const int rangeCount = std::max(1, bucketCount);
    const double span = std::max(1.0, static_cast<double>(axisHi - axisLo));
    const double width = span / rangeCount;
    std::vector<int> buckets(static_cast<std::size_t>(rangeCount), 0);

    for (const auto& entry : distribution)
    {
        int wager = entry.first;
        if (wager <= 0)
            continue;                                // non-positive wagers never reach the chart
        if (wager < axisLo)
        {
            buckets[0] += entry.second;             // below the minimum bet: first bucket only
            continue;
        }

        int index = static_cast<int>((wager - axisLo) / width);
        if (index >= rangeCount)
            index = rangeCount - 1;                 // at/above the upper bound: last bucket
        buckets[static_cast<std::size_t>(index)] += entry.second;
    }

    return buckets;
}

// See HUD.hpp. Base is [10, 100]; a placed wager above 100 becomes the soft max and grows the axis to match it, capped
// hard at MAXIMUM_BET (1000). No data yet still reports the flat default so the chart keeps its scale.
int HUD::computeAxisMax(int maxWagerSeenInWindow)
{
    const int base = 100;
    const int hi = (maxWagerSeenInWindow > base) ? maxWagerSeenInWindow : base;
    return std::min(hi, Constants::MAXIMUM_BET);
}

// One lookup of all three panel bounds; left/right/bottomPanelBounds delegate to it
HUD::Panels HUD::computePanels() const
{
    return {m_leftPanel.bounds(), m_rightPanel.bounds(), m_bottomPanel.bounds()};
}

sf::Rect<float> HUD::leftPanelBounds() const
{
    return computePanels().left;
}

sf::Rect<float> HUD::rightPanelBounds() const
{
    return computePanels().right;
}

sf::Rect<float> HUD::bottomPanelBounds() const
{
    return computePanels().bottom;
}

const std::vector<StatBox*>& HUD::rightPanelStatBoxes() const
{
    return m_rightPanel.statBoxes();
}

const std::vector<StatBox*>& HUD::bottomPanelStatBoxes() const
{
    return m_bottomPanel.statBoxes();
}

void HUD::layoutLeftPanels()
{
    // Recompute the three graph-pane rects from the real left-panel bounds
    // stay correct for both drawing and InputHandler click hit-testing.
    const auto charts = leftPanelCharts(m_leftPanel.bounds());
    m_bankrollChartRect = charts[0];
    m_edgeChartRect     = charts[1];
    m_betsChartRect     = charts[2];
}

