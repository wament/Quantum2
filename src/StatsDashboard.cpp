#include "StatsDashboard.hpp"

#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <array>
#include <functional>

#include "Hand.hpp"
#include "GameRecord.hpp"   // computeCareerStats: single record-derived source of truth for dashboard career totals
#include "ChartPrimitives.hpp"   // the Progress tab's geometry: pure, window-free, headless-testable
#include "FontManager.hpp"

using json = nlohmann::json;

namespace
{
    // Stream any arithmetic value to its canonical textual form. Lets heterogeneous CSV metric fields (ints + floats) share one
    // formatting path so each row can be a plain {name, string} pair; the bytes match streaming the field directly into os.
    template <typename T>
    std::string csvValue(const T& v)
    {
        std::ostringstream os;
        os << v;
        return os.str();
    }

    // Reconstruct a Hand from its recorded cards
    // game scores it), rather than re-implementing card-value logic. addCard() only pushes back
    Hand handFromCards(const std::vector<Card>& cards)
    {
        Hand h;
        for (const auto& c : cards)
            h.addCard(c);
        return h;
    }

    // The rect this dashboard draws INTO, in WORLD units.
    //
    // Game::gameView is a FIXED 1600x900 world rect whose viewport is letterboxed into the OS window
    // (resizeView), so window pixels and world units coincide at exactly ONE size, 1600x900. The
    // renderer's own rule (GameRenderer.cpp, U11) is "draw in WORLD units ... every layout call site
    // must use [the view], never window.getSize()". This file did not follow it: at 900x560 the whole
    // panel drew at 56% scale into the top-left of the window, with the right 394px and bottom 222px
    // showing backdrop -- U3's defect class, which the menu/achievements/rules screens were moved off
    // and the dashboard never was. The view the window is currently drawing with IS the world, so its
    // size is the layout basis; the guard mirrors worldSizeOf's for a not-yet-created view.
    sf::Vector2f worldDrawSize(const sf::RenderWindow& window)
    {
        const sf::Vector2f view = window.getView().getSize();
        return (view.x > 0.0f && view.y > 0.0f)
                   ? view
                   : sf::Vector2f(static_cast<float>(window.getSize().x),
                                  static_cast<float>(window.getSize().y));
    }

    // Centered title text with its origin at the top edge
    sf::Text centeredTitle(sf::RenderWindow& window, const std::string& label, unsigned int size, float y, const sf::Color& color)
    {
        sf::Text t(FontManager::get(), label, size);
        t.setFillColor(color);
        const auto bounds = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, 0.f));
        t.setPosition(sf::Vector2f(worldDrawSize(window).x / 2.0f, y));
        return t;
    }

    // The recorded-shoe vector for one Progress-tab source. One function, so the chart, the populated
    // probe and any future reader cannot disagree about which vector a source names.
    const std::vector<ShoeRecord>& shoesForSource(const StatsDashboard::ProgressData& d, historybrowser::Source source)
    {
        switch (source)
        {
        case historybrowser::Source::Daily:
            return d.daily;
        case historybrowser::Source::Tournament:
            return d.tournament;
        case historybrowser::Source::Career:
            break;
        }
        return d.career;
    }
}

