# Blackjack Game — Technical Documentation

## Overview

A C++ blackjack simulator built with SFML 2.6 for graphics. Supports single or multiple players (up to 3), card counting (Hi-Lo system), Kelly criterion-based betting, basic strategy advice with deviation tracking, NPC players, game replay, and persistent player profiles via JSON serialization.

## Architecture

### Class Hierarchy and Relationships

```
Game (main controller)
├── Deck (shoe management, shuffling, cut card detection)
├── Hand (card collection, value calculation, action flags)
│   └── Card (value, suit, blackjack value computation)
├── Player (bankroll, betting, stats, profile persistence)
│   ├── PlayerProfile (JSON serialization, history storage)
│   │   └── PlayerStats (aggregated statistics)
│   ├── RoundRecord (per-round outcome data)
│   └── ShoeRecord (full shoe history for replay)
├── HUD (status bar rendering)
├── HUDState (status bar data)
├── GameRenderer (all SFML drawing operations)
├── Replay (playback of recorded shoes)
└── FontManager (font loading and caching)
```

### Key Design Pattern

The `Game` class acts as a **God Object / Main Controller**, orchestrating all subsystems. This is a common pattern for single-file-style game architecture where a central controller manages state transitions, event handling, and delegates rendering to a dedicated renderer.

---

## Classes and Functions

### `Card` (struct)
**File:** `include/Card.hpp`, `src/Card.cpp`

Represents a single playing card with blackjack-specific value computation.

| Member | Type | Description |
|--------|------|-------------|
| `value` | `int` | Raw card value (1-13, where 1=Ace, 11-13=face cards) |
| `suit` | `Suit` (enum) | Spades, Hearts, Diamonds, Clubs |

**Methods:**
- `getBlackjackValue() const` → `int`: Returns 11 for Ace, 10 for face cards, face value otherwise
- `getBlackjackValueToString() const` → `std::string`: String representation of blackjack value
- `isAce() const` → `bool`: True if value is 1
- `isFaceCard() const` → `bool`: True if value is 11, 12, or 13
- `isCutCard() const` → `bool`: True if value is 14 (special cut card marker)
- `getHiLoValue() const` → `int`: Hi-Lo counting value (-1 for 10-Ace, 0 for 7-9, +1 for 2-6)
- `getSuitSymbol() const` → `char`: Unicode suit symbol (♠♥♦♣)

### `Deck` (class)
**File:** `include/Deck.hpp`, `src/Deck.cpp`

Manages a shoe of multiple decks, shuffling, drawing, and cut card detection.

| Member | Type | Description |
|--------|------|-------------|
| `cards` | `std::vector<Card>` | All cards in the shoe |
| `rng` | `std::mt19937` | Mersenne Twister random number generator |
| `lastShuffleSeed` | `unsigned long long` | Seed used for last shuffle (for replay) |
| `cutCardReached` | `bool` | Flag indicating cut card has been drawn |
| `activeShoeRecord` | `std::vector<Card>` | Record of current shoe for replay |
| `previousShoeRecord` | `std::vector<Card>` | Record of previous shoe (pending save) |

**Methods:**
- `create(int numberOfDecks)`: Creates a shoe with N decks plus one cut card
- `shuffle()`: Fisher-Yates shuffle using `std::mt19937` with random seed
- `draw()` → `Card`: Removes and returns the top card
- `getCard(std::size_t index) const` → `const Card&`: Access card at index
- `getCards() const` → `const std::vector<Card>&`: Access all cards
- `empty() const` → `bool`: True if no cards remain
- `size() const` → `std::size_t`: Number of cards remaining
- `reachedCutCard() const` → `bool`: True if cut card has been drawn
- `reshuffleWithRemaining(const std::vector<Card>& tableCards)`: Fallback reshuffle when deck runs out before cut card
- `getPreviousShoeRecord()` → `const std::vector<Card>*`: Retrieve previous shoe record (one-time)
- `getFinalShoeRecord() const` → `const std::vector<Card>&`: Get current shoe record
- `getLastShuffleSeed() const` → `unsigned long long`: Get last shuffle seed for replay

### `Hand` (class)
**File:** `include/Hand.hpp`, `src/Hand.cpp`

Represents a player or dealer hand with blackjack-specific logic.

