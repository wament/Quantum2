#include "HUD.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <Constants.hpp>
#include "FontManager.hpp"

// ---------------------------------------------------------------------------
// Feature 3: analytics chart helpers (SFML primitives). Kept in an anonymous
// namespace so they are local to this translation unit.
// ---------------------------------------------------------------------------
namespace {

// Draw a section title centered over a band and return the inner plot rectangle (below the title).
sf::FloatRect drawChartTitle(sf::RenderWindow &window, const std::string &title,
                             sf::FloatRect band, float insetX)
{
    sf::Text text(FontManager::get(), title, 13.f);
    text.setFillColor(sf::Color(255, 215, 0));
    sf::FloatRect tb = text.getLocalBounds();
    text.setOrigin(sf::Vector2f(tb.size.x / 2.f, 0.f));
    text.setPosition(sf::Vector2f(band.position.x + band.size.x / 2.f, band.position.y + 4.f));
    window.draw(text);
    return sf::FloatRect({band.position.x + insetX, band.position.y + 22.f},
                         {band.size.x - 2.f * insetX, band.size.y - 26.f});
}

void drawAxisLine(sf::RenderWindow &window, const sf::Vector2f &a, const sf::Vector2f &b, const sf::Color &c)
{
    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = a;
    line[0].color = c;
    line[1].position = b;
    line[1].color = c;
    window.draw(line);
}

// Line chart over a 1D series (value per index). Y is auto-scaled; a zero baseline is drawn when in range.
void drawLineChart(sf::RenderWindow &window, const sf::FloatRect &plot,
                   const std::vector<double> &y, const sf::Color &color)
{
    std::size_t n = y.size();
    if (n < 2)
        return;

    double lo = y[0], hi = y[0];
    for (double v : y)
    {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    double span = hi - lo;
    if (span <= 0.0)
    {
        lo -= 1.0;
        hi += 1.0; // flat series -> give it a sliver of height so the line is visible
        span = 2.0;
    }

    const float x0 = plot.position.x, y0 = plot.position.y;
    const float w = plot.size.x, h = plot.size.y;
    auto px = [&](std::size_t i)
    { return x0 + (n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) * w : 0.f); };
    auto py = [&](double v)
    { return y0 + (1.0f - static_cast<float>((v - lo) / span)) * h; };

    if (lo <= 0.0 && hi >= 0.0)
        drawAxisLine(window, {x0, py(0.0)}, {x0 + w, py(0.0)}, sf::Color(120, 120, 120));

    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f", hi);
    sf::Text topText(FontManager::get(), buf, 9.f);
    topText.setFillColor(sf::Color(170, 170, 170));
    topText.setPosition(sf::Vector2f(x0 + 2.f, y0 - 3.f));
    window.draw(topText);
    std::snprintf(buf, sizeof buf, "%.0f", lo);
    sf::Text botText(FontManager::get(), buf, 9.f);
    botText.setFillColor(sf::Color(170, 170, 170));
    botText.setPosition(sf::Vector2f(x0 + 2.f, py(lo) - 9.f));
    window.draw(botText);

    sf::VertexArray series(sf::PrimitiveType::Lines, n);
    for (std::size_t i = 0; i < n; ++i)
    {
        series[i].position = sf::Vector2f(px(i), py(y[i]));
        series[i].color = color;
    }
    window.draw(series);
}

// Histogram of bucket counts; bars are scaled to the tallest bucket.
void drawHistogram(sf::RenderWindow &window, const sf::FloatRect &plot,
                   const std::vector<int> &buckets, const sf::Color &color)
{
    std::size_t n = buckets.size();
    if (n == 0)
        return;

    int maxCount = 0;
    for (int c : buckets)
        maxCount = std::max(maxCount, c);
    if (maxCount <= 0)
        return;

    const float x0 = plot.position.x, y0 = plot.position.y;
    const float w = plot.size.x, h = plot.size.y;
    const float gap = 2.f;
    const float bw = (w - gap * (n + 1)) / n;
    for (std::size_t i = 0; i < n; ++i)
    {
        float barH = static_cast<float>(buckets[i]) / maxCount * h;
        sf::RectangleShape bar(sf::Vector2f(std::max(1.f, bw - gap), barH));
        bar.setFillColor(color);
        bar.setPosition(sf::Vector2f(x0 + gap + i * (bw + gap), y0 + h - barH));
        window.draw(bar);
    }
}

}  // namespace

