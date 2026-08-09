# Blackjack2 - Technical Project Summary

## Overview

**Blackjack2** is a single-player casino-style blackjack game built in C++ using the SFML library. The game implements a complete blackjack experience with card counting (Hi-Lo system), Kelly criterion betting, basic strategy guidance, full game recording, and persistent player profiles. Designed for both entertainment and card-counting practice, it tracks detailed statistics across hands, shoes, and sessions.

## Technology Stack

- **Language**: C++17 (CMake minimum 3.10)
- **Graphics/Audio/Input**: SFML 2.5 (graphics, window, system, audio, network)
- **JSON Serialization**: nlohmann/json (embedded single-header library in `include/nlohmann/`)
- **Build System**: CMake with presets
- **Platforms**:
  - Native Linux (GCC 13.3.0)
  - Windows via MinGW cross-compilation (`mingw-win64-toolchain.cmake`)

## Project Structure

```
.
├── main.cpp                     # Entry point; instantiates Game and runs event loop
├── CMakeLists.txt               # Build configuration (SFML linking, asset copying)
├── CMakePresets.json            # CMake preset for GCC 13.3.0 x86_64-linux-gnu
├── mingw-win64-toolchain.cmake  # MinGW cross-compilation toolchain
├── include/                     # Public headers
│   ├── Card.hpp                 # Card representation (value, suit, helpers)
│   ├── Deck.hpp                 # Multi-deck shoe with shuffle, draw, cut card
│   ├── Hand.hpp                 # Player/dealer hand logic (value, blackjack, bust)
│   ├── Game.hpp                 # Core game engine and state machine
│   ├── Player.hpp               # Player entity (bankroll, hands, betting)
│   ├── PlayerProfile.hpp        # Persistent profile + history storage
│   ├── GameRecord.hpp           # Data structures for stats, rounds, shoes
│   ├── Serialization.hpp        # nlohmann/json adapters for all game types
│   ├── Constants.hpp            # Global constants + basic strategy tables
│   ├── Graphics.hpp             # Card rendering utilities
│   ├── HUD.hpp                  # Heads-up display with panels and stats
│   ├── HUDState.hpp             # Data struct passed from Game → HUD
│   ├── Panel.hpp                # Reusable UI panel component
│   ├── StatBox.hpp              # Label/value display widget
│   ├── FontManager.hpp          # Singleton font loader
│   └── nlohmann/json.hpp        # Embedded JSON library
├── src/                         # Implementation files (one per header)
├── assets/
│   ├── fonts/roboto/            # Game fonts (RobotoFlex variable font)
│   └── images/
│       ├── felt.png             # Table background
│       └── cardsLarge_tilemap_packed.png  # Card sprite sheet (64x64 per card)
└── docs/
    ├── screenshots/             # UI screenshots
    └── summaries/               # Previous commit/project summaries
```

## Core Classes and Responsibilities

### Game (include/Game.hpp, src/Game.cpp)
The central orchestrator. Responsibilities:
- Manages the SFML window (1400×800), view, and main loop
- Controls the round state machine:
  - `PlaceBets` → `OfferInsurance` (if dealer shows Ace) → `PlayerTurn` → `DealerTurn` → `Cleanup`
- Handles all input (keyboard for bets/actions, mouse for Kelly slider)
- Maintains Hi-Lo running count and computes true count:
  - `runningCount++` for cards 2–6
  - `runningCount--` for cards 10–A
  - `trueCount = runningCount / (remaining cards / 52)`
- Tracks per-hand actions (for recording)
- Manages shoe lifecycle (reshuffle when cut card reached)
- Persists data on exit and shoe completion

### Player (include/Player.hpp, src/Player.cpp)
Represents the human player:
- Manages bankroll (starting 1000)
- Holds a vector of `Hand` objects (supports splitting up to 4 hands)
- Tracks `currentHand` index for turn-based play
- Delegates betting logic (`placeBet`, `winBet`, `loseBet`, `pushBet`)
- Stores a reference `PlayerProfile` and saves it to disk

### Deck (include/Deck.hpp, src/Deck.cpp)
Manages the shoe:
- Creates a multi-deck shoe (default: 2 decks, configurable via `Constants::NUM_DECKS`)
- Shuffles and tracks a "cut card" (special card with value `CUT_CARD_VALUE = 14`)
- Maintains an active shoe record (initial deck state + all drawn cards)
- Provides `reachedCutCard()` to signal end-of-shoe
- Supports retrieving previous shoe record for persistence

### Hand (include/Hand.hpp, src/Hand.cpp)
Encapsulates a single hand of cards:
- Computes hand value (handles soft/hard aces correctly)
- Flags: `isBlackjack`, `isBust`, `isSoft`, `isFinished`, `isDoubledDown`, `isSurrendered`, `isSplit`
- Determines eligibility for actions: `canSplit` (first two cards same value), `canDouble` (first two cards only)
- Tracks bet amount and insurance bet

