// The Progress tab's AXIS SELECTION, tested headlessly through the same accessors drawProgressTab()
// reads.
//
// The tab has two axes and the whole point of the design is that they are never conflated:
//   SOURCE = which recorded history the top chart reads (Career / Daily Challenge / Tournament) --
//          across shoes, across play windows. This is historybrowser::Source, the Replay browser's
//          model, reused.
//   SCOPE  = the bottom chart's window (this shoe / this session) -- within ONE open play window.
//          This is U12's axis.
// IDEAS-REVIEWED §G #6 was opened because both concepts are now in the product, and a single switch
// offering "session" and "Daily" as siblings would be offering two incommensurable things. These tests
// are the executable form of that decision: the axes are independent, and every series is labelled.

#include <catch2/catch_all.hpp>

#include "StatsDashboard.hpp"
#include "HistoryBrowser.hpp"
#include "GameRecord.hpp"

#include <string>
#include <vector>

namespace {

ShoeRecord shoeEndingAt(int bankroll)
{
    ShoeRecord s;
    s.bankrollAtShoeEnd = bankroll;
    return s;
}

StatsDashboard::ProgressData dataWith(bool career, bool daily, bool tournament)
{
    StatsDashboard::ProgressData d;
    if (career)
        d.career = {shoeEndingAt(1000), shoeEndingAt(900), shoeEndingAt(1100)};
    if (daily)
        d.daily = {shoeEndingAt(500), shoeEndingAt(600)};
    if (tournament)
        d.tournament = {shoeEndingAt(700)};
    d.shoeBankroll = {0.0, 10.0, -5.0};
    d.sessionBankroll = {0.0, 10.0, -5.0, 20.0, 15.0};
    return d;
}

bool asciiOnly(const char* text)
{
    for (const char* p = text; *p; ++p)
        if (static_cast<unsigned char>(*p) >= 0x80)
            return false;
    return true;
}

} // namespace

TEST_CASE("Progress is a fourth tab and the existing tab order is unchanged", "[ProgressTab]")
{
    StatsDashboard dash;
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Overview);

    dash.nextTabForTest();
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Hands);
    dash.nextTabForTest();
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Profits);
    dash.nextTabForTest();
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Progress);

    // Cycling wraps back to Overview, so the new tab did not break the cycle.
    dash.nextTabForTest();
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Overview);

    dash.prevTabForTest();
    CHECK(dash.currentTabForTest() == StatsDashboard::Tab::Progress);
}

TEST_CASE("the source axis opens on the first NON-EMPTY recorded history", "[ProgressTab]")
{
    // The Replay browser's rule, reused: the three histories are populated by three different session
    // kinds, so "exactly one is non-empty" is the common case, and a chart that opens on an empty list
    // reads as a broken feature.
    StatsDashboard careerOnly;
    careerOnly.setProgressData(dataWith(true, false, false));
    CHECK(careerOnly.progressSource() == historybrowser::Source::Career);

    StatsDashboard dailyOnly;
    dailyOnly.setProgressData(dataWith(false, true, false));
    CHECK(dailyOnly.progressSource() == historybrowser::Source::Daily);

    StatsDashboard tournamentOnly;
    tournamentOnly.setProgressData(dataWith(false, false, true));
    CHECK(tournamentOnly.progressSource() == historybrowser::Source::Tournament);

    // Nothing recorded at all: Career, which is the honest answer (an empty list, labelled as such).
    StatsDashboard empty;
    empty.setProgressData(StatsDashboard::ProgressData{});
    CHECK(empty.progressSource() == historybrowser::Source::Career);
    CHECK(empty.progressCareerSeries().empty());
}

