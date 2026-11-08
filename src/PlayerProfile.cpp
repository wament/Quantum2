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

void PlayerProfile::addShoeRecord(const ShoeRecord& record)
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
        out << out_json.dump(2);
    }

    std::error_code ec;
    fs::rename(tmp, filename, ec);
    if (ec)
        Logger::error() << "Error: could not atomically move " << tmp << " to " << filename << ": " << ec.message();
}

const std::vector<ShoeRecord>& PlayerProfile::getLongHistory() const { return longHistory; }

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