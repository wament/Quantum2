#include "PlayerProfile.hpp"
#include "Serialization.hpp"
#include "Constants.hpp"
#include "Logger.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

PlayerProfile::PlayerProfile(const std::string& playerName) : name(playerName) {}

const std::string& PlayerProfile::getName() const { return name; }
void PlayerProfile::setName(const std::string& name) { this->name = name; }

int PlayerProfile::getBankroll() const { return bankroll; }

void PlayerProfile::setBankroll(int amount) { bankroll = amount; }

void PlayerProfile::addRoundRecord(const RoundRecord& record) { currentHistory.push_back(record); }

void PlayerProfile::addShoeRecord(const ShoeRecord& record)
{
    ShoeRecord shoe = record;
    shoe.rounds = currentHistory;
    longHistory.push_back(shoe);
    currentHistory.clear();
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
        const PlayerStats authoritative = careerStatsFromHistory();
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