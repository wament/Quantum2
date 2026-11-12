#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Constants.hpp"
#include "DailyChallenge.hpp"
#include "GameRecord.hpp"
#include "GameRules.hpp"

using json = nlohmann::json;

class PlayerProfile
{
  public:
    PlayerProfile() = default;
    explicit PlayerProfile(const std::string& name);

    static PlayerProfile load(const std::string& filename);
    void save(const std::string& filename) const;

    PlayerStats stats;

    // Last-used table settings, restored at launch
    // plays with instead of the compile-time defaults. Stored as plain ints on purpose: GameSpeed and
    // BasicStrategy::CountingStyle are game enums, and the profile format should not depend on them.
    int preferredPlayerCount = GameConfig::DEFAULT_PLAYER_COUNT;
    int preferredSpeed = static_cast<int>(GameSpeed::Normal);
    int preferredCountingStyle = 1;  // BasicStrategy::CountingStyle::REKO

    // Sound & music volume prefs, stored as plain ints in SFML's 0..100 units
    // any SFML dependency (mirrors preferredSpeed/preferredCountingStyle). Optional with defaults on load
    // saved before sound settings existed keep loading unchanged.
    int preferredSoundVolume = 100;
    int preferredMusicVolume = 60;
    bool preferredMuted = false;

    // the ids of achievements this player has unlocked, persisted as plain ints (see
    // Achievement::toInt/fromInt). Optional with an empty default on load
    // keep loading unchanged: additive schema, exactly like the sound settings above.
    std::vector<int> unlockedAchievementIds;

    // the player's preferred table rules. Stored as a self-contained nested object so
    // the schema stays additive: profiles saved before custom rules existed have no such key and keep their default
    // struct on load. Mirrors how sound settings + achievements ride along additively on the same profile save path.
    GameRules gameRules;

    // ---- Daily Challenge --------------------------------------------------
    // One shoe per calendar day, seeded from the date so it is identical on every machine. These
    // fields are ADDITIVE per the schema policy (CONTEXT.md §5): written unconditionally, read with
    // j.value(key, default), so a profile saved before the Daily existed loads unchanged with an
    // empty history and a zero streak.
    //
    // The Daily seat itself is EPHEMERAL (setPersistable(false)), so career bankroll/stats/history
    // and achievements can never be touched by a Daily run. The ONLY write a Daily makes is
    // commitDailyResult(), which touches the fields below and nothing else.
    std::string dailyLastPlayed;                  // "YYYY-MM-DD" of the last SCORED Daily, "" if never
    int dailyStreak = 0;                          // consecutive calendar days completed
    int dailyBest = 0;                            // best net on a scored Daily shoe
    long long dailyStartedOn = 0;                 // epoch seconds the active shoe began; 0 = no active shoe
    std::string dailyActiveDateKey;               // the day the ACTIVE shoe belongs to, so a quit resumes it
    unsigned long long dailyActiveSeed = 0;       // seed of the active (unfinished) shoe, so a quit resumes it
    std::vector<Daily::DailyResult> dailyHistory; // newest-first, capped at Constants::Daily::MAX_HISTORY_RESULTS

    // ---- Per-mode shoe history ------------------------------------------------
    // Daily and tournament shoes are RECORDED (round records and shoe records included) but kept out of
    // the career history entirely, because career stats are derived from longHistory: a shoe that lives
    // there counts as career progress by construction. addShoeRecord() routes on ShoeRecord::mode, so
    // a non-Regular shoe cannot reach longHistory -- the separation is structural, not a convention.
    //
    // Each vector is capped, and the matching ModeStats is maintained INCREMENTALLY so a rolled-off
    // shoe's peaks survive the cap (the same reasoning as the career rollup carry, at a smaller scale).
    std::vector<ShoeRecord> dailyShoes;
    std::vector<ShoeRecord> tournamentShoes;
    std::vector<ShoeRecord> multiplayerShoes;   // no producer yet; see the router note in PlayerProfile.cpp
    ModeStats dailyStats;
    ModeStats tournamentStats;
    ModeStats multiplayerStats;
    int tournamentsCompleted = 0;               // runs that reached the final stage

    const std::vector<ShoeRecord>& getDailyShoes() const;
    const std::vector<ShoeRecord>& getTournamentShoes() const;
    const std::vector<ShoeRecord>& getMultiplayerShoes() const;
    const ModeStats& dailyModeStats() const;
    const ModeStats& tournamentModeStats() const;
    const ModeStats& multiplayerModeStats() const;

    // True while a Daily shoe is in progress and unfinished. The one-attempt gate keys on the day the
    // shoe STARTED, not the day it finished, so a player who crosses midnight mid-shoe finishes the
    // shoe they started instead of being handed a new one.
    bool dailyHasActiveShoe() const;

    // True when today's Daily has already been scored.
    bool dailyScoredOn(const std::string& dateKey) const;

    // Marks the start of a scored Daily shoe.
    void beginDailyShoe(const std::string& dateKey, long long nowEpochSeconds, unsigned long long seed);

    // Abandons an active shoe without booking it (used when a Daily run is quit). Clears the active
    // fields so the player may start today's Daily again.
    void clearDailyShoe();

    // Books a finished Daily shoe: appends the result, updates the streak and best, and clears the
    // active-shoe fields. Practice runs are recorded but never move the streak.
    void commitDailyResult(const Daily::DailyResult& result, long long nowEpochSeconds);

    const std::string& getName() const;
    void setName(const std::string& name);
    int getBankroll() const;
    void setBankroll(int amount);

    void addRoundRecord(const RoundRecord& record);
    void addShoeRecord(const ShoeRecord& record);

    const std::vector<ShoeRecord>& getLongHistory() const;

    // Career stats recomputed from the recorded shoe history
    // the persisted profile can never drift from what was actually played. The in-progress shoe's rounds (currentHistory) are
    // folded in too, so a quit mid-shoe still books those hands. Only record-derivable fields come back here; the live-tracked,
    // non-recordable trackers (splits/doubles/betsPlaced/betSum/betDistribution/strategy deviations) stay on the in-memory profile
    // and are preserved verbatim by save().
    PlayerStats careerStatsFromHistory() const;

    // Career stats written by save(): the recent (capped) shoe history recomputed via careerStatsFromHistory, merged with the
    // aggregate carry of shoes that have rolled off the longHistory cap. Lossless — a zero carry is the merge identity, so with
    // <= MAX_LONG_HISTORY_SHOES completed shoes this returns exactly careerStatsFromHistory() and save()'s output is unchanged.
    PlayerStats applyRollup() const;

  private:
    std::string name;
    int bankroll = 1000;

    std::vector<RoundRecord> currentHistory;
    std::vector<ShoeRecord> longHistory;

    // T2 (100-shoe history cap): record-derivable career counters of shoes past the cap, aggregated so totals survive exactly.
    RollupCarry rollup;

    // The career branch of the addShoeRecord router, and the shared non-career branch. See the router's
    // note in PlayerProfile.cpp for why every mode is routed explicitly.
    void addCareerShoe(const ShoeRecord& record);
    void addModeShoe(std::vector<ShoeRecord>& shoes, ModeStats& stats, const ShoeRecord& record, std::size_t cap);

    // Allow JSON serialization/deserialization
    friend void to_json(json& j, const PlayerProfile& p);
    friend void from_json(const json& j, PlayerProfile& p);
};
