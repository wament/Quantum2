#include "GameRules.hpp"

namespace
{
    // Deck-count brackets a shoe actually uses. Cycling keeps the picker within realistic values instead of any integer.
    constexpr int DECK_BRACKETS[] = {1, 2, 4, 6, 8};

    SurrenderType surrenderCycle(int index)
    {
        // None -> Late -> Early -> HitOrStand -> (wrap to None). The index is the storage encoding.
        return static_cast<SurrenderType>((index % 4 + 4) % 4);
    }
}

const std::vector<GameRuleRow>& gameRuleRows()
{
    static const std::vector<GameRuleRow> rows = {
        {GameRuleId::NumDecks, "Decks"},
        {GameRuleId::DealerStandsOnSoft17, "Dealer on 17"},
        {GameRuleId::DoublesAfterSplit, "Double After Split"},
        {GameRuleId::ResplitAces, "Resplit Aces"},
        {GameRuleId::ResplitTens, "Resplit Tens"},
        {GameRuleId::MaxSplitHands, "Max Split Hands"},
        {GameRuleId::Surrender, "Surrender"}
    };
    return rows;
}

std::string gameRuleDescription(const GameRules& rules, GameRuleId id)
{
    switch (id)
    {
    case GameRuleId::NumDecks:
        return std::to_string(rules.numDecks) + " decks";
    case GameRuleId::DealerStandsOnSoft17:
        // Default is H17 (dealer hits soft 17); flipping the rule makes it stand on all 17s.
        return rules.dealerStandsOnSoft17 ? "Dealer stands on soft 17" : "Dealer hits soft 17 (H17)";
    case GameRuleId::DoublesAfterSplit:
        return rules.doublesAfterSplitAllowed ? "Allowed" : "Not allowed";
    case GameRuleId::ResplitAces:
        return rules.resplitAcesAllowed ? "Allowed" : "Not allowed";
    case GameRuleId::ResplitTens:
        return rules.resplitTensAllowed ? "Allowed" : "Not allowed";
    case GameRuleId::MaxSplitHands:
        // -1 is the unlimited sentinel; otherwise a plain count of simultaneous hands.
        return rules.maxSplitHands < 0 ? "Unlimited" : std::to_string(rules.maxSplitHands) + " hands";
    case GameRuleId::Surrender:
        switch (rules.surrenderType)
        {
        case SurrenderType::None:  return "Off";
        case SurrenderType::Late:  return "Late";
        case SurrenderType::Early: return "Early";
        case SurrenderType::HitOrStand: return "Hit/Stand";
        }
    }
    return "";
}

void applyGameRuleEdit(GameRules& rules, GameRuleId id, bool forward)
{
    const int dir = forward ? 1 : -1;
    switch (id)
    {
    case GameRuleId::NumDecks:
        // Step through the deck brackets, wrapping around.
        for (size_t i = 0; i < sizeof(DECK_BRACKETS) / sizeof(DECK_BRACKETS[0]); ++i)
        {
            if (rules.numDecks == DECK_BRACKETS[i])
            {
                int next = DECK_BRACKETS[(i + dir + sizeof(DECK_BRACKETS) / sizeof(DECK_BRACKETS[0])) %
                                         (sizeof(DECK_BRACKETS) / sizeof(DECK_BRACKETS[0]))];
                rules.numDecks = next;
                return;
            }
        }
        break;
    case GameRuleId::DealerStandsOnSoft17:
        rules.dealerStandsOnSoft17 = !rules.dealerStandsOnSoft17;
        return;
    case GameRuleId::DoublesAfterSplit:
        rules.doublesAfterSplitAllowed = !rules.doublesAfterSplitAllowed;
        return;
    case GameRuleId::ResplitAces:
        rules.resplitAcesAllowed = !rules.resplitAcesAllowed;
        return;
    case GameRuleId::ResplitTens:
        rules.resplitTensAllowed = !rules.resplitTensAllowed;
        return;
    case GameRuleId::MaxSplitHands:
    {
        // Step through the split-hand brackets (2, 3, 4, then Unlimited), wrapping around. -1 == unlimited; a
        // negative cap is guarded explicitly at every enforcement site so it never reads as a large finite cap.
        constexpr int SPLIT_HAND_LIMITS[] = {2, 3, 4, Constants::MAX_SPLIT_HANDS_UNLIMITED};
        const int n = sizeof(SPLIT_HAND_LIMITS) / sizeof(SPLIT_HAND_LIMITS[0]);
        for (size_t i = 0; i < n; ++i)
        {
            if (rules.maxSplitHands == SPLIT_HAND_LIMITS[i])
            {
                rules.maxSplitHands = SPLIT_HAND_LIMITS[(i + dir + n) % n];
                return;
            }
        }
        break;
    }
    case GameRuleId::Surrender:
        rules.surrenderType = surrenderCycle(static_cast<int>(rules.surrenderType) + dir);
        return;
    }
}
