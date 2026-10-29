#pragma once

#include <vector>
#include <cstddef>
#include "Card.hpp"
#include "SideBet.hpp"

class Hand
{
  public:
    Hand() = default;
    explicit Hand(const std::vector<Card>& cards); // construct from existing cards

    void addCard(const Card& card);
    void clear();

    [[nodiscard]] int getValue() const;

    [[nodiscard]] bool isBlackjack() const;
    [[nodiscard]] bool isBust() const;
    [[nodiscard]] bool isSoft() const;
    [[nodiscard]] bool isSplitAce() const;
    // Split aces receive exactly one additional card and then stand (no further hits).
    [[nodiscard]] bool splitAceNeedsMoreCards() const;

    [[nodiscard]] bool canSplit() const;
    [[nodiscard]] bool canDouble() const;

    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] const Card& getCard(std::size_t index) const;
    const Card& getDealerUpCard() const;
    const std::string getDealerUpCardToString() const;

    Card takeLastCard();

    void setBet(int amount);
    [[nodiscard]] int getBet() const;

    void setInsuranceBet(int amount);
    [[nodiscard]] int getInsuranceBet() const;

    // Side bet (Feature 1): the optional side bet placed on this hand before the deal. setSideBet records the chosen
    // kind + wager and clears any previously resolved payout; the payout is written by Game::resolveSideBets after the
    // cards are known, then copied into HandOutcome for replay/analytics. Side bets resolve independently of the main
    // hand's win/loss (FEATURES.md: "evaluate side bets before main-hand resolution").
    void setSideBet(SideBetKind kind, int wager);
    [[nodiscard]] SideBetKind getSideBetKind() const { return sideBetKind; }
    [[nodiscard]] int getSideBetWager() const { return sideBetWager; }
    [[nodiscard]] int getSideBetPayout() const { return sideBetPayout; } // gross returned (stake+winnings) on a win, 0 on a loss
    void setSideBetPayout(int payout) { sideBetPayout = payout; }

    void setFinished(bool value);
    [[nodiscard]] bool isFinished() const;

    void setDoubledDown(bool value);
    [[nodiscard]] bool isDoubledDown() const;

    void setSurrendered(bool value);
    [[nodiscard]] bool isSurrendered() const;

    void setSplit(bool value, int cardValue);
    [[nodiscard]] bool isSplit() const;

  private:
    std::vector<Card> cards;

    bool finished = false;
    bool doubledDown = false;
    bool surrendered = false;
    bool split = false;
    bool splitAce = false;

    int currentBet = 0;
    int insuranceBet = 0;

    // Feature 1 (Side Bets): the chosen kind + wager, and the resolved gross payout. All three are zeroed by clear()
    // so a fresh round never carries a stale side bet onto a new hand.
    SideBetKind sideBetKind = SideBetKind::None;
    int sideBetWager = 0;
    int sideBetPayout = 0;

};
