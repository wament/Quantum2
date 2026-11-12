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
    // T1/B21: the shoe is stored as (shoeDecks, shuffleSeed) -- two integers that rebuild it exactly -- not as a
    // copy of its 313 cards, which was 792,200 of the real profile's 9,591,391 bytes.
    j = {
        {"shoeDecks", s.shoeDecks},
        {"shuffleSeed", s.shuffleSeed},
        {"shoeCardCount", s.shoeCardCount},
        {"isContinuation", s.isContinuation},
        {"mode", static_cast<int>(s.mode)},
        {"rounds", s.rounds},
        {"completed", s.completed},
        {"bankrollAtShoeEnd", s.bankrollAtShoeEnd}
    };
}

inline void from_json(const json& j, ShoeRecord& s)
{
    // B24's nested-read hazard, closed at this site: j.at() THROWS on a missing key, and the load
    // path's recovery for a throw is "relocate the whole profile, start fresh". A shoe record with no
    // "rounds" key used to be able to destroy the profile over one field. The unguarded
    // j.at("rounds").get_to(...) that used to sit above this guard is what threw; the guard replaces it
    // rather than following it, so a missing or mistyped "rounds" now loads an empty shoe.
    if (j.contains("rounds") && j.at("rounds").is_array())
        j.at("rounds").get_to(s.rounds);
    // Defaults keep a hand-edited or partially written file from throwing; they are not legacy-schema support --
    // the pre-T1 "initialDeck" array is gone, and history was reset with the schema change.
    s.shoeDecks = j.value("shoeDecks", Constants::NUM_DECKS);
    s.shuffleSeed = j.value("shuffleSeed", 0ull);
    s.shoeCardCount = j.value("shoeCardCount", 0);
    // B22: this was declared and set in Game.cpp but never serialized, so a saved continuation shoe came back
    // marked as an ordinary one. It is the only marker of the seed-reconstruction boundary, so it has to persist.
    s.isContinuation = j.value("isContinuation", false);
    // The session kind. Read as a bounded int: an out-of-range value falls back to Regular rather than
    // producing a mode no producer writes, which would silently route a shoe into the wrong history.
    const int modeValue = j.value("mode", static_cast<int>(ShoeMode::Regular));
    s.mode = (modeValue >= static_cast<int>(ShoeMode::Regular) && modeValue <= static_cast<int>(ShoeMode::Multiplayer))
                 ? static_cast<ShoeMode>(modeValue)
                 : ShoeMode::Regular;
    if (j.contains("completed"))
        j.at("completed").get_to(s.completed);
    // additive: absent "completed" keeps its default (true).
    // additive: missing key falls back to 0 (unknown bankroll).
    s.bankrollAtShoeEnd = j.value("bankrollAtShoeEnd", 0);
}

// Rolled-out-shoe carry (T2): additive schema, all fields optional with zero defaults so legacy saves without "rollup" load fine.
// Per-mode aggregates: written unconditionally, read with zero defaults (schema policy).
inline void to_json(json& j, const ModeStats& m)
{
    j = {
        {"shoesPlayed", m.shoesPlayed},
        {"bestBankroll", m.bestBankroll},
        {"mostHandsWon", m.mostHandsWon},
        {"bestFinish", m.bestFinish}
    };
}

inline void from_json(const json& j, ModeStats& m)
{
    m.shoesPlayed = j.value("shoesPlayed", 0);
    m.bestBankroll = j.value("bestBankroll", 0);
    m.mostHandsWon = j.value("mostHandsWon", 0);
    m.bestFinish = j.value("bestFinish", 0);
}

inline void to_json(json& j, const RollupCarry& r)
{
    j = {
        {"wins", r.wins}, {"losses", r.losses}, {"pushes", r.pushes}, {"handsFinished", r.handsFinished},
        {"blackjacks", r.blackjacks}, {"greatestHand", r.greatestHand}, {"greatestHandLoss", r.greatestHandLoss},
        {"handProfitCount", r.handProfitCount}, {"handProfitSum", r.handProfitSum}, {"handProfitSumSq", r.handProfitSumSq},
        {"greatestWin", r.greatestWin}, {"greatestLoss", r.greatestLoss},
        {"greatestSessionWin", r.carriedGreatestSessionWin}, {"greatestSessionLoss", r.carriedGreatestSessionLoss},
        {"longestWinningStreak", r.longestWinningStreak}, {"longestLosingStreak", r.longestLosingStreak},
        {"sideBetWins", r.sideBetWins}, {"sideBetLosses", r.sideBetLosses}, {"sideBetProfitSum", r.sideBetProfitSum},
        {"carriedSessionTotal", r.carriedSessionTotal}
    };
}