| Member | Type | Description |
|--------|------|-------------|
| `cards` | `std::vector<Card>` | Cards in the hand |
| `finished` | `bool` | True if player is done taking actions |
| `doubledDown` | `bool` | True if player doubled down |
| `surrendered` | `bool` | True if player surrendered |
| `split` | `bool` | True if hand was split from a pair |
| `splitAce` | `bool` | True if hand is a split ace (special rules) |
| `currentBet` | `int` | Current bet amount |
| `insuranceBet` | `int` | Insurance bet amount |

**Methods:**
- `addCard(const Card&)`: Add a card to the hand
- `clear()`: Remove all cards
- `getValue() const` → `int`: Calculate hand value (Ace as 1 or 11, whichever is better)
- `isBlackjack() const` → `bool`: True if natural blackjack (Ace + 10-value card)
- `isBust() const` → `bool`: True if value > 21
- `isSoft() const` → `bool`: True if hand contains an Ace counted as 11
- `isSplitAce() const` → `bool`: True if split ace hand
- `canSplit() const` → `bool`: True if hand has exactly 2 cards of same value
- `canDouble() const` → `bool`: True if hand has exactly 2 cards
- `size() const` → `std::size_t`: Number of cards
- `getCard(std::size_t index) const` → `const Card&`: Get card at index
- `getDealerUpCard() const` → `const Card&`: Get dealer's up card (first card)
- `getDealerUpCardToString() const` → `std::string`: String representation of dealer up card
- `takeLastCard()` → `Card`: Remove and return last card (used in splitting)
- `setBet(int)` / `getBet() const`: Set/get bet amount
- `setInsuranceBet(int)` / `getInsuranceBet() const`: Set/get insurance bet
- `setFinished(bool)` / `isFinished() const`: Set/get finished status
- `setDoubledDown(bool)` / `isDoubledDown() const`: Set/get doubled down status
- `setSurrendered(bool)` / `isSurrendered() const`: Set/get surrendered status
- `setSplit(bool, int)` / `isSplit() const`: Set/get split status

### `Player` (class)
**File:** `include/Player.hpp`, `src/Player.cpp`

Represents a player with bankroll, betting, and statistics tracking.

| Member | Type | Description |
|--------|------|-------------|
| `name` | `std::string` | Player name |
| `bankroll` | `int` | Current bankroll (starts at 1000) |
| `profile` | `PlayerProfile` | Persistent profile data |
| `currentHand` | `int` | Index of current hand being played |
| `hands` | `std::vector<Hand>` | All hands for this player (after splits) |

**Methods:**
- `resetRound(int previousBet)`: Reset player state for new round
- `placeBet(int amount)` → `bool`: Place a bet, returns success
- `winBet(int amount)`: Add winnings to bankroll
- `loseBet(int amount)`: Subtract losses from bankroll
- `pushBet(int amount)`: Return bet on push
- `getName() const` → `const std::string&`: Get player name
- `getBankroll() const` → `int`: Get current bankroll
- `getProfile()` → `PlayerProfile`: Get profile (copy)
- `getStats()` → `PlayerStats&`: Get mutable reference to stats
- `updateProfile(PlayerProfile)`: Update profile data
- `saveProfile()`: Save profile to JSON file

### `PlayerProfile` (class)
**File:** `include/PlayerProfile.hpp`, `src/PlayerProfile.cpp`

Handles persistent player data with JSON serialization using nlohmann/json.

| Member | Type | Description |
|--------|------|-------------|
| `name` | `std::string` | Player name |
| `bankroll` | `int` | Persistent bankroll |
| `stats` | `PlayerStats` | Aggregated statistics |
| `currentHistory` | `std::vector<RoundRecord>` | Round records for current shoe |
| `longHistory` | `std::vector<ShoeRecord>` | Complete shoe history |

**Methods:**
- `load(const std::string& filename)` → `PlayerProfile`: Load profile from JSON file
- `save(const std::string& filename) const`: Save profile to JSON file
- `getName() const` → `const std::string&`: Get name
- `getBankroll() const` → `int`: Get bankroll
- `setBankroll(int)`: Set bankroll
- `getCurrentHistory()` → `std::vector<RoundRecord>`: Get current shoe history
- `addRoundRecord(const RoundRecord&)`: Add round to current history
- `addShoeRecord(const ShoeRecord&)`: Add completed shoe to long history
- `getLongHistory() const` → `const std::vector<ShoeRecord>&`: Get long-term history

