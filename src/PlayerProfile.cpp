#include "PlayerProfile.hpp"
#include "Serialization.hpp"
#include "Constants.hpp"
#include "Logger.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

PlayerProfile::PlayerProfile(const std::string& playerName) : name(playerName) {}

const std::string& PlayerProfile::getName() const { return name; }
void PlayerProfile::setName(const std::string& name) { this->name = name; }

int PlayerProfile::getBankroll() const { return bankroll; }

void PlayerProfile::setBankroll(int amount) { bankroll = amount; }

void PlayerProfile::addRoundRecord(const RoundRecord& record) { currentHistory.push_back(record); }

namespace {

// Per-shoe recompute of exactly the record-derivable career counters save() writes from history, as a RollupCarry. computeCareerStats
// applies the same human-seat-only, per-shoe boundary rules live play uses (HandEvaluator/Game), so this equals what the shoe
// contributed before it rolled off longHistory. See PHASE 5 / T2.
RollupCarry rollupForShoe(const ShoeRecord& shoe)
{
    const PlayerStats st = computeCareerStats(shoe);
    RollupCarry r;
    r.wins = st.wins;
    r.losses = st.losses;
    r.pushes = st.pushes;
    r.handsFinished = st.handsFinished;
    r.blackjacks = st.blackjacks;
    r.greatestHand = st.greatestHand;
    r.greatestHandLoss = st.greatestHandLoss;
    r.handProfitCount = st.handProfitCount;
    r.handProfitSum = st.handProfitSum;     // also the shoe's net delta, used for session-running-total continuity below
    r.handProfitSumSq = st.handProfitSumSq;
    r.greatestWin = st.greatestWin;
    r.greatestLoss = st.greatestLoss;
    r.longestWinningStreak = st.longestWinningStreak;
    r.longestLosingStreak = st.longestLosingStreak;
    r.sideBetWins = st.sideBetWins;
    r.sideBetLosses = st.sideBetLosses;
    r.sideBetProfitSum = st.sideBetProfitSum;
    // session peak within this shoe, relative to its own start (floored/ceilinged at 0 like the live field)
    r.carriedGreatestSessionWin = st.greatestSessionWin;
    r.carriedGreatestSessionLoss = st.greatestSessionLoss;
    return r;
}

// Merge one rolled shoe into the running carry, preserving absolute session continuity. Shoes roll off oldest-first and are accumulated
// in that order, so carriedSessionTotal stays the true net from the very first shoe and the peak fields stay the exact career session
// extrema over the rolled region (see PlayerProfile::applyRollup).
void accumulateIntoRollup(RollupCarry& carry, const RollupCarry& shoe)
{
    // record-derivable sums add across shoes
    carry.wins += shoe.wins;
    carry.losses += shoe.losses;
    carry.pushes += shoe.pushes;
    carry.handsFinished += shoe.handsFinished;
    carry.blackjacks += shoe.blackjacks;
    carry.handProfitCount += shoe.handProfitCount;
    carry.handProfitSum += shoe.handProfitSum;
    carry.handProfitSumSq += shoe.handProfitSumSq;
    carry.sideBetWins += shoe.sideBetWins;
    carry.sideBetLosses += shoe.sideBetLosses;
    carry.sideBetProfitSum += shoe.sideBetProfitSum;

    // per-shoe peaks combine as career extrema
    carry.greatestHand = std::max(carry.greatestHand, shoe.greatestHand);
    carry.greatestHandLoss = std::min(carry.greatestHandLoss, shoe.greatestHandLoss);
    carry.longestWinningStreak = std::max(carry.longestWinningStreak, shoe.longestWinningStreak);
    carry.longestLosingStreak = std::max(carry.longestLosingStreak, shoe.longestLosingStreak);
    carry.greatestWin = std::max(carry.greatestWin, shoe.greatestWin);
    carry.greatestLoss = std::min(carry.greatestLoss, shoe.greatestLoss);

    // session-running-total continuity: absolute peak up to and including this shoe (compare in long long, the field width is int)
    const long long enteringTotal = carry.carriedSessionTotal;
    carry.carriedGreatestSessionWin = static_cast<int>(std::max(static_cast<long long>(carry.carriedGreatestSessionWin), enteringTotal + shoe.carriedGreatestSessionWin));
    carry.carriedGreatestSessionLoss = static_cast<int>(std::min(static_cast<long long>(carry.carriedGreatestSessionLoss), enteringTotal + shoe.carriedGreatestSessionLoss));
    carry.carriedSessionTotal += shoe.handProfitSum;   // net delta of this shoe -> absolute running total at its end
}

}  // namespace