HUD::HUD()
{
}

HUD::~HUD()
{
    // StatBox pointers are owned by Panels and cleaned up there

    if (m_sliderLabel)
        delete m_sliderLabel;
    if (m_controlsText)
        delete m_controlsText;
    if (m_strategyHintText)
        delete m_strategyHintText;
}

void HUD::initialize(sf::Font &font)
{
    // Initialize all panels with the font
    m_leftPanel.initialize(font);
    m_rightPanel.initialize(font);
    m_bottomPanel.initialize(font);

    // Initialize slider label once with the shared font
    m_sliderLabel = new sf::Text(FontManager::get(), "", static_cast<unsigned int>(Constants::HUD_FONT_SIZE));
    m_sliderLabel->setFillColor(sf::Color::White);

    const float BOTTOM_HEIGHT = 80.f;

    // Panels are flush against screen edges (no external margin)

    // Bottom panel: full width, anchored to bottom
    m_bottomPanel.setBounds(
        sf::Vector2f(0.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT),
        sf::Vector2f(Constants::WINDOW_WIDTH, BOTTOM_HEIGHT));

    // Left panel: flush left, from top to above bottom panel
    m_leftPanel.setBounds(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));

    // Right panel: flush right, from top to above bottom panel
    m_rightPanel.setBounds(
        sf::Vector2f(Constants::WINDOW_WIDTH - 200.f, 0.f),
        sf::Vector2f(200.f, Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT));

    // Bottom panel stat boxes
    m_bankrollBox = m_bottomPanel.addStatBox("Bankroll");
    m_bankrollBox->initialize(font, "Bankroll");

    m_betBox = m_bottomPanel.addStatBox("Bet");
    m_betBox->initialize(font, "Bet");

    m_statusBox = m_bottomPanel.addStatBox("Status");
    m_statusBox->initialize(font, "Status");

    // New counting/state boxes
    m_countBox = m_bottomPanel.addStatBox("Count");
    m_countBox->initialize(font, "Count");

    m_trueCountBox = m_bottomPanel.addStatBox("True Count");
    m_trueCountBox->initialize(font, "True Count");

    m_cardsRemainingBox = m_bottomPanel.addStatBox("Cards Left");
    m_cardsRemainingBox->initialize(font, "Cards Left");

    m_streakBox = m_bottomPanel.addStatBox("Streak");
    m_streakBox->initialize(font, "Streak");

    m_strategyBox = m_bottomPanel.addStatBox("Strategy");
    m_strategyBox->initialize(font, "Strategy");

    // Stat box positions use PANEL_MARGIN relative to bottom panel's top-left
    float panelX = Constants::PANEL_MARGIN + 5.f;
    float panelY = Constants::WINDOW_HEIGHT - BOTTOM_HEIGHT + Constants::PANEL_MARGIN;

    m_bankrollBox->setPosition(sf::Vector2f(panelX, panelY));
    m_betBox->setPosition(sf::Vector2f(panelX + 110.f, panelY));
    m_statusBox->setPosition(sf::Vector2f(panelX + 220.f, panelY));
    m_countBox->setPosition(sf::Vector2f(panelX + 330.f, panelY));
    m_trueCountBox->setPosition(sf::Vector2f(panelX + 440.f, panelY));
    m_cardsRemainingBox->setPosition(sf::Vector2f(panelX + 560.f, panelY));
    m_streakBox->setPosition(sf::Vector2f(panelX + 680.f, panelY));
    m_strategyBox->setPosition(sf::Vector2f(panelX + 790.f, panelY));

    // Feature 1 & 3: populate the left panel with an always-visible analytics dashboard. Boxes are stacked
    // vertically; their values are refreshed every frame in HUD::update from hudState.playerStats (and the
    // precomputed hand standard deviation). The first box is a section header. Box order must match the
    // value vector rebuilt in update().
    const float leftX = Constants::PANEL_MARGIN + 6.f;
    static const char* const leftTitles[] = {
        "ANALYTICS",       // 0 - section header (no value)
        "Win %",           // 1
        "Deviations",      // 2 - Feature 1: times deviated from basic strategy
        "Wins/Dev",        // 3 - Feature 1: wins after deviating
        "StDev/hand",      // 4 - Feature 3: standard deviation of per-hand profit
        "Win Streak",      // 5 - Feature 3: longest winning streak
        "Loss Streak",     // 6 - Feature 3: longest losing streak
        "Best Hand",       // 7 - Feature 3: greatest single-hand win
        "Worst Hand",      // 8 - Feature 3: greatest single-hand loss (<= 0)
        "Shoe \xc2\xb1",   // 9 - Feature 3: best/worst cumulative shoe result (plus/minus sign)
        "Session \xc2\xb1" // 10 - Feature 3: best/worst session cumulative result
    };
    float leftY = Constants::PANEL_MARGIN;
    for (const char* title : leftTitles)
    {
        StatBox* box = m_leftPanel.addStatBox(title);
        box->initialize(FontManager::get(), title);
        box->setPosition(sf::Vector2f(leftX, leftY));
        m_leftBoxes.push_back(box);
        leftY += 42.f;
    }

    // Slider: use PANEL_MARGIN relative to bottom panel as well
    const float sliderX = panelX + 800.f;
    const float sliderY = panelY + 32.f;
    const float sliderWidth = 350.f;
    const float sliderHeight = 14.f;
    riskSliderBounds = sf::FloatRect({sliderX, sliderY}, {sliderWidth, sliderHeight});

    // Initialize controls text
    updateControls(font);

    // Initialize strategy hint text
    m_strategyHintText = new sf::Text(font, "", 20u);
    m_strategyHintText->setFillColor(sf::Color(255, 215, 0));
    m_strategyHintText->setPosition(sf::Vector2f(Constants::WINDOW_WIDTH / 2.f - 100.f, 20.f));


}

