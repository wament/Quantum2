#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Constants.hpp"
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

  private:
    std::string name;
    int bankroll = 1000;

    std::vector<RoundRecord> currentHistory;
    std::vector<ShoeRecord> longHistory;

    // Allow JSON serialization/deserialization
    friend void to_json(json& j, const PlayerProfile& p);
    friend void from_json(const json& j, PlayerProfile& p);
};
