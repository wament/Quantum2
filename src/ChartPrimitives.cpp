#include "ChartPrimitives.hpp"

#include <algorithm>
#include <limits>

namespace chart
{

Range range(const std::vector<double>& values)
{
    if (values.empty())
        return Range{0.0, 0.0};

    Range r{values.front(), values.front()};
    for (const double v : values)
    {
        r.lo = std::min(r.lo, v);
        r.hi = std::max(r.hi, v);
    }
    return r;
}

bool isFlat(const Range& r)
{
    return !(r.hi > r.lo);
}

float scaleY(double value, const Range& r, const Frame& f)
{
    // Flat range (empty, single point, constant series): map to the vertical centre. The guard is the
    // whole point -- the naive (hi - lo) divisor is 0 for every one of those cases.
    if (isFlat(r))
        return (f.top + f.bottom) * 0.5f;

    const double span = r.hi - r.lo;
    const double t = (value - r.lo) / span;             // 0 at the bottom of the plot, 1 at the top
    return f.bottom - static_cast<float>(t) * (f.bottom - f.top);
}

float scaleX(std::size_t index, std::size_t count, const Frame& f)
{
    if (count <= 1)
        return (f.left + f.right) * 0.5f;
    const double t = static_cast<double>(index) / static_cast<double>(count - 1);
    return f.left + static_cast<float>(t) * (f.right - f.left);
}

std::vector<sf::Vertex> polyline(const std::vector<double>& values, const Frame& f, const sf::Color& color)
{
    std::vector<sf::Vertex> out;
    out.reserve(values.size());
    const Range r = range(values);
    for (std::size_t i = 0; i < values.size(); ++i)
        out.push_back(sf::Vertex{sf::Vector2f(scaleX(i, values.size(), f), scaleY(values[i], r, f)), color});
    return out;
}

std::vector<double> runningPeak(const std::vector<double>& values)
{
    std::vector<double> out;
    out.reserve(values.size());
    double peak = -std::numeric_limits<double>::infinity();
    for (const double v : values)
    {
        peak = std::max(peak, v);
        out.push_back(peak);
    }
    return out;
}

std::vector<double> drawdowns(const std::vector<double>& values)
{
    const std::vector<double> peaks = runningPeak(values);
    std::vector<double> out;
    out.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
        out.push_back(std::max(0.0, peaks[i] - values[i]));
    return out;
}

std::vector<sf::Vertex> drawdownBands(const std::vector<double>& values, const Frame& f, const sf::Color& color)
{
    std::vector<sf::Vertex> out;
    if (values.size() < 2)
        return out;

    // One scale for the whole band pair: the peak series and the value series share the frame, so the
    // band's pixel height is exactly (peak - value) in data units times the frame's pixels-per-unit.
    const Range r = range(values);
    const std::vector<double> peaks = runningPeak(values);
    const double span = isFlat(r) ? 1.0 : (r.hi - r.lo);
    const float pixelsPerUnit = (f.bottom - f.top) / static_cast<float>(span);

    out.reserve((values.size() - 1) * 6);
    for (std::size_t i = 0; i + 1 < values.size(); ++i)
    {
        const float x0 = scaleX(i, values.size(), f);
        const float x1 = scaleX(i + 1, values.size(), f);
        const float peakY0 = scaleY(peaks[i], r, f);
        const float peakY1 = scaleY(peaks[i + 1], r, f);
        const float valY0 = scaleY(values[i], r, f);
        const float valY1 = scaleY(values[i + 1], r, f);

        // A quad between the peak line and the value line, as two triangles.
        out.push_back(sf::Vertex{sf::Vector2f(x0, peakY0), color});
        out.push_back(sf::Vertex{sf::Vector2f(x0, valY0), color});
        out.push_back(sf::Vertex{sf::Vector2f(x1, peakY1), color});

        out.push_back(sf::Vertex{sf::Vector2f(x1, peakY1), color});
        out.push_back(sf::Vertex{sf::Vector2f(x0, valY0), color});
        out.push_back(sf::Vertex{sf::Vector2f(x1, valY1), color});
    }
    return out;
}

std::vector<Extreme> bucketExtremes(const std::vector<double>& values, std::size_t buckets)
{
    std::vector<Extreme> out;
    if (values.empty() || buckets == 0)
        return out;

    const std::size_t count = std::min(buckets, values.size());
    out.reserve(count);
    for (std::size_t b = 0; b < count; ++b)
    {
        // Integer bucket boundaries that cover every value exactly once, with no empty bucket: the
        // first `values.size() % count` buckets take one extra element.
        const std::size_t start = (b * values.size()) / count;
        const std::size_t extra = (b < values.size() % count) ? 1u : 0u;
        const std::size_t end = start + (values.size() / count) + extra;

        Extreme e;
        e.firstIndex = start;
        e.max = values[start];
        e.min = values[start];
        for (std::size_t i = start; i < end; ++i)
        {
            e.max = std::max(e.max, values[i]);
            e.min = std::min(e.min, values[i]);
        }
        out.push_back(e);
    }
    return out;
}

std::vector<double> careerSeries(const std::vector<ShoeRecord>& shoes)
{
    std::vector<double> out;
    out.reserve(shoes.size());
    for (const auto& shoe : shoes)
        out.push_back(static_cast<double>(shoe.bankrollAtShoeEnd));
    return out;
}

} // namespace chart
