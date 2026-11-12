#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

#include "GameRecord.hpp"   // ShoeRecord, for careerSeries()

// Pure chart geometry for the dashboard's Progress tab.
//
// Deliberately free of sf::RenderWindow: every function returns geometry (vertices, points, scalars)
// rather than drawing, so the maths is assertable from a headless test TU. That is the same seam
// ReplayLayout.hpp, PresentationLayout.hpp and HistoryBrowser.hpp exist for -- the presentation layer
// is where this project's defects have lived precisely because nothing above it could be tested.
//
// Two rules the whole file obeys, both from defects this project already shipped:
//   * Every frame is derived from the WINDOW SIZE, never from a hard-coded pixel offset. U3 was
//     exactly "hard-coded offsets overlap at 900x560 while the table reflows fine".
//   * Every scale divides by a guarded span. A one-point, flat or empty series must map somewhere,
//     not divide by zero (IDEAS #6's "a one-point chart must not divide by zero when scaling").
namespace chart
{
    // A plot rectangle in pixels.
    struct Frame
    {
        float left = 0.0f;
        float right = 0.0f;
        float top = 0.0f;
        float bottom = 0.0f;
    };

    // A value range with lo <= hi always. An empty series yields {0, 0}.
    struct Range
    {
        double lo = 0.0;
        double hi = 0.0;
    };

    // A plot rectangle as fractions of the DRAW SIZE. Fractions in, coordinates out, so a chart laid
    // out through this cannot overlap at one resolution and not another. The size is in world units
    // (the caller passes the view size, not window pixels) -- see StatsDashboard::worldDrawSize.
    inline Frame frame(sf::Vector2f drawSize, float leftFrac, float rightFrac, float topFrac, float bottomFrac)
    {
        return Frame{drawSize.x * leftFrac, drawSize.x * rightFrac,
                     drawSize.y * topFrac, drawSize.y * bottomFrac};
    }

    // The min/max of a series. Empty -> {0, 0}.
    Range range(const std::vector<double>& values);

    // True when the series has no vertical extent (empty, one point, or all-equal values). Callers use
    // this to decide whether to draw a curve at all; scaleY never divides by zero either way.
    bool isFlat(const Range& r);

    // The y pixel for a value. A flat range maps to the frame's vertical centre, so a constant series
    // renders as a level line instead of NaN.
    float scaleY(double value, const Range& r, const Frame& f);

    // The x pixel for sample `index` of a `count`-sample series. count <= 1 maps to the horizontal
    // centre, which is what keeps a single point on screen rather than off the left edge.
    float scaleX(std::size_t index, std::size_t count, const Frame& f);

    // A connected line through the series, scaled to its own range. Empty -> empty; one point -> one
    // vertex (a Lines primitive with one vertex draws nothing, which is the honest answer).
    std::vector<sf::Vertex> polyline(const std::vector<double>& values, const Frame& f, const sf::Color& color);

    // peak[i] = max(values[0..i]). Drawdown is measured from the RUNNING peak, which is the same base
    // ComputedStats::maxDrawdown uses -- so the shaded region and the "Max drawdown" number on the
    // Overview tab can never disagree on screen.
    std::vector<double> runningPeak(const std::vector<double>& values);

    // The drawdown as a positive magnitude per sample: runningPeak[i] - values[i], floored at 0.
    std::vector<double> drawdowns(const std::vector<double>& values);

    // Filled drawdown shading: two vertices per sample (a triangle pair) spanning from the running peak
    // down to the value, in the band between sample i and i+1. The band's pixel height therefore equals
    // (peak - value) scaled by the range, which is the property the test asserts.
    // Returned as sf::PrimitiveType::Triangles-ready vertex triples, 6 per band.
    std::vector<sf::Vertex> drawdownBands(const std::vector<double>& values, const Frame& f, const sf::Color& color);

    // One bucket of a downsampled series.
    struct Extreme
    {
        double max = 0.0;
        double min = 0.0;
        std::size_t firstIndex = 0;   // the index the bucket starts at, for labelling
    };

    // Downsample to `buckets` buckets, preserving the true max AND the true min of every bucket.
    // IDEAS #6 asks for "max-per-bucket so spikes survive"; a spike DOWN is just as real on a bankroll
    // curve, so both extremes are kept and the caller draws the max line and the min line.
    // Empty input or buckets == 0 -> empty. Fewer values than buckets -> one bucket per value.
    std::vector<Extreme> bucketExtremes(const std::vector<double>& values, std::size_t buckets);

    // The across-shoes career curve: bankrollAtShoeEnd per shoe, oldest first (the stored vectors are
    // already chronological -- addCareerShoe erases from the front, which is what keeps it so).
    //
    // IDEAS #6's stated trap -- "bankrollAtShoeEnd defaults to 0 for pre-existing profiles, so treat 0
    // as a gap and break the line" -- is MOOT on this schema: the history was wiped when the seed
    // fields landed, so every shoe on disk records a real value. Recorded here so the next reader does
    // not re-add gap handling for a case that cannot occur, and so it is obvious where to add it if a
    // future schema change makes an unknown bankroll representable again.
    std::vector<double> careerSeries(const std::vector<ShoeRecord>& shoes);

} // namespace chart
