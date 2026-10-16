#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Constants.hpp"
#include "GameRecord.hpp"

using json = nlohmann::json;

class PlayerProfile
{
  public:
    PlayerProfile() = default;
    explicit PlayerProfile(const std::string& name);

    static PlayerProfile load(const std::string& filename);
    void save(const std::string& filename) const;

    PlayerStats stats;

    // Last-used table settings, restored at launch so the main menu opens on what the player actually
    // plays with instead of the compile-time defaults. Stored as plain ints on purpose: GameSpeed and
    // BasicStrategy::CountingStyle are game enums, and the profile format should not depend on them.
    int preferredPlayerCount = GameConfig::DEFAULT_PLAYER_COUNT;
    int preferredSpeed = static_cast<int>(GameSpeed::Normal);
    int preferredCountingStyle = 1;  // BasicStrategy::CountingStyle::REKO

    const std::string& getName() const;
    int getBankroll() const;
    void setBankroll(int amount);

    void addRoundRecord(const RoundRecord& record);
    void addShoeRecord(const ShoeRecord& record);

    const std::vector<ShoeRecord>& getLongHistory() const;

  private:
    std::string name;
    int bankroll = 1000;

    std::vector<RoundRecord> currentHistory;
    std::vector<ShoeRecord> longHistory;

    // Allow JSON serialization/deserialization
    friend void to_json(json& j, const PlayerProfile& p);
    friend void from_json(const json& j, PlayerProfile& p);
};