TEST_CASE("cycling the source skips empty histories and is a no-op when only one is populated",
          "[ProgressTab]")
{
    StatsDashboard dash;
    dash.setProgressData(dataWith(true, false, true));

    CHECK(dash.progressSource() == historybrowser::Source::Career);
    dash.cycleProgressSource();
    CHECK(dash.progressSource() == historybrowser::Source::Tournament);   // Daily is empty: skipped
    dash.cycleProgressSource();
    CHECK(dash.progressSource() == historybrowser::Source::Career);

    // All three populated: a full cycle visits each.
    StatsDashboard all;
    all.setProgressData(dataWith(true, true, true));
    std::vector<int> seen;
    for (int i = 0; i < 3; ++i)
    {
        seen.push_back(static_cast<int>(all.progressSource()));
        all.cycleProgressSource();
    }
    CHECK(seen == std::vector<int>{0, 1, 2});

    // One populated source: cycling must not move onto an empty list. It stays put, so the keypress is
    // a no-op rather than a flicker onto a chart with nothing in it.
    StatsDashboard one;
    one.setProgressData(dataWith(true, false, false));
    for (int i = 0; i < 4; ++i)
        one.cycleProgressSource();
    CHECK(one.progressSource() == historybrowser::Source::Career);
}

TEST_CASE("the top chart reads exactly the selected source's shoes", "[ProgressTab]")
{
    StatsDashboard dash;
    dash.setProgressData(dataWith(true, true, true));

    dash.cycleProgressSource();   // Career -> Daily
    CHECK(dash.progressSource() == historybrowser::Source::Daily);
    CHECK(dash.progressCareerSeriesForTest() == std::vector<double>{500.0, 600.0});

    dash.cycleProgressSource();   // Daily -> Tournament
    CHECK(dash.progressCareerSeriesForTest() == std::vector<double>{700.0});

    dash.cycleProgressSource();   // Tournament -> Career
    CHECK(dash.progressCareerSeriesForTest() == std::vector<double>{1000.0, 900.0, 1100.0});
}

TEST_CASE("the scope axis is independent of the source axis", "[ProgressTab]")
{
    // The §G #6 decision, as an assertion: two axes, two controls, and moving one never moves the
    // other. A single switch over {Career, Daily, Tournament, session} is exactly what this rules out.
    StatsDashboard dash;
    dash.setProgressData(dataWith(true, true, true));

    CHECK_FALSE(dash.progressScopeIsSession());
    CHECK(dash.progressLiveSeriesForTest().size() == 3);      // this shoe

    dash.toggleProgressScope();
    CHECK(dash.progressScopeIsSession());
    CHECK(dash.progressLiveSeriesForTest().size() == 5);      // this session
    CHECK(dash.progressSource() == historybrowser::Source::Career);   // source untouched

    dash.cycleProgressSource();
    CHECK(dash.progressSource() == historybrowser::Source::Daily);
    CHECK(dash.progressScopeIsSession());                     // scope untouched
}

TEST_CASE("every chart is labelled with the axis it is showing", "[ProgressTab]")
{
    // The "any surface reading shoe history must say which mode it reads" rule, and the reason the
    // scope label names the window: an unlabelled career curve silently omits Daily play, and an
    // unlabelled live curve reads as a career curve when in fact it resets at every shoe boundary.
    StatsDashboard dash;
    dash.setProgressData(dataWith(true, true, true));

    CHECK(std::string(dash.progressSourceLabel()) == "Career");
    CHECK(std::string(dash.progressLiveLabel()).find("shoe") != std::string::npos);

    dash.cycleProgressSource();
    CHECK(std::string(dash.progressSourceLabel()) == "Daily Challenge");

    dash.toggleProgressScope();
    CHECK(std::string(dash.progressLiveLabel()).find("session") != std::string::npos);
    CHECK(std::string(dash.progressSourceLabel()) == "Daily Challenge");   // scope moved, source label did not

    // U13: every string that reaches sf::Text must be ASCII -- SFML 3 decodes std::string as ANSI.
    CHECK(asciiOnly(dash.progressSourceLabel()));
    CHECK(asciiOnly(dash.progressLiveLabel()));
}