inline void from_json(const json& j, RollupCarry& r)
{
    r.wins = j.value("wins", 0);
    r.losses = j.value("losses", 0);
    r.pushes = j.value("pushes", 0);
    r.handsFinished = j.value("handsFinished", 0);
    r.blackjacks = j.value("blackjacks", 0);
    r.greatestHand = j.value("greatestHand", 0);
    r.greatestHandLoss = j.value("greatestHandLoss", 0);
    r.handProfitCount = j.value("handProfitCount", 0LL);
    r.handProfitSum = j.value("handProfitSum", 0LL);
    r.handProfitSumSq = j.value("handProfitSumSq", 0LL);
    r.greatestWin = j.value("greatestWin", 0);
    r.greatestLoss = j.value("greatestLoss", 0);
    r.carriedGreatestSessionWin = j.value("greatestSessionWin", 0);
    r.carriedGreatestSessionLoss = j.value("greatestSessionLoss", 0);
    r.longestWinningStreak = j.value("longestWinningStreak", 0);
    r.longestLosingStreak = j.value("longestLosingStreak", 0);
    r.sideBetWins = j.value("sideBetWins", 0);
    r.sideBetLosses = j.value("sideBetLosses", 0);
    r.sideBetProfitSum = j.value("sideBetProfitSum", 0LL);
    r.carriedSessionTotal = j.value("carriedSessionTotal", 0LL);
}

// PlayerProfile serialization
// ---------------------------------------------------------------------------
// Type-tolerant profile reads.
//
// nlohmann's j.value(key, default) applies the default ONLY when the key is ABSENT. A key that is
// present with the WRONG TYPE THROWS (type_error.302) -- and PlayerProfile::load's recovery for a
// throw is "relocate the whole file, start fresh". So one corrupt field destroys the player's
// bankroll, shoe history and achievements: a data-loss bug whose blast radius is the entire profile,
// triggered by a single field.
//
// These helpers treat a wrong-typed field exactly like a missing one: fall back to the default and
// keep everything else. Found by the Daily schema test, which wrote a non-string "dailyLastPlayed"
// and watched the loader discard a profile that was 99% readable.
//
// LIMITATION, stated rather than hidden: these guard the TOP-LEVEL fields. A nested object whose own
// members are wrong-typed (e.g. "stats" present and an object, but stats.wins is a string) still
// throws and still relocates the file. Making nested reads tolerant is a separate change.
// ---------------------------------------------------------------------------
inline std::string jsonOrString(const json& j, const char* key, const std::string& fallback)
{
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_string())
        return fallback;
    return j.at(key).get<std::string>();
}

inline int jsonOrInt(const json& j, const char* key, int fallback)
{
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_number_integer())
        return fallback;
    return j.at(key).get<int>();
}

inline long long jsonOrLongLong(const json& j, const char* key, long long fallback)
{
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_number_integer())
        return fallback;
    return j.at(key).get<long long>();
}

inline unsigned long long jsonOrUnsignedLongLong(const json& j, const char* key, unsigned long long fallback)
{
    // is_number_unsigned, not is_number_integer: a negative value cannot name a shuffle seed, and
    // narrowing one into an unsigned long long would silently become a huge one.
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_number_unsigned())
        return fallback;
    return j.at(key).get<unsigned long long>();
}

inline bool jsonOrBool(const json& j, const char* key, bool fallback)
{
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_boolean())
        return fallback;
    return j.at(key).get<bool>();
}

