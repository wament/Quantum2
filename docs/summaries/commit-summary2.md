"# Blackjack HUD & Architecture Changes — Summary

This document summarizes the key changes and design decisions made during the HUD overhaul, font system refactor, and related architecture work.

---

## 1. Font Management

### Problem
We kept running into the issue where `sf::Text` objects would silently fail to render because `setFont()` was never called on them — especially in places like the slider label in HUD. Passing font references through many layers made it easy to forget.

### Solution: FontManager Singleton
We introduced a central `FontManager` that loads the font once at startup and provides a single source of truth.

- **`include/FontManager.hpp` / `src/FontManager.cpp`**: Singleton-style manager.
- Load once at startup in `Game::loadAssets()`:
  ```cpp
  FontManager::load("assets/fonts/roboto/RobotoFlex-VariableFont.ttf");
  ```
- Any code that needs the font simply calls `FontManager::get()`.

This guarantees:
- Font is loaded before anything tries to use it.
- No more “missing font” bugs.
- Easy to swap fonts later without touching every file.

### HUD Font Size Constant
- Added `HUD_FONT_SIZE = 20` to `Constants.hpp`.
- Used by `StatBox` and HUD labels.
- Centralized so we can tweak font size from one place.

---

## 2. HUD Layout & Panel Margins

### Problem
We were confused about what `PANEL_MARGIN` actually meant:
- Initially it was spacing panels away from the screen edges.
- What we actually wanted was for panels to go flush to the edges, and use margin to space UI elements (statboxes, slider) away from the inside edges of their panels.

### Solution
- Panels are now flush against the screen edges:
  - Left panel anchored at `(0, 0)`.
  - Right panel anchored flush right.
  - Bottom panel spans full width, anchored to bottom.
- `PANEL_MARGIN` now controls how far statboxes/slider are inset from their panel’s interior.
- Result: clean edge-to-edge panels with consistent breathing room for elements inside.

---

## 3. Risk of Ruin / Kelly Slider

### Feature
A draggable slider that lets you adjust your Kelly fraction (bet sizing aggressiveness).

### Implementation
- Slider input handled in `Game::processEvents()` via mouse events.
- `HUD::getRiskSliderBounds()` exposes slider geometry for hit-testing.
- Slider value synced through `HUDState.kellyFraction`.
- HUD renders slider track, fill, thumb, and label each frame.

### Label Logic
- Default text:
  - `Kelly: X% (~Y% RoR)` when Kelly ≤ 50%
- “Fun” mode:
  - `Kelly: X% | press it` in red when Kelly > 50%

This makes the risk level intuitive and playful without needing a full risk meter.

### Persistent Label
- HUD now owns a persistent `sf::Text m_sliderLabel`.
- Font, character size, and default color are set once in `HUD::initialize()`.
- Each frame, only the string, color, and position are updated.
- No more creating text on the stack every frame; no more forgetting `setFont()`.

---

## 4. Game Record & Stats Architecture

### Record Structure
We overhauled how rounds and shoes are recorded for analytics and persistence:

- **`HandOutcome`**: Cards, actions, wager, payout, win/loss for a single hand.
- **`RoundRecord`**:
  - Dealer cards
  - List of `HandOutcome` for all player hands
  - Running count, true count, cards remaining
  - Timestamp
- **`ShoeRecord`**:
  - Initial deck state (`std::vector<Card>`)
  - List of `RoundRecord`
  - `completed` flag (cut card reached vs quit mid-shoe)

This makes it easy to:
- Reconstruct deck state at any round.
- Compute shoe-level and session-level stats.
- Save/load history to JSON.

### Stats Tracking
`PlayerStats` is incremented live as you play:
- Hands, wins, losses, pushes, blackjacks, splits, doubles
- Bets placed, total bet sum
- Greatest single hand win
- Shoe-level and session-level greatest win/loss

All stats persist across sessions via `PlayerProfile`.

---

## 5. Persistence & Player Profiles

### JSON Serialization
We implemented full serialization using nlohmann/json:
- `Card`, `HandOutcome`, `RoundRecord`, `ShoeRecord`, `PlayerStats`, `PlayerProfile`
- Centralized in `Serialization.hpp`

### PlayerProfile
- Loads from `save/player_profile.json` on startup.
- Saves on:
  - End of shoe
  - Window close
- Stores bankroll, stats, and long-term shoe history.
- Ensures save directory exists via `std::filesystem::create_directories`.

---

## 6. Deck & Shoe Tracking

### Shoe Records
Deck now tracks:
- `activeShoeRecord`: the current shoe state.
- `previousShoeRecord`: the last completed shoe (ready to be saved).
- `hasPreviousRecord` flag prevents double-saving.

When you shuffle:
1. If there’s an active record, move it to previous.
2. Perform shuffle.
3. Capture new active shoe record.

This cleanly separates “what just finished” from “what’s currently playing”.

---

## 7. Gameplay & Rules

### H17 (Hit Soft 17)
- Added `Hand::isSoft()`.
- Dealer now hits on soft 17, matching common casino rules.

### Betting
- **Right Arrow**: Auto Kelly bet using current true count and bankroll.
- Kelly bet formula:
  - Only uses positive edge (TC ≥ 2).
  - Player edge = `0.005 * (TC - 1) - 0.0005`.
  - Bet = `kellyFraction * bankroll * playerEdge`, clamped to table limits.

### Surrender
- Moved from `R` to `G` to avoid conflict with “restart/next round”.

### Blackjack
- Only natural (non-split) blackjacks pay 3:2.
- Split blackjacks pay even money.

---

## 8. Key Design Principles

1. **Single Source of Truth**
   - FontManager for fonts.
   - Constants for layout values.
   - HUDState as the single data blob between Game and HUD.

2. **Flush Panels, Internal Margins**
   - Panels go edge-to-edge.
   - `PANEL_MARGIN` controls internal spacing, not external.

3. **Persistent UI Objects**
   - Create SFML objects once (labels, statboxes, panels).
   - Update strings/positions each frame instead of recreating.

4. **Rich Game History**
   - Detailed records per round and shoe.
   - Stats updated live and persisted.

5. **Fun but Informative UI**
   - Slider label is both informative (RoR) and playful (“press it”).
   - HUD provides clear counting and state info without clutter.

---

## 9. Files Added/Modified (High Level)

### New Files
- `include/FontManager.hpp` / `src/FontManager.cpp`
- Overhauled `include/GameRecord.hpp`
- Overhauled `include/Serialization.hpp`

### Major Changes
- `include/HUD.hpp`, `src/HUD.cpp`
- `include/Constants.hpp`
- `src/StatBox.cpp`
- `include/Game.hpp`, `src/Game.cpp`
- `include/Deck.hpp`, `src/Deck.cpp`
- `include/Player.hpp`, `src/Player.cpp`
- `include/PlayerProfile.hpp`, `src/PlayerProfile.cpp`
- `include/Hand.hpp`, `src/Hand.cpp`
- `CMakeLists.txt` (added FontManager.cpp)

---

This summary is meant as a living reference — update it as we iterate.
"