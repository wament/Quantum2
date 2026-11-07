#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Constants.hpp"

// Custom Game Rules. A single, narrow data struct describing the tunable table rules. Pure logic and
// free of any SFML dependency so it is headless-safe: it can be constructed, serialized, edited, and asserted over in
// the Graphics-only test binary. Its default values mirror the compile-time Constants exactly (NUM_DECKS=6, dealer
// hits soft 17 = H17, DAS/resplit aces/tents on, late surrender)
// no-op for every existing test and player until someone changes a rule.
//
// Threading model (CONTEXT.md §6 #2): these values are read directly at their own call sites: dealerTurn() soft-17,
// doubleDown() DAS gate, split()/RoundEngine re-split + max hands, the advisor's double-after-split / surrender gates,
// and Deck::create() deck count: rather than rewriting a broad surface. The plain BasicStrategy overload keeps the
// old signature by delegating here with these defaults

enum class SurrenderType
{
    None, // surrender disabled
    Late, // surrender offered on the first two cards (matches the current default behaviour)
    Early, // surrender only when the dealer's up-card is an Ace or ten-value card
    HitOrStand // surrender offered as a hit-or-stand choice; mechanically like Late here
};

struct GameRules
{
    int numDecks = Constants::NUM_DECKS;                    // shoe size (1/2/4/6/8 in practice)
    bool dealerStandsOnSoft17 = false;                     // false == current default: dealer HITS soft 17 (H17)
    bool doublesAfterSplitAllowed = Constants::DOUBLE_AFTER_SPLIT_ALLOWED;
    bool resplitAcesAllowed = Constants::RESPLIT_ACES_ALLOWED;
    bool resplitTensAllowed = Constants::RESPLIT_TENS_ALLOWED;
    SurrenderType surrenderType = SurrenderType::Late;
    int maxSplitHands = Constants::MAX_SPLIT_HANDS;

    // True when surrender is offered at all (any non-None type). Drives the advisor's 'R' availability and the
    // surrender() guard so a "no surrender" rule refuses cleanly.
    bool surrenderEnabled() const { return surrenderType != SurrenderType::None; }
};

// Free-function nlohmann/json hooks (found via ADL in the global namespace)
// wherever it is used: including Serialization.hpp
// no extra translation unit must see them. Stored as one compact nested object with an int-encoded surrender type;
// additive on load (missing fields keep the struct's defaults).
inline void to_json(nlohmann::json& j, const GameRules& r)
{
    j = {
        {"numDecks", r.numDecks},
        {"dealerStandsOnSoft17", r.dealerStandsOnSoft17},
        {"doublesAfterSplitAllowed", r.doublesAfterSplitAllowed},
        {"resplitAcesAllowed", r.resplitAcesAllowed},
        {"resplitTensAllowed", r.resplitTensAllowed},
        {"surrenderType", static_cast<int>(r.surrenderType)},
        {"maxSplitHands", r.maxSplitHands}
    };
}

inline void from_json(const nlohmann::json& j, GameRules& r)
{
    // Additive: every field falls back to the struct's own default when absent (legacy profiles keep loading).
    r.numDecks = j.value("numDecks", r.numDecks);
    r.dealerStandsOnSoft17 = j.value("dealerStandsOnSoft17", r.dealerStandsOnSoft17);
    r.doublesAfterSplitAllowed = j.value("doublesAfterSplitAllowed", r.doublesAfterSplitAllowed);
    r.resplitAcesAllowed = j.value("resplitAcesAllowed", r.resplitAcesAllowed);
    // Additive B12 migration: read the corrected key first, then fall back to the legacy typo'd key ("resplitTentsAllowed")
    // so saves written before this rename still load; when both are present the corrected key wins.
    r.resplitTensAllowed = j.value("resplitTensAllowed",
                                   j.value("resplitTentsAllowed", r.resplitTensAllowed));
    int si = j.value("surrenderType", static_cast<int>(r.surrenderType));
    r.surrenderType = static_cast<SurrenderType>((si % 4 + 4) % 4);

    // maxSplitHands is additive; clamp any stored value into the allowed set {2,3,4,-1}. Invalid or absent values
    // fall back to the struct default (Constants::MAX_SPLIT_HANDS). -1 == unlimited.
    int msh = j.value("maxSplitHands", r.maxSplitHands);
    if (msh == 2 || msh == 3 || msh == 4)
        r.maxSplitHands = msh;
    else if (msh < 0)
        r.maxSplitHands = Constants::MAX_SPLIT_HANDS_UNLIMITED;
    else
        r.maxSplitHands = Constants::MAX_SPLIT_HANDS;
}

// The rules-config UI is driven over this ordered list of rows, each with a stable id + label. Kept here (not in the
// renderer) so GameRenderer can render any future row without knowing rule internals, and so the edit logic stays testable.
enum class GameRuleId
{
    NumDecks,
    DealerStandsOnSoft17,
    DoublesAfterSplit,
    ResplitAces,
    ResplitTens,
    MaxSplitHands,
    Surrender
};

struct GameRuleRow
{
    GameRuleId id;
    const char* label;
};

// Ordered rows as shown in the rules-config panel (label text lives here
const std::vector<GameRuleRow>& gameRuleRows();

// Human-readable current value for one row, e.g. "6 decks", "Dealer hits soft 17 (H17)", "Surrender: Late".
std::string gameRuleDescription(const GameRules& rules, GameRuleId id);

// Apply a single directional edit to `rules` on the given row: toggle booleans, cycle the deck-count brackets and the
// surrender types. Used by the in-game rules editor (InputHandler/inline), kept out of the renderer
void applyGameRuleEdit(GameRules& rules, GameRuleId id, bool forward);