void StatsDashboard::feed(const PlayerStats& career, const std::vector<RoundRecord>& rounds)
{
    computed_ = ComputedStats{};
    // Career-context numbers that aren't record-derived still come straight from the saved profile (splits/doubles/shoesFinished).
    computed_.shoesFinished = career.shoesFinished;
    computed_.splits = career.splits;
    computed_.doubles = career.doubles;

    long long cumulative = 0;   // running bankroll proxy built from successive round net profits
    int peak = 0;               // highest cumulative value reached (baseline is 0)
    int worstDrawdown = 0;      // magnitude of the deepest peak-to-current decline (>= 0)

    for (const auto& round : rounds)
    {
        const int roundProfit = round.netProfit();   // sum of every playerHand's net across the round
        computed_.perRoundProfit.push_back(roundProfit);
        cumulative += roundProfit;
        computed_.bankrollSeries.push_back(cumulative);

        if (cumulative > peak)
            peak = static_cast<int>(cumulative);
        const int drawdown = peak - static_cast<int>(cumulative);
        if (drawdown > worstDrawdown)
            worstDrawdown = drawdown;
    }

    // Career totals come from the single record-derived source of truth: computeCareerStats. This removes the duplicate
    // win/loss/push/blackjack/handsFinished/greatest-hand recompute that used to live here and makes the dashboard match the
    // profile's human-seat accounting exactly
    // flag surrender, so a pushed hand with won=true was counted as a win). busts is chart-level card detection kept out of
    // PlayerStats; per-round bankroll/profit stay here because they're round-over-seat chart data, not career totals.
    ShoeRecord shoe;
    shoe.rounds = rounds;
    const PlayerStats cs = computeCareerStats(shoe);
    computed_.wins = cs.wins;
    computed_.losses = cs.losses;
    computed_.pushes = cs.pushes;
    computed_.blackjacks = cs.blackjacks;
    computed_.handsFinished = cs.handsFinished;
    computed_.greatestHandProfit = cs.greatestHand;
    computed_.worstHandLoss = cs.greatestHandLoss;

    for (const auto& round : rounds)
        for (const auto& hand : round.playerHands)
            if (handFromCards(hand.cards).isBust())
                computed_.busts++;

    const int decisive = computed_.wins + computed_.losses;
    computed_.winRate = decisive > 0 ? static_cast<double>(computed_.wins) / decisive : 0.0;
    computed_.bjFrequency = cs.handsFinished > 0 ? static_cast<double>(cs.blackjacks) / cs.handsFinished : 0.0;
    computed_.bustFrequency = cs.handsFinished > 0 ? static_cast<double>(computed_.busts) / cs.handsFinished : 0.0;
    if (career.strategyTotal > 0)
        computed_.strategyAccuracy = static_cast<double>(career.strategyHits) / career.strategyTotal;

    computed_.peakBankroll = peak;
    computed_.maxDrawdown = worstDrawdown;
}

std::string StatsDashboard::toJson() const
{
    json j = {
        {"handsFinished", computed_.handsFinished},
        {"wins", computed_.wins},
        {"losses", computed_.losses},
        {"pushes", computed_.pushes},
        {"blackjacks", computed_.blackjacks},
        {"busts", computed_.busts},
        {"winRate", computed_.winRate},
        {"bjFrequency", computed_.bjFrequency},
        {"bustFrequency", computed_.bustFrequency},
        {"strategyAccuracy", computed_.strategyAccuracy},
        {"shoesFinished", computed_.shoesFinished},
        {"splits", computed_.splits},
        {"doubles", computed_.doubles},
        {"greatestHandProfit", computed_.greatestHandProfit},
        {"worstHandLoss", computed_.worstHandLoss},
        {"peakBankroll", computed_.peakBankroll},
        {"maxDrawdown", computed_.maxDrawdown},
        {"perRoundProfit", computed_.perRoundProfit},
        {"bankrollSeries", computed_.bankrollSeries}
    };
    return j.dump(2);
}

std::string StatsDashboard::toCsv() const
{
    std::ostringstream os;
    os << "metric,value\n";
    // One pre-formatted line per metric, mirroring toJson()'s data-driven table instead of 17 near-identical `os << name,value`
    // lines. Each string is the row already assembled ("name," + streamed value)
    // path; output is byte-for-byte identical to streaming the field directly into os.
    const std::array<std::string, 17> rows = {
        "hands_finished," + csvValue(computed_.handsFinished),
        "wins," + csvValue(computed_.wins),
        "losses," + csvValue(computed_.losses),
        "pushes," + csvValue(computed_.pushes),
        "blackjacks," + csvValue(computed_.blackjacks),
        "busts," + csvValue(computed_.busts),
        "win_rate," + csvValue(computed_.winRate),
        "bj_frequency," + csvValue(computed_.bjFrequency),
        "bust_frequency," + csvValue(computed_.bustFrequency),
        "strategy_accuracy," + csvValue(computed_.strategyAccuracy),
        "shoes_finished," + csvValue(computed_.shoesFinished),
        "splits," + csvValue(computed_.splits),
        "doubles," + csvValue(computed_.doubles),
        "greatest_hand_profit," + csvValue(computed_.greatestHandProfit),
        "worst_hand_loss," + csvValue(computed_.worstHandLoss),
        "peak_bankroll," + csvValue(computed_.peakBankroll),
        "max_drawdown," + csvValue(computed_.maxDrawdown)
    };
    for (const auto& line : rows)
        os << line << '\n';

    os << "\nround_index,net_profit,cumulative_bankroll\n";
    for (std::size_t i = 0; i < computed_.perRoundProfit.size(); ++i)
        os << i << ',' << computed_.perRoundProfit[i] << ',' << computed_.bankrollSeries[i] << "\n";
    return os.str();
}

