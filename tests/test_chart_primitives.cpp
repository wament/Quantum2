// The Progress tab's geometry, tested as pure maths with no window.
//
// This is the seam the whole chart feature hangs from. IDEAS #6 lists three ways a bankroll chart lies:
// a one-point series dividing by zero, downsampling hiding a spike, and a chart whose drawdown
// disagrees with the "Max drawdown" number on the same screen. All three are arithmetic, so all three
// are locked here rather than eyeballed. The layout assertions use the window sizes U3 was found at:
// 1600x900 and 900x560.

#include <catch2/catch_all.hpp>

#include "ChartPrimitives.hpp"
#include "GameRecord.hpp"

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

chart::Frame frameAt(unsigned w, unsigned h, float l, float r, float t, float b)
{
    // The size is in WORLD units (the view size), which is what the dashboard passes in production.
    return chart::frame(sf::Vector2f(static_cast<float>(w), static_cast<float>(h)), l, r, t, b);
}

ShoeRecord shoeEndingAt(int bankroll)
{
    ShoeRecord s;
    s.bankrollAtShoeEnd = bankroll;
    return s;
}

} // namespace

TEST_CASE("range: empty, flat and normal series", "[ChartPrimitives]")
{
    const chart::Range empty = chart::range({});
    CHECK(empty.lo == 0.0);
    CHECK(empty.hi == 0.0);
    CHECK(chart::isFlat(empty));                       // empty has no extent: never divide by it

    const chart::Range flat = chart::range({7.0, 7.0, 7.0});
    CHECK(flat.lo == 7.0);
    CHECK(flat.hi == 7.0);
    CHECK(chart::isFlat(flat));

    const chart::Range r = chart::range({5.0, -20.0, 3.0, 12.0});
    CHECK(r.lo == -20.0);
    CHECK(r.hi == 12.0);
    CHECK_FALSE(chart::isFlat(r));
}

TEST_CASE("scaling never divides by zero: one point, flat series, empty series", "[ChartPrimitives]")
{
    const chart::Frame f = frameAt(1600, 900, 0.12f, 0.88f, 0.28f, 0.55f);

    // A one-point series is IDEAS #6's gotcha stated verbatim: "a one-point chart must not divide by
    // zero when scaling".
    const std::vector<double> one{42.0};
    const chart::Range rOne = chart::range(one);
    const float y = chart::scaleY(42.0, rOne, f);
    CHECK(std::isfinite(y));
    CHECK(y == Catch::Approx((f.top + f.bottom) * 0.5f));   // a flat range maps to the vertical centre

    const chart::Frame f2 = frameAt(900, 560, 0.12f, 0.88f, 0.28f, 0.55f);
    CHECK(std::isfinite(chart::scaleY(0.0, chart::range({}), f2)));

    // count 0 and 1 must not divide by (count - 1).
    CHECK(std::isfinite(chart::scaleX(0, 0, f)));
    CHECK(std::isfinite(chart::scaleX(0, 1, f)));
    CHECK(chart::scaleX(0, 1, f) == Catch::Approx((f.left + f.right) * 0.5f));

    // A normal series maps its min to the bottom of the plot and its max to the top.
    const std::vector<double> s{-100.0, 0.0, 50.0};
    const chart::Range rs = chart::range(s);
    CHECK(chart::scaleY(-100.0, rs, f) == Catch::Approx(f.bottom));
    CHECK(chart::scaleY(50.0, rs, f) == Catch::Approx(f.top));
}

TEST_CASE("polyline: one vertex per sample, empty in empty out", "[ChartPrimitives]")
{
    const chart::Frame f = frameAt(1600, 900, 0.12f, 0.88f, 0.28f, 0.55f);
    CHECK(chart::polyline({}, f, sf::Color(255, 255, 255)).empty());

    const std::vector<double> s{1.0, 5.0, 3.0};
    const std::vector<sf::Vertex> line = chart::polyline(s, f, sf::Color(255, 255, 255));
    REQUIRE(line.size() == s.size());
    for (const auto& v : line)
        CHECK(std::isfinite(v.position.y));

    // x increases monotonically across the samples.
    for (std::size_t i = 1; i < line.size(); ++i)
        CHECK(line[i].position.x > line[i - 1].position.x);
}

