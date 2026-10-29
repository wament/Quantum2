#include "Achievement.hpp"
#include <algorithm>

namespace
{
    constexpr int kTextbookStreak = 50;   // consecutive strategy-following hands for Textbook Player (career)
    constexpr int kCountStreak = 10;      // positive-true-count hands in a row for Counter's Edge (shoe)
    constexpr int kStreakBjs = 3;         // blackjacks in one shoe for BlackJack Streak
    constexpr int kSplitTotal = 5;        // split pairs in one shoe for Split Master
    constexpr double kComebackThreshold = 500.0;   // $500 down then $500 up within a shoe (Comeback King)
    constexpr double kBigBetRatio = 0.10;           // wager >= 10% of bankroll for Big Bet
}

AchievementManager::AchievementManager()
{
    // Build the static achievement table (id + name/description) once. The unlocked flag is filled from unlocked_ so a
    // loadUnlockedFrom()'d persisted state shows up immediately in all().
    const std::vector<std::pair<Achievement::Id, std::pair<const char*, const char*>>> table = {
        {Achievement::Id::FirstDeal,        {"First Deal", "Play your first hand"}},
        {Achievement::Id::Natural21,        {"Natural 21", "Get your first blackjack"}},
        {Achievement::Id::SplitSuccess,     {"Split Success", "Split a pair and win both hands"}},
        {Achievement::Id::CountersEdge,     {"Counter's Edge", "Hold a positive true count for 10 consecutive hands (shoe)"}},
        {Achievement::Id::BigBet,           {"Big Bet", "Wager 10% or more of your bankroll"}},
        {Achievement::Id::KellyMaster,      {"Kelly Master", "Use full Kelly for an entire shoe (shoe)"}},
        {Achievement::Id::ComebackKing,     {"Comeback King", "Win $500 after being down $500 in a shoe (shoe)"}},
        {Achievement::Id::PerfectShoe,      {"Perfect Shoe", "Finish a shoe ahead without busting (shoe)"}},
        {Achievement::Id::SplitMaster,      {"Split Master", "Split 5 pairs in one shoe (shoe)"}},
        {Achievement::Id::BlackJackStreak,  {"Blackjack Streak", "Get 3 blackjacks in one shoe (shoe)"}},
        {Achievement::Id::TextbookPlayer,   {"Textbook Player", "Play 50 consecutive hands per basic strategy"}},
        {Achievement::Id::DeviationFree,    {"Deviation Free", "Finish a shoe with zero strategy deviations (shoe)"}},
    };
    definitions_.reserve(table.size());
    for (const auto& [id, names] : table)
        definitions_.push_back(Achievement{id, names.first, names.second, unlocked_.count(id) > 0});
}

bool AchievementManager::isUnlocked(Achievement::Id id) const
{
    const auto it = unlocked_.find(id);
    return it != unlocked_.end() && it->second;
}

void AchievementManager::unlock(Achievement::Id id, std::vector<Achievement::Id>& newlyUnlocked)
{
    if (unlocked_[id])
        return;  // already unlocked — don't report a repeat unlock within the same call/shoe
    unlocked_[id] = true;
    for (auto& def : definitions_)
        if (def.id == id)
            def.unlocked = true;
    newlyUnlocked.push_back(id);
}

void AchievementManager::resetPerShoe()
{
    shoeHands_ = 0;
    shoeBlackjacks_ = 0;
    shoeSplitWins_ = 0;
    positiveCountStreak_ = 0;
    anyDeviationThisShoe_ = false;
    kellyFullAllShoe_ = true;
    kellyHands_ = 0;
    // Bankroll swing tracking (seed flag + start/low/high/last) is deliberately kept across the boundary so Comeback King
    // can measure a worst-to-recovered dip that spans shoes; career counters and permanent achievements are untouched here.
    anyBustThisShoe_ = false;
}

