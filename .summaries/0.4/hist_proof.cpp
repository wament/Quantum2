// B13 standalone proof — quitting mid-shoe silently deletes those hands from career stats.
//
// This is the program described in CONTEXT.md section 6.10. It drives the real PlayerProfile with no game,
// no display and no test framework, so the defect can be re-demonstrated in 30 seconds without touching the
// suite. It was originally written to /tmp/hist_proof.cpp, which did not survive a reboot; it now lives
// beside these docs so it cannot be lost again.
//
// Build and run (from the repo root):
//   g++ -std=c++17 -I include -I /usr/local/include .summaries/0.4/hist_proof.cpp \
//       src/PlayerProfile.cpp src/Logger.cpp \
//       -o /tmp/hist_proof -L/usr/local/lib -lsfml-graphics -lsfml-system
//   /tmp/hist_proof
//
// Expected while B13 is OPEN:
//   reload #1: handsFinished=8  shoes=1   (expected 8)
//   reload #2: handsFinished=6  shoes=1   (expected 9)
//   BUG: hands silently lost from career stats
//
// Expected once B13 is FIXED (currentHistory serialised): the second reload reports 9 and it prints OK.

#include <iostream>
#include <string>
#include <vector>

#include "PlayerProfile.hpp"

namespace
{
// One settled, winning human hand. handsFinished is derived from the record as
// (wins + losses + pushes), so a single winning HandOutcome books exactly one career hand.
RoundRecord makeRound()
{
    RoundRecord r;
    HandOutcome h;
    h.wager = 10;
    h.payout = 20;
    h.won = true;
    h.playerIndex = 0;
    h.playerName = "William";
    r.playerHands.push_back(h);
    return r;
}

void addRounds(PlayerProfile& p, int n)
{
    for (int i = 0; i < n; ++i)
        p.addRoundRecord(makeRound());
}

int countShoes(const PlayerProfile& p)
{
    return static_cast<int>(p.getLongHistory().size());
}
} // namespace

int main()
{
    const std::string path = "/tmp/hist_proof_profile.json";

    // --- Shoe 1: five rounds, then the cut card -> the shoe is filed away complete.
    PlayerProfile p;
    addRounds(p, 5);
    ShoeRecord shoe;
    shoe.rounds = std::vector<RoundRecord>(5, makeRound());
    shoe.completed = true;
    p.addShoeRecord(shoe);

    // --- Shoe 2: three rounds played, then the player closes the window mid-shoe.
    addRounds(p, 3);
    p.save(path);

    PlayerProfile a = PlayerProfile::load(path);
    const int aHands = a.stats.handsFinished;
    const int aShoes = countShoes(a);
    std::cout << "reload #1: handsFinished=" << aHands << "  shoes=" << aShoes << "   (expected 8)\n";

    // --- A new session continues that shoe with one more round, then saves again.
    addRounds(a, 1);
    a.save(path);

    PlayerProfile b = PlayerProfile::load(path);
    const int bHands = b.stats.handsFinished;
    const int bShoes = countShoes(b);
    std::cout << "reload #2: handsFinished=" << bHands << "  shoes=" << bShoes << "   (expected 9)\n";

    if (bHands < 9)
    {
        std::cout << "BUG: hands silently lost from career stats\n";
        return 1;
    }
    std::cout << "OK\n";
    return 0;
}
