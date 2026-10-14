# Proposed Features for Blackjack2

This document outlines 10 new features to enhance Blackjack2, with detailed implementation notes for the top 6.

---

## All 10 Proposed Features

| # | Feature | Category |
|---|---------|----------|
| 1 | Side Bets (Perfect Pairs, 21+3) | Gameplay |
| 2 | Strategy Trainer Mode | Education |
| 3 | Custom Game Rules | Configuration |
| 4 | Statistics Dashboard | Analytics |
| 5 | Achievement System | Engagement |
| 6 | Additional Card Counting Systems | Strategy |
| 7 | Sound Effects & Music | Polish |
| 8 | Animated Card Dealing | Polish |
| 9 | Tournament Mode | Gameplay |
| 10 | Multiplayer Networking | Social |

---

## Detailed Feature Proposals

### 1. Side Bets

**Overview:** Add optional side bets that players can place before each deal. These are independent of the main hand outcome and offer higher payouts for specific card combinations.

**Side Bets to Include:**

- **Perfect Pairs** (placed before deal)
  - Mixed Pair (same rank, different suit/color): 10:1
  - Colored Pair (same rank, same color): 25:1
  - Perfect Pair (same rank, same suit): 30:1

- **21+3** (placed before deal)
  - Three of a Kind: 30:1
  - Straight Flush: 40:1
  - Flush: 9:1
  - Straight: 5:1
  - Pair: 2:1
  - Uses player's two cards + dealer's up card

**Implementation Approach:**
- Add `SideBet` enum and `SideBetType` to track active side bets
- Extend betting phase to include side bet options
- Evaluate side bets after cards are dealt, before main hand resolution
- Add side bet results to `RoundRecord` for replay and stats
- Update HUD to show side bet amounts and results
- Track side bet win/loss separately in `PlayerStats`

**Files to Modify:**
- `include/Hand.hpp` / `src/Hand.cpp` - Side bet evaluation helpers
- `include/Game.hpp` / `src/Game.cpp` - Betting flow, side bet placement
- `include/PlayerStats.hpp` - Side bet statistics
- `include/GameRenderer.hpp` / `src/GameRenderer.cpp` - Side bet UI
- `include/HUDState.hpp` - Side bet display data

**Estimated Effort:** Medium (2-3 days)

---

### 2. Strategy Trainer Mode

**Overview:** A dedicated practice mode that presents random hands to the player and scores their decisions against basic strategy. Ideal for learning and improving play.

**Features:**
- Random hand generation with configurable parameters (dealer up card, hand type)
- Instant feedback on each decision (correct/incorrect)
- Score tracking: accuracy percentage, streaks, time per decision
- Focus modes: soft hands only, pairs only, hard hands only, all hands
- Timed challenge: answer as many as possible in 2 minutes
- Deviation history: review mistakes after session
- Leaderboard for best accuracy scores

**Implementation Approach:**
- New `AppMode::StrategyTrainer` in the app mode enum
- `StrategyTrainer` class with its own state machine
- Reuse `giveBasicStrategyAdvice()` for answer checking
- Generate random hands using existing `Deck` and `Hand` classes
- Track session stats separately from regular gameplay
- Add menu option in main menu

**Files to Modify:**
- `include/Game.hpp` / `src/Game.cpp` - New app mode, event handling
- `include/StrategyTrainer.hpp` / `src/StrategyTrainer.cpp` - New class
- `include/GameRenderer.hpp` / `src/GameRenderer.cpp` - Trainer UI
- `include/PlayerStats.hpp` - Trainer session stats

**Estimated Effort:** Medium (2-3 days)

---

### 3. Custom Game Rules

**Overview:** Allow players to configure house rules that affect both gameplay and basic strategy advice. Different casinos have different rules, and this adds replay variety.

