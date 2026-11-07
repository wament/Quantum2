#include <catch2/catch_all.hpp>

// B9 (overlay-guard half): drawSummaryOverlay renders one per-player round summary but indexes BOTH
// playerSummaries[i] and game.players[i], feeding i into getPlayerPosition too. When those two ever diverge in length
// the loop would read out of bounds on both vectors -- a real crash when a seat is cleared under a stale summary list.
// The guard is one shared, pure helper (no Game / SFML window needed), so its contract is asserted here headlessly:
// the row count can never exceed either vector's size, and the per-hand fan width cannot underflow for an empty seat.

// Include Game.hpp first: it pulls in GameRenderer.hpp at its line 63, before Game's own class body -- which owns a
// GameRenderer value member (Game.hpp) -- is parsed. That breaks the Game<->GameRenderer include cycle so both types end
// up complete; including GameRenderer.hpp afterwards is harmless (include guard).
#include "Game.hpp"
#include "GameRenderer.hpp"

#include <array>

TEST_CASE("B9: summaryOverlayRowCount clamps to min(summaryCount, playerCount)", "[B9][renderer]")
{
    // The bound must never exceed either side, so i < rows guarantees players[i] and playerSummaries[i] are valid.
    // The set spans the divergence cases that used to go out of bounds (summaries > seats and vice versa).
    const std::array<size_t, 6> counts = {0u, 1u, 2u, 3u, 5u, 8u};
    for (size_t summaryCount : counts)
    {
        for (size_t playerCount : counts)
        {
            const size_t rows = summaryOverlayRowCount(summaryCount, playerCount);
            REQUIRE(rows <= summaryCount);   // never reads past the summary strings
            REQUIRE(rows <= playerCount);    // never reads past the seats
            // And it is the largest such safe value -- never gratuitously smaller than a valid index range.
            REQUIRE(rows == std::min(summaryCount, playerCount));
        }
    }

    // The exact historical hazards: more summaries than seats, and fewer summaries than seats, both clamp to seats.
    REQUIRE(summaryOverlayRowCount(8u, 3u) == 3u);
    REQUIRE(summaryOverlayRowCount(3u, 8u) == 3u);
}

TEST_CASE("B9: summaryRowFanWidth has no unsigned underflow for an empty or single hand", "[B9][renderer]")
{
    // An empty/divergent seat and a lone un-split hand both contribute zero extra fan width -- the pre-fix
    // hands.size() - 1 would have underflowed size_t (~4e18) to a huge, mis-placed summary row.
    REQUIRE(summaryRowFanWidth(0u) == 0.f);
    REQUIRE(summaryRowFanWidth(1u) == 0.f);
    // Each additional split hand adds exactly one 16px slot (behaviour unchanged for real split hands).
    REQUIRE(summaryRowFanWidth(2u) == 16.f);
    REQUIRE(summaryRowFanWidth(3u) == 32.f);
    REQUIRE(summaryRowFanWidth(4u) == 48.f);
}
