#pragma once

#include <string>

// Pure label builders for the main-menu rows that surface a per-mode aggregate. Kept free of any SFML
// type so they can be unit-tested headlessly and reused by GameRenderer without pulling graphics into
// a test TU -- the same shape as sidebet::resultLabel (SideBetLabels.hpp).
//
// Both follow one rule: a player who has never played that mode sees the PLAIN label, not a "streak 0"
// / "0 runs" that reads like a defect. Every string here is ASCII (U13: SFML 3 decodes std::string as
// ANSI, so a non-ASCII literal reaches sf::Text as mojibake).
namespace modestats
{
    // The Daily streak rides on its row: it is the whole retention hook.
    inline std::string dailyMenuLabel(int streak)
    {
        return streak > 0 ? "Daily Challenge   (streak " + std::to_string(streak) + ")"
                          : std::string("Daily Challenge");
    }

    // The tournament has no streak (a run is not a daily habit), so its row carries the run count and
    // the furthest stage ever reached. `bestFinish` is 0 when no stage has been recorded.
    inline std::string tournamentMenuLabel(int runsCompleted, int bestFinish)
    {
        if (runsCompleted <= 0 && bestFinish <= 0)
            return std::string("Tournament Mode");

        std::string label = "Tournament Mode   (";
        if (runsCompleted > 0)
            label += std::to_string(runsCompleted) + (runsCompleted == 1 ? " run" : " runs");
        if (bestFinish > 0)
        {
            if (runsCompleted > 0)
                label += ", ";
            label += "best stage " + std::to_string(bestFinish);
        }
        return label + ")";
    }

} // namespace modestats
