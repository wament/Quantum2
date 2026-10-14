# Blackjack2 - Project Plan

## Phase 1: Replay System

### Core Concept
The replay system turns a recorded `ShoeRecord` back into a visual, step-by-step re-enactment. Users select a past shoe and watch it play out exactly as it happened—same cards, same decisions, same counts—but as a spectator with full information and analytical overlays. It's reviewing game film.

---

### User Experience Flow

#### Entry Point
- After a session (or from main menu), user chooses "Review History"
- Displays list of shoes ordered by date
- For each shoe: round count, cards played, final net result, completion status
- User selects a shoe → enters replay mode

#### Replay Controls
- Step forward (one decision/card)
- Step backward
- Jump to any round via scrollbar or round index
- Play/pause auto-advance at configurable speed
- Skip forward through current hand
- Skip forward through current round
- Exit back to history list or live game
- Keyboard shortcuts for power users (arrows to step, space to play/pause)

---

### Granularity: Action-Level with Skip (Option C)
**Decision: Option C with skip**

Each step = one discrete decision (hit, stand, double, split, dealer draws a card, etc.)

- **Default:** One step advances one action/card
- **Skip hand:** Fast-forward through remaining actions in current hand
- **Skip round:** Fast-forward through entire round

This gives maximum fidelity when studying decisions, and speed when skimming.

---

### Analytical Overlays (Study Mode)
**Decision: All overlays on the right HUD panel, visible only during replay**

The right panel (currently unused during live play) becomes the replay analysis display:

1. **Basic Strategy Overlay**
   - Shows recommended action vs. actual action taken
   - Flags deviations (neutral colors—yellow, not red)
   - Frames as insight, not judgment

2. **Count Overlay**
   - Running count and true count at each decision point
   - Watch how the count evolved over the shoe

3. **Bet Sizing / Kelly Overlay**
   - Kelly-optimal bet vs. actual bet
   - Highlights when bets didn't match the advantage

4. **Hand Profit / Shoe Profit Track**
   - Running totals: "This hand: +30", "Shoe so far: +147"
   - See where the shoe turned

5. **Session Context (Optional)**
   - "Shoe 3 of 5 today"
   - Session running total entering this shoe

All overlays are visible simultaneously on the right panel. Only appears in replay mode.

---

### Replay Modes (Phased Approach)

**Decision: Prioritize Mode 2, design for Mode 3**

#### Mode 2: Analysis Replay (Priority)
All overlays active. Every decision is visible with context. Deviations from basic strategy flagged. Bets compared to Kelly. This is for study.

This is the first implementation target.

#### Mode 3: "What If" Replay (Future)
At any decision point, user can override their original action and see what would have happened given the same remaining cards in the shoe.

Example: "I hit 16 vs 10 and busted. What if I'd stood? The dealer drew to 18, so I'd have lost anyway."

Supports this because:
- Full card order is recorded (`ShoeRecord.initialDeck`)
- Game logic is deterministic
- Can re-run from any decision point with different choices

This is the ultimate direction but requires more complex state management.

---

### Technical Considerations

#### Data We Already Have
- Full card order: `ShoeRecord.initialDeck`
- Every card dealt: reconstructed via round records
- Every action: `HandOutcome.actions`
- Every bet: `HandOutcome.wager`
- Counts: `RoundRecord.runningCount`, `trueCount`
- Results: `HandOutcome.won`, `payout`

#### Data We Need to Add
- **Insurance decisions** — Track whether insurance was taken and the amount
- **Surrender decisions** — Track which hands were surrendered
- **Shuffle seed** — Store the MT19937 seed used when the shoe was created (enables challenge sharing)
- **Action sequence timing** — Either store timestamps per decision or reconstruct deterministically from shoe order + game logic

These need to be added to `HandOutcome`, `RoundRecord`, or `ShoeRecord` structures.

#### Reconstructing the Flow
From a `ShoeRecord`, we can deterministically reconstruct every decision point because:
1. Initial deck is fixed
2. Rules are fixed
3. Actions are recorded
4. Card draw order is determined by the above