// ---------------------------------------------------------------------------
// Progress tab: the two axes, kept apart.
//
// The top chart is the ACROSS-SHOES axis and always names the source it reads (Career / Daily
// Challenge / Tournament) -- the rule MS1 created: career longHistory is Regular-only, so a chart that
// reads "the history" without saying which one silently omits Daily and tournament play.
// The bottom chart is the WITHIN-WINDOW axis (U12's per-shoe vs per-session) and always names its
// scope. The two axes are on separate controls; neither is ever offered as an option of the other.
// ---------------------------------------------------------------------------
void StatsDashboard::setProgressData(const ProgressData& data)
{
    progress_ = data;
    // Opening lands on the first NON-EMPTY source, exactly like the Replay browser: a chart that opens
    // on an empty list reads as a broken feature. Career is preferred when it has shoes.
    progressSource_ = historybrowser::preferredOpen(historybrowser::Source::Career, progressPopulated());
    progressSession_ = false;
}

historybrowser::Populated StatsDashboard::progressPopulated() const
{
    historybrowser::Populated populated{};
    populated[static_cast<int>(historybrowser::Source::Career)] = !progress_.career.empty();
    populated[static_cast<int>(historybrowser::Source::Daily)] = !progress_.daily.empty();
    populated[static_cast<int>(historybrowser::Source::Tournament)] = !progress_.tournament.empty();
    return populated;
}

void StatsDashboard::cycleProgressSource()
{
    // Skipping empty sources is the point: the three histories are populated by three different session
    // kinds, so "exactly one is non-empty" is the common case. When nothing else has shoes this returns
    // the current source unchanged, which makes the keypress a no-op rather than a flicker.
    progressSource_ = historybrowser::nextNonEmpty(progressSource_, progressPopulated());
}

std::vector<double> StatsDashboard::progressCareerSeries() const
{
    // The one function that maps a source to a vector, shared with the populated probe.
    return chart::careerSeries(shoesForSource(progress_, progressSource_));
}

std::vector<double> StatsDashboard::progressLiveSeries() const
{
    return progressSession_ ? progress_.sessionBankroll : progress_.shoeBankroll;
}

const char* StatsDashboard::progressLiveLabel() const
{
    // ASCII only: this reaches sf::Text (U13 -- SFML 3 decodes std::string as ANSI).
    return progressSession_ ? "Bankroll this session (one open play window)"
                            : "Bankroll this shoe (resets at the shoe boundary)";
}

const char* StatsDashboard::progressSourceLabel() const
{
    return historybrowser::label(progressSource_);
}

void StatsDashboard::update(float deltaTime)
{
    // Frame pacing for AppMode parity (mirrors Tournament / StrategyTrainer). Pure-logic tests never call this on the
    // deterministic path, so it only accumulates a timer here; tab switching is driven by InputHandler instead.
    frameTimer_ += deltaTime;
}

void StatsDashboard::advanceTab(bool forward)
{
    const int count = static_cast<int>(Tab::Progress) + 1;   // Overview=0, Hands=1, Profits=2, Progress=3
    int idx = (static_cast<int>(tab_) + (forward ? 1 : -1) + count) % count;
    tab_ = static_cast<Tab>(idx);
}