void HUD::updateControls(sf::Font &font)
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

void HUD::update(const HUDState &hudState)
{
    if (m_bankrollBox)
        m_bankrollBox->setValue(std::to_string(hudState.bankroll));

    if (m_betBox)
        m_betBox->setValue(std::to_string(hudState.currentBet));

    if (m_statusBox)
    {
        m_statusBox->setValue(
            hudState.statusMessage.empty() ? "Playing" : hudState.statusMessage);
    }

    // Counting info from HUDState
    if (m_countBox)
        m_countBox->setValue(std::to_string(hudState.runningCount));

    if (m_trueCountBox)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << hudState.trueCount << " (" << hudState.countingStyleNameStr << ")";
        m_trueCountBox->setValue(oss.str());
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
            if (FontManager::hasUnicodeSupport())
              streakStr += hudState.streakIsWin ? "★" : "✗";
          else
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

    // Sync slider state from HUDState
    sliderValue = hudState.kellyFraction;
    sliderDragging = hudState.riskSliderDragging;

    // Feature 3: snapshot the stats pointer so drawAnalyticsCharts can render against this frame's data.
    m_analyticsStats = hudState.playerStats;

    // Feature 1 & 3: refresh the left-panel analytics dashboard values. Box order must match m_leftTitles.
    if (hudState.playerStats && !m_leftBoxes.empty())
    {
        const PlayerStats& s = *hudState.playerStats;
        int decisions = s.wins + s.losses + s.pushes;
        bool hasHands = s.handsFinished > 0;

        char numBuf[64];
        std::vector<std::string> vals(m_leftBoxes.size());
        vals[0] = ""; // header carries no value
        vals[1] = decisions > 0 ? (std::to_string((100 * s.wins) / decisions) + "%") : "-";
        vals[2] = std::to_string(s.strategyDeviations);                    // Feature 1
        vals[3] = hasHands ? std::to_string(s.strategyDeviationWins) : "-"; // Feature 1
        std::snprintf(numBuf, sizeof numBuf, "%.1f", hudState.handStandardDeviation);
        vals[4] = hasHands ? numBuf : "-";                                 // Feature 3
        vals[5] = std::to_string(s.longestWinningStreak);                  // Feature 3
        vals[6] = std::to_string(s.longestLosingStreak);                   // Feature 3
        vals[7] = hasHands ? (std::string(s.greatestHand > 0 ? "+" : "") + std::to_string(s.greatestHand)) : "-"; // Feature 3: Best Hand
        vals[8] = hasHands ? (std::string(s.greatestSingleHandLoss > 0 ? "+" : "") + std::to_string(s.greatestSingleHandLoss)) : "-"; // Worst Hand
        {
            char a[64], b[64];
            std::snprintf(a, sizeof a, "%+d", s.greatestWin);
            std::snprintf(b, sizeof b, "%+d", s.greatestLoss);
            vals[9] = hasHands ? (std::string(a) + " / " + b) : "-";       // Feature 3: Shoe Best/Worst
        }
        {
            char a[64], b[64];
            std::snprintf(a, sizeof a, "%+d", s.greatestSessionWin);
            std::snprintf(b, sizeof b, "%+d", s.greatestSessionLoss);
            vals[10] = hasHands ? (std::string(a) + " / " + b) : "-";      // Feature 3: Session Best/Worst
        }
        for (std::size_t i = 0; i < m_leftBoxes.size(); ++i)
            m_leftBoxes[i]->setValue(vals[i]);
    }

    // Update strategy hint
    if (m_strategyHintText)
    {
        // Feature 1: show the transient green/red result after an action; otherwise the standing advice.
        if (hudState.advisorFeedbackTtl > 0 && !hudState.advisorFeedbackText.empty())
            m_strategyHint = hudState.advisorFeedbackText;
        else
            m_strategyHint = hudState.basicStrategyHint;

        m_strategyHintText->setString(m_strategyHint);

        // Tint the feedback green (matched strategy) or red (deviated) while it is visible, gold otherwise.
        if (hudState.advisorFeedbackTtl > 0)
            m_strategyHintText->setFillColor(
                hudState.advisorResult == 'C' ? sf::Color(80, 220, 120) : sf::Color(230, 80, 80));
        else
            m_strategyHintText->setFillColor(sf::Color(255, 215, 0));
    }



}

