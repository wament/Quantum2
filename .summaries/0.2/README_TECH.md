# Blackjack C++ Engine - Technical Overview

This document details the architecture, design choices, class breakdown, and strategic algorithms implemented in this C++ Blackjack engine.

---

## 1. Design Choices & Architecture

### Core Design Philosophy
The engine is built around a **Finite State Machine (FSM)** pattern for game flow and a strict separation between **Game Logic** and **UI State**. It leverages modern C++ features for performance and type safety while maintaining a dependency on SFML for rendering.

### Key Architectural Decisions

*   **Finite State Machine (FSM):** The `RoundState` enum (`PlaceBets`, `PlayerTurn`, `DealerTurn`, etc.) acts as the central dispatcher. The `Game::update()` function switches behavior based on the current state, preventing complex nested conditionals.
*   **Data-Driven UI (HUDState):** Instead of the HUD querying the `Game` or `Player` classes directly, `Game::updateHUDState()` populates a data-only struct (`HUDState`). This decouples the rendering layer from the logic layer, making the UI code purely about drawing what it's given.
*   **Shoe Management:** The `Deck` class manages the "Shoe" (collection of cards) and handles the **Cut Card** mechanism. It maintains a history of the shoe (`activeShoeRecord`) to allow for "Shoe Replays" or post-game analysis by reconstructing the deck state at specific rounds.
*   **Modern C++ Usage:**
    *   **`enum class`**: Used for `Suit`, `RoundState`, and `Action` to prevent integer overflow bugs and improve scope safety.
    *   **`constexpr`**: All layout and rule constants are defined at compile time for zero-overhead lookups.
    *   **`std::vector` & `std::unique_ptr`**: Used for resource management and dynamic arrays.

### Strategic Integration
The game is designed not just as a simulator, but as a **Counting Trainer**.
1.  **Hi-Lo Counting:** The `Game::afterDraw()` method updates the Running Count every time a card is drawn.
2.  **Kelly Criterion:** The `Game::calculateOptimalBet()` function uses the True Count to suggest optimal bet sizes based on the player's bankroll, reducing risk of ruin.

---

## 2. Class Breakdown & Contents

### `Card` (Core Entity)
**File:** `include/Card.hpp`, `src/Card.cpp`
A simple struct representing a playing card.
*   **Members:** `int value` (1-13), `Suit suit`.
*   **Methods:**
    *   `getBlackjackValue()`: Returns 11 for Ace, 10 for Face cards, otherwise face value.
    *   `isAce()`, `isFaceCard()`, `isCutCard()`: Type checking helpers.

### `Deck` (Shoe Management)
**File:** `include/Deck.hpp`, `src/Deck.cpp`
Manages the physical cards and the cut card logic.
*   **Members:** `std::vector<Card> cards`, `activeShoeRecord` (vector of cards for replay), `previousShoeRecord`.
*   **Key Logic:**
    *   `create()`: Initializes a multi-deck shoe.
    *   `shuffle()`: Shuffles the deck, inserts the Cut Card (value 14), and starts recording the active shoe.
    *   `draw()`: Returns the top card. If the Cut Card is drawn, it sets `cutCardReached` to true and immediately returns the *next* card from the shoe.

### `Hand` (Game State)
**File:** `include/Hand.hpp`
Represents a single hand (Player or Dealer).
*   **Members:** `std::vector<Card> cards`, `currentBet`, `insuranceBet`, `finished`, `doubledDown`, `split`.
*   **Methods:**
    *   `getValue()`: Calculates hand total. **Critical:** Handles Aces dynamically (Soft vs. Hard totals).
    *   `isSoft()`: Returns true if the hand contains an Ace counted as 11.
    *   `canDouble()`, `canSplit()`: Validates rules (e.g., can only double on first two cards).

### `Player` (Wallet & Multi-Hand)
**File:** `include/Player.hpp`, `src/Player.cpp`
Manages the user's bankroll and multiple simultaneous hands (for splits).
*   **Members:** `bankroll`, `std::vector<Hand> hands`, `currentHand` (index).
*   **Methods:**
    *   `placeBet()`, `winBet()`, `loseBet()`: Atomic updates to bankroll.
    *   `getProfile()` / `updateProfile()`: Connects the game session to persistent JSON data (`PlayerProfile`).