### Card (include/Card.hpp, src/Card.cpp)
Represents a playing card:
- `value`: 1–13 for playing cards, 14 for the cut card
- `suit`: enum `Spades`, `Hearts`, `Diamonds`, `Clubs`
- Methods: `getBlackjackValue()` (A=1/11, face cards = 10), `isAce()`, `isFaceCard()`, `isCutCard()`

### PlayerProfile (include/PlayerProfile.hpp, src/PlayerProfile.cpp)
Persistent storage layer:
- Stores player name, bankroll, `PlayerStats`, and full history (`vector<ShoeRecord>`)
- Loads/saves as JSON to `../saves/player_profile.json` (configurable via `Constants::PLAYER_FILE`)
- Provides `addRoundRecord()` and `addShoeRecord()` for incremental updates

## Data Structures

### PlayerStats (include/GameRecord.hpp)
Tracks lifetime performance:
```cpp
struct PlayerStats {
    int shoesFinished, handsFinished;
    int wins, losses, pushes, blackjacks, splits, doubles;
    int betsPlaced;        // Count of individual bets (incl. doubles/splits)
    int betSum;            // Sum of all bets placed
    int greatestHand;      // Largest single-hand profit
    int greatestWin;       // Best cumulative win during a shoe
    int greatestLoss;      // Worst cumulative loss during a shoe
    int greatestSessionWin;     // Best cumulative win across all shoes this session
    int greatestSessionLoss;    // Worst cumulative loss across all shoes this session
};
```

### RoundRecord
Represents a single completed round:
```cpp
struct RoundRecord {
    vector<Card> dealerCards;
    vector<HandOutcome> playerHands;
    int runningCount, cardsRemaining;
    float trueCount;
    long long timestamp;    // Unix epoch seconds
};
```

### HandOutcome
Records one hand's result within a round:
```cpp
struct HandOutcome {
    vector<Card> cards;
    vector<string> actions;  // e.g., {"H", "H", "S"} for hit-hit-stand
    int wager, payout;
    bool won;
};
```

### ShoeRecord
Full record of a shoe from shuffle to cut card (or player exit):
```cpp
struct ShoeRecord {
    vector<Card> initialDeck;
    vector<RoundRecord> rounds;
    bool completed;          // true if cut card reached, false if mid-shoe quit
};
```

### HUDState (include/HUDState.hpp)
Data passed from Game to HUD each frame:
```cpp
struct HUDState {
    int bankroll, currentBet;
    string statusMessage;
    int streak;
    bool streakIsWin;
    int runningCount;
    float trueCount;
    float kellyFraction;
    bool riskSliderDragging;
    const PlayerStats* playerStats;
    const ShoeRecord* currentShoe;
    int cardsRemaining, decksRemaining;
};
```

## Betting System

### Kelly Criterion
The game implements fractional Kelly betting for optimal bet sizing:
```cpp
int calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet)
```
- Only bets above minimum when `trueCount >= 2.0` (positive player edge)
- Edge estimation (for H17, 3:2 rules): `playerEdge = 0.005 * (TC - 1) - 0.0005`
- Raw bet: `kellyFraction * bankroll * playerEdge`
- Default `kellyFraction = 0.33` (1/3 Kelly, ~3% risk of ruin)
- Player adjusts via slider in the HUD
- Clamped to `[MINIMUM_BET, MAXIMUM_BET]` (10–500 by default)

### Bet Controls
- Up/Down arrows: adjust bet by 5
- Left arrow: auto-set to Kelly-optimal bet
- Right arrow: confirm bet and deal

## Game Rules

| Rule | Value |
|------|-------|
| Decks in shoe | 2 (`Constants::NUM_DECKS`) |
| Blackjack payout | 3:2 (`Constants::BLACKJACK_PAYOUT = 1.5`) |
| Dealer stands on | 17 (`Constants::DEALER_STAND`), hits soft 17 (H17) |
| Max split hands | 4 (`Constants::MAX_SPLIT_HANDS`) |
| Double down | First two cards only |
| Surrender | First two cards only (late surrender), returns half bet |
| Insurance | Up to half of original bet; pays 2:1 |
| Min/Max bet | 10 / 500 |

## Basic Strategy

Embedded in `Constants.hpp` as lookup tables (`BasicStrategy` namespace):
- `HARD[10][10]`: Hard totals (8–17) vs dealer upcard (2–A)
- `SOFT[8][10]`: Soft totals (A2–A9) vs dealer upcard
- `PAIRS[10][10]`: Split decisions for each pair vs dealer upcard
- `SURRENDER[2][10]`: Surrender recommendations for 15 and 16

Notation:
- `H` = Hit
- `S` = Stand
- `D` = Double
- `M` = Double if allowed, else Stand (`DOUBLES_STAND`)
- `Y` = Split
- `N` = Don't split
- `R` = Surrender
- ` ` (space) = No surrender recommendation