**Configurable Rules:**
- Number of decks: 1, 2, 4, 6, or 8
- Dealer behavior on soft 17: Hit or Stand
- Double after split: Allowed or Not
- Resplit aces: Allowed or Not
- Surrender type: Late, Early, or None
- Blackjack payout: 3:2 or 6:5
- Number of players: 1-3 (existing)

**Implementation Approach:**
- Create `GameRules` struct with all configurable options
- Add rules configuration screen accessible from main menu or pause menu
- Pass rules to `giveBasicStrategyAdvice()` for accurate recommendations
- Update `Deck::create()` to accept deck count
- Modify `dealerTurn()` to respect soft 17 rule
- Update split/double logic to respect DAS and resplit rules
- Save/load rules with player profile

**Files to Modify:**
- `include/GameRules.hpp` / `src/GameRules.cpp` - New class
- `include/Game.hpp` / `src/Game.cpp` - Rules application throughout
- `include/Deck.hpp` / `src/Deck.cpp` - Variable deck count
- `include/PlayerProfile.hpp` - Save rules with profile
- `include/GameRenderer.hpp` / `src/GameRenderer.cpp` - Rules config UI

**Estimated Effort:** Medium-High (3-4 days)

---

### 4. Statistics Dashboard

**Overview:** A comprehensive analytics screen showing detailed performance metrics with visualizations. Helps players understand their strengths and weaknesses.

**Metrics to Display:**
- Overall win rate (hands, shoes, sessions)
- Win rate by hand type: hard hands, soft hands, pairs
- Win rate by dealer up card (2 through Ace)
- Bankroll trend graph over time (line chart)
- Betting efficiency: actual returns vs expected Kelly returns
- Strategy adherence percentage
- Average bet size vs bankroll percentage
- Bust frequency
- Blackjack frequency

**Implementation Approach:**
- New `AppMode::StatsDashboard`
- `StatsDashboard` class that reads from `PlayerStats` and profile history
- Simple line chart rendering using SFML shapes
- Bar charts for win rate by dealer card
- Tabbed interface: Overview, By Hand Type, By Dealer Card, Trends
- Export functionality: save stats as CSV or JSON

**Files to Modify:**
- `include/Game.hpp` / `src/Game.cpp` - New app mode
- `include/StatsDashboard.hpp` / `src/StatsDashboard.cpp` - New class
- `include/PlayerStats.hpp` - Additional stat categories
- `include/GameRenderer.hpp` / `src/GameRenderer.cpp` - Chart rendering
- `include/Serialization.hpp` - CSV/JSON export

**Estimated Effort:** Medium-High (3-4 days)

---

### 5. Achievement System

**Overview:** Fun, shareable achievements that encourage replay and reward skill development.

**Achievement Categories and Examples:**

*First Steps:*
- "First Deal" - Play your first hand
- "Natural 21" - Get your first blackjack
- "Split Success" - Split a pair and win both hands

*Card Counting:*
- "Counter's Edge" - Maintain positive true count for 10 consecutive hands
- "Big Bet" - Place a bet of 10% or more of your bankroll
- "Kelly Master" - Use full Kelly (fraction = 1.0) for an entire shoe

*Performance:*
- "Comeback King" - Win $500 after being down $500 in a shoe
- "Perfect Shoe" - Finish a shoe ahead without busting
- "Split Master" - Split 5 pairs in a single shoe
- "Blackjack Streak" - Get 3 blackjacks in one shoe

*Strategy:*
- "Textbook Player" - Play 50 consecutive hands perfectly per basic strategy
- "Deviation Free" - Complete a full shoe with zero strategy deviations

**Implementation Approach:**
- `Achievement` struct with ID, name, description, condition, unlocked status
- `AchievementManager` class that checks conditions after each hand/shoe
- Achievement notification popup when unlocked
- Achievement list view in pause menu or main menu
- Persist unlocked achievements in player profile JSON
- Achievement progress tracking (e.g., "3/5 splits this shoe")