### `Game` (The Brain)
**File:** `include/Game.hpp`, `src/Game.cpp`
The central controller.
*   **Members:** `window`, `deck`, `dealer`, `player`, `HUD`, `hudState`, `kellyFraction`.
*   **Key Methods:**
    *   `run()`: The main game loop (Input -> Update -> Render).
    *   `evaluateHands()`: Compares player hands against dealer. Handles insurance, pushes, and payouts.
    *   `calculateOptimalBet()`: Implements the **Kelly Criterion**: `Bet = KellyFraction * Bankroll * Edge`.
    *   `giveBasicStrategyAdvice()`: Returns a char ('H', 'S', 'D', etc.) based on the hardcoded strategy table in `Constants`.

### `HUD` & `HUDState` (Interface)
**File:** `include/HUD.hpp`, `include/HUDState.hpp`
*   **HUDState:** A "Data Transfer Object" (DTO). It holds simple integers/floats (Bankroll, Current Bet, Running Count) so the HUD doesn't need to know about `Game` internals.
*   **HUD:** Renders the panels, slider, and text using SFML. It listens to mouse events for the Kelly Fraction slider.

### `PlayerProfile` (Persistence)
**File:** `include/PlayerProfile.hpp`
Handles saving/loading the player's bankroll and history.
*   **Members:** `nlohmann::json` support.
*   **Data:** Stores `PlayerStats` (wins, losses, splits) and `RoundRecord`s for history.

### `Constants` (Configuration)
**File:** `include/Constants.hpp`
*   **Static Data:** Window size, card sprite padding, payout ratios (1.5x for Blackjack).
*   **Strategy Tables:** Hardcoded `char` arrays (`HARD[10][10]`, `SOFT[10][10]`) defining the optimal play for every possible player/dealer card combination.

---

## 3. Strategic Integration: The Math

The engine implements two key mathematical models:

### Hi-Lo Card Counting
Implemented in `Game::afterDraw()`.
*   **Logic:** 
    *   Cards 2-6: +1
    *   Cards 7-9: 0
    *   Cards 10-A: -1
*   **True Count:** `RunningCount / (DecksRemaining / 52)`. This normalizes the count based on deck penetration, essential for accurate betting advice.

### Kelly Criterion
Implemented in `Game::calculateOptimalBet()`.
*   **Logic:** `BetSize = KellyFraction * Bankroll * Edge`.
*   **Edge:** Derived from the True Count.
*   **Purpose:** Maximizes long-term growth while minimizing risk of ruin. The default `kellyFraction` is set to `0.33` (1/3 Kelly) for conservative play.

---

## 4. What We Are Positioned to Make Progress On

### 1. UI Integration of Basic Strategy
Currently, `BasicStrategy` is fully implemented in `Constants.hpp` and logic exists in `Game::giveBasicStrategyAdvice()`, but it only prints to the console.
*   **Action:** Modify `HUD` to display the "Book Suggestion" (e.g., a ghost icon or text next to the hand) to help the player learn.

### 2. Advanced Splitting Rules
The current `Hand` and `Game` logic supports splitting, but edge cases need refinement:
*   **Re-splitting Aces:** Currently, splitting an Ace usually results in a "Hard 11" or stops drawing. The engine needs to explicitly handle "One Card Only" logic when Aces are split.
*   **Resplitting Face Cards:** Verify if the engine prevents re-splitting 10s (usually allowed) vs Aces.

### 3. Advanced Analytics (Shoe Replays)
The `Deck` class saves `ShoeRecord`.
*   **Action:** Implement a "Replay Mode" where the user can press a key to step through the previous shoe card-by-card, visualizing the count and deck penetration.

### 4. Testing Suite Expansion
`catch.hpp` is included in `include/catch.hpp`.
*   **Action:** The `tests/` folder exists but is sparse. Add unit tests for `Hand::getValue()` (Soft/Hard transitions) and `Deck::shuffle()` (statistical randomness) to ensure logic stability.