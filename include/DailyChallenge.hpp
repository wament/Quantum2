#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "Constants.hpp"
#include "GameRules.hpp"

// =============================================================================
// Daily Challenge: one shoe per calendar day, provably identical on every machine.
//
// There is no server, so the Daily cannot rank players against each other. What it CAN do is
// guarantee that the shoe dealt today is the same shoe on every install, offer exactly one scored
// attempt per day, and keep a streak the player protects. The value is the ritual and the
// self-comparison, not a leaderboard.
//
// Everything here is pure logic with no SFML dependency, so it is headless-safe: the seed function,
// the date arithmetic and the streak rules are all assertable in the test binary with no window.
// =============================================================================
namespace Daily
{
// -----------------------------------------------------------------------------
// Seeding. The seed must be identical on every install with nobody coordinating, which rules out
// the obvious one-liner: std::hash<std::string> is implementation-defined (libstdc++, libc++ and
// MSVC disagree, and libstdc++ randomises it per-process in profiled builds). A Daily seeded that
// way would be a different shoe on a different machine, silently. This is the same class of defect
// as the old std::shuffle shoe (CONTEXT.md "Shuffle determinism"): a value that looks portable
// and is not.
//
// FNV-1a 64-bit is specified here in full, so the shoe is defined by this program.
// -----------------------------------------------------------------------------
inline constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull; // 0xcbf29ce484222325
inline constexpr std::uint64_t kFnvPrime = 1099511628211ull;             // 0x100000001b3

inline std::uint64_t fnv1a(std::string_view text)
{
    std::uint64_t hash = kFnvOffsetBasis;
    for (char c : text)
    {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= kFnvPrime;
    }
    return hash;
}

// Bumping the salt changes every Daily shoe in history at once. It exists so a defect in the seed
// function can be fixed without every past Daily silently becoming a different shoe under the same
// name -- the same reason the shuffle golden test says "regenerate deliberately, never by fixing a
// red test".
inline constexpr const char* kSeedSalt = "quantum2-daily-v1";

// The seed for the Daily shoe of `dateKey` ("YYYY-MM-DD"). Salt first, so a future date-key format
// change cannot collide with a past one.
inline unsigned long long seedForDateKey(const std::string& dateKey)
{
    return static_cast<unsigned long long>(fnv1a(std::string(kSeedSalt) + dateKey));
}

// -----------------------------------------------------------------------------
// Civil-date arithmetic. Deliberately not <chrono> calendar types: the algorithms below are
// specified here, so "yesterday" means the same thing on every standard library. All of it works on
// the proleptic Gregorian calendar with no time zone; the time zone enters only where a local clock
// reading is converted into a date key.
// -----------------------------------------------------------------------------

// Days from 1970-01-01 for a civil date (days_from_civil). Exact for any real year.
long long daysFromCivil(int year, unsigned month, unsigned day);

// Inverse of daysFromCivil.
void civilFromDays(long long days, int& year, unsigned& month, unsigned& day);

// "YYYY-MM-DD" for a civil date.
std::string makeDateKey(int year, unsigned month, unsigned day);

// Parses "YYYY-MM-DD". Returns false (leaving the outparams untouched) on malformed input, so a
// corrupt save degrades instead of throwing.
bool parseDateKey(const std::string& dateKey, int& year, unsigned& month, unsigned& day);

// The calendar day immediately before `dateKey`. Returns "" for a malformed key, which callers treat
// as "not consecutive" -- the safe direction is to restart the streak, never to extend it.
std::string previousDateKey(const std::string& dateKey);

// Whole calendar days from `fromKey` to `toKey` (negative if toKey is earlier). 0 for malformed keys.
long long daysBetweenKeys(const std::string& fromKey, const std::string& toKey);

// Local calendar date key for an epoch-seconds value. "Today" is the PLAYER's local day: a player
// crossing a time zone changes it, which is accepted and documented rather than chased.
std::string dateKeyFromEpochSeconds(long long epochSeconds);

// Today's local date key.
std::string todayDateKey();

// -----------------------------------------------------------------------------
// The fixed rule set. Comparison is meaningless if two players run different rules, so the Daily
// ignores the player's custom rules. These mirror the compile-time defaults exactly, which keeps the
// Daily's default table identical to the app's.
// -----------------------------------------------------------------------------
GameRules dailyRules();

// -----------------------------------------------------------------------------
// The one-attempt gate, as a pure decision.
//
// It lives here rather than inline in InputHandler::activateMainMenuOption() for two reasons: that
// handler is private and needs a Game plus a profile to reach, and the gate is the Daily's whole
// rule set -- one scored attempt per day, a shoe in progress outranks everything, and a scored day
// still has a way to practise. A rule that cannot be asserted is a rule that quietly drifts.
// -----------------------------------------------------------------------------
enum class GateAction
{
    StartToday,        // nothing in progress and today unscored: deal today's shoe, scored
    ResumeActive,      // a shoe was started and not finished: resume it, whatever day it belongs to
    PracticeYesterday, // today is scored: no second scored attempt, offer the previous day as practice
    Nothing            // today is scored and there is no previous day to replay
};

// `hasActiveShoe` / `activeDateKey` / `lastPlayedKey` come straight off the profile. The active shoe
// is checked FIRST: the gate keys on the day a shoe STARTED, so a player who crosses midnight
// mid-shoe resumes the shoe they are playing instead of being handed a new one.
GateAction gateFor(const std::string& todayKey,
                   bool hasActiveShoe,
                   const std::string& activeDateKey,
                   const std::string& lastPlayedKey);

// The score of a Daily run: chips won over the fixed starting bankroll. Every player starts the
// Daily with the same bankroll, so `net` is comparable across days and machines.
inline constexpr int kStartingBankroll = 1000;

// One scored (or practice) Daily run, kept newest-first in PlayerProfile::dailyHistory.
struct DailyResult
{
    std::string dateKey;         // the day this result belongs to
    int net = 0;                 // chips won over kStartingBankroll
    bool isContinuation = false; // the shoe ran out mid-hand and continued (B22): kept so comparisons stay honest
    bool practice = false;       // a replay of a past Daily: never counts toward the streak
};

// JSON hooks, found by ADL (same pattern as GameRules). Written unconditionally inside the history
// array; read additively, so a profile without dailyHistory loads unchanged.
inline void to_json(nlohmann::json& j, const DailyResult& r)
{
    j = {
        {"dateKey", r.dateKey},
        {"net", r.net},
        {"isContinuation", r.isContinuation},
        {"practice", r.practice}
    };
}

inline void from_json(const nlohmann::json& j, DailyResult& r)
{
    r.dateKey = j.value("dateKey", std::string{});
    r.net = j.value("net", 0);
    r.isContinuation = j.value("isContinuation", false);
    r.practice = j.value("practice", false);
}

} // namespace Daily

