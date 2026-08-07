#include "PlayerProfile.hpp"
#include "Serialization.hpp"
#include "Constants.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

PlayerProfile::PlayerProfile(const std::string& playerName)
    : name(playerName)
{
}

const std::string& PlayerProfile::getName() const
{
    return name;
}

int PlayerProfile::getBankroll() const
{
    return bankroll;
}

void PlayerProfile::setBankroll(int amount)
{
    bankroll = amount;
}

std::vector<RoundRecord> PlayerProfile::getCurrentHistory()
{
    return currentHistory;
}

void PlayerProfile::addRoundRecord(const RoundRecord& record)
{
    currentHistory.push_back(record);
}

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
        std::cout << "No saved profile found at " << filename
                  << ". Starting fresh." << std::endl;
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
        std::cerr << "Error loading profile: " << e.what()
                  << ". Starting fresh." << std::endl;
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
        std::cerr << "Error: could not open " << filename
                  << " for writing." << std::endl;
        return;
    }

    json j = *this;
    file << j.dump(2);
}
