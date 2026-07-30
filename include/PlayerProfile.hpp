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
    explicit PlayerProfile(const std::string& name);

    static PlayerProfile load(const std::string& filename);
    void save(const std::string& filename) const;

    // ---------------- Player Stats ----------------
    PlayerStats stats;
    const std::string& getName() const;
    int getBankroll() const; 
    //void setBankroll(int amount);
    //void addMoney(int amount);
    //bool removeMoney(int amount);

    // ---------------- Records ----------------
    std::vector<RoundRecord> getCurrentHistory();
    void addRoundRecord(const RoundRecord& record);
    void addShoeRecord(const ShoeRecord& record);

private:
    std::string name;
    int bankroll = 1000;

    std::vector<RoundRecord> currentHistory;
    std::vector<ShoeRecord> longHistory;
};

/*
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    PlayerStats,
    shoesFinished,
    handsFinished,
    wins,
    losses,
    pushes,
    blackjacks,
    splits,
    doubles,
    betsPlaced,
    betSum,
    largestHand,
    greatestWin,
    greatestLoss,
    greatestSessionWin,
    greatestSessionLoss
)


inline void to_json(json& j,const PlayerProfile& p)
{
    j =
    {
        {"name",p.getName()},
        {"bankroll",p.getBankroll()},
        {"stats",p.stats},
        {"currentHistory",p.currentHistory},
        {"longHistory",p.longHistory}
    };
}

void save()
{
    json j = *this;
    std::ofstream(filename) << j.dump(4);
}

PlayerProfile load()
{
    json j;
    std::ifstream(filename) >> j;

    return j.get<PlayerProfile>();
}
*/