#pragma once

#include <nlohmann/json.hpp>

#include "Card.hpp"
#include "Hand.hpp"
#include "Deck.hpp"
#include "GameRecord.hpp"
#include "PlayerProfile.hpp"

using json = nlohmann::json;

// Card serialization
inline void to_json(json& j, const Card& c)
{
    j = {
        {"value", c.value},
        {"suit", static_cast<int>(c.suit)}
    };
}

inline void from_json(const json& j, Card& c)
{
    c.value = j.at("value").get<int>();
    c.suit = static_cast<Suit>(j.at("suit").get<int>());
}

// Suit serialization (helper, in case needed standalone)
inline void to_json(json& j, const Suit& s)
{
    j = static_cast<int>(s);
}

inline void from_json(const json& j, Suit& s)
{
    s = static_cast<Suit>(j.get<int>());
}

// PlayerStats serialization
inline void to_json(json& j, const PlayerStats& s)
{
    j = {
        {"shoesFinished", s.shoesFinished},
        {"handsFinished", s.handsFinished},
        {"wins", s.wins},
        {"losses", s.losses},
        {"pushes", s.pushes},
        {"blackjacks", s.blackjacks},
        {"splits", s.splits},
        {"doubles", s.doubles},
        {"betsPlaced", s.betsPlaced},
        {"betSum", s.betSum},
        {"greatestHand", s.greatestHand},
        {"greatestWin", s.greatestWin},
        {"greatestLoss", s.greatestLoss},
        {"greatestSessionWin", s.greatestSessionWin},
        {"greatestSessionLoss", s.greatestSessionLoss},
        {"winsAfterDeviation", s.winsAfterDeviation},
        {"greatestHandLoss", s.greatestHandLoss},
        {"longestWinningStreak", s.longestWinningStreak},
        {"longestLosingStreak", s.longestLosingStreak},
        {"handProfitCount", s.handProfitCount},
        {"handProfitSum", s.handProfitSum},
        {"handProfitSumSq", s.handProfitSumSq},
        {"betDistribution", s.betDistribution},
        {"sideBetWins", s.sideBetWins},
        {"sideBetLosses", s.sideBetLosses},
        {"sideBetProfitSum", s.sideBetProfitSum}
    };
}

inline void from_json(const json& j, PlayerStats& s)
{
    j.at("shoesFinished").get_to(s.shoesFinished);
    j.at("handsFinished").get_to(s.handsFinished);
    j.at("wins").get_to(s.wins);
    j.at("losses").get_to(s.losses);
    j.at("pushes").get_to(s.pushes);
    j.at("blackjacks").get_to(s.blackjacks);
    j.at("splits").get_to(s.splits);
    j.at("doubles").get_to(s.doubles);
    j.at("betsPlaced").get_to(s.betsPlaced);
    j.at("betSum").get_to(s.betSum);
    j.at("greatestHand").get_to(s.greatestHand);
    j.at("greatestWin").get_to(s.greatestWin);
    j.at("greatestLoss").get_to(s.greatestLoss);
    j.at("greatestSessionWin").get_to(s.greatestSessionWin);
    j.at("greatestSessionLoss").get_to(s.greatestSessionLoss);
    // additive: missing key falls back to default; pre-Feature-1/3 profiles still load.
    s.winsAfterDeviation = j.value("winsAfterDeviation", int{0});
    s.greatestHandLoss = j.value("greatestHandLoss", int{0});
    s.longestWinningStreak = j.value("longestWinningStreak", int{0});
    s.longestLosingStreak = j.value("longestLosingStreak", int{0});
    s.handProfitCount = j.value("handProfitCount", (long long)0);
    s.handProfitSum = j.value("handProfitSum", (long long)0);
    s.handProfitSumSq = j.value("handProfitSumSq", (long long)0);
    // additive: missing key falls back to an empty histogram; old profiles still load.
    s.betDistribution = j.value("betDistribution", std::map<int, int>{});
    // additive: missing key falls back to default; pre-Feature-1 side-bet profiles still load.
    s.sideBetWins = j.value("sideBetWins", int{0});
    s.sideBetLosses = j.value("sideBetLosses", int{0});
    s.sideBetProfitSum = j.value("sideBetProfitSum", (long long)0);
}

// HandOutcome serialization
inline void to_json(json& j, const HandOutcome& h)
{
    j = {
        {"cards", h.cards},
        {"actions", h.actions},
        {"wager", h.wager},
        {"payout", h.payout},
        {"won", h.won},
        {"surrendered", h.surrendered},
        {"insuranceBet", h.insuranceBet},
        {"insuranceTaken", h.insuranceTaken},
        {"playerName", h.playerName},
        {"sideBetKind", static_cast<int>(h.sideBetKind)},
        {"sideBetWager", h.sideBetWager},
        {"sideBetPayout", h.sideBetPayout}
    };
}

