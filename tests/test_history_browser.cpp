#include <catch2/catch_all.hpp>

#include "HistoryBrowser.hpp"
#include "ModeStatsLabels.hpp"
#include "PresentationLayout.hpp"  // isSfmlSafeAscii (U13)

#include <string>

// ---------------------------------------------------------------------------
// Per-mode presentation helpers.
//
// Two things the shoe-mode split needs on the UI side, both extracted from classes a test TU cannot
// construct (Replay takes an sf::RenderWindow/Texture/Font; GameRenderer needs a window) so the logic
// lives in SFML-free headers and is locked here -- the same shape as ReplayLayout.hpp /
// SideBetLabels.hpp.
//
//   * HistoryBrowser.hpp  -- the "Review History" source switch. Daily and tournament shoes live in
//     their own vectors, so the browser has to pick a source to have anything to show.
//   * ModeStatsLabels.hpp -- the main-menu rows that surface a per-mode aggregate.
//
// Every string here reaches sf::Text, so each case also asserts isSfmlSafeAscii: SFML 3 decodes
// std::string as ANSI, so a non-ASCII literal arrives as mojibake (U13).
// ---------------------------------------------------------------------------

using PresentationLayout::isSfmlSafeAscii;

using historybrowser::Source;
using Pop = historybrowser::Populated;

TEST_CASE("historybrowser: labels name each source and are ASCII-safe", "[HistoryBrowser][U13]")
{
    CHECK(std::string(historybrowser::label(Source::Career)) == "Career");
    CHECK(std::string(historybrowser::label(Source::Daily)) == "Daily Challenge");
    CHECK(std::string(historybrowser::label(Source::Tournament)) == "Tournament");
    for (int i = 0; i < historybrowser::kCount; ++i)
        REQUIRE(isSfmlSafeAscii(historybrowser::label(static_cast<Source>(i))));
}

TEST_CASE("historybrowser: cycling visits every populated source and wraps", "[HistoryBrowser]")
{
    const Pop all{true, true, true};
    CHECK(historybrowser::nextNonEmpty(Source::Career, all) == Source::Daily);
    CHECK(historybrowser::nextNonEmpty(Source::Daily, all) == Source::Tournament);
    CHECK(historybrowser::nextNonEmpty(Source::Tournament, all) == Source::Career);
}

// The common case: three session kinds populate three vectors, and most players have only one.
// Landing on a zero-row list reads as a broken feature, so empties are skipped.
TEST_CASE("historybrowser: cycling skips empty sources", "[HistoryBrowser]")
{
    const Pop careerAndTournament{true, false, true};
    CHECK(historybrowser::nextNonEmpty(Source::Career, careerAndTournament) == Source::Tournament);
    CHECK(historybrowser::nextNonEmpty(Source::Tournament, careerAndTournament) == Source::Career);

    const Pop dailyOnly{false, true, false};
    CHECK(historybrowser::nextNonEmpty(Source::Career, dailyOnly) == Source::Daily);
    CHECK(historybrowser::nextNonEmpty(Source::Daily, dailyOnly) == Source::Daily);
}

// Exactly one populated source: cycling must be a no-op, not a flicker back onto the same list.
TEST_CASE("historybrowser: cycling is a no-op when nothing else has shoes", "[HistoryBrowser]")
{
    const Pop careerOnly{true, false, false};
    CHECK(historybrowser::nextNonEmpty(Source::Career, careerOnly) == Source::Career);

    const Pop none{false, false, false};
    CHECK(historybrowser::nextNonEmpty(Source::Daily, none) == Source::Daily);
}

TEST_CASE("historybrowser: opening never lands on an empty view", "[HistoryBrowser]")
{
    const Pop dailyOnly{false, true, false};
    // Asked for Career, which is empty -> open on the source that has shoes.
    CHECK(historybrowser::preferredOpen(Source::Career, dailyOnly) == Source::Daily);
    // Asked for a populated source -> keep it.
    CHECK(historybrowser::preferredOpen(Source::Daily, dailyOnly) == Source::Daily);

    const Pop careerAndTournament{true, false, true};
    CHECK(historybrowser::preferredOpen(Source::Tournament, careerAndTournament) == Source::Tournament);
    CHECK(historybrowser::preferredOpen(Source::Daily, careerAndTournament) == Source::Career);

    // Nothing recorded at all: Career is the honest answer, and the view shows the empty-list message.
    const Pop none{false, false, false};
    CHECK(historybrowser::preferredOpen(Source::Tournament, none) == Source::Career);
}

TEST_CASE("modestats: the Daily row shows the streak, and stays plain at zero", "[ModeLabels][U13]")
{
    CHECK(modestats::dailyMenuLabel(0) == "Daily Challenge");
    CHECK(modestats::dailyMenuLabel(1) == "Daily Challenge   (streak 1)");
    CHECK(modestats::dailyMenuLabel(37) == "Daily Challenge   (streak 37)");
    REQUIRE(isSfmlSafeAscii(modestats::dailyMenuLabel(37)));
}

TEST_CASE("modestats: the Tournament row shows runs and best finish", "[ModeLabels][U13]")
{
    // Never played: the plain label, not "0 runs, best stage 0".
    CHECK(modestats::tournamentMenuLabel(0, 0) == "Tournament Mode");

    CHECK(modestats::tournamentMenuLabel(1, 0) == "Tournament Mode   (1 run)");
    CHECK(modestats::tournamentMenuLabel(3, 0) == "Tournament Mode   (3 runs)");
    // A run in progress records stages reached before any run has completed.
    CHECK(modestats::tournamentMenuLabel(0, 4) == "Tournament Mode   (best stage 4)");
    CHECK(modestats::tournamentMenuLabel(2, 6) == "Tournament Mode   (2 runs, best stage 6)");

    for (int runs : {0, 1, 2, 40})
        for (int best : {0, 1, 6})
            REQUIRE(isSfmlSafeAscii(modestats::tournamentMenuLabel(runs, best)));
}
