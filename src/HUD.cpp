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

    const float BOTTOM_HEIGHT = 80.f;

    // Panels are flush against screen edges (no external margin)

    m_bottomPanel.setBounds(
        sf::Vector2f(0.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT),
        sf::Vector2f(Constants::WINDOW_WIDTH, BOTTOM_HEIGHT));

    m_leftPanel.setBounds(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));

    m_rightPanel.setBounds(
        sf::Vector2f(Constants::WINDOW_WIDTH - 200.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));

    // --- Bottom bar boxes (Bankroll + Bet keep their original left positions) ---
    m_bankrollBox = m_bottomPanel.addStatBox("Bankroll");
    m_bankrollBox->initialize(font, "Bankroll");

    m_betBox = m_bottomPanel.addStatBox("Bet");
    m_betBox->initialize(font, "Bet");

    // Streak + Strategy moved down next to Bankroll/Bet in the bottom bar
    m_streakBox = m_bottomPanel.addStatBox("Streak");
    m_streakBox->initialize(font, "Streak");

    m_strategyBox = m_bottomPanel.addStatBox("Strategy");
    m_strategyBox->initialize(font, "Strategy");

    // New: Book advisor — advises basic strategy for the current hand
    m_bookBox = m_bottomPanel.addStatBox("Book");
    m_bookBox->initialize(font, "Book");

    // --- Right panel boxes stacked vertically at the top of the panel ---
    // Counting Style is first so it sits at the very top; clicking it toggles between Hi-Lo and Reko.
    m_countingStyleBox = m_rightPanel.addStatBox("Counting Style");
    m_countingStyleBox->initialize(font, "Counting Style");

    m_countBox = m_rightPanel.addStatBox("Count");
    m_countBox->initialize(font, "Count");

    m_trueCountBox = m_rightPanel.addStatBox("True Count");
    m_trueCountBox->initialize(font, "True Count");

    m_cardsRemainingBox = m_rightPanel.addStatBox("Cards Left");
    m_cardsRemainingBox->initialize(font, "Cards Left");

    // Bottom bar: horizontal stack. Bankroll and Bet keep their original left positions; Streak,
    // Strategy and Book follow to the right with even spacing. Positions are absolute screen coords.
    float panelX = Constants::PANEL_MARGIN + 5.f;
    float panelY = Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT + Constants::PANEL_MARGIN;

    const float SPACING = 140.f;
    m_bankrollBox->setPosition(sf::Vector2f(panelX, panelY));
    m_betBox->setPosition(sf::Vector2f(panelX + SPACING, panelY));
    m_streakBox->setPosition(sf::Vector2f(panelX + 2.f * SPACING, panelY));
    m_strategyBox->setPosition(sf::Vector2f(panelX + 3.f * SPACING, panelY));
    m_bookBox->setPosition(sf::Vector2f(panelX + 4.f * SPACING, panelY));

    // Right panel: vertical stack at the top of the panel. Positions are absolute screen coords so the
    // boxes sit inside the right panel (whose top-left is WINDOW_WIDTH-200, 0).
    const float boxHeight = 46.f;
    const float rightPanelX = Constants::WINDOW_WIDTH - 200.f + Constants::PANEL_MARGIN;
    const float rightPanelY = Constants::PANEL_MARGIN;

    m_countingStyleBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY));
    m_countBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY + boxHeight));
    m_trueCountBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY + 2.f * boxHeight));
    m_cardsRemainingBox->setPosition(sf::Vector2f(rightPanelX, rightPanelY + 3.f * boxHeight));

    // Feature 3 analytics boxes moved to the RIGHT panel so the LEFT panel is dedicated to graphs. They stack
    // directly below the four counting boxes (Counting Style / Count / True Count / Cards Left), continuing the
    // same vertical rhythm with boxHeight spacing. Positions are absolute screen coords inside the right panel.
    const float analyticsBaseY = rightPanelY + 4.f * boxHeight;

    m_stdDevBox = m_rightPanel.addStatBox("Std Dev");
    m_stdDevBox->initialize(font, "Std Dev");
    m_stdDevBox->setPosition(sf::Vector2f(rightPanelX, analyticsBaseY));

    m_longWinBox = m_rightPanel.addStatBox("Long Win");
    m_longWinBox->initialize(font, "Long Win");
    m_longWinBox->setPosition(sf::Vector2f(rightPanelX, analyticsBaseY + boxHeight));

    m_longLossBox = m_rightPanel.addStatBox("Long Loss");
    m_longLossBox->initialize(font, "Long Loss");
    m_longLossBox->setPosition(sf::Vector2f(rightPanelX, analyticsBaseY + 2.f * boxHeight));

    m_edgeBox = m_rightPanel.addStatBox("Edge %");
    m_edgeBox->initialize(font, "Edge %");
    m_edgeBox->setPosition(sf::Vector2f(rightPanelX, analyticsBaseY + 3.f * boxHeight));

    // Layout the graph panes once so their rects are valid for the first frame's drawing and click hit-testing.
    layoutLeftPanels();

    updateControls(font);
}

