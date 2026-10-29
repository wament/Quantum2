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
    // Newer fields: fall back to the struct's default (0) so profiles saved before Feature 1/3 still load.
    s.winsAfterDeviation = j.value("winsAfterDeviation", int{0});
    s.greatestHandLoss = j.value("greatestHandLoss", int{0});
    s.longestWinningStreak = j.value("longestWinningStreak", int{0});
    s.longestLosingStreak = j.value("longestLosingStreak", int{0});
    s.handProfitCount = j.value("handProfitCount", (long long)0);
    s.handProfitSum = j.value("handProfitSum", (long long)0);
    s.handProfitSumSq = j.value("handProfitSumSq", (long long)0);
    // Backward compatible: older profiles have no bet histogram yet.
    s.betDistribution = j.value("betDistribution", std::map<int, int>{});
    // Feature 1 (Side Bets): additive — older profiles lack these fields and fall back to the struct defaults.
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
    // Feature 1 (Side Bets): additive — older round records lack these and fall back to the struct defaults.
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
    // else defaults to true for backward compatibility
    s.shuffleSeed = j.value("shuffleSeed", 0ull);
    // Older saves won't have this field; default to 0 (unknown bankroll at that shoe).
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
        {"preferredPlayerCount", p.preferredPlayerCount},
        {"preferredSpeed", p.preferredSpeed},
        {"preferredCountingStyle", p.preferredCountingStyle},
        {"preferredSoundVolume", p.preferredSoundVolume},
        {"preferredMusicVolume", p.preferredMusicVolume},
        {"preferredMuted", p.preferredMuted}
    };
}

inline void from_json(const json& j, PlayerProfile& p)
{
    j.at("name").get_to(p.name);
    j.at("bankroll").get_to(p.bankroll);
    j.at("stats").get_to(p.stats);
    j.at("longHistory").get_to(p.longHistory);
    // Backward compatible: profiles saved before table settings existed fall back to the struct defaults.
    p.preferredPlayerCount = j.value("preferredPlayerCount", GameConfig::DEFAULT_PLAYER_COUNT);
    p.preferredSpeed = j.value("preferredSpeed", static_cast<int>(GameSpeed::Normal));
    p.preferredCountingStyle = j.value("preferredCountingStyle", 1);
    // Backward compatible: profiles saved before sound settings existed fall back to sensible defaults, so existing
    // saved profiles keep loading unchanged (additive schema — see CONTEXT.md §4 / §6). Defaults match Constants::Sound.
    p.preferredSoundVolume = j.value("preferredSoundVolume", 100);
    p.preferredMusicVolume = j.value("preferredMusicVolume", 60);
    p.preferredMuted = j.value("preferredMuted", false);
}
