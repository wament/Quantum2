#include "DailyChallenge.hpp"

#include <cstdio>
#include <ctime>

namespace Daily
{
// -----------------------------------------------------------------------------
// Civil-date arithmetic (proleptic Gregorian, no time zone).
// -----------------------------------------------------------------------------
long long daysFromCivil(int year, unsigned month, unsigned day)
{
    long long y = static_cast<long long>(year);
    y -= (month <= 2);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);            // [0, 399]
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long days, int& year, unsigned& month, unsigned& day)
{
    long long z = days + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);              // [0, 146096]
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    long long y = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);             // [0, 365]
    const unsigned mp = (5 * doy + 2) / 153;                                   // [0, 11]
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;                           // [1, 31]
    const unsigned m = (mp < 10) ? (mp + 3u) : (mp - 9u);                       // [1, 12]
    y += (m <= 2);
    year = static_cast<int>(y);
    month = m;
    day = d;
}

std::string makeDateKey(int year, unsigned month, unsigned day)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", year, month, day);
    return std::string(buffer);
}

bool parseDateKey(const std::string& dateKey, int& year, unsigned& month, unsigned& day)
{
    // Strict "YYYY-MM-DD": ten characters, dashes at 4 and 7, digits elsewhere. Deliberately not
    // std::get_time, whose parsing is locale-dependent and lenient about separators.
    if (dateKey.size() != 10 || dateKey[4] != '-' || dateKey[7] != '-')
        return false;

    int values[3] = {0, 0, 0};
    const int spans[3][2] = {{0, 4}, {5, 7}, {8, 10}};
    for (int field = 0; field < 3; ++field)
    {
        for (int i = spans[field][0]; i < spans[field][1]; ++i)
        {
            const char c = dateKey[static_cast<std::size_t>(i)];
            if (c < '0' || c > '9')
                return false;
            values[field] = values[field] * 10 + (c - '0');
        }
    }

    const int y = values[0];
    const unsigned m = static_cast<unsigned>(values[1]);
    const unsigned d = static_cast<unsigned>(values[2]);
    if (m < 1 || m > 12 || d < 1 || d > 31)
        return false;

    year = y;
    month = m;
    day = d;
    return true;
}

std::string previousDateKey(const std::string& dateKey)
{
    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    if (!parseDateKey(dateKey, year, month, day))
        return std::string{};

    int py = 0;
    unsigned pm = 0;
    unsigned pd = 0;
    civilFromDays(daysFromCivil(year, month, day) - 1, py, pm, pd);
    return makeDateKey(py, pm, pd);
}

long long daysBetweenKeys(const std::string& fromKey, const std::string& toKey)
{
    int fy = 0, ty = 0;
    unsigned fm = 0, tm = 0, fd = 0, td = 0;
    if (!parseDateKey(fromKey, fy, fm, fd) || !parseDateKey(toKey, ty, tm, td))
        return 0;
    return daysFromCivil(ty, tm, td) - daysFromCivil(fy, fm, fd);
}

std::string dateKeyFromEpochSeconds(long long epochSeconds)
{
    std::time_t asTime = static_cast<std::time_t>(epochSeconds);
    std::tm broken{};

#if defined(_MSC_VER)
    if (localtime_s(&broken, &asTime) != 0)
        return std::string{};
#else
    if (localtime_r(&asTime, &broken) == nullptr)
        return std::string{};
#endif

    // tm_year is years since 1900; tm_mon is 0-based.
    return makeDateKey(broken.tm_year + 1900, static_cast<unsigned>(broken.tm_mon) + 1u,
                       static_cast<unsigned>(broken.tm_mday));
}

std::string todayDateKey()
{
    const long long now = static_cast<long long>(std::time(nullptr));
    return dateKeyFromEpochSeconds(now);
}

// The one rule set every Daily shoe uses. Written out in full rather than copied from the player's
// profile, so the Daily's rules are visible here and frozen by a test.
GameRules dailyRules()
{
    GameRules rules;
    rules.numDecks = Constants::NUM_DECKS;
    rules.dealerStandsOnSoft17 = false; // dealer hits soft 17 (H17), the app default
    rules.doublesAfterSplitAllowed = Constants::DOUBLE_AFTER_SPLIT_ALLOWED;
    rules.resplitAcesAllowed = Constants::RESPLIT_ACES_ALLOWED;
    rules.resplitTensAllowed = Constants::RESPLIT_TENS_ALLOWED;
    rules.surrenderType = SurrenderType::Late;
    rules.maxSplitHands = Constants::MAX_SPLIT_HANDS;
    return rules;
}

GateAction gateFor(const std::string& todayKey,
                   bool hasActiveShoe,
                   const std::string& activeDateKey,
                   const std::string& lastPlayedKey)
{
    // An unfinished shoe outranks everything, including a scored day: the gate keys on the day the
    // shoe STARTED, so midnight mid-shoe resumes the shoe in hand rather than dealing a new one.
    if (hasActiveShoe && !activeDateKey.empty())
        return GateAction::ResumeActive;

    // Today already scored: no second scored attempt. Practice yesterday if there is a yesterday.
    if (!lastPlayedKey.empty() && lastPlayedKey == todayKey)
        return previousDateKey(todayKey).empty() ? GateAction::Nothing : GateAction::PracticeYesterday;

    return GateAction::StartToday;
}

} // namespace Daily