// The router. Career stats are DERIVED from longHistory (careerStatsFromHistory + applyRollup), so the
// only way to record a Daily/tournament shoe WITHOUT it counting as career progress is to keep it out
// of that vector. Routing here makes that structural: every ShoeMode has an explicit case, so a mode
// cannot fall through into the career history by omission. Multiplayer has no producer yet and is
// routed to its own vector for exactly that reason -- adding it later must not silently pollute stats.
void PlayerProfile::addShoeRecord(const ShoeRecord& record)
{
    switch (record.mode)
    {
    case ShoeMode::DailyChallenge:
        addModeShoe(dailyShoes, dailyStats, record, Constants::Daily::MAX_HISTORY_SHOES);
        return;
    case ShoeMode::Tournament:
        addModeShoe(tournamentShoes, tournamentStats, record, Constants::TournamentHistory::MAX_HISTORY_SHOES);
        return;
    case ShoeMode::Multiplayer:
        addModeShoe(multiplayerShoes, multiplayerStats, record, Constants::MultiplayerHistory::MAX_HISTORY_SHOES);
        return;
    case ShoeMode::Regular:
        break;
    }
    addCareerShoe(record);
}

// Ordinary play. The shoe's rounds live in currentHistory (each settled round appends there), so the
// record is completed from it and the mirror is cleared -- the existing career behaviour, unchanged.
void PlayerProfile::addCareerShoe(const ShoeRecord& record)
{
    ShoeRecord shoe = record;
    shoe.rounds = currentHistory;
    longHistory.push_back(shoe);
    currentHistory.clear();
    // T2: bound completed-shoe history to MAX_LONG_HISTORY_SHOES. When the window would grow, roll the oldest shoe off into an aggregate
    // carry (accumulateIntoRollup) so its career contribution survives exactly in applyRollup(). Erasing from the front keeps longHistory
    // chronological, which computeCareerStatsHistory relies on for session-running-total continuity.
    while (longHistory.size() > Constants::MAX_LONG_HISTORY_SHOES)
    {
        accumulateIntoRollup(rollup, rollupForShoe(longHistory.front()));
        longHistory.erase(longHistory.begin());
    }
}

// A non-career mode's shoe. The caller supplies a COMPLETE record including its rounds: this path must
// never fill rounds from currentHistory, because on the explicit-commit path (read the career file,
// append, write it back) currentHistory is the CAREER's in-progress shoe, not this session's. Filling
// it here would attach the wrong hands to the record -- the kind of corruption a round-trip test
// catches and a comment alone does not.
//
// The aggregates are maintained here, incrementally, so a peak set by a shoe that later rolls off the
// cap survives -- the same reasoning as the career rollup carry, at a smaller scale.
void PlayerProfile::addModeShoe(std::vector<ShoeRecord>& shoes, ModeStats& stats, const ShoeRecord& record,
                                std::size_t cap)
{
    ShoeRecord shoe = record;
    shoes.push_back(shoe);

    stats.shoesPlayed += 1;
    stats.bestBankroll = std::max(stats.bestBankroll, shoe.bankrollAtShoeEnd);
    stats.mostHandsWon = std::max(stats.mostHandsWon, handsWonInShoe(shoe));

    // Front-erase keeps the vector chronological, matching longHistory's convention.
    while (shoes.size() > cap)
        shoes.erase(shoes.begin());
}

PlayerProfile PlayerProfile::load(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        Logger::warn() << "No saved profile found at " << filename << ". Starting fresh.";
        return PlayerProfile();
    }

    try
    {
        json j;
        file >> j;
        return j.get<PlayerProfile>();
    }
    catch (const std::exception& e)
    {
        // A corrupt or partially-written save must never be silently blanked: relocate it to a dated .corrupt.<ts>.json
        // copy so the user can recover it, then start fresh WITHOUT overwriting the damaged file. The next successful
        // save replaces that recovered copy via the atomic rename path in save().
        std::error_code ec;
        const auto epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                               .count();
        const fs::path base(filename);
        const fs::path recovered = base.parent_path() / (base.stem().string() + ".corrupt." + std::to_string(epoch) + base.extension().string());
        fs::rename(base, recovered, ec);   // error_code overload returns void; inspect ec afterwards
        if (ec)
            Logger::warn() << "Could not relocate corrupt profile " << filename << ": " << ec.message();
        else
            Logger::error() << "Corrupt profile at " << filename << " relocated to " << recovered.string()
                            << " (" << e.what() << "). Starting fresh.";

        return PlayerProfile();
    }
}

