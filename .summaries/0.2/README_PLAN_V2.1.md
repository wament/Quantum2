"# Next Iteration Plan (v2.1)

## Overview
This document carries forward the remaining tasks from v2 and adds polish items discovered during implementation. The core multi-player architecture is complete; what remains is UX polish, NPC behavior refinement, and cleanup.

## Completed in v2
- ✅ Visual Basic Strategy Advisor (HUD-integrated)
- ✅ Advanced Splitting Rules (split aces, one-card-only)
- ✅ Testing Suite Expansion (test_game.cpp added)
- ✅ HUD Streak Logic
- ✅ Multi-Player Table Support (core architecture)
- ✅ Strategy Deviation Tracking

## Remaining Multi-Player Polish

### Priority 1: Player Count Selection UI
**Goal**: Let the user choose 1-6 players from the main menu.
**Current State**: Player count is hardcoded (1 human). Adding NPCs requires editing source code.

#### Tasks
1. **Update `include/Game.hpp`**:
   - Add `int playerCount = 1;` member.
   - Add `void setPlayerCount(int count);` method.
2. **Update `src/Game.cpp` (`startNewGame`)**:
   - Clear `players` vector and populate based on `playerCount`.
   - First player is human ("William"), rest are NPCs with distinct names.
3. **Update main menu**:
   - Add "Player Count" submenu or slider (1-6).
   - Show current selection and allow adjustment before starting game.

### Priority 2: Distinct NPC Names
**Goal**: NPCs should have recognizable names instead of all being "William".
**Current State**: All `Player` objects are constructed with "William".

#### Tasks
1. **Update `src/Game.cpp`**:
   - Define a list of NPC names (e.g., "Alice", "Bob", "Carol", "Dave", "Eve").
   - Assign unique names when creating NPC players.
2. **Update HUD** (optional):
   - Display player names above their hands or in the bottom bar.

### Priority 3: Smart NPC Insurance
**Goal**: NPCs should take insurance when the true count justifies it (TC ≥ 3).
**Current State**: NPCs auto-decline insurance (no logic implemented).

#### Tasks
1. **Update `src/Game.cpp` (`npcTakeTurn` or insurance handling)**:
   - When `RoundState::OfferInsurance` and active player is NPC:
     - Calculate true count.
     - If TC ≥ 3, NPC accepts insurance (half their bet).
     - Otherwise, NPC declines.
   - Auto-confirm NPC insurance decision (no user input needed).

### Priority 4: Active Player Highlight
**Goal**: Visually indicate which player's turn it is.
**Current State**: No visual distinction between active and inactive players.

#### Tasks
1. **Update `src/HUD.cpp` or `src/Game.cpp` (`drawPlayers`)**:
   - Draw a highlight (border, glow, or label) around the active player's hands.
   - Could use a colored rectangle or text label ("YOUR TURN").

### Priority 5: Multi-Player HUD Bankroll Display
**Goal**: Show bankrolls for all players, not just the human.
**Current State**: HUD only shows `players[0].getBankroll()`.

#### Tasks
1. **Update `include/HUDState.hpp`**:
   - Add `std::vector<int> playerBankrolls;` or similar.
2. **Update `src/Game.cpp` (`updateHUDState`)**:
   - Populate bankroll data for all players.
3. **Update `src/HUD.cpp`**:
   - Render each player's bankroll in the bottom bar or side panel.

## Cleanup & Bug Fixes

### Priority 6: Undefined `finishRound()`
**Goal**: Remove or implement the dangling function declaration.
**Current State**: `finishRound()` is declared in `Game.hpp` but never defined.

#### Tasks
1. **Update `include/Game.hpp`**:
   - Remove `void finishRound();` declaration (it's unused).

### Priority 7: NPC Split Ace Rule Verification
**Goal**: Ensure the "one card only" split ace rule applies to NPCs.
**Current State**: Rule is implemented in `update()`, but should be verified with NPCs.

#### Tasks
1. **Add test case** in `tests/test_game.cpp`:
   - Simulate NPC splitting aces.
   - Verify each split ace hand receives exactly one card and auto-stands.

## Stretch Goals

### Priority 8: NPC Betting Strategy
**Goal**: NPCs could use a simplified Kelly criterion or fixed bet.
**Current State**: NPCs always bet the minimum.

#### Tasks
1. **Update `src/Game.cpp` (`confirmBet`)**:
   - NPCs could bet based on their own bankroll percentage.
   - Or use a fixed bet amount configurable per NPC.

### Priority 9: End-of-Round Summary
**Goal**: Show a brief summary of each player's results after the dealer finishes.
**Current State**: Results are printed to console but not shown in HUD.

#### Tasks
1. **Update `src/HUD.cpp`**:
   - Add a temporary overlay showing each player's win/loss for the round.
   - Fade out after a few seconds or on next input.

---

## Implementation Order
1. **Player Count Selection UI** (unlocks all other multi-player features)
2. **Distinct NPC Names** (quick win)
3. **Undefined `finishRound()` Cleanup** (quick win)
4. **Smart NPC Insurance** (gameplay realism)
5. **Active Player Highlight** (UX improvement)
6. **Multi-Player HUD Bankroll Display** (UX improvement)
7. **NPC Split Ace Test** (verification)
8. **Stretch Goals** (as time permits)
"