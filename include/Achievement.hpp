// a self-contained, headless-safe achievement engine. The manager owns a fixed set of
// achievements (career-long and per-shoe conditions) plus the progress counters that feed them, exposes which ones just
// unlocked so Game can surface a popup, and serialises its unlocked set as plain ints in the player profile (additive:
// legacy profiles without the field load with nothing unlocked). All of this is pure logic: no SFML anywhere
// over the Graphics-only test binary. Presentation of the list/notification lives in GameRenderer/HUDState; Game wires the
// checks into real play (after each settled hand and at every shoe boundary) via recordHand()/onNewShoe().
#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>

// A single achievement: its identity, human-readable title/description and whether it is currently unlocked. Value type.
struct Achievement
{
    enum class Id : int
    {
        FirstDeal = 0,          // First Steps - play your first hand
        Natural21 = 1,          // First Steps - get your first blackjack
        SplitSuccess = 2,       // First Steps - split a pair and win both hands
        CountersEdge = 3,       // Card Counting - maintain a positive true count for 10 consecutive hands (shoe)
        BigBet = 4,             // Card Counting - place a bet of 10% or more of your bankroll
        KellyMaster = 5,        // Card Counting - use full Kelly (fraction == 1.0) for an entire shoe (shoe)
        ComebackKing = 6,       // Performance - win $500 after being down $500 in a shoe (shoe)
        PerfectShoe = 7,        // Performance - finish a shoe ahead without busting (shoe)
        SplitMaster = 8,        // Performance - split 5 pairs in a single shoe (shoe)
        BlackJackStreak = 9,    // Performance - get 3 blackjacks in one shoe (shoe)
        TextbookPlayer = 10,    // Strategy - play 50 consecutive hands perfectly per basic strategy (career)
        DeviationFree = 11      // Strategy - complete a full shoe with zero strategy deviations (shoe)
    };

    Id id = Id::FirstDeal;
    const char* name = "";
    const char* description = "";
    bool unlocked = false;

    // Stable integer encoding used for persistence. The scoped enum carries an explicit int base
    // directly and the profile stores no enum-name dependency (mirrors preferredSpeed / preferredCountingStyle).
    static int toInt(Id id) { return static_cast<int>(id); }
    static Id fromInt(int v) { return static_cast<Id>(v); }
};

class AchievementManager
{
  public:
    // A single completed hand, fed by Game after it settles. Only seat-0 (human) data is meaningful; the game aggregates
    // the human seat's outcome(s) into one event per round before calling recordHand(). Fields are opt-in: every field
    // defaults to "not applicable" so tests and the game only pass what they actually know.
    struct HandEvent
    {
        bool isBlackjack = false;     // the human hand was a natural 21
        bool followedStrategy = true; // matched basic strategy advice (no deviation logged for this round)
        bool busted = false;          // at least one of the human's hands bust this round
        bool splitHand = false;       // the human split a pair this round
        bool wonAny = false;          // at least one of the human's hands won
        int  bet = 0;                 // the human's total wager for the round
        float bankrollAfter = 0.0f;   // human bankroll immediately after the round
        bool kellyFull = false;       // the human's bet this round was full Kelly (fraction == 1.0)
        bool positiveTrueCount = false;  // true count > 0 for this hand (Counter's Edge needs a run of these)
    };

    AchievementManager();

    // Record one completed hand: updates career + per-shoe counters and returns the ids of any achievements that became
    // unlocked by this call. Pure logic; safe to call from tests with any HandEvent you like.
    std::vector<Achievement::Id> recordHand(const HandEvent& event);

    // End a shoe: resolve the conditions that only make sense once the shoe is finished (Deviation Free, Comeback King,
    // Perfect Shoe, Kelly Master) using this shoe's accumulated progress, then reset every per-shoe counter for the next
    // shoe. Returns newly-unlocked ids. A no-op-resolve when no hand was played in the finished shoe.
    std::vector<Achievement::Id> onNewShoe();

    // --- Read-only accessors (mirror the *ForTest() convention) ---
    const std::vector<Achievement>& all() const { return definitions_; }
    bool isUnlocked(Achievement::Id id) const;
    size_t unlockedCount() const
    {
        return static_cast<size_t>(std::count_if(unlocked_.begin(), unlocked_.end(),
            [](const auto& kv) { return kv.second; }));
    }
    int totalAchievements() const { return static_cast<int>(definitions_.size()); }

    // Career tallies the manager keeps (useful for the achievements list view and tests). Grouped behind the accessors below,
    // which read from CareerProgress_
    int careerHands() const { return career_.hands; }
    int careerBlackjacks() const { return career_.blackjacks; }
    int careerSplitWins() const { return career_.splitWins; }
    int perfectStreak() const { return career_.perfectStreak; }

    // Stamp the manager's currently-unlocked set into a profile field; load it back. Round-trips as plain ints
    // JSON stays additive: profiles saved before achievements existed simply contribute an empty list here.
    void persistUnlockedTo(std::vector<int>& outIds) const;
    void loadUnlockedFrom(const std::vector<int>& inIds);

  private:
    void unlock(Achievement::Id id, std::vector<Achievement::Id>& newlyUnlocked);

    // Static achievement table (name/description) merged with live unlocked flags into definitions_. Built once.
    std::vector<Achievement> definitions_;
    std::map<Achievement::Id, bool> unlocked_;

    // Career progress (accumulates across shoes; never reset by onNewShoe()). Grouped
    // one unit instead of being scattered (Phase 4 refactor).
    struct CareerProgress {
        int hands = 0;
        int blackjacks = 0;
        int splitWins = 0;
        int perfectStreak = 0;   // consecutive strategy-following hands, ever (spans shoes)
    };

    CareerProgress career_;

    // Per-shoe progress counters/flags, reset in resetPerShoe() each shoe boundary. Grouped as one unit (Phase 4).
    struct ShoeProgress {
        int hands = 0;
        int blackjacks = 0;
        int splitWins = 0;
        int positiveCountStreak = 0;   // consecutive hands with true count > 0 this shoe
        bool anyDeviationThisShoe = false;
        bool kellyFullAllShoe = true;  // stays true only while every fed hand was full Kelly
        int kellyHands = 0;            // number of hands told full-kelly (so an empty shoe can't "win")
        bool anyBustThisShoe = false;
    };

    ShoeProgress shoe_;

    // Bankroll swing tracking for Comeback King. Deliberately kept across the shoe boundary (not inside ShoeProgress)
    // measure a worst-to-recovered dip that spans shoes; only the seed flag + start/low/high/last are carried over here.
    bool bankrollSeeded_ = false;
    float shoeBankrollStart_ = 0.0f;
    float shoeBankrollLow_ = 0.0f;
    float shoeBankrollHigh_ = 0.0f;
    float shoeLastBankroll_ = 0.0f;

    void resetPerShoe();
};
