#pragma once

#include <string>
#include <vector>

#include "GameRecord.hpp"

struct PlayerStats
{
    int shoesFinished = 0;
    int handsFinished = 0;

    int wins = 0;
    int losses = 0;
    int pushes = 0;
    int blackjacks = 0;
    int splits = 0;
    int doubles = 0;

    int betsPlaced = 0;
    int betSum = 0;
    int largestHand = 0;

    int greatestWin = 0;
    int greatestLoss = 0;

    int greatestSessionWin = 0;
    int greatestSessionLoss = 0;
};



class PlayerProfile
{
public:
    PlayerProfile() = default;
    explicit PlayerProfile(const std::string& name);

    static PlayerProfile load(const std::string& filename);
    void save(const std::string& filename) const;


    void addRoundRecord(const RoundRecord& record);
    void addShoeRecord(const ShoeRecord& record);

    const std::string& getName() const;
    int getBankroll() const;

    void setBankroll(int amount);
    void addMoney(int amount);
    bool removeMoney(int amount);

    PlayerStats stats;
    std::vector<RoundRecord> getCurrentHistory();
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