### `Game` (class)
**File:** `include/Game.hpp`, `src/Game.cpp`

Main game controller managing state, events, betting, and game flow.

**State Enums:**
- `AppMode`: MainMenu, Playing, Paused, ReplayHistory, ReplayPlaying
- `RoundState`: PlaceBets, OfferInsurance, PlayerTurn, DealerTurn, Cleanup
- `GameState`: Started, LastHand, GameOver
- `GameSpeed`: Fast, Normal, Slow

**Key Members:**
- `window`: SFML render window (1280x720)
- `deck`: The shoe of cards
- `dealer`: Dealer's hand
- `players`: Vector of player objects
- `activePlayerIndex`: Index of player whose turn it is
- `roundState`: Current phase of the round
- `gameState`: Overall game state
- `initalBet`: Current bet amount
- `runningCount`: Hi-Lo running count
- `trueCount`: Running count adjusted for remaining decks
- `kellyFraction`: Kelly criterion fraction for betting (0-1)
- `playerCount`: Number of players (1-3)
- `hud`: HUD renderer
- `hudState`: HUD data
- `renderer`: Game renderer

**Key Methods:**
- `run()`: Main game loop (event processing, update, render)
- `startNewGame()`: Initialize a new game session
- `loadAssets()`: Load textures and fonts
- `nextRound()`: Transition to next round
- `deal()`: Deal initial cards to all players and dealer
- `processEvents()`: Handle SFML events based on app mode
- `update(float deltaTime)`: Update game state
- `updateHUDState()`: Refresh HUD data
- `increaseBet()` / `decreaseBet()`: Adjust bet amount
- `confirmBet()`: Confirm current bet
- `calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet)` → `int`: Kelly criterion bet calculation
- `hit()` / `stand()` / `doubleDown()` / `split()` / `surrender()`: Player actions
- `increaseInsurance()` / `decreaseInsurance()` / `confirmInsurance()` / `declineInsurance()`: Insurance actions
- `afterDraw(Card)`: Update count after each card draw
- `collectTableCards() const` → `std::vector<Card>`: Gather all table cards for reshuffle
- `handleEmptyDeck()`: Handle deck exhaustion before cut card
- `dealCardToPlayer(Hand&)` / `dealCardToDealer()`: Deal cards
- `dealerTurn()`: Execute dealer's automatic play
- `offerInsurance()`: Check if insurance should be offered
- `evaluateHands()`: Compare player hands against dealer
- `evaluateBlackjack()`: Check for natural blackjacks
- `getMaxInsurance()` → `int`: Calculate maximum insurance bet
- `giveBasicStrategyAdvice(Hand&, Hand&)` → `char`: Get basic strategy recommendation
- `logStrategyDeviation(char)`: Record strategy deviation
- `npcTakeTurn()`: Execute NPC player's turn
- `getGlobalHandIndexForPlayer(size_t, size_t)` → `size_t`: Compute flat hand index
- `incrementGlobalHandIndex(size_t)` → `size_t`: Advance to next hand

### `GameRenderer` (class)
**File:** `include/GameRenderer.hpp`, `src/GameRenderer.cpp`

Handles all SFML drawing operations, separated from game logic.

**Methods:**
- `draw(const Game&)`: Render the entire game state
- `drawCard(sf::RenderTarget&, const Card&, float x, float y, bool faceDown)`: Draw a single card
- `drawBackground(const Game&)`: Draw table background
- `drawDealerHand(const Game&)`: Draw dealer's hand
- `drawPlayerHands(const Game&)`: Draw all player hands
- `drawBetDisplay(const Game&)`: Draw bet amounts
- `drawCountDisplay(const Game&)`: Draw card count display
- `drawMainMenu(const Game&)`: Draw main menu
- `drawPauseMenu(const Game&)`: Draw pause menu
- `drawReplayMenu(const Game&)`: Draw replay history menu
- `drawSlider(const Game&)`: Draw Kelly fraction slider

### `HUD` (class)
**File:** `include/HUD.hpp`, `src/HUD.cpp`

Renders the status bar at the bottom of the screen.

**Methods:**
- `update(const HUDState&)`: Update HUD state data
- `draw(sf::RenderTarget&)`: Render the HUD

### `HUDState` (class)
**File:** `include/HUDState.hpp`, `src/HUDState.cpp`

Data structure for HUD information (bankroll, bet, count, etc.)

