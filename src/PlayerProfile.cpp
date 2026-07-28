#include "PlayerProfile.hpp"
#include "Constants.hpp"


PlayerProfile::PlayerProfile(const std::string& playerName)
    : name(playerName)
{
    //load player profile 

}



/*
std::vector<RoundRecord> PlayerProfile::getCurrentHistory()
{
    return currentHistory;
}
*/

void PlayerProfile::addRoundRecord(const RoundRecord& record)
{
    currentHistory.push_back(record);
}


void PlayerProfile::addShoeRecord(const ShoeRecord& record)
{
    ShoeRecord shoe = record;

    shoe.rounds = currentHistory;

    longHistory.push_back(shoe);
}

    