// ---------------------------------------------------------------------------
// Presentation only. Reads computed_ (set by feed()); mutates nothing in the game.
// Tabbed interface: Overview lists career totals, Hands lists recent rounds, Profits draws
// the bankroll curve + per-round profit bars from the chart data arrays.
// ---------------------------------------------------------------------------
void StatsDashboard::draw(sf::RenderWindow& window) const
{
    // World units, not window pixels -- see worldDrawSize(). Every tab below lays out against this.
    const sf::Vector2f world = worldDrawSize(window);
    const float w = world.x;
    const float h = world.y;
    const float cx = w / 2.0f;

    window.clear(sf::Color(15, 22, 30));   // deep-teal backdrop distinct from every other mode

    window.draw(centeredTitle(window, "STATISTICS DASHBOARD", 34u, h * 0.08f, sf::Color(255, 215, 0)));

    // Tab bar: equally-spaced labels with the active one highlighted (navigation lives in InputHandler).
    // The width is DERIVED from the tab count, not a fixed fraction: adding the Progress tab to a
    // hard-coded 0.28 * w spacing would push the fourth label off the right edge at every resolution
    // (the U3 class of defect -- layout that only fits one size).
    const int tabCount = static_cast<int>(Tab::Progress) + 1;
    const char* tabLabels[static_cast<int>(Tab::Progress) + 1] = {"Overview", "Hands", "Profits", "Progress"};
    const float tabTop = h * 0.16f;
    const float tabWidth = w / static_cast<float>(tabCount);
    for (int i = 0; i < tabCount; ++i)
    {
        bool active = (tab_ == static_cast<Tab>(i));
        sf::Text t(FontManager::get(), tabLabels[i], active ? 24u : 20u);
        t.setFillColor(active ? sf::Color(255, 255, 255) : sf::Color(150, 160, 170));
        const auto bounds = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, 0.f));
        t.setPosition(sf::Vector2f(tabWidth * i + tabWidth / 2.0f, tabTop));
        window.draw(t);

        sf::RectangleShape bar(sf::Vector2f(tabWidth - 12.f, 3.f));
        bar.setFillColor(active ? sf::Color(255, 215, 0) : sf::Color(70, 80, 90));
        bar.setPosition(sf::Vector2f(tabWidth * i + 6.f, tabTop + bounds.size.y + 4.f));
        window.draw(bar);
    }

    switch (tab_)
    {
    case StatsDashboard::Tab::Overview:
    {
        const ComputedStats& c = computed_;
        struct Row { std::string label; std::string value; };
        const std::vector<Row> rows = {
            {"Shoes finished", std::to_string(c.shoesFinished)},
            {"Hands played", std::to_string(c.handsFinished)},
            {"Wins / Losses / Pushes", std::to_string(c.wins) + " / " + std::to_string(c.losses) + " / " + std::to_string(c.pushes)},
            {"Win rate", (std::to_string(static_cast<int>(c.winRate * 100.0f)) + "%")},
            {"Blackjacks", std::to_string(c.blackjacks)},
            {"BJ frequency", (std::to_string(static_cast<int>(c.bjFrequency * 100.0f)) + "%")},
            {"Busts", std::to_string(c.busts)},
            {"Bust frequency", (std::to_string(static_cast<int>(c.bustFrequency * 100.0f)) + "%")},
            {"Strategy accuracy", (std::to_string(static_cast<int>(c.strategyAccuracy * 100.0f)) + "%")},
            {"Splits / Doubles", std::to_string(c.splits) + " / " + std::to_string(c.doubles)},
            {"Best hand net", std::to_string(c.greatestHandProfit)},
            {"Worst hand net", std::to_string(c.worstHandLoss)},
            {"Peak bankroll", std::to_string(c.peakBankroll)},
            {"Max drawdown", std::to_string(c.maxDrawdown)}
        };
        float y = h * 0.28f;
        for (const auto& r : rows)
        {
            sf::Text l(FontManager::get(), r.label, 18);
            l.setFillColor(sf::Color(190, 200, 210));
            l.setPosition(sf::Vector2f(cx - w * 0.34f, y));
            window.draw(l);

            sf::Text v(FontManager::get(), r.value, 18);
            v.setFillColor(sf::Color(255, 255, 255));
            v.setPosition(sf::Vector2f(cx + w * 0.10f, y));
            window.draw(v);
            y += 34.f;
        }
        break;
    }
    case StatsDashboard::Tab::Hands:
    {
        const ComputedStats& c = computed_;
        const std::size_t visible = 16u;
        const std::size_t start = c.perRoundProfit.size() > visible ? c.perRoundProfit.size() - visible : 0u;
        sf::Text head(FontManager::get(), "ROUND   RESULT            NET", 16);
        head.setFillColor(sf::Color(255, 215, 0));
        head.setPosition(sf::Vector2f(cx - w * 0.30f, h * 0.24f));
        window.draw(head);

        float y = h * 0.30f;
        for (std::size_t i = start; i < c.perRoundProfit.size() && i < start + 13; ++i)
        {
            int net = c.perRoundProfit[i];
            std::string result = net > 0 ? "WIN" : (net < 0 ? "LOSS" : "PUSH");
            int r0 = 200, r1 = 200, r2 = 200;
            if (net > 0) { r0 = 103; r1 = 255; r2 = 103; }
            else if (net < 0) { r0 = 255; r1 = 120; r2 = 120; }
            sf::Text line(FontManager::get(), std::to_string(i) + "   " + result + "          (" + std::to_string(net) + ")", 16);
            line.setFillColor(sf::Color(r0, r1, r2));
            line.setPosition(sf::Vector2f(cx - w * 0.30f, y));
            window.draw(line);
            y += 26.f;
        }
        if (c.perRoundProfit.size() > visible)
        {
            sf::Text more(FontManager::get(), "showing last " + std::to_string(visible) + " of " + std::to_string(c.perRoundProfit.size()) + " rounds", 13);
            more.setFillColor(sf::Color(140, 150, 160));
            more.setPosition(sf::Vector2f(cx, h * 0.90f));
            window.draw(more);
        }
        break;
    }
    case StatsDashboard::Tab::Profits:
    {
        const ComputedStats& c = computed_;

        // Bankroll curve (polyline) in the upper chart region, scaled to its own min/max
        std::vector<long long> series(c.bankrollSeries);
        if (!series.empty())
        {
            long long lo = series[0], hi = series[0];
            for (long long v : series) { lo = std::min(lo, v); hi = std::max(hi, v); }
            const float plotLeft = w * 0.12f;
            const float plotRight = w * 0.88f;
            const float plotTop = h * 0.26f;
            const float plotBottom = h * 0.55f;
            const float span = (hi - lo) > 0 ? static_cast<float>(hi - lo) : 1.0f;

            sf::VertexArray curve(sf::PrimitiveType::Lines, series.size());
            for (std::size_t i = 0; i < series.size(); ++i)
            {
                const float x = series.size() > 1 ? plotLeft + (static_cast<float>(i) / (series.size() - 1)) * (plotRight - plotLeft) : (plotLeft + plotRight) / 2.0f;
                const float yv = plotBottom - (static_cast<float>(series[i] - lo) / span) * (plotBottom - plotTop);
                curve[i].position = sf::Vector2f(x, yv);
                curve[i].color = sf::Color(255, 215, 0);
            }
            window.draw(curve);

            sf::Text axis(FontManager::get(), "Bankroll progression over rounds", 15);
            axis.setFillColor(sf::Color(170, 180, 190));
            axis.setPosition(sf::Vector2f(cx, plotTop - 14.f));
            window.draw(axis);
        }

        // Per-round profit bars in the lower chart region (green for gains, red for losses).
        if (!c.perRoundProfit.empty())
        {
            float maxAbs = 1.0f;
            for (int v : c.perRoundProfit)
                maxAbs = std::max(maxAbs, static_cast<float>(std::abs(v)));
            const float midLine = h * 0.85f;
            const float plotTop = h * 0.62f;
            const float barW = (w * 0.76f) / std::max<std::size_t>(1, c.perRoundProfit.size());
            for (std::size_t i = 0; i < c.perRoundProfit.size(); ++i)
            {
                float barH = static_cast<float>(c.perRoundProfit[i]) / maxAbs * (midLine - plotTop);
                sf::RectangleShape bar(sf::Vector2f(std::max(1.f, barW - 1.f), std::abs(barH)));
                bar.setFillColor(c.perRoundProfit[i] >= 0 ? sf::Color(103, 255, 103) : sf::Color(255, 120, 120));
                float top = c.perRoundProfit[i] >= 0 ? midLine - barH : midLine;
                bar.setPosition(sf::Vector2f(w * 0.12f + i * barW, top));
                window.draw(bar);
            }

            sf::Text axis(FontManager::get(), "Per-round net profit", 15);
            axis.setFillColor(sf::Color(170, 180, 190));
            axis.setPosition(sf::Vector2f(cx, plotTop - 14.f));
            window.draw(axis);
        }
        break;
    }
    case StatsDashboard::Tab::Progress:
        drawProgressTab(window);
        break;
    }

    // The hint names every control the CURRENT tab responds to. The Progress tab has two extra keys,
    // and a control that is not printed is a control that is never found.
    const char* hintText = (tab_ == Tab::Progress)
        ? "Left/Right: tab   Up/Down: which history   S: shoe / session   Esc: back"
        : "Left/Right: tab   Esc: back";
    sf::Text hint(FontManager::get(), hintText, 14);
    hint.setFillColor(sf::Color(140, 150, 160));
    {
        const auto bounds = hint.getLocalBounds();
        hint.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, 0.f));
    }
    hint.setPosition(sf::Vector2f(cx, h - 24.f));
    window.draw(hint);
}

