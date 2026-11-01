#include "PlayerProfile.hpp"
#include "Serialization.hpp"
#include "Constants.hpp"
#include "Logger.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

PlayerProfile::PlayerProfile(const std::string& playerName) : name(playerName) {}

const std::string& PlayerProfile::getName() const { return name; }

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
        Logger::error() << "Error loading profile: " << e.what() << ". Starting fresh.";
        return PlayerProfile();
    }
}

void PlayerProfile::save(const std::string& filename) const
{
    // Ensure save directory exists
    fs::path p(filename);
    fs::create_directories(p.parent_path());

    std::ofstream file(filename);
    if (!file.is_open())
    {
        Logger::error() << "Error: could not open " << filename << " for writing.";
        return;
    }

    // Streamline persistence around a single source of truth. When there is recorded history, the career stats written to disk are
    // recomputed from those shoes via computeCareerStatsHistory -- so the profile can never drift from what was actually played and
    // load == save stays round-trip stable. Only record-derivable fields are overwritten; the live-tracked, non-recordable trackers
    // (splits/doubles/betsPlaced/betSum/betDistribution/strategy deviations) stay exactly as they were on the in-memory profile. A
    // profile with no shoes yet has nothing to recompute and is written verbatim -- unchanged from before this refactor.
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

    json j = reconciled;
    file << j.dump(2);
}

const std::vector<ShoeRecord>& PlayerProfile::getLongHistory() const { return longHistory; }

PlayerStats PlayerProfile::careerStatsFromHistory() const
{
    // Stitch the completed shoes (longHistory) with the in-progress shoe's rounds (currentHistory), so a quit mid-shoe still books
    // those hands. The synthetic current shoe needs only .rounds -- computeCareerStatsHistory reads no other ShoeRecord field.
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