#include <catch2/catch_all.hpp>
#include "ReplayLayout.hpp"

// ---------------------------------------------------------------------------
// Follow-up #8 (replay layout): Replay::hoveredHistoryRow()/historyVisibleRowCount() and
// ReplayView::drawHistoryList() used to duplicate the history-list row geometry (topStart=120,
// lineHeight=40, the per-row top formula + header clip, and the x hit-test band). That logic now
// lives once in ReplayLayout so a drawn row can never drift from its hover target. These cases lock
// every branch of the shared geometry AND re-derive the ORIGINAL formulas to prove byte-for-byte
// identity after the extraction (no behavior change to scrolling / hover / scrollbar height).
// ---------------------------------------------------------------------------

namespace
{
// Faithful copies of the pre-extraction expressions, used only to prove the helpers match them.
float originalRowTop(int rowIndex, int scrollOffset)
{
    constexpr float topStart = 120.f;
    constexpr float lineHeight = 40.f;
    return topStart + static_cast<float>(rowIndex) * lineHeight -
           static_cast<float>(scrollOffset) * lineHeight;
}

int originalVisibleRows(int windowHeight)
{
    constexpr float topStart = 120.f;
    constexpr float lineHeight = 40.f;
    if (windowHeight <= topStart + lineHeight)
        return 0;
    return static_cast<int>((windowHeight - topStart - lineHeight) / lineHeight) + 1;
}
} // namespace

TEST_CASE("ReplayLayout: fixed history-list geometry constants", "[Replay][ReplayLayout]")
{
    REQUIRE(ReplayLayout::kRowTopStart == 120.f);
    REQUIRE(ReplayLayout::kRowHeight == 40.f);
    REQUIRE(ReplayLayout::kListLeftX == 50.f);
    REQUIRE(ReplayLayout::kListRightX == 1250.f);
}

TEST_CASE("ReplayLayout: history row top matches the original formula and clips into the header", "[Replay][ReplayLayout]")
{
    // Row 0 at scroll 0 sits exactly at the header boundary; higher rows step down by lineHeight.
    REQUIRE(ReplayLayout::historyRowTop(0, 0) == 120.f);
    REQUIRE(ReplayLayout::historyRowTop(1, 0) == 160.f);
    REQUIRE(ReplayLayout::historyRowTop(3, 0) == 240.f);

    // Scrolling up shifts rows by whole lineHeight steps; once a row's top would fall under the
    // header (top < 120) the helper returns -1 so callers skip it.
    REQUIRE(ReplayLayout::historyRowTop(0, 1) == -1.f);
    REQUIRE(ReplayLayout::historyRowTop(0, 2) == -1.f);
    REQUIRE(ReplayLayout::historyRowTop(3, 1) == 200.f);

    // Byte-for-byte vs the original per-row expression over a representative index/scroll grid:
    // raw rows (>= header) match exactly; rows scrolled under the header become -1 (skipped).
    for (int i = 0; i < 24; ++i)
        for (int s = 0; s < 8; ++s)
        {
            float raw = originalRowTop(i, s);
            if (raw < 120.f)
                REQUIRE(ReplayLayout::historyRowTop(i, s) == -1.f);
            else
                REQUIRE(ReplayLayout::historyRowTop(i, s) == raw);
        }
}

TEST_CASE("ReplayLayout: x hit-test band is inclusive at both edges", "[Replay][ReplayLayout]")
{
    REQUIRE(ReplayLayout::inSelectableXBand(50.f));
    REQUIRE(ReplayLayout::inSelectableXBand(1250.f));
    REQUIRE(!ReplayLayout::inSelectableXBand(49.9f));
    REQUIRE(!ReplayLayout::inSelectableXBand(1250.1f));
    REQUIRE(ReplayLayout::inSelectableXBand(640.f));
}

TEST_CASE("ReplayLayout: visible row count matches the original formula", "[Replay][ReplayLayout]")
{
    // A window that only clears the header (<= 160px) shows nothing.
    REQUIRE(ReplayLayout::visibleHistoryRows(160) == 0);
    REQUIRE(ReplayLayout::visibleHistoryRows(159) == 0);
    // Just over the header -> exactly one row; larger windows scale by lineHeight.
    REQUIRE(ReplayLayout::visibleHistoryRows(161) == 1);
    REQUIRE(ReplayLayout::visibleHistoryRows(720) == 15);

    for (int h = 100; h <= 1080; h += 7)
        REQUIRE(ReplayLayout::visibleHistoryRows(h) == originalVisibleRows(h));
}