inline void from_json(const json& j, HandOutcome& h)
{
    j.at("cards").get_to(h.cards);
    j.at("actions").get_to(h.actions);
    j.at("wager").get_to(h.wager);
    j.at("payout").get_to(h.payout);
    j.at("won").get_to(h.won);
    j.at("surrendered").get_to(h.surrendered);
    j.at("insuranceBet").get_to(h.insuranceBet);
    j.at("insuranceTaken").get_to(h.insuranceTaken);
    if (j.contains("playerName"))
        j.at("playerName").get_to(h.playerName);
    // else defaults to "" for backward compatibility with existing saves
    // additive: older round records lack these and fall back to the struct defaults.
    h.sideBetKind = static_cast<SideBetKind>(j.value("sideBetKind", int{0}));
    h.sideBetWager = j.value("sideBetWager", int{0});
    h.sideBetPayout = j.value("sideBetPayout", int{0});
}

// RoundRecord serialization
inline void to_json(json& j, const RoundRecord& r)
{
    j = {
        {"dealerCards", r.dealerCards},
        {"playerHands", r.playerHands},
        {"runningCount", r.runningCount},
        {"trueCount", r.trueCount},
        {"cardsRemaining", r.cardsRemaining},
        {"timestamp", r.timestamp}
    };
}

inline void from_json(const json& j, RoundRecord& r)
{
    j.at("dealerCards").get_to(r.dealerCards);
    j.at("playerHands").get_to(r.playerHands);
    j.at("runningCount").get_to(r.runningCount);
    j.at("trueCount").get_to(r.trueCount);
    j.at("cardsRemaining").get_to(r.cardsRemaining);
    j.at("timestamp").get_to(r.timestamp);
}

// ShoeRecord serialization
inline void to_json(json& j, const ShoeRecord& s)
{
    j = {
        {"initialDeck", s.initialDeck},
        {"rounds", s.rounds},
        {"completed", s.completed},
        {"bankrollAtShoeEnd", s.bankrollAtShoeEnd},
        {"shuffleSeed", s.shuffleSeed}
    };
}

inline void from_json(const json& j, ShoeRecord& s)
{
    j.at("initialDeck").get_to(s.initialDeck);
    j.at("rounds").get_to(s.rounds);
    if (j.contains("completed"))
        j.at("completed").get_to(s.completed);
    // additive: absent "completed" keeps its default (true); old profiles still load.
    s.shuffleSeed = j.value("shuffleSeed", 0ull);
    // additive: missing key falls back to 0 (unknown bankroll); old saves still load.
    s.bankrollAtShoeEnd = j.value("bankrollAtShoeEnd", 0);
}

// PlayerProfile serialization
inline void to_json(json& j, const PlayerProfile& p)
{
    j = {
        {"name", p.name},
        {"bankroll", p.bankroll},
        {"stats", p.stats},
        {"longHistory", p.longHistory},
        // in-progress shoe rounds; serialised alongside longHistory so quitting mid-shoe still books those hands.
        // Written unconditionally (like longHistory); read additively on load so legacy saves without it keep loading.
        {"currentHistory", p.currentHistory},
        {"preferredPlayerCount", p.preferredPlayerCount},
        {"preferredSpeed", p.preferredSpeed},
        {"preferredCountingStyle", p.preferredCountingStyle},
        {"preferredSoundVolume", p.preferredSoundVolume},
        {"preferredMusicVolume", p.preferredMusicVolume},
        {"preferredMuted", p.preferredMuted}
    };
    // additive schema: persist unlocked ids only when non-empty
    if (!p.unlockedAchievementIds.empty())
        j["unlockedAchievementIds"] = p.unlockedAchievementIds;
    // written unconditionally to round-trip; additive on load (see from_json)
    j["gameRules"] = p.gameRules;
}

inline void from_json(const json& j, PlayerProfile& p)
{
    // Core fields read additively (j.value with the struct's own default) so a file missing one key degrades instead of
    // throwing -- consistent with every other additive field, and required by B14 so a partial save keeps its present
    // fields rather than being discarded to a fresh profile. name/bankroll/stats/longHistory all carry defaults below.
    p.name = j.value("name", std::string{});
    p.bankroll = j.value("bankroll", 1000);
    p.stats = j.value("stats", PlayerStats{});
    p.longHistory = j.value("longHistory", std::vector<ShoeRecord>{});
    // additive: absent "currentHistory" keeps its default (empty) so legacy mid-shoe-quit saves before this field still load.
    p.currentHistory = j.value("currentHistory", std::vector<RoundRecord>{});
    // additive: missing key falls back to default; pre-table-settings profiles still load.
    p.preferredPlayerCount = j.value("preferredPlayerCount", GameConfig::DEFAULT_PLAYER_COUNT);
    p.preferredSpeed = j.value("preferredSpeed", static_cast<int>(GameSpeed::Normal));
    p.preferredCountingStyle = j.value("preferredCountingStyle", 1);
    // additive: missing key falls back to sensible defaults (match Constants::Sound); pre-sound profiles still load.
    p.preferredSoundVolume = j.value("preferredSoundVolume", 100);
    p.preferredMusicVolume = j.value("preferredMusicVolume", 60);
    p.preferredMuted = j.value("preferredMuted", false);
    // additive: missing key falls back to an empty list; pre-Achievement profiles still load unchanged.
    p.unlockedAchievementIds = j.value("unlockedAchievementIds", std::vector<int>{});
    // additive: missing key keeps struct defaults; pre-Rules profiles load unchanged (nested GameRules also falls back).
    if (j.contains("gameRules"))
        j.at("gameRules").get_to(p.gameRules);
}