TEST_CASE("running peak and drawdowns match the dashboard's max-drawdown definition",
          "[ChartPrimitives]")
{
    // StatsDashboard::feed() computes maxDrawdown as the worst peak-to-current decline, the peak
    // running from the series start. The chart must use the SAME base, or the shaded region and the
    // Overview tab's number disagree on screen and one of them is visibly wrong.
    const std::vector<double> s{0.0, 100.0, 40.0, 60.0, -20.0, 30.0};
    const std::vector<double> peaks = chart::runningPeak(s);
    REQUIRE(peaks.size() == s.size());
    CHECK(peaks == std::vector<double>{0.0, 100.0, 100.0, 100.0, 100.0, 100.0});

    const std::vector<double> dd = chart::drawdowns(s);
    CHECK(dd == std::vector<double>{0.0, 0.0, 60.0, 40.0, 120.0, 70.0});

    // The deepest band equals ComputedStats::maxDrawdown for the same series.
    double worst = 0.0;
    for (const double d : dd)
        worst = std::max(worst, d);
    CHECK(worst == 120.0);

    // Monotone non-decreasing, and never below the value it peaks from.
    for (std::size_t i = 0; i < peaks.size(); ++i)
    {
        CHECK(peaks[i] >= s[i]);
        if (i > 0)
            CHECK(peaks[i] >= peaks[i - 1]);
    }
}

TEST_CASE("drawdown bands: pixel depth equals (peak - value) at every sample", "[ChartPrimitives]")
{
    const chart::Frame f = frameAt(1600, 900, 0.0f, 1.0f, 100.0f, 300.0f);   // 200 px tall
    const std::vector<double> s{0.0, -50.0, -100.0, -50.0};
    const chart::Range r = chart::range(s);
    const float pixelsPerUnit = (f.bottom - f.top) / static_cast<float>(r.hi - r.lo);   // 200 / 100 = 2

    const std::vector<sf::Vertex> bands = chart::drawdownBands(s, f, sf::Color(255, 0, 0));
    REQUIRE(bands.size() == (s.size() - 1) * 6);          // two triangles per band

    const std::vector<double> dd = chart::drawdowns(s);
    for (std::size_t b = 0; b + 1 < s.size(); ++b)
    {
        const sf::Vertex& peak = bands[b * 6 + 0];        // left edge, on the peak line
        const sf::Vertex& val = bands[b * 6 + 1];         // left edge, on the value line
        CHECK(peak.position.x == Catch::Approx(val.position.x));
        const float depth = val.position.y - peak.position.y;
        CHECK(depth == Catch::Approx(static_cast<float>(dd[b]) * pixelsPerUnit));
    }

    // A series that never falls from its peak shades nothing.
    const std::vector<double> rising{0.0, 10.0, 20.0};
    const std::vector<sf::Vertex> none = chart::drawdownBands(rising, f, sf::Color(255, 0, 0));
    REQUIRE(none.size() == (rising.size() - 1) * 6);
    for (std::size_t i = 0; i < none.size(); i += 6)
        CHECK(std::abs(none[i + 1].position.y - none[i].position.y) < 0.001f);

    // Fewer than two samples shades nothing at all, and stays finite.
    CHECK(chart::drawdownBands({}, f, sf::Color(255, 0, 0)).empty());
    CHECK(chart::drawdownBands({5.0}, f, sf::Color(255, 0, 0)).empty());
}