void HUD::updateControls(sf::Font& font)
{

    m_controlsText = new sf::Text(font, "", 13u);
    m_controlsText->setFillColor(sf::Color(200, 200, 200));
    m_controlsText->setPosition(sf::Vector2f(0.f, Constants::WINDOW_HEIGHT - 80.f - 20.f));

    // Build a single-line controls display
    std::string controls =
        " [Up/Down | Adjust Bet]   [ <- | Auto Kelly]   [ -> | Confirm]                     "
        "[H | Hit]    [S | Stand]    [D | Double]    [P | Split]    [Y | Surrender]                     "
        "[R | Next Round/Restart]";

    m_controlsText->setString(controls);

    // Center horizontally
    sf::FloatRect bounds = m_controlsText->getLocalBounds();
    m_controlsText->setOrigin(sf::Vector2f(bounds.size.x / 2.f, 0.f));
    m_controlsText->setPosition(sf::Vector2f(Constants::WINDOW_WIDTH / 2.f, Constants::WINDOW_HEIGHT - 80.f - 20.f));
}

void HUD::update(const HUDState& hudState)
{
    // Recompute graph-pane geometry each frame so it tracks any panel-size change before drawing + hit-testing.
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
        if (hudState.playerStats && hudState.playerStats->strategyTotal > 0)
        {
            int pct = (hudState.playerStats->strategyHits * 100) / hudState.playerStats->strategyTotal;
            strategyStr = std::to_string(pct) + "%";
        }
        else
        {
            strategyStr = "-";
        }
        m_strategyBox->setValue(strategyStr);
    }

    // Book advisor: show the advised action during the player's turn, and hide it (dash) on the
    // Book advisor feedback (Feature 1): after a completed action show the verdict — green when it matched basic
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

    // Feature 3 analytics stat boxes + chart backing data (left-panel dashboard). updateHUDState() derives all of
    // these live each frame; dash anything not yet measured.
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

    // Recalibrate the per-shoe graphs: when a new shoe begins, clear the accumulated series so the charts show only
    // this shoe's data and never drift persistently upward across shoes. Game also clears its source series at shoe
    // boundaries; keeping the reset here makes the HUD self-contained so the baseline is fresh even if that changes.
    if (hudState.currentShoeId != m_lastShoeId)
    {
        m_bankrollSeries.clear();
        m_edgeSeries.clear();
        m_trueCountPerRound_.clear();
        m_lastShoeId = hudState.currentShoeId;
    }

    // Chart backing data, copied from HUDState so drawCharts can render without the per-frame HUDState lifetime.
    m_bankrollSeries = hudState.bankrollSeries;
    m_edgeSeries = hudState.edgeSeries;
    m_trueCountPerRound_.assign(hudState.trueCountPerRound_.begin(), hudState.trueCountPerRound_.end());
    m_analyticsStdDev = hudState.analyticsStdDev;
    m_longestWinStreak = hudState.longestWinningStreak;
    m_longestLoseStreak = hudState.longestLosingStreak;
    m_edgePercent = hudState.edgePercent;
    if (hudState.playerStats)
        m_betDistribution.assign(hudState.playerStats->betDistribution.begin(),
                                 hudState.playerStats->betDistribution.end());
}

