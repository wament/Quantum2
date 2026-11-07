#include <catch2/catch_all.hpp>

#include "Game.hpp"

#include <sstream>
#include <string>

// B11 -- the startup "PLAYER CONTROLS" banner mislabels the betting-phase keys (CONTEXT §11 / NEXT_STEPS). The real
// InputHandler.cpp PlaceBets bindings are Left = auto Kelly bet (calculateOptimalBet() + setBet) and Right = confirm.
// Game::startupControlsBanner() is the single source of truth for that printed block: it is pure (no SFML, no mutable
// Game state), so a headless Game(true) reads it directly and we assert the labels match the bindings. The two banner
// assertions ("Left Arrow" present / "Right Arrow" absent on the auto-kelly row) are FALSE before the fix -- the row
// read "Right Arrow   Auto Kelly Bet" -- so this test fails without B11 and passes after it.

namespace
{
    // Return the first banner line that contains needle (used to inspect a single control row independently).
    std::string lineContaining(const std::string& banner, const std::string& needle)
    {
        std::istringstream ss(banner);
        std::string line;
        while (std::getline(ss, line))
            if (line.find(needle) != std::string::npos)
                return line;
        return {};
    }
}

TEST_CASE("B11: startup banner labels Left=Auto Kelly and Right=Confirm", "[Game][B11]")
{
    Game game(true); // headless: the banner is pure, so no window or assets are needed to read it
    const std::string banner = game.startupControlsBanner();

    REQUIRE(!banner.empty());

    // Nothing was dropped when the inline banner was extracted into a helper.
    for (const char* row : {"Increase Bet", "Decrease Bet", "Auto Kelly Bet", "Confirm Bet",
                            "Hit", "Stand", "Double Down", "Split", "Surrender"})
        REQUIRE(banner.find(row) != std::string::npos);

    const std::string kellyLine = lineContaining(banner, "Auto Kelly");

    // Auto Kelly is bound to the LEFT arrow (InputHandler.cpp PlaceBets: Left -> calculateOptimalBet()/setBet).
    REQUIRE(kellyLine.find("Left Arrow") != std::string::npos);
    REQUIRE(kellyLine.find("Right Arrow") == std::string::npos);

    // The whole banner must not retain the old mislabel ("Right Arrow" fed the auto-kelly row) after the fix.
    REQUIRE(banner.find("Right Arrow") == std::string::npos);

    // Confirm is bound to the RIGHT arrow (Right -> confirmBet()); its row still renders.
    const std::string confirmLine = lineContaining(banner, "Confirm Bet");
    REQUIRE(!confirmLine.empty());
}