void PlayerProfile::save(const std::string& filename) const
{
    // Ensure save directory exists
    fs::path p(filename);
    fs::create_directories(p.parent_path());

    // Streamline persistence around a single source of truth. When there is recorded history, the career stats written to disk are
    // recomputed from those shoes via computeCareerStatsHistory --
    // load == save stays round-trip stable. Only record-derivable fields are overwritten; the live-tracked, non-recordable trackers
    // (splits/doubles/betsPlaced/betSum/betDistribution/strategy deviations) stay exactly as they were on the in-memory profile. A
    // profile with no shoes yet has nothing to recompute and is written verbatim
    PlayerProfile reconciled = *this;
    if (!longHistory.empty())
    {
        const PlayerStats authoritative = applyRollup();
        reconciled.stats.wins = authoritative.wins;
        reconciled.stats.losses = authoritative.losses;
        reconciled.stats.pushes = authoritative.pushes;
        reconciled.stats.handsFinished = authoritative.handsFinished;
        reconciled.stats.blackjacks = authoritative.blackjacks;
        reconciled.stats.greatestHand = authoritative.greatestHand;
        reconciled.stats.greatestHandLoss = authoritative.greatestHandLoss;
        reconciled.stats.handProfitCount = authoritative.handProfitCount;
        reconciled.stats.handProfitSum = authoritative.handProfitSum;
        reconciled.stats.handProfitSumSq = authoritative.handProfitSumSq;
        reconciled.stats.greatestWin = authoritative.greatestWin;
        reconciled.stats.greatestLoss = authoritative.greatestLoss;
        reconciled.stats.greatestSessionWin = authoritative.greatestSessionWin;
        reconciled.stats.greatestSessionLoss = authoritative.greatestSessionLoss;
        reconciled.stats.longestWinningStreak = authoritative.longestWinningStreak;
        reconciled.stats.longestLosingStreak = authoritative.longestLosingStreak;
        reconciled.stats.sideBetWins = authoritative.sideBetWins;
        reconciled.stats.sideBetLosses = authoritative.sideBetLosses;
        reconciled.stats.sideBetProfitSum = authoritative.sideBetProfitSum;
    }

    // Atomic write: stream to a sibling .tmp file, then rename over the target. fs::rename within one filesystem is atomic
    // on POSIX, so an interrupted or crashed write can never leave truncated JSON where a future load() would read it. The
    // recovery path in load() moves any pre-existing damaged file aside first, making this overwrite safe.
    const std::string tmp = filename + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out.is_open())
        {
            Logger::error() << "Error: could not open " << tmp << " for writing.";
            return;
        }
        json out_json = reconciled;
        // T1: compact, not dump(2). Pretty-printing was 64.6% of the real profile (6,199,262 of 9,591,391 bytes,
        // 427,145 lines) for a file no human reads; the load path parses either form identically.
        out << out_json.dump();
    }

    std::error_code ec;
    fs::rename(tmp, filename, ec);
    if (ec)
        Logger::error() << "Error: could not atomically move " << tmp << " to " << filename << ": " << ec.message();
}

const std::vector<ShoeRecord>& PlayerProfile::getLongHistory() const { return longHistory; }

const std::vector<ShoeRecord>& PlayerProfile::getDailyShoes() const { return dailyShoes; }
const std::vector<ShoeRecord>& PlayerProfile::getTournamentShoes() const { return tournamentShoes; }
const std::vector<ShoeRecord>& PlayerProfile::getMultiplayerShoes() const { return multiplayerShoes; }
const ModeStats& PlayerProfile::dailyModeStats() const { return dailyStats; }
const ModeStats& PlayerProfile::tournamentModeStats() const { return tournamentStats; }
const ModeStats& PlayerProfile::multiplayerModeStats() const { return multiplayerStats; }

PlayerStats PlayerProfile::careerStatsFromHistory() const
{
    // Stitch the completed shoes (longHistory) with the in-progress shoe's rounds (currentHistory)
    // those hands. The synthetic current shoe needs only .rounds
    std::vector<ShoeRecord> history = longHistory;
    if (!currentHistory.empty())
    {
        ShoeRecord inProgress;
        inProgress.rounds = currentHistory;
        inProgress.completed = false;
        history.push_back(std::move(inProgress));
    }
    return computeCareerStatsHistory(history);
}

