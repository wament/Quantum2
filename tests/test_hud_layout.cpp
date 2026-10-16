#include <catch2/catch_all.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <vector>
#include <Constants.hpp>

#include "HUD.hpp"
#include "StatBox.hpp"

using Catch::Approx;

// HUD layout invariants. These exist because the side panels silently drifted apart (left 260 vs right 200) when
// the charts were added: nothing asserted that the two panels match, that content stays inside its panel, or that
// the distributed boxes/charts stay evenly spaced. All of it is measured from the real rendered bounds.

namespace {

// Shared font for the layout tests. Loaded once; it must outlive every HUD built from it, hence the static.
// Non-const because HUD::initialize takes a mutable font reference (SFML fonts are cheap to share internally).
sf::Font& testFont()
{
    static sf::Font font;
    static const bool loaded = font.openFromFile("assets/fonts/roboto/RobotoFlex-VariableFont.ttf");
    REQUIRE(loaded);
    return font;
}

} // namespace

TEST_CASE("Side panels stay symmetric and tile the window", "[HUD][Layout]")
{
    HUD hud;
    hud.initialize(testFont());

    const sf::Rect<float> left = hud.leftPanelBounds();
    const sf::Rect<float> right = hud.rightPanelBounds();
    const sf::Rect<float> bottom = hud.bottomPanelBounds();

    const float W = static_cast<float>(Constants::WINDOW_WIDTH);
    const float H = static_cast<float>(Constants::WINDOW_HEIGHT);

    // The regression this guards: one panel was resized for the charts and the other was not.
    REQUIRE(left.size.x == Approx(Constants::Hud::SIDE_WIDTH));
    REQUIRE(right.size.x == Approx(Constants::Hud::SIDE_WIDTH));
    REQUIRE(left.size.x == Approx(right.size.x));
    REQUIRE(left.size.y == Approx(right.size.y));

    // Flush to the outer edges, sharing the same top edge and the same height.
    REQUIRE(left.position.x == Approx(0.f));
    REQUIRE(left.position.y == Approx(0.f));
    REQUIRE(right.position.x + right.size.x == Approx(W));
    REQUIRE(right.position.y == Approx(0.f));

    // Side panels stop exactly where the bottom bar starts; the bar spans the full width.
    REQUIRE(left.size.y + bottom.size.y == Approx(H));
    REQUIRE(bottom.position.y == Approx(H - Constants::Hud::BOTTOM_HEIGHT));
    REQUIRE(bottom.size.x == Approx(W));
}

TEST_CASE("Right-panel boxes are evenly distributed and fill the panel", "[HUD][Layout]")
{
    HUD hud;
    hud.initialize(testFont());

    const sf::Rect<float> panel = hud.rightPanelBounds();
    std::vector<StatBox*> boxes = hud.rightPanelStatBoxes();
    REQUIRE(boxes.size() == 8u);

    // Fill in values so the containment check measures the boxes the way they render on screen.
    for (StatBox* box : boxes)
        box->setValue("1234");

    std::sort(boxes.begin(), boxes.end(),
              [](const StatBox* a, const StatBox* b) { return a->getPosition().y < b->getPosition().y; });

    // Nothing may spill outside the panel it belongs to.
    for (const StatBox* box : boxes)
    {
        const sf::FloatRect r = box->getTitleBounds();
        REQUIRE(r.position.x >= panel.position.x);
        REQUIRE(r.position.x + r.size.x <= panel.position.x + panel.size.x);
        REQUIRE(r.position.y >= panel.position.y);
        REQUIRE(r.position.y + r.size.y <= panel.position.y + panel.size.y);
    }

    // Evenness is measured on the placement anchors. Rendered text bounds hug the actual glyphs ("Long Win" has a
    // descender, "Count" does not), so gaps measured from them are never uniform even when the layout is.
    const float pitch = (panel.size.y - 2.f * Constants::PANEL_MARGIN) / static_cast<float>(boxes.size());
    for (std::size_t i = 0; i + 1 < boxes.size(); ++i)
        REQUIRE(boxes[i + 1]->getPosition().y - boxes[i]->getPosition().y == Approx(pitch).margin(0.01f));

    // Equal top/bottom margins: the panel is filled rather than top-crammed with dead space below, which is what the
    // old fixed 46px pitch did (it left roughly half the panel empty).
    const float topMargin = boxes.front()->getPosition().y - panel.position.y;
    const float bottomMargin = (panel.position.y + panel.size.y)
                             - (boxes.back()->getPosition().y + boxes.back()->blockHeight());
    REQUIRE(topMargin == Approx(bottomMargin).margin(0.01f));
    REQUIRE(topMargin >= Constants::PANEL_MARGIN);

    // The stack must reach well past the panel's midpoint instead of stopping in the top half.
    const float span = boxes.back()->getPosition().y + boxes.back()->blockHeight() - boxes.front()->getPosition().y;
    REQUIRE(span / panel.size.y > 0.85f);
}