std::vector<Achievement::Id> AchievementManager::recordHand(const HandEvent& e)
{
    std::vector<Achievement::Id> newlyUnlocked;

    // Career counters advance on every real hand.
    careerHands_++;
    perfectStreak_ = e.followedStrategy ? perfectStreak_ + 1 : 0;
    if (e.isBlackjack)
        careerBlackjacks_++;
    if (e.splitHand && e.wonAny)
        careerSplitWins_++;

    // Per-shoe counters. A non-full-kelly hand permanently breaks the "entire shoe full Kelly" condition for this shoe.
    shoeHands_++;
    if (e.isBlackjack)
        shoeBlackjacks_++;
    if (e.splitHand && e.wonAny)
        shoeSplitWins_++;
    positiveCountStreak_ = e.positiveTrueCount ? positiveCountStreak_ + 1 : 0;
    if (!e.followedStrategy)
        anyDeviationThisShoe_ = true;
    if (e.busted)
        anyBustThisShoe_ = true;
    if (!e.kellyFull)
        kellyFullAllShoe_ = false;

    // Bankroll tracking for per-shoe swing achievements. Seed the baseline on the first hand of the shoe.
    if (!bankrollSeeded_)
    {
        shoeBankrollStart_ = e.bankrollAfter;
        shoeBankrollLow_ = e.bankrollAfter;
        shoeBankrollHigh_ = e.bankrollAfter;
        bankrollSeeded_ = true;
    }
    shoeBankrollLow_ = std::min(shoeBankrollLow_, e.bankrollAfter);
    shoeBankrollHigh_ = std::max(shoeBankrollHigh_, e.bankrollAfter);
    shoeLastBankroll_ = e.bankrollAfter;

    // ---- Inline (per-hand) career unlocks ----
    if (careerHands_ == 1) unlock(Achievement::Id::FirstDeal, newlyUnlocked);
    if (careerBlackjacks_ >= 1) unlock(Achievement::Id::Natural21, newlyUnlocked);
    if (careerSplitWins_ >= 1) unlock(Achievement::Id::SplitSuccess, newlyUnlocked);
    if (e.bet > 0 && e.bankrollAfter > 0.0f && static_cast<double>(e.bet) >= kBigBetRatio * e.bankrollAfter)
        unlock(Achievement::Id::BigBet, newlyUnlocked);
    if (perfectStreak_ >= kTextbookStreak) unlock(Achievement::Id::TextbookPlayer, newlyUnlocked);

    // ---- Inline (per-hand) per-shoe unlocks ----
    if (positiveCountStreak_ >= kCountStreak) unlock(Achievement::Id::CountersEdge, newlyUnlocked);
    if (shoeBlackjacks_ >= kStreakBjs) unlock(Achievement::Id::BlackJackStreak, newlyUnlocked);
    if (shoeSplitWins_ >= kSplitTotal) unlock(Achievement::Id::SplitMaster, newlyUnlocked);

    return newlyUnlocked;
}

std::vector<Achievement::Id> AchievementManager::onNewShoe()
{
    std::vector<Achievement::Id> newlyUnlocked;

    // Resolve the conditions that only make sense once a shoe has actually been played, using its accumulated progress.
    if (shoeHands_ > 0)
    {
        if (!anyDeviationThisShoe_)
            unlock(Achievement::Id::DeviationFree, newlyUnlocked);
        if (kellyFullAllShoe_)
            unlock(Achievement::Id::KellyMaster, newlyUnlocked);
        if (!anyBustThisShoe_ && shoeLastBankroll_ > shoeBankrollStart_)
            unlock(Achievement::Id::PerfectShoe, newlyUnlocked);

        // Comeback King: was down at least the threshold from the start, then recovered at least the threshold above
        // that low point. Both measured against the shoe's bankroll extremes.
        const float wasDownBy = shoeBankrollStart_ - shoeBankrollLow_;
        const float gainedAfterLow = shoeLastBankroll_ - shoeBankrollLow_;
        if (wasDownBy >= kComebackThreshold && gainedAfterLow >= kComebackThreshold)
            unlock(Achievement::Id::ComebackKing, newlyUnlocked);
    }

    // Re-lock the inline, per-shoe achievements so every fresh shoe lets them be re-earned. Career achievements and the
    // end-of-shoe-resolved ones above stay permanently unlocked (a badge of honour): several tests assert isUnlocked() stays
    // true right after the resolving onNewShoe(). Only these three reset their "unlocked" flag at a shoe boundary.
    auto reLock = [&](Achievement::Id id)
    {
        unlocked_[id] = false;
        for (auto& def : definitions_)
            if (def.id == id)
                def.unlocked = false;
    };
    reLock(Achievement::Id::CountersEdge);
    reLock(Achievement::Id::BlackJackStreak);
    reLock(Achievement::Id::SplitMaster);

    resetPerShoe();
    return newlyUnlocked;
}

void AchievementManager::persistUnlockedTo(std::vector<int>& outIds) const
{
    outIds.clear();
    for (const auto& [id, on] : unlocked_)
        if (on)
            outIds.push_back(Achievement::toInt(id));
}

void AchievementManager::loadUnlockedFrom(const std::vector<int>& inIds)
{
    std::vector<Achievement::Id> discard;  // unlock() also stamps definitions_ flags, which is what we want live
    for (int v : inIds)
        unlock(Achievement::fromInt(v), discard);
}
