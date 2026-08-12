#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "GameRecord.hpp"

using json = nlohmann::json;

class PlayerProfile
{
public:
    // ---------------- Utility ----------------
    PlayerProfile() = default;
    explicit PlayerProfile(const std::string &name);

    static PlayerProfile load(const std::string &filename);
    void save(const std::string &filename) const;

    // ---------------- Player Stats ----------------
    PlayerStats stats;
    const std::string &getName() const;
    int getBankroll() const;
    void setBankroll(int amount);

    // ---------------- Records ----------------
    std::vector<RoundRecord> getCurrentHistory();
    void addRoundRecord(const RoundRecord &record);
    void addShoeRecord(const ShoeRecord &record);

    const std::vector<ShoeRecord> &getLongHistory() const;

private:
    std::string name;
    int bankroll = 1000;

    std::vector<RoundRecord> currentHistory;
    std::vector<ShoeRecord> longHistory;

    // Allow JSON serialization/deserialization
    friend void to_json(json &j, const PlayerProfile &p);
    friend void from_json(const json &j, PlayerProfile &p);
};