void HUD::draw(sf::RenderWindow& window)
{
    // Highlight whichever graph pane the cursor hovers over so the click-to-change panes read as interactive. Uses the
    // same pixel->world mapping as the count-style click so hover tracks correctly under any active view.
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool hasHover = false;
    sf::Rect<float> hoverRect{};
    for (const auto& pane : graphPaneBounds())
        if (pane.first.contains(mouse)) { hasHover = true; hoverRect = pane.first; break; }

    if (m_showLeft)
        m_leftPanel.draw(window);

    if (m_showRight)
        m_rightPanel.draw(window);

    // Bottom panel always visible
    m_bottomPanel.draw(window);

    // Feature 3: bankroll-curve, edge-tracking and bet-distribution charts on the empty left panel.
    drawCharts(window);

    // Draw controls above bottom panel
    window.draw(*m_controlsText);

    // Interactive hover highlight over the hovered graph pane, drawn last so it sits on top of everything.
    if (hasHover)
    {
        sf::RectangleShape outline(hoverRect.size);
        outline.setPosition(hoverRect.position);
        outline.setFillColor(sf::Color(40, 45, 60, 90));
        outline.setOutlineThickness(3.f);
        outline.setOutlineColor(sf::Color(255, 215, 0));
        window.draw(outline);
    }
}

sf::FloatRect HUD::countTitleBounds() const
{
    // The Counting Style box (e.g. "Counting Style" / "Hi-Lo") is clickable — see InputHandler toggling the
    // counting style. Bounds span both its label and value lines so clicks anywhere on it register.

    return m_countingStyleBox ? m_countingStyleBox->getTitleBounds() : sf::FloatRect{};
}

namespace {

    // The left panel is now dedicated to graphs. These stacked plot rects tile the whole panel (geometry derived
    // from the actual panel bounds so they adapt if the panel size ever changes), reserving a top title strip and a
    // bottom x-axis strip for each chart with modest spacing between them.
    constexpr float kTitleStrip = 16.f;  // height reserved at the top of each chart for its title label
    constexpr float kXAxisStrip = 16.f;  // height reserved at the bottom of each chart for round-number ticks