TEST_CASE("Bottom-bar boxes are evenly spaced, centered, and never overlap", "[HUD][Layout]")
{
    HUD hud;
    hud.initialize(testFont());

    const sf::Rect<float> panel = hud.bottomPanelBounds();
    std::vector<StatBox*> boxes = hud.bottomPanelStatBoxes();
    REQUIRE(boxes.size() == 5u);

    // Realistic values: "Bankroll 10000" is the widest pair in the bar, so this is the collision case that matters.
    for (StatBox* box : boxes)
        box->setValue("10000");

    std::sort(boxes.begin(), boxes.end(),
              [](const StatBox* a, const StatBox* b) { return a->getPosition().x < b->getPosition().x; });

    for (const StatBox* box : boxes)
    {
        const sf::FloatRect r = box->getTitleBounds();
        REQUIRE(r.position.x >= panel.position.x);
        REQUIRE(r.position.x + r.size.x <= panel.position.x + panel.size.x);
        REQUIRE(r.position.y >= panel.position.y);
        REQUIRE(r.position.y + r.size.y <= panel.position.y + panel.size.y);
    }

    // Uniform pitch across the bar (measured on the anchors), wide enough that no two boxes can collide.
    const float pitch = (panel.size.x - 2.f * Constants::PANEL_MARGIN) / static_cast<float>(boxes.size());
    for (std::size_t i = 0; i + 1 < boxes.size(); ++i)
    {
        REQUIRE(boxes[i + 1]->getPosition().x - boxes[i]->getPosition().x == Approx(pitch).margin(0.01f));

        const sf::FloatRect left = boxes[i]->getTitleBounds();
        const sf::FloatRect right = boxes[i + 1]->getTitleBounds();
        REQUIRE(right.position.x >= left.position.x + left.size.x);
    }

    // Vertically centered in the bar: equal margins above and below the two-line block.
    const float topMargin = boxes.front()->getPosition().y - panel.position.y;
    const float bottomMargin = (panel.position.y + panel.size.y)
                             - (boxes.front()->getPosition().y + boxes.front()->blockHeight());
    REQUIRE(topMargin == Approx(bottomMargin).margin(0.01f));
}

TEST_CASE("Graph panes tile the left panel evenly without overlapping", "[HUD][Layout]")
{
    HUD hud;
    hud.initialize(testFont());

    const sf::Rect<float> panel = hud.leftPanelBounds();
    std::vector<sf::Rect<float>> panes = hud.chartPaneBounds();
    REQUIRE(panes.size() == 3u);

    for (const sf::Rect<float>& pane : panes)
    {
        REQUIRE(pane.position.x >= panel.position.x);
        REQUIRE(pane.position.y >= panel.position.y);
        REQUIRE(pane.position.x + pane.size.x <= panel.position.x + panel.size.x);
        REQUIRE(pane.position.y + pane.size.y <= panel.position.y + panel.size.y);
        // Each pane has to be big enough to plot in (gutter + title/x-axis strips must not eat it alive).
        REQUIRE(pane.size.x > 150.f);
        REQUIRE(pane.size.y > 150.f);
    }

    std::sort(panes.begin(), panes.end(), [](const sf::Rect<float>& a, const sf::Rect<float>& b)
              { return a.position.y < b.position.y; });

    for (std::size_t i = 0; i < panes.size(); ++i)
    {
        REQUIRE(panes[i].size.y == Approx(panes.front().size.y).margin(0.5f));
        REQUIRE(panes[i].size.x == Approx(panes.front().size.x).margin(0.5f));
        if (i + 1 < panes.size())
        {
            const float gap = panes[i + 1].position.y - (panes[i].position.y + panes[i].size.y);
            REQUIRE(gap == Approx(Constants::Hud::CHART_GAP).margin(0.5f));
        }
    }
}

TEST_CASE("Split-hand clamp band stays clear of both side panels", "[HUD][Layout]")
{
    // The HUD draws after the table, so a split hand clamped inside a panel's footprint would be hidden by it.
    HUD hud;
    hud.initialize(testFont());

    REQUIRE(Constants::HANDS_LEFT_MARGIN >= hud.leftPanelBounds().size.x);
    REQUIRE(Constants::HANDS_RIGHT_MARGIN <= hud.rightPanelBounds().position.x);
    REQUIRE(Constants::HANDS_LEFT_MARGIN < Constants::HANDS_RIGHT_MARGIN);
}
