#pragma once

#include "Hand.hpp"
#include "Deck.hpp"
#include "PlayerProfile.hpp"
#include "Constants.hpp"
#include "BasicStrategy.hpp"

#include <cstddef>

enum class BettingStrategy
{
    Flat,
    Fibonacci,
    Paroli,
    Martingale,
    Kelly
};

// How a single hand settles against the dealer, reported by Player::settle.
enum class SettlementResult
{
    Surrender,
    Lose,
    Win,
    Push
};

class Player
{
  public:
    Player(const std::string& playerName);

    // NPC seat
    Player(const std::string& playerName, int startingBankroll);

    void resetRound(int previousBet);

    int currentHand = 0;
    std::vector<Hand> hands;

    // T1 (auto-save cadence): human hands settled since the last auto-save, evaluated at every round end so the profile is rewritten
    // to disk about once per AUTO_SAVE_EVERY_HANDS hands instead of every round (reducing save amplification). Forced writes at shoe
    // boundary, pause-quit and exit are unaffected. Test-visible like currentHand/hands.
    unsigned long long autosaveHandCount = 0;
    void addAutosaveHand(int n = 1) { if (n > 0) autosaveHandCount += static_cast<unsigned long long>(n); }
    bool shouldAutosave() const { return autosaveHandCount >= Constants::AUTO_SAVE_EVERY_HANDS; }
    void resetAutosaveCounter() { autosaveHandCount = 0; }

    // T1: times this seat actually wrote its profile to disk (persistable path only). Test-visible so a byte-counting test can prove the
    // round-end autosave is now gated instead of firing once per settled round.
    int saveWriteCount = 0;

    // Betting
    bool placeBet(int amount);
    void winBet(int amount);
    void loseBet(int amount);

    // Betting strategy
    void setBettingStrategy(BettingStrategy strategy, int baseBet = 10);
    BettingStrategy getBettingStrategy() const { return bettingStrategy; }
    int calculateNextBet(bool lastRoundWon);
    void recordRoundResult(bool won);

    int determineRoundBet(float trueCount);

    void adjustHandBet(int delta, std::size_t handIndex = 0);

    bool shouldTakeInsurance(int bet, float trueCount, BasicStrategy::CountingStyle style = BasicStrategy::CountingStyle::HI_LO) const;

    void takeInsurance(int amount);

    // Raise/lower the active hand's insurance bet by 1, clamped to [0, half the main bet].
    void raiseInsurance();
    void lowerInsurance();

    // Settle one hand against the dealer and apply the result to the bankroll. Reports how it settled;
    // writes the payout. A natural (2-card) unsplit blackjack pays 3:2: any other hand totaling 21
    // settles at even money, so the !hand.isSplit() guard drives every branch below.
    // dealerHasBlackjack must reflect whether the DEALER's 21 is also a natural (exactly two cards):
    // a player natural only pushes against a dealer natural, never against a dealer 21 built with
    // three or more cards.
    SettlementResult settle(const Hand& hand, int dealerValue, bool dealerBust, bool dealerHasBlackjack, int& payout);

    // Bankroll
    void setBankroll(int amount);

    // Getters
    const std::string& getName() const;
    void setName(const std::string& name);
    int getBankroll() const;
    PlayerProfile getProfile();
    PlayerStats& getStats();
    PlayerProfile updateProfile(PlayerProfile profile);
    void saveProfile();

    // The on-disk career profile path, resolved by the same $HOME logic saveProfile() uses. Exposed
    // because a feature whose own seat is EPHEMERAL may still need to write the real career file --
    // the Daily Challenge books its streak there while its seat must never touch it. Duplicating the
    // $HOME fallback at that call site is how a second, subtly-different path gets born.
    static std::string savePath();

    // When false, saveProfile() is a no-op: the seat's in-memory state must never be written to disk. The human session persists
    // (default true); NPCs, the ephemeral tournament run and the Daily Challenge set it false so their throwaway chips/stats/rules
    // cannot clobber the career file.
    void setPersistable(bool value) { persistable = value; }

    // The Daily streak straight off the seat's profile. A cheap accessor, deliberately: the main-menu
    // row shows the streak, and getProfile() returns the profile BY VALUE -- copying a capped 100-shoe
    // history every rendered frame to read one int is not acceptable in a draw loop.
    int dailyStreak() const { return profile.dailyStreak; }

    // Escrow bookkeeping for the round in progress. getAvailableBankroll() is bankroll minus both committed amounts and is the one
    // predicate every bet clamp + HUD hint reads, so a main bet + side bet can never be double-counted against one bankroll (B5).
    // commit* record what is on the table; clearPendingBets() resets them at each round start.
    int getAvailableBankroll() const { return bankroll - pendingMainBet - pendingSideBet; }
    void commitMainBet(int amount) { pendingMainBet = amount; }
    void commitSideBet(int amount) { pendingSideBet = amount; }  // a 0 amount opts the side bet out
    void clearPendingBets() { pendingMainBet = 0; pendingSideBet = 0; }

  private:
    std::string name;

    PlayerProfile profile;
    int bankroll = 1000;

    // Amounts committed this round but not yet settled. getAvailableBankroll() subtracts both so no bet clamp can over-commit the
    // same bankroll against a main + side pair (B5). Cleared at each round start via clearPendingBets().
    int pendingMainBet = 0;
    int pendingSideBet = 0;

    // Guards saveProfile(): false for ephemeral seats (NPCs, the tournament run) so their in-memory state is never persisted.
    bool persistable = true;

    BettingStrategy bettingStrategy = BettingStrategy::Flat;
    int baseBet = 10;
    int currentProgressiveBet = 0;
    int consecutiveWins = 0;
    int fibIndex = 0;
    bool lastRoundWon = false;
};