    std::vector<sf::Rect<float>> leftPanelCharts(const sf::Rect<float>& panel)
    {
        const int chartCount = 3;
        const float margin = 6.f;
        const float gap = 22.f;

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

    // Inner plotting box of a chart rect: inset for the left y-axis gutter and the top/bottom title/x strips.
    sf::Rect<float> plotArea(const sf::Rect<float>& rect)
    {
        const float gutter = 30.f;  // space on the left for right-aligned value labels
        return sf::Rect<float>(sf::Vector2f(rect.position.x + gutter, rect.position.y + kTitleStrip + 4.f),
                               sf::Vector2f(rect.size.x - gutter - 6.f,
                                            rect.size.y - kTitleStrip - 4.f - kXAxisStrip - 4.f));
    }

    // Round a target spacing up to a "nice" step (1/2/5 x power of ten) so axis labels come out clean.
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

    // Which secondary overlay drawLineChart adds on top of its primary line (display-only; toggled per graph pane).
    enum class GraphOverlay { None, BankrollSwings, EdgeTrueCount };

    // Secondary view for the bankroll chart: per-round net-profit swings (delta[i] = equity[i]-equity[i-1]) drawn as
    // sign-colored bars centered on a breakeven line and normalized to the plot height by the largest swing. Scaled
    // independently of the primary axis so it always fills the pane, revealing the volatility hidden inside the smooth
    // equity curve (a large cumulative gain can still be built from volatile up/down rounds). Element type is
    // templated so the float bankroll series binds without conversion.
    template <typename Equity>
    void drawBankrollSwings(sf::RenderWindow& window, const sf::Rect<float>& pbox, const Equity& equity)
    {
        const std::size_t n = equity.size();
        if (n < 2)
            return;

        std::vector<double> swings(n);
        swings[0] = static_cast<double>(equity[0]);
        for (std::size_t i = 1; i < n; ++i) swings[i] = static_cast<double>(equity[i]) - static_cast<double>(equity[i - 1]);

        double maxAbs = 0.0;
        for (double v : swings) maxAbs = std::max(maxAbs, std::fabs(v));
        if (!(maxAbs > 0.0))
            return;

        const float center = pbox.position.y + 0.5f * pbox.size.y;   // breakeven at the vertical middle
        const float halfH = 0.46f * pbox.size.y;                      // bars reach ~46% up/down at max swing
        const float barW = std::max(1.f, pbox.size.x / static_cast<float>(n) * 0.6f);
        auto xCenter = [&](std::size_t i) { return pbox.position.x + pbox.size.x * static_cast<float>(i) / static_cast<float>(n - 1); };

        // Faint breakeven line so "up vs down" is unambiguous.
        sf::VertexArray breakLine(sf::PrimitiveType::Lines, 2);
        breakLine[0].position = sf::Vector2f(pbox.position.x, center);
        breakLine[1].position = sf::Vector2f(pbox.position.x + pbox.size.x, center);
        breakLine[0].color = breakLine[1].color = sf::Color(180, 180, 180, 90);
        window.draw(breakLine);

        for (std::size_t i = 0; i < n; ++i)
        {
            const float barH = static_cast<float>(halfH * std::fabs(swings[i]) / maxAbs);
            sf::Color color = swings[i] > 0.0 ? sf::Color(90, 200, 90) : (swings[i] < 0.0 ? sf::Color(220, 70, 70) : sf::Color(150, 150, 150));
            sf::RectangleShape bar(sf::Vector2f(barW, barH));
            bar.setFillColor(color);
            // Bars grow up from breakeven for wins, down for losses.
            bar.setPosition(swings[i] >= 0.0 ? sf::Vector2f(xCenter(i) - barW * 0.5f, center - barH)
                                             : sf::Vector2f(xCenter(i) - barW * 0.5f, center));
            window.draw(bar);
        }
    }

    // Secondary view for the edge chart: the raw true count per round (the counting input behind the derived edge),
    // drawn as a translucent ribbon + line normalized to its own range so it always reads clearly regardless of how
    // far the cumulative edge has drifted. The most recent value is marked and labelled so the overlay stays
    // interpretable without sharing the primary numeric axis.
    void drawTrueCountLine(sf::RenderWindow& window, const sf::Rect<float>& pbox, const std::vector<double>& counts)
    {
        if (counts.size() < 2)
            return;

        double lo = counts.front(), hi = counts.front();
        for (double v : counts) { lo = std::min(lo, v); hi = std::max(hi, v); }
        if (hi <= lo)
            hi = lo + 1.0;

        const float plotBottom = pbox.position.y + pbox.size.y;
        const std::size_t n = counts.size();
        auto xCenter = [&](std::size_t i) { return pbox.position.x + pbox.size.x * static_cast<float>(i) / static_cast<float>(n - 1); };
        auto yAt = [&](double v) { double frac = (v - lo) / (hi - lo); return plotBottom - static_cast<float>(frac) * pbox.size.y; };

        // Translucent filled ribbon for a sense of the signal's shape.
        sf::VertexArray ribbon(sf::PrimitiveType::TriangleStrip, n * 2);
        for (std::size_t i = 0; i < n; ++i)
        {
            float x = xCenter(i);
            float y = yAt(counts[i]);
            ribbon[i * 2].position     = sf::Vector2f(x, y);
            ribbon[i * 2].color        = sf::Color(120, 220, 220, 55);
            ribbon[i * 2 + 1].position = sf::Vector2f(x, plotBottom);
            ribbon[i * 2 + 1].color    = sf::Color(120, 220, 220, 55);
        }
        window.draw(ribbon);

        // The signal line itself.
        sf::VertexArray line(sf::PrimitiveType::LineStrip, n);
        for (std::size_t i = 0; i < n; ++i) { line[i].position = sf::Vector2f(xCenter(i), yAt(counts[i])); line[i].color = sf::Color(120, 220, 220, 200); }
        window.draw(line);

        // Highlight + label the most recent true count so the overlay is interpretable on its own.
        sf::RectangleShape mark(sf::Vector2f(4.f, 4.f));
        mark.setFillColor(sf::Color(200, 255, 255));
        mark.setPosition(sf::Vector2f(xCenter(n - 1) - 2.f, yAt(counts[n - 1]) - 2.f));
        window.draw(mark);

        std::ostringstream oss;
        oss << "TC now: " << std::fixed << std::setprecision(1) << counts[n - 1];
        sf::Text label(FontManager::get(), oss.str(), 10u);
        label.setFillColor(sf::Color(120, 220, 220));
        sf::FloatRect lb = label.getLocalBounds();
        label.setOrigin(sf::Vector2f(lb.size.x + 4.f, 0.f));
        label.setPosition(sf::Vector2f(pbox.position.x + pbox.size.x - 4.f, pbox.position.y + 2.f));
        window.draw(label);
    }

    // Horizontal grid lines + right-aligned value labels for a y axis spanning [lo, hi], plus a zero baseline when
    // it falls inside the range and the left axis line. Labels sit in the gutter to the left of the data area.
    void drawYAxis(sf::RenderWindow& window, const sf::Rect<float>& pbox, double lo, double hi, int precision)
    {
        if (hi <= lo)
            hi = lo + 1.0;

        const float axisX = pbox.position.x;
        const float plotRight = pbox.position.x + pbox.size.x;
        const float top = pbox.position.y;
        const float bottom = pbox.position.y + pbox.size.y;

        double step = niceStep((hi - lo) / 4.0);
        double axisLo = std::floor(lo / step) * step;
        double axisHi = std::ceil(hi / step) * step;

        for (double v = axisLo; v <= axisHi + step * 0.5; v += step)
        {
            double frac = (v - lo) / (hi - lo);
            float y = bottom + static_cast<float>(frac) * (top - bottom);

            sf::VertexArray grid(sf::PrimitiveType::Lines, 2);
            grid[0].position = sf::Vector2f(axisX, y);
            grid[1].position = sf::Vector2f(plotRight, y);
            grid[0].color = grid[1].color = sf::Color(120, 120, 120, 110);
            window.draw(grid);

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(precision) << v;
            sf::Text label(FontManager::get(), oss.str(), 11u);
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

        if (lo <= 0.0 && hi >= 0.0)
        {
            double frac = (0.0 - lo) / (hi - lo);
            float y = bottom + static_cast<float>(frac) * (top - bottom);
            sf::VertexArray zero(sf::PrimitiveType::Lines, 2);
            zero[0].position = sf::Vector2f(axisX, y);
            zero[1].position = sf::Vector2f(plotRight, y);
            zero[0].color = zero[1].color = sf::Color(150, 150, 150, 180);
            window.draw(zero);
        }
    }

    // Evenly spaced round-number x ticks ("Round n") centered in the bottom strip of a chart.
    void drawXTicks(sf::RenderWindow& window, const sf::Rect<float>& pbox, std::size_t points)
    {
        if (points < 2)
            return;

        const int labelCount = 4;
        const float bottom = pbox.position.y + pbox.size.y;
        for (int i = 0; i < labelCount; ++i)
        {
            double frac = static_cast<double>(i) / static_cast<double>(labelCount - 1);
            float x = pbox.position.x + static_cast<float>(frac) * pbox.size.x;
            long roundNo = 1 + static_cast<long>(std::lround(frac * static_cast<double>(points - 1)));

            sf::VertexArray tick(sf::PrimitiveType::Lines, 2);
            tick[0].position = sf::Vector2f(x, bottom);
            tick[1].position = sf::Vector2f(x, bottom + 4.f);
            tick[0].color = tick[1].color = sf::Color(150, 150, 150, 150);
            window.draw(tick);

            std::ostringstream oss;
            oss << "Round " << roundNo;
            sf::Text label(FontManager::get(), oss.str(), 10u);
            label.setFillColor(sf::Color(160, 160, 160));
            sf::FloatRect lb = label.getLocalBounds();
            label.setOrigin(sf::Vector2f(lb.size.x * 0.5f, lb.size.y));  // bottom-center anchor
            label.setPosition(sf::Vector2f(x, bottom + 4.f));
            window.draw(label);
        }
    }

    template <typename Series>
    void drawLineChart(sf::RenderWindow& window, const sf::Rect<float>& rect, const Series& data,
                       sf::Color lineColor, int precision, const char* title, GraphOverlay overlay,
                       const std::vector<double>& trueCounts)
    {
        sf::RectangleShape bg(sf::Vector2f(rect.size.x - 6.f, rect.size.y - 6.f));
        bg.setPosition(sf::Vector2f(rect.position.x + 3.f, rect.position.y + 3.f));
        bg.setFillColor(sf::Color(20, 25, 40, 130));
        window.draw(bg);

        // Title label stays in the reserved top strip.
        {
            sf::Text titleLabel(FontManager::get(), title, 13u);
            titleLabel.setFillColor(sf::Color(220, 220, 220));
            sf::FloatRect lb = titleLabel.getLocalBounds();
            titleLabel.setOrigin(sf::Vector2f(lb.size.x + 6.f, 0.f));
            titleLabel.setPosition(sf::Vector2f(rect.position.x + 6.f, rect.position.y + 4.f));
            window.draw(titleLabel);
        }

        if (data.size() < 2)
            return;

        const sf::Rect<float> pbox = plotArea(rect);
        double lo = data.front(), hi = data.front();
        for (const auto& v : data) { lo = std::min(lo, static_cast<double>(v)); hi = std::max(hi, static_cast<double>(v)); }
        if (hi <= lo) hi = lo + 1.0;

        drawYAxis(window, pbox, lo, hi, precision);
        drawXTicks(window, pbox, data.size());

        const float plotBottom = pbox.position.y + pbox.size.y;
        auto xAt = [&](std::size_t i) { return pbox.position.x + pbox.size.x * static_cast<float>(i) / static_cast<float>(data.size() - 1); };
        auto yAt = [&](double v) { float frac = static_cast<float>((v - lo) / (hi - lo)); return plotBottom - frac * pbox.size.y; };

        sf::VertexArray line(sf::PrimitiveType::LineStrip, data.size());
        for (std::size_t i = 0; i < data.size(); ++i) { line[i].position = sf::Vector2f(xAt(i), yAt(data[i])); line[i].color = lineColor; }
        window.draw(line);

        // Secondary overlay on top of the primary curve. Scaled independently (see drawBankrollSwings /
        // drawTrueCountLine) so it always reads clearly even though it shares a different unit/scale than the axis.
        if (overlay != GraphOverlay::None)
        {
            if (overlay == GraphOverlay::BankrollSwings)
                drawBankrollSwings(window, pbox, data);
            else
                drawTrueCountLine(window, pbox, trueCounts);

            const char* legend = (overlay == GraphOverlay::BankrollSwings) ? "OVERLAY: per-round swings" : "OVERLAY: true-count signal";
            sf::Text leg(FontManager::get(), legend, 10u);
            leg.setFillColor(sf::Color(240, 220, 120));
            sf::FloatRect bl = leg.getLocalBounds();
            leg.setOrigin(sf::Vector2f(bl.size.x + 5.f, 0.f));
            leg.setPosition(sf::Vector2f(rect.position.x + rect.size.x - 5.f, rect.position.y + 3.f));
            window.draw(leg);
        }
    }

    void drawHistogram(sf::RenderWindow& window, const sf::Rect<float>& rect, const std::vector<std::pair<int, int>>& entries, bool showCumulative)
    {
        sf::RectangleShape bg(sf::Vector2f(rect.size.x - 6.f, rect.size.y - 6.f));
        bg.setPosition(sf::Vector2f(rect.position.x + 3.f, rect.position.y + 3.f));
        bg.setFillColor(sf::Color(20, 25, 40, 130));
        window.draw(bg);

        {
            sf::Text titleLabel(FontManager::get(), "Bets Placed", 13u);
            titleLabel.setFillColor(sf::Color(220, 220, 220));
            sf::FloatRect lb = titleLabel.getLocalBounds();
            titleLabel.setOrigin(sf::Vector2f(lb.size.x + 6.f, 0.f));
            titleLabel.setPosition(sf::Vector2f(rect.position.x + 6.f, rect.position.y + 4.f));
            window.draw(titleLabel);
        }

        if (entries.empty())
            return;
        int maxFreq = 0;
        for (const auto& e : entries) maxFreq = std::max(maxFreq, e.second);
        if (maxFreq == 0)
            return;

        const sf::Rect<float> pbox = plotArea(rect);
        drawYAxis(window, pbox, 0.0, static_cast<double>(maxFreq), 0);

        const float plotBottom = pbox.position.y + pbox.size.y;
        const float barW = pbox.size.x / static_cast<float>(entries.size());

        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            float barH = pbox.size.y * static_cast<float>(entries[i].second) / static_cast<float>(maxFreq);
            sf::RectangleShape bar(sf::Vector2f(std::max(1.f, barW - 2.f), barH));
            bar.setFillColor(sf::Color(255, 215, 0));
            bar.setPosition(sf::Vector2f(pbox.position.x + i * barW, plotBottom - barH));
            window.draw(bar);
        }

        // Cumulative-distribution overlay: frequency summed across bet sizes (already sorted ascending), drawn as a
        // staircase sharing the bars' units, with a marker at the median bet size. Shows where half of all bets sit
        // rather than only the raw per-size histogram. `pbox`/`plotBottom` are in scope from above.
        if (showCumulative)
        {
            long total = 0;
            for (const auto& e : entries) total += e.second;
            if (total > 0)
            {
                const std::size_t count = entries.size();
                std::size_t idx = 0;
                double cum = 0.0;

                sf::VertexArray stair(sf::PrimitiveType::LineStrip, count + 1);
                for (const auto& e : entries)
                {
                    float x = pbox.position.x + pbox.size.x * static_cast<float>(idx + 0.5f) / static_cast<float>(count);
                    cum += e.second;
                    float y = plotBottom - pbox.size.y * static_cast<float>(cum) / static_cast<float>(total);
                    stair[idx].position = sf::Vector2f(x, y);
                    stair[idx].color = sf::Color(230, 160, 240, 220);
                    ++idx;
                }
                {
                    float x = pbox.position.x + pbox.size.x * static_cast<float>(count - 0.5f) / static_cast<float>(count);
                    stair[idx].position = sf::Vector2f(x, plotBottom);
                    stair[idx].color = sf::Color(230, 160, 240, 220);
                }
                window.draw(stair);

                long target = (total + 1) / 2;
                std::size_t mIdx = 0;
                double mcum = 0.0;
                for (; mIdx < count; ++mIdx)
                {
                    mcum += static_cast<double>(entries[mIdx].second);
                    if (mcum >= target) break;
                }
                float xm = pbox.position.x + pbox.size.x * static_cast<float>(mIdx + 0.5f) / static_cast<float>(count);
                float ym = plotBottom - pbox.size.y * static_cast<float>(mcum) / static_cast<float>(total);

                sf::VertexArray med(sf::PrimitiveType::Lines, 2);
                med[0].position = sf::Vector2f(xm, plotBottom);
                med[1].position = sf::Vector2f(xm, ym);
                med[0].color = med[1].color = sf::Color(230, 160, 240, 220);
                window.draw(med);

                sf::RectangleShape dot(sf::Vector2f(5.f, 5.f));
                dot.setFillColor(sf::Color(235, 175, 245));
                dot.setPosition(sf::Vector2f(xm - 2.5f, ym - 2.5f));
                window.draw(dot);

                std::ostringstream oss;
                oss << entries[mIdx].first;
                sf::Text label(FontManager::get(), oss.str(), 10u);
                label.setFillColor(sf::Color(230, 160, 240));
                sf::FloatRect lb = label.getLocalBounds();
                label.setOrigin(sf::Vector2f(lb.size.x * 0.5f, -6.f));
                label.setPosition(sf::Vector2f(xm, ym - 6.f));
                window.draw(label);
            }

            sf::Text leg(FontManager::get(), "OVERLAY: cumulative distribution", 10u);
            leg.setFillColor(sf::Color(230, 160, 240));
            sf::FloatRect bl = leg.getLocalBounds();
            leg.setOrigin(sf::Vector2f(bl.size.x + 5.f, 0.f));
            leg.setPosition(sf::Vector2f(rect.position.x + rect.size.x - 5.f, rect.position.y + 3.f));
            window.draw(leg);
        }

        // A few bet-size x labels (first / middle / last) centered under the bars.
        const int labelCount = 3;
        for (int i = 0; i < labelCount; ++i)
        {
            double frac = static_cast<double>(i) / static_cast<double>(labelCount - 1);
            std::size_t idx = static_cast<std::size_t>(std::lround(frac * static_cast<double>(entries.size() - 1)));
            float x = pbox.position.x + static_cast<float>(frac) * pbox.size.x;
            const float bottom = pbox.position.y + pbox.size.y;

            sf::VertexArray tick(sf::PrimitiveType::Lines, 2);
            tick[0].position = sf::Vector2f(x, bottom);
            tick[1].position = sf::Vector2f(x, bottom + 4.f);
            tick[0].color = tick[1].color = sf::Color(150, 150, 150, 150);
            window.draw(tick);

            std::ostringstream oss;
            oss << entries[idx].first;
            sf::Text label(FontManager::get(), oss.str(), 10u);
            label.setFillColor(sf::Color(160, 160, 160));
            sf::FloatRect lb = label.getLocalBounds();
            label.setOrigin(sf::Vector2f(lb.size.x * 0.5f, lb.size.y));
            label.setPosition(sf::Vector2f(x, bottom + 4.f));
            window.draw(label);
        }
    }

} // namespace

void HUD::drawCharts(sf::RenderWindow& window) const
{
    if (!m_bankrollSeries.empty())
        drawLineChart(window, m_bankrollChartRect, m_bankrollSeries, sf::Color(90, 200, 90), 0, "Bankroll Curve",
                      m_bankrollShowSecondary ? GraphOverlay::BankrollSwings : GraphOverlay::None, m_trueCountPerRound_);
    if (!m_edgeSeries.empty())
        drawLineChart(window, m_edgeChartRect, m_edgeSeries, sf::Color(120, 170, 255), 1, "Edge Over Time",
                      m_edgeShowSecondary ? GraphOverlay::EdgeTrueCount : GraphOverlay::None, m_trueCountPerRound_);
    if (!m_betDistribution.empty())
        drawHistogram(window, m_betsChartRect, m_betDistribution, m_betsShowSecondary);
}

void HUD::toggleGraphSecondary(GraphPane pane)
{
    // Display-only toggle: flip which secondary overlay is drawn on the clicked graph pane. The overlay data is always
    // available (copied into the HUD each frame), so this only decides whether it is shown — a stray click can't
    // corrupt game state. Hovering doesn't change anything either.
    switch (pane)
    {
    case GraphPane::Bankroll: m_bankrollShowSecondary = !m_bankrollShowSecondary; break;
    case GraphPane::Edge:     m_edgeShowSecondary     = !m_edgeShowSecondary;     break;
    case GraphPane::Bets:     m_betsShowSecondary     = !m_betsShowSecondary;     break;
    }
}

std::vector<std::pair<sf::Rect<float>, HUD::GraphPane>> HUD::graphPaneBounds() const
{
    // Draw order (top to bottom): Bankroll, Edge, Bets. Hit-testing returns them in that order so the topmost pane is
    // checked first; the panes don't overlap, so a click matches at most one.
    return {
        {m_bankrollChartRect, GraphPane::Bankroll},
        {m_edgeChartRect,     GraphPane::Edge},
        {m_betsChartRect,     GraphPane::Bets},
    };
}

void HUD::layoutLeftPanels()
{
    // Recompute the three graph-pane rects from the real left-panel bounds so they track any panel-size change and
    // stay correct for both drawing and InputHandler click hit-testing.
    const auto charts = leftPanelCharts(m_leftPanel.bounds());
    m_bankrollChartRect = charts[0];
    m_edgeChartRect     = charts[1];
    m_betsChartRect     = charts[2];
}