TEST_CASE("bucketExtremes preserves the true max AND min of every bucket", "[ChartPrimitives]")
{
    CHECK(chart::bucketExtremes({}, 10).empty());
    CHECK(chart::bucketExtremes({1.0, 2.0, 3.0}, 0).empty());

    // One value in, one bucket out, exact.
    const std::vector<chart::Extreme> single = chart::bucketExtremes({-9.0}, 4);
    REQUIRE(single.size() == 1);
    CHECK(single[0].max == -9.0);
    CHECK(single[0].min == -9.0);

    // The spike case IDEAS #6 warns about. It asks for "max-per-bucket so spikes survive"; a spike
    // DOWN is just as real on a bankroll curve, so a deep single value has to survive in the min line.
    std::vector<double> series(40, 100.0);
    series[23] = -500.0;
    const std::vector<chart::Extreme> buckets = chart::bucketExtremes(series, 8);
    REQUIRE(buckets.size() == 8);

    double globalMax = -1e18, globalMin = 1e18;
    for (const auto& b : buckets)
    {
        globalMax = std::max(globalMax, b.max);
        globalMin = std::min(globalMin, b.min);
        CHECK(b.max >= b.min);
    }
    CHECK(globalMax == 100.0);
    CHECK(globalMin == -500.0);                       // the spike DOWN survived

    // Buckets cover every value exactly once, with none empty.
    std::size_t total = 0;
    for (std::size_t b = 0; b < buckets.size(); ++b)
    {
        const std::size_t start = buckets[b].firstIndex;
        const std::size_t end = (b + 1 < buckets.size()) ? buckets[b + 1].firstIndex : series.size();
        CHECK(end > start);
        total += end - start;
    }
    CHECK(total == series.size());

    // More buckets than values: one bucket per value, never an empty one.
    const std::vector<chart::Extreme> many = chart::bucketExtremes({3.0, 1.0, 2.0}, 10);
    REQUIRE(many.size() == 3);
    CHECK(many[0].max == 3.0);
    CHECK(many[1].min == 1.0);
}

TEST_CASE("careerSeries reads bankrollAtShoeEnd oldest first", "[ChartPrimitives]")
{
    const std::vector<ShoeRecord> shoes{shoeEndingAt(1000), shoeEndingAt(950), shoeEndingAt(1200)};
    const std::vector<double> series = chart::careerSeries(shoes);
    CHECK(series == std::vector<double>{1000.0, 950.0, 1200.0});
    CHECK(chart::careerSeries({}).empty());
}

TEST_CASE("the two Progress chart bands stay disjoint and on screen at every window size",
          "[ChartPrimitives][layout]")
{
    // U3's defect class: layout that fits one window size and overlaps at another. The Progress tab
    // gives its two charts disjoint vertical bands; this asserts they cannot collide, and that both
    // sit inside the window -- at 1600x900, at 900x560, and at an in-between size.
    const sf::Vector2u sizes[] = {sf::Vector2u(1600, 900), sf::Vector2u(900, 560), sf::Vector2u(1280, 720)};
    for (const sf::Vector2u& size : sizes)
    {
        const chart::Frame top = chart::frame(sf::Vector2f(static_cast<float>(size.x), static_cast<float>(size.y)),
                                              0.12f, 0.88f, 0.28f, 0.55f);
        const chart::Frame bottom = chart::frame(sf::Vector2f(static_cast<float>(size.x), static_cast<float>(size.y)),
                                                 0.12f, 0.88f, 0.66f, 0.90f);

        CHECK(top.bottom < bottom.top);                 // disjoint: the charts cannot overlap
        CHECK(top.left >= 0.0f);
        CHECK(top.right <= static_cast<float>(size.x));
        CHECK(top.top >= 0.0f);
        CHECK(bottom.bottom <= static_cast<float>(size.y));
        CHECK(top.right > top.left);
        CHECK(bottom.bottom > bottom.top);

        // The footer hint sits below the lower chart, inside the window.
        const float hintY = static_cast<float>(size.y) - 24.0f;
        CHECK(hintY > bottom.bottom);
    }
}