Replay = parse records → rebuild game state step by step → render it.

#### Architecture
- Reuse existing Graphics/HUD system
- Replay doesn't run full game loop; simpler state machine: "load state → render → wait for input → advance"
- Clean separation from live game logic (reads records, doesn't modify them)

---

### UI/Layout

#### Same Window, Different State
Replay reuses the existing game window. The table, cards, and HUD are already built. We:
- Disable live input controls
- Show replay controls
- Populate state from records instead of live game

#### Bottom Panel: Replay Controls
**Decision: Replay controls replace normal gameplay controls along the bottom**

- Play/pause button
- Step backward / step forward buttons
- Skip hand / skip round buttons
- Round selector slider (shows all rounds; click to jump)
- Hovering slider shows round summary: "Round 14: Won 45, TC +4"

#### Right Panel: Analysis Overlay
**Decision: Analysis overlay on right HUD panel, only visible during replay mode**

All five overlays stacked in the right panel. Hidden during normal gameplay.

#### Keyboard Shortcuts
- Arrow keys: step backward/forward
- Space: play/pause
- H: skip hand
- R: skip round
- I: toggle basic strategy highlight (optional)

---

### Emotional and Motivational Design

- **Don't make replay feel like scolding** — Neutral colors, no red "you failed" indicators
- **Frame as insight** — "Deviation from book" not "Wrong play"
- **Celebrate good plays** — Highlight strong decisions too
- **Make it satisfying** — Smooth animations, responsive controls, fast navigation

If replay feels good, people will use it. If it feels like homework, they won't.

---

### Edge Cases

- **Incomplete shoes** — Replay up to last completed round
- **Very long shoes** (100+ rounds) — Need fast navigation via slider/jump
- **Split hands** — Replay each split hand's flow clearly
- **Insurance** — Show that decision point when offered
- **Surrender** — Show surrendered hands with visual indication
- **Missing/corrupt data** — Graceful error: "Can't replay this shoe"

---

### Future Extensions (Post-Phase 1)

- Export replays as video (screen capture or frame-by-frame render)
- Share a shoe (export `ShoeRecord` JSON for friends to replay)
- Aggregate statistics from replays ("You deviated from basic strategy 23% of the time")
- Heat map of count vs. betting (chart overlay)
- "What If" mode implementation

---

## Phase 2: Daily Shuffle Seed Competition

### Core Concept
A competition mode where players face a deterministic shoe seeded from the date. Everyone who plays on the same day faces identical cards. Results are self-reported and verifiable via exported `ShoeRecord` data.

This is extended beyond "daily" to support any seeded run: players can share their replays and challenge others to beat their result on the same seed.

---

### Seed Generation

#### Daily Challenge Seed
**Decision: Use the date as the seed, rely on social trust**

- The MT19937 shuffle seed is derived from the current date (e.g., `YYYYMMDD` as an integer, or a hash of it)
- Everyone playing on the same date gets the same card order
- No external entropy source, no published schedule needed
- Relies on players acting in good faith

#### Per-Shoe Seed Tracking
**Decision: Store the seed in every `ShoeRecord`**

When a shoe is created:
- The shuffle seed is generated and recorded in `ShoeRecord`
- This enables any shoe to become a challenge, not just daily ones
- Players can share their seed/shoe record and friends can replay the exact same deck

This means:
- Daily challenges use date-derived seed
- Any completed shoe can be exported as a challenge ("Can you beat my +147 on seed X?")
- Competition is not limited to one shoe per day

---

### First Playthrough Enforcement

**Decision: Option A + D combined (simple local tracking + verifiable export)**

#### Local Tracking
When a player starts a daily challenge:
1. Game seeds the deck with the date-derived seed
2. Game marks in the save file: `"daily_challenges": { "2025-01-15": { "completed": true, "net_result": 127, "hands_played": 42, "startingBankroll": 1000 } }`
3. If the player tries to play that date again as a challenge:
   - Game shows previous result ("You already played Jan 15. Result: +127")
   - Offers to replay for practice (not counted as a new challenge result)

This is soft enforcement: players can edit their save file, but honest play is the default.

#### Shoe Record Flag
When playing in challenge mode:
- `ShoeRecord` gets a flag: `"challengeDate": "2025-01-15"` or `"challengeSeed": "X"`
- `ShoeRecord` includes the seed used
- Player exports this shoe record as proof of a legitimate run

Anyone can:
1. Load the seed for that date (or use the shared seed)
2. Replay the shoe record
3. Verify the actions match a legitimate game
4. Confirm the result matches the claim

#### Philosophy
We're not building an anti-cheat system. We're building a tool that makes honest play easy and verifiable, and cheating socially awkward. If someone farms a shoe by replaying 20 times and submitting their best run, friends can call them out. Trust is social, not cryptographic.

---

### Normalized Starting Conditions

For fair comparison, challenge mode enforces:

- **Bankroll:** Fixed at 1000
- **Rules:** Default game rules (2 decks, 3:2, H17, late surrender, etc.)
- **Minimum/Maximum bet:** Default (10 / 500)
- **Kelly fraction:** Player-chosen (not locked, not tracked)

Players can adjust their Kelly fraction to their comfort level. It doesn't need to be reported—two players on the same shoe with different Kelly fractions are still competing on the same card order, just with different risk profiles.

---

### Playing Ahead / Future Dates

**Decision: Accept it as harmless**

- Players can change their system clock or select a future date's seed
- But so can everyone else. No advantage gained.
- The spirit of the challenge is "everyone faces the same cards," not "no one can prepare"
- If someone wants to practice on a future shoe, that's fine—they're just getting better at blackjack

If this becomes a concern later, seeds could be revealed monthly instead of all at once. For now: keep it simple.

---

### Result Export and Sharing

#### Export Format
Simple JSON export:
```json
{
  "challengeDate": "2025-01-15",
  "seed": "a1b2c3d4...",
  "startingBankroll": 1000,
  "endingBankroll": 1127,
  "netResult": 127,
  "handsPlayed": 42,
  "shoeCompleted": true,
  "playerName": "Alice"
}
```

Alternatively, just export the full `ShoeRecord` (already contains all this data).

#### Verification
Anyone can:
1. Load the seed for that date
2. Confirm the shoe record matches the expected card order
3. Replay the actions and verify the result
4. See it's a legitimate game

No server needed. Pure peer-to-peer trust.

#### Sharing Methods
- JSON export posted in a Discord group or chat
- Screenshot of the game's summary screen after a challenge run
- Full `ShoeRecord` export for friends to replay and try to beat
- Future: a simple static leaderboard site that aggregates submitted JSON files

---

### In-Game Flow

#### Daily Challenge Mode Entry
From main menu or HUD: "Daily Challenge" button.

Game:
1. Reads today's date
2. Checks save file: have you already played this date's challenge?
   - Yes: show previous result, offer to replay for practice (not counted)
   - No: start challenge
3. Sets `challengeMode = true`, locks starting bankroll to 1000
4. Seeds deck with date-derived seed
5. Plays normally (player can adjust Kelly fraction)
6. On completion (or quit):
   - Saves result to `"daily_challenges"` in profile
   - Offers to export result JSON
   - Shows summary screen with net result and stats

#### Shared Challenges (Any Seed)
Player completes any shoe (challenge or regular):
1. Chooses to "Share this shoe as a challenge"
2. Game exports `ShoeRecord` with seed embedded
3. Friend imports the record, loads the same seed
4. Friend plays against the same card order
5. Friend compares their result to the original

This turns every completed shoe into a potential competition.

#### Replay of a Challenge
Player can replay any challenge:
- Same seed, same cards
- Marked as practice, doesn't overwrite official result
- All replay/analysis features available
- For learning, not for gaming the system

---

### Edge Cases

- **Player quits mid-shoe:** Save partial result. Mark as incomplete. They can't replay for a better result under the same challenge date, but they can continue or restart as practice.
- **Player changes their name:** Name is included in export for attribution but not tied to verification. Anyone can verify the shoe.
- **Player plays multiple dates in one session:** Fine. Each date is independent.
- **Time zones / leap years:** Use local system date. Be forgiving—if someone plays Jan 15 in their timezone, that's their Jan 15.

---

### Relationship to Replay System
The daily challenge and replay systems are tightly coupled:
- Challenge runs produce `ShoeRecord`s that are replayable
- Replay mode lets players study their challenge runs (or their friends')
- Shared challenges are just replays with a competitive frame
- The seed tracking added for challenges benefits replay by making any shoe perfectly reproducible

--- 

## Phase 3: Configurable Rules Engine

### Core Concept
A rules engine that lets players change blackjack rules—number of decks, payout ratios, dealer behavior, action restrictions—and see how those changes affect strategy and edge. Ships with curated presets and a custom mode for full control. This turns Blackjack2 into a blackjack platform rather than a single-rule-set game.

---

### RuleSet Definition (High-Level)

A single cohesive ruleset that controls all game behavior. Every configurable aspect lives here. The game engine reads from this ruleset at runtime instead of hardcoded constants.

Core components:
- **Shoe structure:** number of decks (1–8), cut card depth
- **Payouts:** blackjack payout ratio (3:2, 6:5, etc.)
- **Dealer behavior:** hit/stand on soft 17
- **Player actions:** double down allowed, double after split allowed, split allowed, max split hands, resplit aces allowed, hit split aces allowed
- **Optional features:** surrender allowed, insurance allowed

Fixed rules (not configurable):
- Dealer always peeks for blackjack (standard)
- Double down only on first two cards (not on 3+ cards)

The default ruleset matches the current game exactly—no breaking changes.

---

### Configuration Style: Presets + Custom

**Decision: Hybrid (Option C)**

#### Preset Profiles
Ship with named rule sets reflecting real-world casino conditions:

1. **Standard (Default)** — Current game: 2 decks, 3:2, H17, DAS, late surrender
2. **Single Deck Parlor** — 1 deck, 3:2, S17, NDAS, no surrender, restricted splits
3. **6:5 Floor** — 6 decks, 6:5 payout, H17 (common unfavorable rules)
4. **Counter's Dream** — 6 decks, 3:2, S17, DAS, late surrender, deep penetration

Player selects one with a click. No configuration needed.

#### Custom Mode
A "Custom" profile where players can tweak each rule individually. Changes take effect on the next shoe, not mid-game.

Players can save their custom configurations as named profiles for easy switching later.

---

### Basic Strategy Handling

**Decision: Ship tables for presets; derive custom tables via simulation**

- Each preset profile ships with a precomputed basic strategy table tuned to its rules
- If player selects a custom rule set: no table available immediately; basic strategy advisor is disabled or shows a message
- **Long-term goal:** use the game itself to simulate millions of hands under any rule set and generate a custom basic strategy table. This aligns with the project's philosophy—this is a learning and experimentation tool, so deriving strategy via simulation is a feature, not a workaround
- The simulation would: enumerate all possible player/dealer combinations, play them out with the given rules using the known deck composition, compute expected value for each action, and build an optimal strategy chart
- Generated tables can be saved and loaded alongside rule profiles

This keeps the feature honest (no misleading advice for custom rules) while leveraging the tool for its intended purpose.

---

### Card Counting: Hi-Lo and RE-KO

**Decision: Implement RE-KO alongside Hi-Lo; display both; let player compare**

RE-KO (Running Easy - Knockout) is a popular unbalanced counting system:
- Cards 2–7: +1
- Cards 8–9: 0
- Cards 10–A: -1
- No true count conversion needed (uses running count directly)
- Uses "red cards remaining" concept instead of decks remaining

Implementation approach:
- Track both Hi-Lo and RE-KO counts in parallel during gameplay
- Both counts update on every card dealt
- HUD displays both running counts side by side
- Player can choose which count to base their Kelly betting on
- In replay mode, both counts are shown in the analysis overlay

This lets players:
- Compare how the two systems behave on the same shoe
- See which one they prefer or trust more
- Study the differences in count evolution

---

### Interaction with Challenges

- Daily challenge always uses default ruleset
- Custom rule challenges are allowed for shared challenges (seed + ruleset exported together)
- A challenge is defined by: seed + ruleset. Both must match for fair comparison
- Strategy tables for a challenge depend on its ruleset

---

### Technical Considerations

- All current `Constants` that define rules move into `RuleSet`
- `Game`, `Deck`, `Hand`, and betting logic read from `RuleSet` instead of constants
- `RuleSet` is serializable (JSON) so it can be stored in profiles and exported with challenges
- Old save files and `ShoeRecord`s load with default ruleset for backward compatibility
- New `ShoeRecord`s embed the `RuleSet` used
- Mid-game rule changes not allowed; rules lock when shoe starts

---

## Phase 4: Pause Menu / Settings HUD

### Core Concept
A persistent menu system accessible during gameplay for changing settings, selecting rule sets, browsing history, and customizing cosmetics. Lives behind a button in the top-left corner. Pauses the game while open.

This centralizes all non-play actions into one discoverable interface instead of scattered keybinds or console commands.

---

### Menu Entry

- **Button:** Small icon (hamburger menu or gear) in the top-left corner of the game window
- **Behavior:** Clicking opens the menu and pauses the game. Clicking again or pressing Escape closes it and resumes.
- **Visual:** Semi-transparent overlay panel that dims the table behind it. Doesn't obscure the entire screen—table remains partially visible.

---

### Menu Sections

The menu is organized into tabs or collapsible sections:

#### 1. Rule Sets
- List of available rule set profiles (presets + custom)
- Currently active ruleset highlighted
- Click to switch (takes effect on next shoe)
- "Edit" or "Custom" option to tweak rules manually

#### 2. Counting System
- Choose primary count for betting decisions: Hi-Lo, RE-KO, or both
- Toggle visibility of each count on the HUD

#### 3. History / Replays
- "Review History" button → opens shoe list for replay mode
- Quick access to recent shoes and challenge results

#### 4. Challenges
- "Daily Challenge" button → start or view today's challenge
- "Share Challenge" → export current or past shoe as a challenge
- "Import Challenge" → load a friend's shared shoe record

#### 5. Cosmetics (Future)
- Card style selector (different sprite sheets)
- Table felt selector (different backgrounds)
- Color/theme options (future expansion)

#### 6. Profile
- Player name display/edit
- Bankroll reset option (for fresh starts)
- View lifetime stats

#### 7. Exit
- Return to main menu or quit game

---

### Design Principles

- **Non-intrusive:** Menu is optional; players who prefer keyboard-only play can ignore it
- **Discoverable:** Button is always visible; no hidden commands needed
- **Pause-safe:** Opening the menu freezes the game state; no cards deal while menu is open
- **Consistent:** Same menu available in live play, replay mode, and challenge mode (with mode-specific options shown/hidden as appropriate)
- **Simple:** No nested submenus deeper than one level. Everything reachable in two clicks max.

---

### Relationship to Other Phases

- **Phase 1 (Replay):** History/Replay section provides the entry point to replay mode
- **Phase 2 (Challenges):** Challenges section provides entry to daily and shared challenges
- **Phase 3 (Rules):** Rule Sets section is the primary interface for selecting and editing rules
- **Cosmetics:** Phase 4 defines the slot for cosmetics even if assets aren't ready yet

Phase 4 is the glue that makes all other features accessible and discoverable to the player.

---

### Technical Considerations

- Menu is a new SFML UI layer (buttons, panels, text labels)
- Reuses existing `Panel`, `StatBox`, and `FontManager` infrastructure
- Menu state is simple: open/closed; current active tab
- Settings changes persist immediately to player profile or config file
- Menu rendering happens only when game is paused (no double work)

---

## Notes

- All phases build on existing architecture without breaking changes
- Replay system (Phase 1) is foundational for challenge/sharing features (Phase 2)
- Seed tracking in `ShoeRecord` should be added during Phase 1 to support Phase 2 seamlessly
- RuleSet and RE-KO additions in Phase 1's "Data We Need to Add" section support Phase 3
- Pause Menu (Phase 4) serves as the central hub for all features across all phases