Press `I` during player turn to see the recommended action.

## Card Rendering

Card sprites are drawn from a packed tilemap (`cardsLarge_tilemap_packed.png`):
- Each card is 64×64 pixels (`Constants::CARD_WIDTH`, `Constants::CARD_HEIGHT`)
- Grid layout: 14 values (1–13 + cut card) × 4 suits = 56 tiles
- X padding between columns: 11 pixels
- Y padding between rows: 2 pixels
- `Graphics::makeCardSprite()` computes the correct texture rect for each card
- `Graphics::makeCardBackSprite()` returns the card back graphic

## UI / HUD

### Panels
- **Left Panel**: Player statistics (bankroll, wins/losses, blackjacks, etc.)
- **Right Panel**: Current shoe progress (rounds played, cards remaining)
- **Bottom Panel**: Current bet, running count, true count, status messages

### Slider
- Risk-of-Ruin slider controls `kellyFraction` (0.0 to 1.0)
- Draggable with mouse; value reflected in HUD

### Controls (printed to console at startup)
```
BETTING PHASE
  ↑        Increase Bet
  ↓        Decrease Bet
  ←        Auto Kelly Bet
  →        Confirm Bet

PLAYER TURN
  H        Hit
  S        Stand
  D        Double Down
  P        Split
  Y        Surrender
  I        Basic Strategy Advice

ROUND / GAME
  R        Next Round (after round ends)
  R        Restart Game (when Game Over)
```

## Persistence

### Save Format
Player profile saved as JSON to `../saves/player_profile.json`:
```json
{
  "name": "William",
  "bankroll": 1000,
  "stats": { /* PlayerStats fields */ },
  "longHistory": [ /* array of ShoeRecord objects */ ]
}
```

### Save Triggers
- On game exit (window closed): current incomplete shoe saved, profile updated
- On shoe completion (cut card reached): shoe marked `completed = true`, profile updated
- After each round: round record added to in-memory shoe record (synced to profile)

## Serialization

All game types are serializable via `nlohmann/json` adapters defined in `Serialization.hpp`:
- `Card`, `Suit`, `PlayerStats`, `HandOutcome`, `RoundRecord`, `ShoeRecord`, `PlayerProfile`
- Uses `to_json`/`from_json` free functions
- Backward-compatible loading (e.g., `ShoeRecord.completed` defaults to `true` if missing)

## Build Details

### CMakeLists.txt highlights
- Defines executable `Blackjack2`
- Includes `include/` directory
- Links SFML components: `sfml-graphics`, `sfml-window`, `sfml-system`, `sfml-audio`, `sfml-network`
- Copies `assets/` directory to build output after each build
- For Windows (MinGW):
  - Copies SFML DLLs to `bin/` subdirectory
  - Copies MinGW runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`, etc.)

### Presets
- Default: "GCC 13.3.0 x86_64-linux-gnu" with Debug build type

## Key Design Decisions

1. **Cut Card**: Instead of tracking penetration separately, a special card (value 14) is inserted into the shoe. When drawn, `Deck::reachedCutCard()` returns `true`, signaling the shoe should be reshuffled after the current round.

2. **Hand Actions Tracking**: Parallel `vector<vector<string>> handActions` tracks actions per hand per round for later analysis/replay.

3. **State Machine**: Clear separation between round states prevents invalid actions (e.g., can't hit during betting phase).

4. **Kelly Betting**: Positive edge only when TC ≥ 2, based on edge formula derived from simulation data for H17/3:2 rules.

5. **Deck Reconstruction**: `reconstructDeckAtRound()` utility rebuilds the deck state at any point in a shoe's history by removing dealt cards sequentially. Useful for replay or analysis features.

6. **HUD Separation**: `HUDState` struct decouples game logic from rendering; Game populates the struct, HUD reads and renders.

7. **Font Singleton**: `FontManager` loads the font once at startup and provides global access, avoiding redundant loads.

## Current Player

Hardcoded player name "William" in `Game::Game()` constructor. The `Player` class accepts a name parameter but is always instantiated with "William" in the current implementation.

## Potential Future Enhancements (Not Yet Implemented)

- Reconstruct and replay past shoes from `ShoeRecord`
- Multiple player profiles / login system
- Configurable game rules (number of decks, payout ratios, etc.)
- Visual card counter display with color-coded advantage
- Sound effects via SFML Audio
- Deck visualization toggle (`drawDeck()` exists but is commented out)
- Surrender via 'Y' key

## Dependencies

- **Required**: SFML 2.5+ (with graphics, window, system, audio, network modules)
- **Embedded**: nlohmann/json (header-only, no external install needed)
- **Build**: CMake 3.10+, GCC 13.3.0 (or compatible C++17 compiler)
- **Assets**: All assets included in `assets/` directory; CMake copies them to build output.