### `Replay` (class)
**File:** `include/Replay.hpp`, `src/Replay.cpp`

Handles playback of recorded shoe history.

**Methods:**
- `load(const ShoeRecord&)`: Load a shoe record for replay
- `update(float deltaTime)`: Advance replay state
- `isFinished() const` → `bool`: Check if replay is complete
- `getCurrentRound() const` → `const RoundRecord&`: Get current round in replay

### `FontManager` (class)
**File:** `include/FontManager.hpp`, `src/FontManager.cpp`

Manages SFML font loading and caching.

**Methods:**
- `getFont(const std::string& path)` → `sf::Font&`: Get or load font by path

### `Serialization` (namespace)
**File:** `include/Serialization.hpp`, `src/Serialization.cpp`

JSON serialization helpers for game data structures.

---

## Game Flow

```
┌─────────────────┐
│   Start Game    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐     ┌─────────────────┐
│   Main Menu     │────▶│  Player Count   │
│                 │     │  (1-3 players)   │
│ • Play Game     │     └────────┬────────┘
│ • Player Count  │              │
│ • Game Speed    │              ▼
│ • Review History│     ┌─────────────────┐
└────────┬────────┘     │  Kelly Fraction │
         │              │  Slider (0-1)   │
         │              └────────┬────────┘
         ▼                       │
┌─────────────────┐              ▼
│  Load Profile   │     ┌─────────────────┐
│  (if exists)    │     │   Start Game    │
└────────┬────────┘     └────────┬────────┘
         │                       │
         └───────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   New Shoe      │
                    │   (create &     │
                    │    shuffle)     │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │   Place Bets    │◀──┐
                    │   (Kelly calc)  │   │
                    └────────┬────────┘   │
                             │            │
                             ▼            │
                    ┌─────────────────┐   │
                    │     Deal        │   │
                    │   (2 cards each)│   │
                    └────────┬────────┘   │
                             │            │
                             ▼            │
                    ┌─────────────────┐   │
                    │  Insurance?     │   │
                    │  (dealer Ace?)  │   │
                    └────────┬────────┘   │
                             │            │
                             ▼            │
                    ┌─────────────────┐   │
                    │   Player Turn   │───┘
                    │   (hit/stand/   │
                    │    split/double)│
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │   Dealer Turn   │
                    │   (hit to 17)   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │   Evaluate      │
                    │   (win/loss/push)│
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │   Cleanup       │
                    │   (update stats,│
                    │    save record) │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  Cut Card       │
                    │  Reached?       │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                   YES               NO
                    │                 │
                    ▼                 ▼
           ┌─────────────────┐ ┌─────────────────┐
           │  Shoe Complete  │ │   Next Round    │
           │  (save & new)   │ │   (back to bets)│
           └─────────────────┘ └─────────────────┘
```

### Round State Machine

```
PlaceBets → OfferInsurance → PlayerTurn → DealerTurn → Cleanup → (next round or new shoe)
```

### Player Turn Actions

During `PlayerTurn`, each player can:
- **Hit**: Draw another card
- **Stand**: End their turn
- **Double Down**: Double bet and draw exactly one card (only on 2-card hands)
- **Split**: Split a pair into two separate hands (only on 2-card hands with same value)
- **Surrender**: Give up half the bet and end the hand

### Card Counting Integration

The game uses the **Hi-Lo counting system**:
- Cards 2-6: +1
- Cards 7-9: 0
- Cards 10-Ace: -1

The **true count** is calculated as: `runningCount / remainingDecks`

The true count is used for:
1. **Kelly Criterion Betting**: `bet = kellyFraction * (trueCount / decks) * bankroll`
2. **Basic Strategy Deviations**: Adjusting play based on count

### Basic Strategy

The game includes a basic strategy engine (`giveBasicStrategyAdvice`) that recommends optimal plays based on:
- Player's hand value
- Dealer's up card
- Whether the hand is soft (contains Ace counted as 11)
- Whether the hand is a pair

Deviations from basic strategy are tracked and logged.

---

## Data Types and Structures

### Card Value System

```cpp
// Raw values: 1=Ace, 2-9=face value, 10-12=face cards, 13=Ace (alternative), 14=cut card
// Blackjack values: Ace=11, 10-12=10, 2-9=face value
// Hi-Lo values: 2-6=+1, 7-9=0, 10-Ace=-1
```

### Hand Value Calculation

