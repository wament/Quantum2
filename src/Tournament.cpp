#include "Tournament.hpp"

#include "BasicStrategy.hpp"

#include <algorithm>

using namespace BasicStrategy;

namespace
{
    // Clamp `stage` to the valid 1..stages range so callers (UI labels, HUD) never read out of bounds.
    int clampStage(int stage, int stages)
    {
        if (stage < 1)
            return 1;
        if (stage > stages)
            return stages;
        return stage;
    }
}

void Tournament::start(const TournamentConfig& config)
{
    config_ = config;
    // A sensible run always has at least one stage and a non-negative growth schedule; guard against malformed input.
    if (config_.stages < 1)
        config_.stages = 1;
    if (config_.minBetGrowth < 0)
        config_.minBetGrowth = 0;

    stage_ = 1;
    shoesPlayed_ = 0;
    complete_ = false;
}

char Tournament::aiAdvice(const Hand& hand, const Card& dealerUpCard) const
{
    int value = hand.getValue();
    bool hasAce = hand.isSoft();
    bool isPair = hand.size() == 2 &&
                  hand.getCard(0).getBlackjackValue() == hand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? hand.getCard(0).getBlackjackValue() : 0;

    // Tournament aggressions layered over the book play, in order of specificity -- both exist so seats try to grow their
    // bankroll inside the short window rather than playing conservatively to push:
    if (isPair && pairValue == 10)
        return 'P';                       // always split tens: a hard 20 is weak vs strong dealer up-cards in the short window
    if (!isPair && hand.size() == 2 && value == 8)
        return 'D';                       // double on any hard eight, even against dealers that normally stand

    // Everything else follows ordinary basic strategy unchanged.
    return getRecommendedAction(value, dealerUpCard.getBlackjackValue(), hasAce, isPair, pairValue);
}

int Tournament::minBetForStage(int stage) const
{
    int s = clampStage(stage, config_.stages);
    return config_.minBetStage1 + (s - 1) * config_.minBetGrowth;
}

bool Tournament::advanceStage()
{
    if (complete_)
        return false;

    ++shoesPlayed_;
    if (stage_ >= config_.stages)
    {
        complete_ = true;
        return false;
    }
    ++stage_;
    return true;
}

std::vector<std::size_t> Tournament::rankOrder(const std::vector<std::string>&, const std::vector<int>& bankrolls) const
{
    std::vector<std::size_t> order(bankrolls.size());
    for (std::size_t i = 0; i < bankrolls.size(); ++i)
        order[i] = i;

    // Stable sort by chips descending: equal bankrolls keep their original seat order, so the leaderboard + winner are
    // deterministic regardless of how the seats were dealt.
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return bankrolls[a] > bankrolls[b]; });
    return order;
}