// ---------------------------------------------------------------------------
// The Progress tab. Two charts, two axes, every frame derived from window.getSize().
//
// Nothing here uses a hard-coded pixel offset. U3 was exactly this defect class: the achievements and
// rules screens laid out in absolute pixels and their footers overlapped the rows at 900x560 while the
// table reflowed fine. Every y below is a fraction of h, every x a fraction of w, and the two charts
// are given DISJOINT vertical bands (0.28-0.55 and 0.66-0.90) so they cannot collide at any size.
// ---------------------------------------------------------------------------
void StatsDashboard::drawProgressTab(sf::RenderWindow& window) const
{
    // World units, like the rest of draw(). The two charts' bands are fractions of the WORLD rect, so
    // they stay disjoint and inside the panel at every window size, letterboxed and all.
    const sf::Vector2f world = worldDrawSize(window);
    const float w = world.x;
    const float h = world.y;
    const float cx = w / 2.0f;

    // A pixel budget for the across-shoes chart. With the T2 cap at 100 shoes the raw series is small,
    // but the budget keeps the geometry honest if the cap ever grows (IDEAS #3's "downsample long
    // careers to a fixed pixel budget").
    constexpr std::size_t kPixelBudget = 240;

    // ---- Top chart: bankroll at shoe end, across shoes, for the SELECTED source ----
    const std::vector<double> career = progressCareerSeries();
    const chart::Frame topFrame = chart::frame(world, 0.12f, 0.88f, 0.28f, 0.55f);

    // The title names the source. "Which history am I reading" is not optional on a surface that reads
    // shoe history: career longHistory is Regular-only, so an unlabelled chart silently omits the Daily
    // and tournament shoes.
    {
        sf::Text title(FontManager::get(), std::string("Bankroll at shoe end - ") + progressSourceLabel(), 17);
        title.setFillColor(sf::Color(255, 215, 0));
        title.setPosition(sf::Vector2f(cx - w * 0.34f, topFrame.top - 26.f));
        window.draw(title);
    }

    if (career.empty())
    {
        sf::Text empty(FontManager::get(), std::string("No ") + progressSourceLabel() + " shoes recorded yet.", 16);
        empty.setFillColor(sf::Color(150, 160, 170));
        empty.setPosition(sf::Vector2f(cx - w * 0.20f, (topFrame.top + topFrame.bottom) * 0.5f));
        window.draw(empty);
    }
    else
    {
        const std::vector<chart::Extreme> buckets = chart::bucketExtremes(career, kPixelBudget);
        std::vector<double> maxima, minima;
        maxima.reserve(buckets.size());
        minima.reserve(buckets.size());
        for (const auto& b : buckets)
        {
            maxima.push_back(b.max);
            minima.push_back(b.min);
        }

        // One scale for both lines and the shading, taken from the FULL series, so downsampling cannot
        // move the axis.
        const chart::Range r = chart::range(career);

        // Drawdown shading, measured from the RUNNING peak -- the same base as the Overview tab's
        // "Max drawdown" number, so the chart and the number can never disagree on screen.
        const std::vector<sf::Vertex> bands = chart::drawdownBands(career, topFrame, sf::Color(120, 40, 40, 120));
        if (!bands.empty())
            window.draw(bands.data(), bands.size(), sf::PrimitiveType::Triangles);

        const std::vector<sf::Vertex> hi = chart::polyline(maxima, topFrame, sf::Color(255, 215, 0));
        const std::vector<sf::Vertex> lo = chart::polyline(minima, topFrame, sf::Color(255, 150, 90, 150));
        if (hi.size() >= 2)
            window.draw(hi.data(), hi.size(), sf::PrimitiveType::Lines);
        else if (hi.size() == 1)
            window.draw(hi.data(), 1, sf::PrimitiveType::Points);
        if (lo.size() >= 2)
            window.draw(lo.data(), lo.size(), sf::PrimitiveType::Lines);

        // The two charts have different x-axes (shoes vs rounds). IDEAS #6's own note: label them, or
        // the pair reads as one continuous story.
        sf::Text xTop(FontManager::get(),
                      "x: shoes (oldest left)   y: bankroll " + std::to_string(static_cast<long long>(r.lo)) +
                          " to " + std::to_string(static_cast<long long>(r.hi)), 13);
        xTop.setFillColor(sf::Color(150, 160, 170));
        xTop.setPosition(sf::Vector2f(cx - w * 0.34f, topFrame.bottom + 6.f));
        window.draw(xTop);
    }


    // ---- Bottom chart: the live curve, within ONE open play window ----
    // This is the U12 SCOPE axis, and it is on its own control. It is never presented as an option
    // alongside Career / Daily / Tournament, because those name a recorded history and this names a
    // play window -- the two are different axes (CONTEXT §5, the §G #6 decision).
    const std::vector<double> live = progressLiveSeries();
    const chart::Frame bottomFrame = chart::frame(world, 0.12f, 0.88f, 0.66f, 0.90f);
    {
        sf::Text title(FontManager::get(), progressLiveLabel(), 17);
        title.setFillColor(sf::Color(180, 220, 255));
        title.setPosition(sf::Vector2f(cx - w * 0.34f, bottomFrame.top - 26.f));
        window.draw(title);
    }

    if (live.empty())
    {
        sf::Text empty(FontManager::get(), "Nothing played in this window yet.", 16);
        empty.setFillColor(sf::Color(150, 160, 170));
        empty.setPosition(sf::Vector2f(cx - w * 0.20f, (bottomFrame.top + bottomFrame.bottom) * 0.5f));
        window.draw(empty);
    }
    else
    {
        const std::vector<sf::Vertex> curve = chart::polyline(live, bottomFrame, sf::Color(120, 220, 255));
        if (curve.size() >= 2)
            window.draw(curve.data(), curve.size(), sf::PrimitiveType::Lines);
        else
            window.draw(curve.data(), 1, sf::PrimitiveType::Points);   // a one-point curve renders as a dot

        const chart::Range r = chart::range(live);
        sf::Text xBottom(FontManager::get(),
                         "x: rounds this " + std::string(progressSession_ ? "session" : "shoe") +
                             "   y: net " + std::to_string(static_cast<long long>(r.lo)) + " to " +
                             std::to_string(static_cast<long long>(r.hi)), 13);
        xBottom.setFillColor(sf::Color(150, 160, 170));
        xBottom.setPosition(sf::Vector2f(cx - w * 0.34f, bottomFrame.bottom + 6.f));
        window.draw(xBottom);
    }
}