The `Hand::getValue()` method handles Ace flexibility:
1. Sum all cards (Ace = 11 initially)
2. If sum > 21 and hand has Ace, convert one Ace from 11 to 1
3. Repeat until sum ≤ 21 or no more Aces

This allows the hand to automatically use the optimal Ace value.

### Betting System

- **Initial Bet**: Set by player (or calculated via Kelly criterion)
- **Double Down**: Doubles the bet, requires one more card
- **Split**: Creates a new hand with the same bet
- **Insurance**: Side bet up to half the original bet when dealer shows Ace

### Statistics Tracking

`PlayerStats` tracks comprehensive metrics:
- **Shoe-level**: Shoes finished, greatest win/loss per shoe
- **Session-level**: Greatest win/loss across all shoes
- **Hand-level**: Wins, losses, pushes, blackjacks, splits, doubles
- **Betting**: Total bets placed, sum of bets, greatest single hand win
- **Strategy**: Hits matching basic strategy, deviations, recent deviation details

### Persistence

Player profiles are saved as JSON files using nlohmann/json:
```json
{
    "name": "Player",
    "bankroll": 1000,
    "stats": { ... },
    "currentHistory": [ ... ],
    "longHistory": [ ... ]
}
```

---

## Design Decisions

### 1. God Object Pattern for Game

The `Game` class centralizes all game logic, state management, and event handling. This simplifies the architecture for a single-player game and makes state transitions explicit. The trade-off is a large class, but it avoids complex inter-object communication.

### 2. Separation of Rendering

`GameRenderer` is a separate class that takes a `const Game&` reference, ensuring rendering is purely a view of game state. This allows for potential headless operation or different rendering backends.

### 3. Value Semantics for Cards

`Card` is a simple struct with value semantics, making it easy to copy, store in vectors, and pass around. This is appropriate for a lightweight data type.

### 4. Hand Value Computation

The `Hand::getValue()` method computes the optimal value on each call rather than caching it. This is simple and correct, and the performance cost is negligible for a card game.

### 5. JSON for Persistence

Using nlohmann/json provides a human-readable format for player profiles and game records, making debugging and data inspection easy.

### 6. Hi-Lo Card Counting

The Hi-Lo system is the most widely used card counting method, balancing simplicity with effectiveness. The true count adjustment accounts for the number of decks remaining.

### 7. Kelly Criterion Betting

The Kelly criterion provides a mathematically optimal betting strategy for positive-expectation games. The `kellyFraction` parameter allows the player to choose between full Kelly (aggressive) and fractional Kelly (conservative).

### 8. Strategy Deviation Tracking

Tracking deviations from basic strategy provides valuable feedback for learning and improvement. The recent deviations list helps identify patterns in suboptimal play.

---

## Build and Run

### Prerequisites
- CMake 3.15+
- SFML 2.6+
- C++17 compiler

### Build
```bash
mkdir build
cd build
cmake ..
make
```

### Run
```bash
./build/Blackjack
```

### Controls
- **1**: Hit
- **2**: Stand
- **3**: Double Down
- **4**: Split
- **5**: Surrender
- **↑/↓**: Adjust bet/insurance
- **Enter**: Confirm bet/insurance
- **P**: Pause
- **R**: Restart
- **Q**: Quit

---

## File Structure

```
.
├── CMakeLists.txt          # Build configuration
├── main.cpp                # Entry point
├── include/                # Header files
│   ├── Card.hpp
│   ├── Deck.hpp
│   ├── Hand.hpp
│   ├── Player.hpp
│   ├── PlayerProfile.hpp
│   ├── Game.hpp
│   ├── GameRenderer.hpp
│   ├── HUD.hpp
│   ├── HUDState.hpp
│   ├── Replay.hpp
│   ├── FontManager.hpp
│   ├── Serialization.hpp
│   └── ...
├── src/                    # Implementation files
│   ├── Card.cpp
│   ├── Deck.cpp
│   ├── Hand.cpp
│   ├── Player.cpp
│   ├── PlayerProfile.cpp
│   ├── Game.cpp
│   ├── GameRenderer.cpp
│   ├── HUD.cpp
│   ├── HUDState.cpp
│   ├── Replay.cpp
│   ├── FontManager.cpp
│   ├── Serialization.cpp
│   └── ...
├── assets/                 # Game assets (fonts, images)
└── tests/                  # Unit tests
```