**Files to Modify:**
- `include/Achievement.hpp` / `src/Achievement.cpp` - New classes
- `include/Game.hpp` / `src/Game.cpp` - Achievement checking hooks
- `include/PlayerProfile.hpp` - Persist achievements
- `include/GameRenderer.hpp` / `src/GameRenderer.cpp` - Notification UI, achievement list
- `include/HUDState.hpp` - Achievement notification data

**Estimated Effort:** Medium (2-3 days)

---

### 6. Additional Card Counting Systems

**Overview:** Beyond the current Hi-Lo system, add support for other popular counting systems to give serious players more tools.

**Counting Systems to Add:**

*KO (Knock-Out) Count:*
- Unbalanced system (no true count needed)
- 2-6: +1, 7-9: 0, 10-Ace: -1
- Starting count = 0 (no adjustment for decks)
- Simpler than Hi-Lo for beginners

*Wong Halves:*
- More accurate fractional system
- 2: +0.5, 3: +0.5, 4: +1, 5: +1, 6: +1, 7: 0, 8: 0, 9: -0.5, 10-Ace: -1
- Requires true count calculation
- Higher correlation with actual advantage

*Omega II:*
- Multi-level system
- 2, 7: +1, 3-6: +2, 8, 9, 10: 0, Ace: -2
- Requires true count calculation
- Very accurate but complex

**Implementation Approach:**
- `CountingSystem` enum: HiLo, KO, WongHalves, Omega2
- `CardCounter` class that encapsulates counting logic
- Each system has its own value table and (if balanced) true count calculation
- Strategy deviation tables per counting system (or simplified: use Hi-Lo deviations for all)
- Add counting system selector in menu
- Display running count and true count with system name
- Track accuracy of count (if player manually enters count for practice)

**Files to Modify:**
- `include/CardCounter.hpp` / `src/CardCounter.cpp` - New class
- `include/Game.hpp` / `src/Game.cpp` - Counting system integration
- `include/GameRenderer.hpp` / `src/GameRenderer.cpp` - Count display, selector UI
- `include/PlayerProfile.hpp` - Save preferred counting system

**Estimated Effort:** Medium-High (3-4 days)

---

## Quick Wins (Lower Effort)

### 7. Sound Effects & Music
- Card dealing, chip placement, win/loss sounds
- Background music with volume controls
- SFML Audio module already a dependency
- **Effort:** Low (1 day)

### 8. Animated Card Dealing
- Cards slide from shoe to table position
- Card flip animation when revealed
- Dealer hole card reveals with animation
- Simple tweening in `GameRenderer`
- **Effort:** Low-Medium (1-2 days)

### 9. Tournament Mode
- Fixed number of hands, equal starting chips
- Highest chip count wins
- Blind betting (no choice of bet size)
- Reuses most existing round logic
- **Effort:** Medium (2-3 days)

### 10. Multiplayer Networking
- Local network play using SFML Network module
- Host/join game lobby
- Chat functionality
- Spectator mode
- **Effort:** High (5-7 days)

---

## Recommended Implementation Order

1. **Sound Effects & Music** (quick win, immediate polish)
2. **Side Bets** (high player demand, fits architecture)
3. **Strategy Trainer Mode** (educational value, reuses existing code)
4. **Achievement System** (engagement, moderate effort)
5. **Custom Game Rules** (replay variety, strategic depth)
6. **Statistics Dashboard** (analytics for serious players)
7. **Additional Card Counting Systems** (depth for counters)
8. **Animated Card Dealing** (visual polish)
9. **Tournament Mode** (new gameplay variant)
10. **Multiplayer Networking** (highest complexity, highest reward)

---

## Notes

- All features should maintain compatibility with existing save files (JSON profiles)
- New features should be toggleable where appropriate
- Unit tests should be added for new game logic (side bet evaluation, counting systems, etc.)
- CI/CD pipeline should be updated to test new features
- Documentation (README, TECHNICAL_README) should be updated for each feature