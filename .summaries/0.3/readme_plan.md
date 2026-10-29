# Blackjack2 - Feature Roadmap

## Overview

This document outlines the next 5 features to add to Blackjack2, prioritized by implementation effort and educational value. Each feature builds on the existing architecture without requiring major refactoring.

---

## 1. UI Integration of Basic Strategy Advisor

**Current State:** The basic-strategy logic lives in `BasicStrategy.hpp`/`BasicStrategy.cpp`, and `Game::giveBasicStrategyAdvice()` still returns the optimal move. It now renders in-game as a Book-box verdict (green = correct, red = deviation) instead of printing to console, and both deviation count and wins-after-deviation are tracked in `PlayerStats`. **Status: largely implemented.**

**Implementation:**
- Modify `HUD` to display the strategy recommendation as a visual element (e.g., "Book: HIT" text or icon next to the player's hand).
- Display this after the player makes action. Green if they acted as basic strategy would have suggested, red if they deviate from basic strategy.
- Add a toggle key (e.g., `B`) to show/hide the advisor
- Display the recommendation during `PlayerTurn` state only 
- Add a player statistic to track the number of times they deviate from basic strategy.
- Additionally, add a player statistic that tracks the number of times the player wins the hand after deviating from basic strategy. 
- Sometimes deviating from basic strategy is good 

**Value:** Transforms the game from a simulator into a learning tool, directly supporting the educational philosophy outlined in README_PHIL.md.

---

## 2. Advanced Splitting Rules & Edge Cases

**Current State:** Split aces (one card per hand) and re-split tens rules already exist (`test_split_aces.cpp`, `test_resplit_tens.cpp`). **Status: mostly implemented** — remaining work is exposing Double-After-Split / resplit toggles as configurable rules via `Constants.hpp`/`PlayerProfile`.

**Implementation:**
- **One-Card Aces:** When Aces are split, each new hand receives exactly one card (no hitting allowed)
- **Re-split Logic:** Allow re-splitting pairs (e.g., split 8s, hit an 8, split again)
- **Double After Split (DAS):** Allow doubling down on split hands
- **Resplitting Tens:** Add rule option to allow/disallow re-splitting 10-value cards
- Add rule configuration to `Constants.hpp` or `PlayerProfile`

**Value:** Makes the engine ruleset complete and configurable, matching real casino variations.

---

## 3. Advanced Analytics Dashboard

**Current State:** A full per-shoe analytics system now exists (`ShoeAnalytics`, surfaced through HUD boxes/graphs): greatest hand/shoe/session records, variance (standard deviation), longest winning/losing streaks, a bankroll curve, edge tracking over time, and a bet-distribution histogram — persisted via profile history. **Status: engine implemented** — remaining is a dedicated dashboard screen with tabs and CSV/JSON export.

**Implementation:**
- **Session History:** Track and display greatest single-hand win/loss, greatest shoe win/loss, greatest session win/loss
- **Variance Metrics:** Show standard deviation of hand results, longest losing streak, longest winning streak
- **Bankroll Curve:** Simple line chart showing bankroll over time (using SFML's drawing primitives)
- **Edge Tracking:** Display estimated player edge over time based on count
- **Bet Distribution:** Histogram of bet sizes placed during session
- Save analytics to `PlayerProfile` JSON for persistence across sessions

**Value:** Transforms the game into a serious training tool for advantage players, providing the feedback loop needed to understand variance and bankroll management.

---

## 4. Multi-Player / Table Simulation

**Current State:** NPC opponents already exist — up to 3 seats (human + NPCs playing basic strategy), selectable at setup, plus an AutoPlay/fast-forward mode that advances every seat on basic strategy for statistical analysis. **Status: implemented.**

**Implementation:**
- Add 1-3 AI opponents that play basic strategy
- Each AI has its own bankroll and betting strategy (conservative, aggressive, Kelly-based, progressive)
- Player can choose to play against the house or observe AI players
- Add "Fast Forward" mode to simulate thousands of hands for statistical analysis
- Display each player's bankroll and betting pattern

**Value:** Creates a more realistic casino environment and enables advanced analysis of how different betting strategies perform over time. Supports the "Player as Investor" philosophy by allowing comparison of different capital allocation strategies.
 

---

## 5. Reko Counting System

**Current State:** The Reko counting system is now implemented (`BasicStrategy::calculateRekoValue`/`calculateRekoBet`, its own starting-offset and bet-pattern tables), selectable via the count-style toggle and labeled on the HUD alongside Hi-Lo. **Status: implemented.**

**Implementation:**
- Introduce the reko counting method.
- Rules are the same as hi-lo counting system except for these changes:
    - 7s are +1 instead of 0
    - There is no need to calculate the true count, we only track the running count. 
    - The true count is the running count.
    - The running count begins at -1 for single deck, -5 for 2 deck, -12 for 4 deck, -20 for 6 deck, -27 for 8 deck.
    - At TC <= 0, bet 1 unit. 0 < TC < 2 bet 2 units. For every TC > 2, bet (1 + TC) units
- Add an constant to toggle which counting system to use. Modify count functions to comply with a new counting system.
- Display which system they're using nearby the true count on the HUD. Either "Hi-Lo" or "REKO".

**Value:** Adds another counting system that has simplified deviation points. No need to memorize complicated deviations like in the hi lo system.

--- 
