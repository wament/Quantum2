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