void HUD::draw(sf::RenderWindow &window)
{
    if (m_showLeft)
        m_leftPanel.draw(window);

    if (m_showRight)
        m_rightPanel.draw(window);

    // Bottom panel always visible
    m_bottomPanel.draw(window);

    drawRiskSlider(window);

    // Draw controls above bottom panel
    window.draw(*m_controlsText);
    // Draw strategy hint if present
    if (m_strategyHintText && !m_strategyHint.empty())
    {
        window.draw(*m_strategyHintText);
    }
}

const sf::FloatRect &HUD::getRiskSliderBounds() const
{
    return riskSliderBounds;
}

void HUD::drawRiskSlider(sf::RenderWindow &window)
{
    if (riskSliderBounds.size.x <= 0.f || riskSliderBounds.size.y <= 0.f)
        return;
    sf::Vector2f pos = riskSliderBounds.position;
    sf::Vector2f size = riskSliderBounds.size;

    // Track background
    sf::RectangleShape track;
    track.setPosition(pos);
    track.setSize(size);
    track.setFillColor(sf::Color(30, 30, 30));
    track.setOutlineColor(sf::Color(100, 100, 100));
    track.setOutlineThickness(1.f);
    window.draw(track);

    // Filled portion
    float fillWidth = riskSliderBounds.size.x * sliderValue;
    sf::RectangleShape fill;
    fill.setPosition(pos);
    fill.setSize(sf::Vector2f(fillWidth, size.y));
    fill.setFillColor(sliderDragging ? sf::Color(50, 180, 255) : sf::Color(0, 160, 200));
    window.draw(fill);

    // Thumb
    float thumbX = pos.x + fillWidth - 6.f;
    sf::CircleShape thumb(7.f);
    thumb.setPosition(sf::Vector2f(thumbX, pos.y - 2.f));
    thumb.setFillColor(sf::Color::White);
    thumb.setOutlineColor(sf::Color(0, 140, 180));
    thumb.setOutlineThickness(2.f);
    window.draw(thumb);

    // Calculate Kelly % and RoR
    float kellyPct = sliderValue * 100.f;
    float rorApprox = 0.f;
    if (sliderValue > 0.0f)
    {
        rorApprox = 0.135f * sliderValue * sliderValue * 4.f;
        if (rorApprox > 0.9f)
            rorApprox = 0.9f;
    }

    // Build label string
    std::ostringstream labelStr;
    labelStr << "Kelly: " << static_cast<int>(kellyPct) << "%";
    if (kellyPct > 50.f)
    {
        labelStr << " (~" << static_cast<int>(rorApprox * 100.f) << "% RoR) PRESS IT!";
    }
    else
    {
        labelStr << " (~" << static_cast<int>(rorApprox * 100.f) << "% RoR)";
    }

    // Update persistent label
    m_sliderLabel->setString(labelStr.str());

    // Color: red when above 50%, white otherwise
    if (kellyPct > 50.f)
    {
        m_sliderLabel->setFillColor(sf::Color(255, 60, 60));
    }
    else
    {
        m_sliderLabel->setFillColor(sf::Color::White);
    }

    sf::FloatRect lb = m_sliderLabel->getLocalBounds();
    m_sliderLabel->setOrigin(sf::Vector2f(lb.size.x / 2.f, 0.f));
    m_sliderLabel->setPosition(sf::Vector2f(pos.x + size.x / 2.f, pos.y - 32.f));
    window.draw(*m_sliderLabel);
}

