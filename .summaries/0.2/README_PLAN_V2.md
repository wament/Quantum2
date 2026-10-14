# Next Iteration Plan (v2)

## Overview
This document outlines the remaining development tasks for **Blackjack2**, carrying forward uncompleted items from the original plan and adding new feature proposals.

## Remaining from Original Plan

### Priority 1: Visual Basic Strategy Advisor
**Goal**: Move the basic strategy hint from the console to the in-game HUD.
**Current State**: Pressing 'I' still prints "Book says: S" to `std::cout`. No HUD integration.

#### Tasks
1. **Update `include/HUDState.hpp`**:
   - Add `std::string basicStrategyHint;` to the `HUDState` struct.
2. **Update `src/Game.cpp`**:
   - In `processGameEvents()`, when 'I' is pressed, set `hudState.basicStrategyHint` to the advice string instead of printing to console.
3. **Update `src/HUD.cpp`**:
   - Implement rendering for `hudState.basicStrategyHint`.
   - Display it near the player's hand or in a dedicated "Advice" panel.
   - Clear the hint when the player takes an action (Hit/Stand/etc.).

### Priority 2: Advanced Splitting Rules
**Goal**: Implement standard casino rules for splitting Aces and correct payout logic.
**Current State**: The engine supports splitting, but edge cases like "One Card Only" for Aces are missing.

#### Tasks
1. **Update `include/Hand.hpp`**:
   - Add `bool isSplitAce = false;` to the `Hand` class.
   - Add getter/setter methods.
2. **Update `src/Game.cpp` (`split` function)**:
   - When splitting an Ace, mark both hands: `hand.isSplitAce = true;` and `newHand.isSplitAce = true;`.
3. **Update `src/Game.cpp` (`update` function)**:
   - In the `RoundState::PlayerTurn` loop, check if the current hand is a split Ace with 1 card. After dealing the second card, auto-set `hand.setFinished(true)` to enforce the "One Card Only" rule.
4. **Update `src/Game.cpp` (`evaluateBlackjack` function)**:
   - Ensure that a split Ace receiving a 10-value card (total 21) pays **1:1**, not **3:2**.
   - Verify that `isBlackjack()` returns true for split Aces with a 10, and the payout logic handles the exception.

### Priority 3: Testing Suite Expansion (Remaining)
**Goal**: Complete code coverage for game logic.
**Current State**: `test_card.cpp`, `test_hand.cpp`, and `test_deck.cpp` exist. `test_game.cpp` is missing.

#### Tasks
1. **Add `tests/test_game.cpp`**:
   - Test `Game::calculateOptimalBet()` with various True Counts (e.g., TC < 1 should return min bet).
   - Test `Game::giveBasicStrategyAdvice()` for specific hand/dealer combinations.
   - Test insurance logic (offer, win, lose scenarios).
2. **Add shuffle randomness tests to `tests/test_deck.cpp`**:
   - Verify deck size after shuffle.
   - Basic distribution check (no obvious patterns).

### Priority 4: HUD Streak Logic
**Goal**: Wire up the streak display that already exists in the HUD.
**Current State**: `HUDState` has `streak` and `streakIsWin` fields, and `HUD.cpp` renders a "Streak" stat box, but `Game.cpp` never updates these values.

#### Tasks
1. **Update `src/Game.cpp`**:
   - Track consecutive wins/losses in `evaluateHands()`.
   - Update `hudState.streak` and `hudState.streakIsWin` accordingly.
   - Reset streak on a loss after wins (and vice versa).

---

## New Feature Proposals

### Priority 5: Multi-Player Table Support
**Goal**: Support 2-6 players at the table instead of just one.
**Rationale**: Real blackjack tables have multiple players. This would make the game more realistic and social.

#### Tasks
1. **Update `include/Game.hpp` / `src/Game.cpp`**:
   - Replace single `Player` with `std::vector<Player>`.
   - Track whose turn it is (`currentPlayerIndex`).
   - Cycle through players in `RoundState::PlayerTurn`.
2. **Update `include/HUDState.hpp` / `src/HUD.cpp`**:
   - Show all players' hands and bankrolls.
   - Highlight the active player.
3. **Update `include/PlayerProfile.hpp`**:
   - Support multiple player profiles.
4. **Update `src/Game.cpp` (`processGameEvents`)**:
   - Route input to the active player only.

### Priority 6: Strategy Deviation Tracking
**Goal**: Track when the player deviates from basic strategy and the cost of that deviation.
**Rationale**: One of the biggest learning tools for blackjack players is understanding the impact of bad decisions. This would show "You hit on 12 vs 6 (should stand) — cost: $X over time."

#### Tasks
1. **Update `include/PlayerStats.hpp`**:
   - Add `int strategyHits;` (correct plays), `int strategyDeviations;` (wrong plays).
   - Add `std::vector<DeviationRecord>` to store details of each deviation.
2. **Update `src/Game.cpp`**:
   - After each player action, compare it to `giveBasicStrategyAdvice()`.
   - If different, log a deviation with the hand, dealer up-card, action taken, correct action, and eventual outcome.
3. **Update `src/HUD.cpp`**:
   - Add a "Strategy" panel showing compliance rate (e.g., "95% Basic Strategy").
   - Optionally show recent deviations.
4. **Add tests** for deviation tracking logic.

---

## Implementation Order
1. **Testing Suite Expansion** (ensures logic correctness before changes)
2. **Advanced Splitting Rules** (core rule refinement)
3. **HUD Streak Logic** (quick win, existing UI)
4. **Visual Basic Strategy Advisor** (major UX improvement)
5. **Strategy Deviation Tracking** (learning tool)
6. **Multi-Player Table Support** (major feature, most complex)