PlayerStats PlayerProfile::applyRollup() const
{
    // Recent (capped) history recomputed via the same rules live play uses, then merged with the rolled-out-shoe carry using the identical
    // per-field semantics as accumulateIntoRollup. A zero carry is the merge identity (sums += 0; extrema max/min with 0 are unchanged
    // because those fields are already floored/ceilinged at 0), so with <= MAX_LONG_HISTORY_SHOES completed shoes this returns exactly
    // careerStatsFromHistory() and save()'s output is byte-identical to before — T2 is purely additive.
    PlayerStats out = careerStatsFromHistory();

    out.wins += rollup.wins;
    out.losses += rollup.losses;
    out.pushes += rollup.pushes;
    out.handsFinished += rollup.handsFinished;
    out.blackjacks += rollup.blackjacks;
    out.handProfitCount += rollup.handProfitCount;
    out.handProfitSum += rollup.handProfitSum;
    out.handProfitSumSq += rollup.handProfitSumSq;
    out.sideBetWins += rollup.sideBetWins;
    out.sideBetLosses += rollup.sideBetLosses;
    out.sideBetProfitSum += rollup.sideBetProfitSum;

    out.greatestHand = std::max(out.greatestHand, rollup.greatestHand);
    out.greatestHandLoss = std::min(out.greatestHandLoss, rollup.greatestHandLoss);
    out.longestWinningStreak = std::max(out.longestWinningStreak, rollup.longestWinningStreak);
    out.longestLosingStreak = std::max(out.longestLosingStreak, rollup.longestLosingStreak);
    out.greatestWin = std::max(out.greatestWin, rollup.greatestWin);
    out.greatestLoss = std::min(out.greatestLoss, rollup.greatestLoss);

    // greatestSession* offset by the carried session running-total so a capped window still yields the exact career peak (compare in long long, field width is int).
    const long long base = rollup.carriedSessionTotal;
    out.greatestSessionWin = static_cast<int>(std::max(static_cast<long long>(rollup.carriedGreatestSessionWin), base + out.greatestSessionWin));
    out.greatestSessionLoss = static_cast<int>(std::min(static_cast<long long>(rollup.carriedGreatestSessionLoss), base + out.greatestSessionLoss));
    return out;
}

// =============================================================================
// Daily Challenge bookkeeping. Pure profile logic: no clock is read here, every function takes the
// epoch/date-key it needs, so the streak rules are fully assertable headlessly with fabricated dates.
// =============================================================================

bool PlayerProfile::dailyHasActiveShoe() const { return dailyStartedOn != 0; }

bool PlayerProfile::dailyScoredOn(const std::string& dateKey) const
{
    return !dailyLastPlayed.empty() && dailyLastPlayed == dateKey;
}

void PlayerProfile::beginDailyShoe(const std::string& dateKey, long long nowEpochSeconds, unsigned long long seed)
{
    dailyStartedOn = nowEpochSeconds;
    dailyActiveSeed = seed;
    dailyActiveDateKey = dateKey;
    // dailyLastPlayed is deliberately NOT written here: the shoe is in progress, and the one-attempt
    // gate keys on the day it STARTED. Marking today scored at the start would lock the player out of
    // finishing the shoe they are mid-way through.
}

void PlayerProfile::clearDailyShoe()
{
    dailyStartedOn = 0;
    dailyActiveSeed = 0;
    dailyActiveDateKey.clear();
}

void PlayerProfile::commitDailyResult(const Daily::DailyResult& result, long long nowEpochSeconds)
{
    (void)nowEpochSeconds; // the result carries its own date key; the epoch is kept for call-site symmetry

    // Newest first, then capped, so the save file cannot grow without bound.
    dailyHistory.insert(dailyHistory.begin(), result);
    const std::size_t cap = static_cast<std::size_t>(Constants::Daily::MAX_HISTORY_RESULTS);
    if (dailyHistory.size() > cap)
        dailyHistory.resize(cap);

    clearDailyShoe();

    // A practice replay of a past Daily is recorded for the history view but must never move the
    // streak or the best score: the streak is only ever a count of distinct consecutive days scored.
    if (result.practice)
        return;

    dailyBest = std::max(dailyBest, result.net);

    if (dailyLastPlayed.empty())
    {
        dailyStreak = 1;
        dailyLastPlayed = result.dateKey;
        return;
    }

    const long long gap = Daily::daysBetweenKeys(dailyLastPlayed, result.dateKey);
    if (gap == 1)
    {
        // Consecutive day: the streak continues.
        dailyStreak += 1;
        dailyLastPlayed = result.dateKey;
    }
    else if (gap > 1)
    {
        // A lapse restarts the streak at 1, not 0: missing a day should not wipe the reward for
        // playing today.
        dailyStreak = 1;
        dailyLastPlayed = result.dateKey;
    }
    else if (gap == 0)
    {
        // Already scored today (the gate should prevent reaching this). Streak and anchor unchanged.
    }
    else
    {
        // gap < 0: a result dated in the past, i.e. a clock rollback. Never extend the streak, and
        // never rewind the anchor -- the safe direction is to leave the streak alone.
    }
}
