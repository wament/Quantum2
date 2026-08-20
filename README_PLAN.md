# Next Iteration Plan

## Overview
This document outlines the development plan for the next iteration of **Blackjack2**. The focus is on enhancing the player experience through visual aids, refining core rules (specifically splitting), and expanding the testing suite to ensure robustness.

## Priority 1: Visual Basic Strategy Advisor
**Goal**: Move the basic strategy hint from the console to the in-game HUD.
**Current State**: Currently, pressing 'I' prints the advice (e.g., "Book says: S") to `std::cout`.

### Tasks
1. **Update `include/HUDState.hpp`**:
   - Add `std::string basicStrategyHint;` to the `HUDState` struct.
2. **Update `src/Game.cpp`**:
   - In `processEvents()`, when 'I' is pressed, set `hudState.basicStrategyHint = getBasicStrategyAdvice(...)` instead of printing to console.
3. **Update `src/HUD.cpp`**:
   - Implement rendering for `hudState.basicStrategyHint`.
   - Display it near the player's hand or in a dedicated "Advice" panel.
   - Clear the hint when the player takes an action (Hit/Stand/etc.).

## Priority 2: Advanced Splitting Rules
**Goal**: Implement standard casino rules for splitting Aces and correct payout logic.
**Current State**: The engine supports splitting, but edge cases like "One Card Only" for Aces need refinement.

### Tasks
1. **Update `include/Hand.hpp`**:
   - Add `bool isSplitAce = false;` to the `Hand` struct.
2. **Update `src/Game.cpp` (`split` function)**:
   - When splitting an Ace, mark the new hand: `newHand.isSplitAce = true;`.
3. **Update `src/Game.cpp` (`update` function)**:
   - In the `RoundState::PlayerTurn` loop, check if the current hand is a split Ace with 2 cards. If so, auto-set `hand.setFinished(true)` to enforce the "One Card Only" rule.
4. **Update `src/Game.cpp` (`evaluateBlackjack` function)**:
   - Ensure that a split Ace receiving a 10-value card (total 21) pays **1:1**, not **3:2**.
   - Current logic: `if (hand.isBlackjack() && !hand.isSplit())` pays 3:2. This is correct, but verify that `isBlackjack()` returns true for split Aces with a 10 (it likely does), and the payout logic handles the exception.

## Priority 3: Shoe Replay Mode
**Goal**: Allow users to review the cards dealt in the current or previous shoe.
**Current State**: `Deck` stores `activeShoeRecord` and `previousShoeRecord`, but no UI exists to view them.

### Tasks
1. **Update `include/HUDState.hpp`**:
   - Add `bool showReplay = false;` and `int replayStep = 0;`.
2. **Update `src/HUD.cpp`**:
   - Add a "Replay" button or key binding (e.g., 'R' in game over, or a button in HUD).
   - Render the card history from `hudState.currentShoe->rounds` (or `activeShoeRecord`).
3. **Update `src/Game.cpp`**:
   - Implement logic to step through the record: `replayStep++`, updating the HUD to show the state at that step.

## Priority 4: Testing Suite Expansion
**Goal**: Increase code coverage and stability.
**Current State**: Tests exist in `tests/` but are sparse.

### Tasks
1. **Add `tests/test_hand.cpp`**:
   - Test `Hand::getValue()` with multiple Aces (Soft vs. Hard transitions).
   - Test `Hand::isSoft()` logic.
2. **Add `tests/test_deck.cpp`**:
   - Test `Deck::shuffle()` for basic randomness (e.g., ensure no adjacent duplicates in a small sample, or verify deck size).
   - Test `Deck::draw()` and cut card behavior.
3. **Add `tests/test_game.cpp`**:
   - Test `Game::calculateOptimalBet()` with various True Counts (e.g., TC < 1 should return min bet).
   - Test `Game::giveBasicStrategyAdvice()` for specific hand/dealer combinations.

## Priority 5: HUD Polish (Optional)
**Goal**: Improve the visual feedback of the HUD.
**Current State**: `HUDState` has `streak` and `streakIsWin` but they are not used.

### Tasks
1. **Update `src/Game.cpp`**:
   - Update `hudState.streak` and `hudState.streakIsWin` in `evaluateHands()`.
2. **Update `src/HUD.cpp`**:
   - Display streak information (e.g., "3 Win Streak") in the status message or a dedicated area.

---

## Implementation Order
1. **Testing Suite Expansion** (Ensures logic correctness before changes)
2. **Advanced Splitting Rules** (Core rule refinement)
3. **Visual Basic Strategy Advisor** (Major UX improvement)
4. **Shoe Replay Mode** (Advanced feature)
5. **HUD Polish** (Nice-to-have)