void HUD::drawAnalyticsCharts(sf::RenderWindow &window)
{
    if (!m_showRight || m_analyticsStats == nullptr)
        return;
    const PlayerStats& s = *m_analyticsStats;

    // Right panel region (matches the bounds set in HUD::initialize).
    const float rx = Constants::WINDOW_WIDTH - 200.f;
    const float ry = 0.f;
    const float rw = 200.f;
    const float rh = Constants::WINDOW_HEIGHT - 80.f;
    const float bandH = rh / 3.f;
    const float insetX = 24.f;

    // Bankroll curve over time (line chart of the human bankroll sampled per round).
    if (!s.bankrollCurve.empty())
    {
        sf::FloatRect plot = drawChartTitle(window, "Bankroll Curve",
                                            sf::FloatRect({rx, ry}, {rw, bandH}), insetX);
        std::vector<double> curve(s.bankrollCurve.begin(), s.bankrollCurve.end());
        drawLineChart(window, plot, curve, sf::Color(80, 220, 120));
    }

    // Edge tracking over time (cumulative human profit driven by count-based betting).
    if (s.edgeHistory.size() >= 2)
    {
        sf::FloatRect plot = drawChartTitle(window, "Edge Over Time",
                                            sf::FloatRect({rx, ry + bandH}, {rw, bandH}), insetX);
        std::vector<double> profit;
        profit.reserve(s.edgeHistory.size());
        for (const auto& e : s.edgeHistory)
            profit.push_back(e.cumulativeProfit);
        drawLineChart(window, plot, profit, sf::Color(90, 180, 255));
    }

    // Bet-size distribution across the session (histogram).
    if (!s.betBuckets.empty())
    {
        sf::FloatRect plot = drawChartTitle(window, "Bet Distribution",
                                            sf::FloatRect({rx, ry + 2.f * bandH}, {rw, bandH}), insetX);
        drawHistogram(window, plot, s.betBuckets, sf::Color(255, 215, 0));
    }
}

void HUD::toggleLeftPanel()
{
    m_showLeft = !m_showLeft;
}

void HUD::toggleRightPanel()
{
    m_showRight = !m_showRight;
}

bool HUD::leftVisible() const
{
    return m_showLeft;
}

bool HUD::rightVisible() const
{
    return m_showRight;
}