template <typename T>
inline std::vector<T> jsonOrArray(const json& j, const char* key, const std::vector<T>& fallback)
{
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_array())
        return fallback;
    return j.at(key).get<std::vector<T>>();
}


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
        // additive: rolled-out-shoe carry (T2); absent key loads a zero RollupCarry, the merge identity, so legacy saves load unchanged.
        {"rollup", p.rollup},
        {"preferredPlayerCount", p.preferredPlayerCount},
        {"preferredSpeed", p.preferredSpeed},
        {"preferredCountingStyle", p.preferredCountingStyle},
        {"preferredSoundVolume", p.preferredSoundVolume},
        {"preferredMusicVolume", p.preferredMusicVolume},
        {"preferredMuted", p.preferredMuted}
    };
    // Daily Challenge fields: written UNCONDITIONALLY per the schema policy (CONTEXT.md §5). These are
    // fields this program always writes, so the j.value() reads below are a partial-write guard (a save
    // interrupted mid-write keeps its present fields, per B14), not legacy support. A profile saved
    // before the Daily existed has none of these keys and loads with an empty history and zero streak.
    j["dailyLastPlayed"] = p.dailyLastPlayed;
    j["dailyStreak"] = p.dailyStreak;
    j["dailyBest"] = p.dailyBest;
    j["dailyStartedOn"] = p.dailyStartedOn;
    j["dailyActiveDateKey"] = p.dailyActiveDateKey;
    j["dailyActiveSeed"] = p.dailyActiveSeed;
    j["dailyHistory"] = p.dailyHistory;
    // Per-mode shoe history + aggregates. Written unconditionally per the schema policy; a profile
    // saved before per-mode history existed loads with empty vectors and zero aggregates, so the
    // career derivation and the T2 rollup behave exactly as before.
    j["dailyShoes"] = p.dailyShoes;
    j["tournamentShoes"] = p.tournamentShoes;
    j["multiplayerShoes"] = p.multiplayerShoes;
    j["dailyStats"] = p.dailyStats;
    j["tournamentStats"] = p.tournamentStats;
    j["multiplayerStats"] = p.multiplayerStats;
    j["tournamentsCompleted"] = p.tournamentsCompleted;
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
    p.name = jsonOrString(j, "name", std::string{});
    p.bankroll = jsonOrInt(j, "bankroll", 1000);
    // Nested objects are guarded by an is_object() check: a present-but-non-object "stats"/"rollup"
    // would otherwise throw and take the whole profile down with it.
    if (j.is_object() && j.contains("stats") && j.at("stats").is_object())
        j.at("stats").get_to(p.stats);
    p.longHistory = jsonOrArray<ShoeRecord>(j, "longHistory", std::vector<ShoeRecord>{});
    // additive: absent "currentHistory" keeps its default (empty) so legacy mid-shoe-quit saves before this field still load.
    p.currentHistory = jsonOrArray<RoundRecord>(j, "currentHistory", std::vector<RoundRecord>{});
    // additive: missing "rollup" loads a zero RollupCarry (the applyRollup merge identity) so pre-T2 saves keep their stats verbatim.
    if (j.is_object() && j.contains("rollup") && j.at("rollup").is_object())
        j.at("rollup").get_to(p.rollup);
    // additive: missing key falls back to default; pre-table-settings profiles still load.
    p.preferredPlayerCount = jsonOrInt(j, "preferredPlayerCount", GameConfig::DEFAULT_PLAYER_COUNT);
    p.preferredSpeed = jsonOrInt(j, "preferredSpeed", static_cast<int>(GameSpeed::Normal));
    p.preferredCountingStyle = jsonOrInt(j, "preferredCountingStyle", 1);
    // additive: missing key falls back to sensible defaults (match Constants::Sound); pre-sound profiles still load.
    p.preferredSoundVolume = jsonOrInt(j, "preferredSoundVolume", 100);
    p.preferredMusicVolume = jsonOrInt(j, "preferredMusicVolume", 60);
    p.preferredMuted = jsonOrBool(j, "preferredMuted", false);
    // additive: missing key falls back to an empty list; pre-Achievement profiles still load unchanged.
    p.unlockedAchievementIds = jsonOrArray<int>(j, "unlockedAchievementIds", std::vector<int>{});
    // additive: missing key keeps struct defaults; pre-Rules profiles load unchanged (nested GameRules also falls back).
    if (j.is_object() && j.contains("gameRules") && j.at("gameRules").is_object())
        j.at("gameRules").get_to(p.gameRules);
    // Daily Challenge: same additive, type-tolerant reads. A pre-Daily profile has none of these keys
    // and lands on the struct defaults (no streak, no history, no active shoe), so it loads exactly
    // as before.
    p.dailyLastPlayed = jsonOrString(j, "dailyLastPlayed", std::string{});
    p.dailyStreak = jsonOrInt(j, "dailyStreak", 0);
    p.dailyBest = jsonOrInt(j, "dailyBest", 0);
    p.dailyStartedOn = jsonOrLongLong(j, "dailyStartedOn", 0);
    p.dailyActiveDateKey = jsonOrString(j, "dailyActiveDateKey", std::string{});
    p.dailyActiveSeed = jsonOrUnsignedLongLong(j, "dailyActiveSeed", 0);
    p.dailyHistory = jsonOrArray<Daily::DailyResult>(j, "dailyHistory", std::vector<Daily::DailyResult>{});
    p.dailyShoes = jsonOrArray<ShoeRecord>(j, "dailyShoes", std::vector<ShoeRecord>{});
    p.tournamentShoes = jsonOrArray<ShoeRecord>(j, "tournamentShoes", std::vector<ShoeRecord>{});
    p.multiplayerShoes = jsonOrArray<ShoeRecord>(j, "multiplayerShoes", std::vector<ShoeRecord>{});
    if (j.is_object() && j.contains("dailyStats") && j.at("dailyStats").is_object())
        j.at("dailyStats").get_to(p.dailyStats);
    if (j.is_object() && j.contains("tournamentStats") && j.at("tournamentStats").is_object())
        j.at("tournamentStats").get_to(p.tournamentStats);
    if (j.is_object() && j.contains("multiplayerStats") && j.at("multiplayerStats").is_object())
        j.at("multiplayerStats").get_to(p.multiplayerStats);
    p.tournamentsCompleted = jsonOrInt(j, "tournamentsCompleted", 0);
}
