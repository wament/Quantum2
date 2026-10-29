#include "StatsDashboard.hpp"

#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <cstdlib>

#include "Hand.hpp"
#include "FontManager.hpp"

using json = nlohmann::json;

namespace
{
    // Reconstruct a Hand from its recorded cards so bust / natural detection reuses production scoring (identical to how the
    // game scores it), rather than re-implementing card-value logic. addCard() only pushes back, so this is side-effect free.
    Hand handFromCards(const std::vector<Card>& cards)
    {
        Hand h;
        for (const auto& c : cards)
            h.addCard(c);
        return h;
    }

    // Centered title text with its origin at the top edge, so vertical placement is stable regardless of font metrics.
    sf::Text centeredTitle(sf::RenderWindow& window, const std::string& label, unsigned int size, float y, const sf::Color& color)
    {
        sf::Text t(FontManager::get(), label, size);
        t.setFillColor(color);
        const auto bounds = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, 0.f));
        t.setPosition(sf::Vector2f(window.getSize().x / 2.0f, y));
        return t;
    }
}

void StatsDashboard::feed(const PlayerStats& career, const std::vector<RoundRecord>& rounds)
{
    computed_ = ComputedStats{};
    // Career-context numbers come straight from the saved profile; per-round / per-hand metrics are recomputed below.
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

        // Per-hand granularity for win/loss/bust/natural, matching how PlayerStats tallies each played hand.
        for (const auto& hand : round.playerHands)
        {
            const int netHand = hand.payout - hand.wager;   // single-hand net profit vs its own stake

            if (hand.surrendered)
                computed_.pushes++;
            else if (hand.won)
                computed_.wins++;
            else
                computed_.losses++;

            const Hand h = handFromCards(hand.cards);
            if (h.isBlackjack())
                computed_.blackjacks++;
            if (h.isBust())
                computed_.busts++;

            if (netHand > computed_.greatestHandProfit)
                computed_.greatestHandProfit = netHand;
            if (netHand < computed_.worstHandLoss)
                computed_.worstHandLoss = netHand;
        }
    }

    const int decisive = computed_.wins + computed_.losses;
    const int handsFinished = decisive + computed_.pushes;
    computed_.handsFinished = handsFinished;
    computed_.winRate = decisive > 0 ? static_cast<double>(computed_.wins) / decisive : 0.0;
    computed_.bjFrequency = handsFinished > 0 ? static_cast<double>(computed_.blackjacks) / handsFinished : 0.0;
    computed_.bustFrequency = handsFinished > 0 ? static_cast<double>(computed_.busts) / handsFinished : 0.0;
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
    os << "hands_finished," << computed_.handsFinished << "\n";
    os << "wins," << computed_.wins << "\n";
    os << "losses," << computed_.losses << "\n";
    os << "pushes," << computed_.pushes << "\n";
    os << "blackjacks," << computed_.blackjacks << "\n";
    os << "busts," << computed_.busts << "\n";
    os << "win_rate," << computed_.winRate << "\n";
    os << "bj_frequency," << computed_.bjFrequency << "\n";
    os << "bust_frequency," << computed_.bustFrequency << "\n";
    os << "strategy_accuracy," << computed_.strategyAccuracy << "\n";
    os << "shoes_finished," << computed_.shoesFinished << "\n";
    os << "splits," << computed_.splits << "\n";
    os << "doubles," << computed_.doubles << "\n";
    os << "greatest_hand_profit," << computed_.greatestHandProfit << "\n";
    os << "worst_hand_loss," << computed_.worstHandLoss << "\n";
    os << "peak_bankroll," << computed_.peakBankroll << "\n";
    os << "max_drawdown," << computed_.maxDrawdown << "\n";

    os << "\nround_index,net_profit,cumulative_bankroll\n";
    for (std::size_t i = 0; i < computed_.perRoundProfit.size(); ++i)
        os << i << ',' << computed_.perRoundProfit[i] << ',' << computed_.bankrollSeries[i] << "\n";
    return os.str();
}

void StatsDashboard::update(float deltaTime)
{
    // Frame pacing for AppMode parity (mirrors Tournament / StrategyTrainer). Pure-logic tests never call this on the
    // deterministic path, so it only accumulates a timer here; tab switching is driven by InputHandler instead.
    frameTimer_ += deltaTime;
}

void StatsDashboard::advanceTab(bool forward)
{
    const int count = static_cast<int>(Tab::Profits) + 1;   // Overview=0, Hands=1, Profits=2
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
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float cx = w / 2.0f;

    window.clear(sf::Color(15, 22, 30));   // deep-teal backdrop distinct from every other mode

    window.draw(centeredTitle(window, "STATISTICS DASHBOARD", 34u, h * 0.08f, sf::Color(255, 215, 0)));

    // Tab bar: three equally-spaced labels with the active one highlighted (navigation lives in InputHandler).
    const char* tabLabels[static_cast<int>(Tab::Profits) + 1] = {"Overview", "Hands", "Profits"};
    const float tabTop = h * 0.16f;
    const float tabWidth = w * 0.28f;
    for (int i = 0; i < static_cast<int>(Tab::Profits) + 1; ++i)
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

        // Bankroll curve (polyline) in the upper chart region, scaled to its own min/max so it fills the panel.
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
    }

    sf::Text hint(FontManager::get(), "Left/Right: tab   Esc: back", 14);
    hint.setFillColor(sf::Color(140, 150, 160));
    {
        const auto bounds = hint.getLocalBounds();
        hint.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, 0.f));
    }
    hint.setPosition(sf::Vector2f(cx, h - 24.f));
    window.draw(hint);
}
