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

// AnalyticsEdgeSample serialization (Feature 3)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AnalyticsEdgeSample, roundIndex, trueCount, cumulativeProfit)

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
        {"strategyDeviationWins", s.strategyDeviationWins},
        {"greatestSingleHandLoss", s.greatestSingleHandLoss},
        {"longestWinningStreak", s.longestWinningStreak},
        {"longestLosingStreak", s.longestLosingStreak},
        {"handProfitSum", s.handProfitSum},
        {"handProfitSqSum", s.handProfitSqSum},
        {"bankrollCurve", s.bankrollCurve},
        {"edgeHistory", s.edgeHistory},
        {"betBuckets", s.betBuckets}
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
    // Feature 1 & 3 analytics (may be absent in older saves -> keep defaults)
    s.strategyDeviationWins = j.value("strategyDeviationWins", s.strategyDeviationWins);
    s.greatestSingleHandLoss = j.value("greatestSingleHandLoss", s.greatestSingleHandLoss);
    s.longestWinningStreak = j.value("longestWinningStreak", s.longestWinningStreak);
    s.longestLosingStreak = j.value("longestLosingStreak", s.longestLosingStreak);
    s.handProfitSum = j.value("handProfitSum", s.handProfitSum);
    s.handProfitSqSum = j.value("handProfitSqSum", s.handProfitSqSum);
    s.bankrollCurve = j.value("bankrollCurve", s.bankrollCurve);
    s.edgeHistory = j.value("edgeHistory", s.edgeHistory);
    s.betBuckets = j.value("betBuckets", s.betBuckets);
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
        {"insuranceTaken", h.insuranceTaken}
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
}

// PlayerProfile serialization
inline void to_json(json& j, const PlayerProfile& p)
{
    j = {
        {"name", p.name},
        {"bankroll", p.bankroll},
        {"stats", p.stats},
        {"longHistory", p.longHistory}
    };
}

inline void from_json(const json& j, PlayerProfile& p)
{
    j.at("name").get_to(p.name);
    j.at("bankroll").get_to(p.bankroll);
    j.at("stats").get_to(p.stats);
    j.at("longHistory").get_to(p.longHistory);
}
