# IDEAS — Blackjack2 (Phase 0.4 backlog, **not committed**)

**Status: propositions only.** Nothing here is scheduled, estimated as commitment, or part of the 0.4 defect plan
in `NEXT_STEPS.md`. This is a menu. Read `CONTEXT.md` for the state of the code these would build on.

**Effort scale:** S ≈ under a day · M ≈ 1–2 days · L ≈ 3–5 days · XL ≈ a week or more.
All estimates assume the 0.4 defect pass has landed and the suite is green.

---

## The one-paragraph version

Blackjack2 is unusually well positioned for a certain kind of feature: it already records **everything** (full
deck order per shoe, per-round count and edge, every strategy deviation) and it already has a **headless engine**.
Most of the ideas below are therefore not new engines — they are new *questions the player can ask* of data the
app is already writing down. The cheapest high-value ones (1, 3, 4, 6) are mostly presentation plus one or two
pure functions. The expensive ones (2, 5) point the headless engine at a question, which is what it is already
built for.

| # | Feature | Effort | New engine? | Depends on |
|---|---|---|---|---|
| 1 | Decision Fork — "what would have happened" | L | No — reuses `RoundEngine` | B13, B16 |
| 2 | The Simulation Lab (Monte Carlo) | L | Thin wrapper on `Game(true)` | B8, B9, T1 |
| 3 | Leak Report — EV-priced mistakes | M | No — one EV table | B3 pattern |
| 4 | Shoe Codes + Daily Challenge | M | No — needs seed injection | B14, B15 |
| 5 | Bet Spread Designer | L | No — needs #2 | #2 |
| 6 | Bankroll Chart tab | **S** | No — data already computed | B13 |
| 7 | Achievements → Challenges | M | No — generalises a table | B2 |
| 8 | Count-Only Drill Mode | **S** | No — a HUD filter | B4 |
| 9 | Ghost Rival | M | No — replays a recorded shoe | #4 |
| 10 | Onboarding Campaign + name entry | L | No — a scripted mode | U7 |

Runners-up are at the end, along with what was deliberately left out and why.

### Two cross-cutting notes that apply to almost everything below

- **Any new persisted field must land after B15.** There are currently two live copies of `Serialization.hpp`
  (`src/` and `include/`), and the game and the test suite compile different ones. Adding a field to one copy
  means the shipped binary and its own tests disagree about the save format.
- **Anything that draws a new chart or a new string should use the B3 pattern** — extract the string or the
  layout into a pure helper and test the helper. Every feature here that skipped that step becomes untestable
  again, which is the root cause of the whole 0.4 defect list.

---
## 1. Decision Fork — "what would have happened"

**Effort L · flagship teaching feature · highest value-per-line in this document**

### Pitch
Pause a Replay at any decision point, choose the action you *didn't* take, and watch the hand play out from that
branch. Two lines, one divergence point. "You hit 16 vs a 10. Standing pushes. Here it is."

### Why this app, why now
The app already writes the entire shoe order to disk: `ShoeRecord::initialDeck` is a verbatim copy of
`deck.getCards()` captured at shoe start (`src/Game.cpp:276`, `:1349`; `src/SessionLifecycle.cpp:55`, `:198`) and
it is already serialised (`Serialization.hpp:170`). `Deck::setCards()` exists specifically to install an exact
card order. `RoundEngine` is already a tick-driven state machine that a headless `Game` can run without a window.
Every part of a fork — restore a deck state, apply a different action, run the engine, compare — already exists as
a separate part. Nobody has screwed them together yet.

This is what turns the app from *a game you can review* into *a game you can learn from*. The advisor and the
trainer can only ever tell you what was right in the abstract; a fork shows you the consequence.

### The experience
1. Finish a shoe, open it from the history list as today.
2. Step through with the existing slider. At any point where **you** had a decision, a small **⑂** marker appears
   on the timeline.
3. Press **F**. The replay freezes; the legal actions for that moment light up, with the one you took marked, and
   the advisor's recommendation marked too.
4. Pick a different one. Your hand resolves, the dealer plays out, the round settles. The original line stays
   visible as a ghost; the fork line is drawn in a contrasting colour.
5. A one-line verdict under the table: **stood → push (+$0) · hit → bust (−$50)**.
6. Optionally "run the whole shoe from here" to watch the divergence compound — with the honesty caveat printed
   on screen (see gotchas).

### Design detail
- **Fork scope:** *this round* (default — exact and defensible) or *rest of shoe* (dramatic, drifts). Ship
  round-only first; the shoe-length mode is v2.
- **Forks are never authoritative.** They never touch `PlayerStats`, never write `longHistory`, never unlock an
  achievement, never move the bankroll. Enforce this structurally — a fork runs on a throwaway headless `Game`,
  never on the live one — and lock it down with a test, because it is the one rule that, if broken, silently
  corrupts saved progress.
- **Fork budget:** unlimited per view, but cap stored concurrent forks per shoe (say 8) so replay data can't grow
  without bound.
- **Legal fork points:** player decisions only — hit / stand / double / split / surrender / insurance. Betting is a
  different species of what-if and belongs to #5.

### Implementation sketch
- **New** `include/DecisionFork.hpp` / `src/DecisionFork.cpp` — owns a headless `Game`, a restored `Deck`, and the
  resulting `RoundRecord`. Pure logic; no SFML beyond types.
- **New pure helper** `deckStateAtDeal(shoe, cardsDealtSoFar)` — the shoe vector with the first *n* dealt cards
  removed **from the back**. Do **not** reuse `reconstructDeckAtRound()`: it is dead code that pops the front and
  is therefore wrong (B16). Write the correct one and delete or fix the old one in the same commit.
- **Changed** `Replay` / `ReplayView` — fork markers on the step list, an action picker, a two-line comparison
  strip. `AppMode` gains `ReplayFork` with its own tick/draw/input path, per the rule that a mode owns its state
  machine.
- **Data** forks live in memory only, keyed by `(shoeIndex, roundIndex, handIndex)`. If you later want to pin one
  to the profile, add an optional `forks` array to `ShoeRecord` — additive, default empty.

### Headless test surface
Unusually easy to test, which is a good sign for the feature:
- Restore a recorded shoe to just before a known decision, apply each legal action, assert the resulting
  `RoundRecord` matches a hand-written expectation — the deck order is fixed, so outcomes are exact.
- Assert a fork leaves the saved profile **byte-identical**. That is the "never authoritative" rule as a test.
- Assert the fork-marker set for a recorded shoe equals exactly the set of rounds where the human had a choice.

### Edge cases & gotchas
- **The cut card.** `Deck::draw()` silently consumes an extra card when it hits the cut card
  (`src/Deck.cpp:126-132`). A reconstruction that doesn't replay that rule lands one card out and produces a
  confidently wrong answer. This is the bug that will bite hardest, because nothing looks wrong on screen.
- **Mid-shoe reshuffle.** After `reshuffleWithRemaining()` the remaining cards are re-ordered, so a fork past that
  point is not derivable from `initialDeck` alone. Detect it (`ShoeRecord::isContinuation`) and **refuse to fork
  past a reshuffle** rather than inventing an answer.
- **Card drift on long forks.** A different action consumes a different number of cards, so from the next round on
  everyone gets different cards. That is realistic, not a bug — but it must be stated on screen or players will
  read the second line as fate.
- **Surrender and insurance** change the card count in ways the reconstruction must model explicitly.

### Dependencies
B13 (forks must not corrupt the history they read from) and B16 (the reconstruction helper). Nothing else — no
new persistence, no new RNG.

### Open questions
- Should forks earn achievements ("found the winning line 10 times")? Tempting, and it would cheapen them.
  Probably no — but forks earning *trainer* credit is more defensible.
- A fork has to reveal the dealer's hole card to be legible, which makes it a cheating lens. Fine in a study tool;
  worth saying out loud in the UI so it never leaks into a challenge mode (#4) or a race (#9).

### Where it grows
Fork-on-bet (compare bet sizes on the same shoe) merges into #5. Fork-and-share — export a fork as a puzzle,
"what's the right play here?" — merges into #4's challenge framework and is a genuinely good social hook that
needs no networking at all.

---

## 2. The Simulation Lab (Monte Carlo)

**Effort L · the strategic bet · Feature 11 aimed at a question instead of at auto-play**

### Pitch
"Run 50,000 hands with *these* rules and *this* bet spread — what actually happens?" Convergence plot, realised
house edge, risk of ruin, expected time to double, and the variance band you should expect to live inside.

### Why this app, why now
`Game(bool headless)` is already a display-free engine and the whole test suite is built on driving it. Feature
11's per-instance concurrency spec is already written in the 0.3 notes. The Simulation Lab is that machinery
pointed at the one question the entire app is really about: **does counting plus Kelly actually work, and what
does it feel like over 10,000 hands when you can't sit through 10,000 hands?**

It is also the only feature here that answers a question the player currently *cannot* answer at all. Everything
else in the app is about the past; this is the only one about the future.

### The experience
1. Main menu → **Simulation Lab**.
2. Configure: rules (reuse the existing rules panel verbatim), counting style, bet policy (flat / Kelly at
   fraction *f* / Reko ramp / your own spread from #5), bankroll, stop condition (hands, shoes, or ruin).
3. **Run.** A progress bar and a live convergence plot; the app stays responsive because it runs on a worker
   thread.
4. Results: realised edge vs theoretical, win rate, max drawdown distribution, risk of ruin over N shoes, a
   bankroll-percentile fan chart, and — the persuasive one — **the same run with flat betting**, side by side.

### Design detail
- **Policies, not scripts.** A `BetPolicy` interface with `betFor(trueCount, bankroll, min, max)`; ship
  `Flat`, `Kelly(fraction)`, `RekoRamp`, `BasicStrategyNoCount`. These already exist as scattered logic in
  `BettingHandler` / `BasicStrategy::calculateKellyBet` / `calculateRekoBet` — the work is extracting them behind
  one seam, not writing them.
- **Strategy policy** separately: `BasicOnly` vs `CountAwareDeviations`, so you can measure the cost of playing
  basic strategy while betting like a counter.
- **Determinism is mandatory.** Every run takes a seed and prints it, so a result can be re-run or argued about.
  This requires seed injection into `Deck` — see #4, which needs it anyway.
- **Never touches disk.** No profile writes, no achievements, no history. This must be structural, not a
  conditional at each call site — see T1.

### Implementation sketch
- **New** `include/Simulator.hpp` / `src/Simulator.cpp` — owns N headless `Game`s, a policy, and an aggregate
  result struct. No SFML types at all, so it links into the test binary trivially.
- **New** `SimResult` — hands, shoes, net, edge realised, peak, max drawdown, ruin count, percentile array, and
  the convergence series sampled every *k* hands.
- **Threading** start simple: one worker thread, N games sequentially, chunked so the UI can poll progress. The
  0.3 per-instance concurrency spec describes the parallel version; do not start there. Sequential is already
  fast enough for 50k hands and removes every shared-state question.
- **Changed** `StatsDashboard` gains a fourth `Tab::Simulation` (the `Tab` enum is already `Overview/Hands/Profits`).
- **Data** results are ephemeral by default. "Save this run" would be an optional `simulations` array on the
  profile — additive, and honestly probably unnecessary.

### Headless test surface
The best in this document — the feature *is* headless:
- A fixed seed reproduces a `SimResult` exactly, bit for bit.
- Flat betting over a very large run converges to the theoretical edge within a stated tolerance (a statistical
  assertion with a generous band; keep the seed fixed so it is deterministic, not flaky).
- Ruin detection, stop conditions, and policy boundaries (never below `MINIMUM_BET`, never above `MAXIMUM_BET`,
  never above bankroll — which is exactly the B5 question, so this feature will surface it if B5 is still open).

### Edge cases & gotchas
- **Per-round disk writes will destroy this.** `HandEvaluator::evaluateHands()` ends with `saveProfile()`
  (`src/HandEvaluator.cpp:297`). A 50,000-hand run would write the profile 50,000 times. T1 is a hard
  prerequisite, not a nice-to-have.
- **Achievements and stats must be inert** in sim mode, or a run silently rewrites career history. Same structural
  gate as B1's tournament fix — and it argues for one shared "this game does not persist" flag rather than two.
- **`Deck::draw()` has no empty-shoe guard** (B8). A long headless soak is precisely how you fall off the end of a
  shoe. Fix B8 first or the Lab will crash in ways that look like the Lab's fault.
- **Speed vs realism.** A sim that never pauses plays faster than a human. Report hands *and* an estimated
  real-time equivalent (hands/hour × shoe count) or the numbers mislead about how long this takes to live through.

### Dependencies
B8 (empty-shoe guard), B9 (you want logs when a long run dies), T1 (write amplification), and #4's seed injection.

### Open questions
- Is a sim a mode (`AppMode::Simulation`) or a panel inside the dashboard? A mode is cleaner and matches the
  project's rule; a panel is cheaper. Leaning mode.
- Should the Lab be reachable from the main menu at all, or tucked behind the dashboard as "advanced"? It is the
  most powerful thing in the app and the least game-like.

### Where it grows
#5 is the direct child. Beyond that: a "replay your own career under flat betting" counterfactual — the app has
your entire history, so it can tell you what flat betting would have done to *your* bankroll. That is a
three-hour feature once the Lab exists, and it is the most persuasive screen you could possibly show a player.

---

## 3. Leak Report — pricing your mistakes in dollars

**Effort M · best value-to-effort on this list after #6**

### Pitch
The app already knows every place you deviated from basic strategy. It does not know what that *cost* you. Price
each one against an action-EV table and hand back a ranked list: "you lost an expected **$12.40** this shoe, and
**$8.10** of it was standing on 16 vs a 10."

### Why this app, why now
`DeviationRecord` already captures `playerValue`, `dealerUpValue`, `wasSoft`, `wasPair`, `pairValue`,
`actionTaken`, `correctAction`, `bet` and `won`, and `PlayerStats.recentDeviations` keeps the last ten. The
advisor already computes `correctAction` via `BasicStrategy::getRecommendedAction(rules, ...)`. Every input to a
leak report is already in memory. The only thing missing is a table of expected values — and that table has the
same shape as the strategy tables the project already maintains.

This is also the feature that makes the existing deviation tracking *legible*. Right now "3 deviations" is a
number with no consequences; "you donated $8" is a reason to care.

### The experience
- A **Leaks** section on the shoe-end summary and a per-shoe card in the dashboard.
- Ranked list, biggest leak first: the situation rendered as actual cards, your action, the right action, the EV
  delta in dollars at the bet you actually made.
- A career rollup: "your top 3 leaks this career", which is the single most actionable screen in the app.
- Optional: a **Drill these** button that feeds exactly those situations into the Strategy Trainer.

### Design detail
- **EV table shape:** `expectedValue(total, dealerUp, isSoft, isPair, pairValue, action, rules)` → float, from a
  `constexpr` table of per-action EVs indexed like the existing strategy tables. Values are per-unit-staked, so
  the dollar figure is `evDelta × bet`.
- **Honesty labels.** These are infinite-deck EVs; with removal effects and a non-neutral count they are
  approximations. Print "approximate, count ignored" once, in the panel, not in a tooltip nobody reads.
- **Only price decisions you could actually make.** Insurance and surrender have rule-dependent EVs; if
  `gameRules_.surrenderType == Off`, don't show a surrender delta.
- **The Drill-these loop** is the whole point: `StrategyTrainer::startSession(deckCount, secondsPerSession)`
  exists, so add a mode that draws questions from `recentDeviations` instead of uniformly.

### Implementation sketch
- **New** `include/ActionEV.hpp` / `src/ActionEV.cpp` — the table plus `float actionEV(const HandSituation&, char action, const GameRules&)`
  and `float leakDollars(const DeviationRecord&, const GameRules&)`. Pure, header-light, no SFML.
- **New** `leakReportFor(const std::vector<DeviationRecord>&, const GameRules&)` → ranked `std::vector<Leak>`,
  sorted by dollars, with a stable tie-break so the UI doesn't jitter.
- **Changed** the shoe-end summary and `StatsDashboard` (a `Tab::Leaks`, or fold into `Hands`).
- **Data** nothing new persisted — `recentDeviations` is already stored. If you want a career leak ledger, derive
  it from `longHistory` at read time rather than adding a field, consistent with the "record is the source of
  truth" design in CONTEXT §5.

### Headless test surface
- Every cell of the EV table against a published reference set — this is a data table, so test it exhaustively,
  not by sampling.
- A hand-built `DeviationRecord` prices to an exact dollar figure.
- A deviation where you got *lucky* still shows a negative EV. This is the pedagogically important case and the
  one most likely to be "fixed" wrongly later, so pin it with a named test.
- `surrenderType == Off` produces no surrender delta.

### Edge cases & gotchas
- **`recentDeviations` only holds the last ten.** A career leak ledger cannot be built from it. Either raise the
  cap (cheap, but it grows the save file — see T2) or derive from `longHistory`. Decide before writing code.
- **Deviations are recorded per *play*, not per *hand*.** A split hand can contain several. Summing naively
  double-counts; group by hand first.
- **Doubling changes the stake.** The EV delta must be priced at the total wagered, not the original bet.
- **Don't show a leak for a decision the advisor itself would take twice.** Where two actions are EV-equivalent
  (soft 18 vs 2 is the classic), the delta is ~0 and showing it as a "leak" teaches the wrong lesson. Suppress
  below a threshold.

### Dependencies
None hard. Use the B3 pattern (extract the string, test the string) so the formatting is covered too.

### Open questions
- Do leaks count against achievements, or is that punishing honesty?
- Should the number be shown as dollars or as a percentage of the wager? Dollars are visceral; percentages
  compare across bet sizes. Probably dollars with the percentage in parentheses.

### Where it grows
Feed #10's onboarding ("here's the one mistake most players make"). Feed #9's ghost: a ghost that plays perfect
basic strategy *is* a walking leak report. And with #2 available, leaks can be priced exactly rather than from a
table — run both lines 10,000 times and report the measured difference.

---

## 4. Shoe Codes + Daily Challenge

**Effort M · cheapest thing here with the longest tail · the real foundation for Feature 10**

### Pitch
Every shoe becomes addressable. "Code `7QK4-2F`" replays exactly that shoe order — yours or someone else's. Add
one date-seeded shoe a day and a local streak, and you have a reason to come back tomorrow and something to
compare with a friend, with no networking whatsoever.

### Why this app, why now
`ShoeRecord::shuffleSeed` already exists and is already persisted (`GameRecord.hpp:166`,
`Serialization.hpp:174/185`) — but it is write-only today, and `Deck::shuffle()` reseeds from
`std::random_device` on every call (`src/Deck.cpp:38-41`, and again at `:94-97` for the continuation reshuffle),
so the recorded seed can be *read back* but never *replayed*. That gap — a seed that is stored but not usable —
is the whole feature.

Two ways to close it, and the second is better than the first:
- **(a) `Deck::shuffleWithSeed(uint64_t)`** — replace the `random_device` reseed with an injectable engine. Small,
  but note `std::shuffle`'s output depends on the standard library implementation, so a code is only portable
  across builds of the same libstdc++. Fine locally, fragile if codes ever leave the machine.
- **(b) Encode the deck order itself.** `initialDeck` is already recorded. Compress it (52·*n* card indices pack
  into a few hundred bytes, base32 to a shareable string) and you get a code that is portable forever and immune
  to RNG changes. **Do (b) for shareable codes, (a) for the Daily**, which doesn't need portability.

### The experience
- Shoe end: the code is printed on the shoe summary; **C** copies it.
- Main menu → **Enter code** → play that exact shoe from scratch. Your result vs the original result, side by
  side at the end.
- **Daily** on the main menu: one fixed shoe, one attempt per day, a streak counter, and a local list of your
  last 30 results. No server, no leaderboard, no accounts — just a date-seeded shoe and your own history.

### Design detail
- **Daily seed = hash(date string + a fixed salt)**, so the Daily is the same for every install without anyone
  coordinating. Store `lastDailyDate` and `dailyStreak` on the profile (additive, defaults 0/empty).
- **One attempt per day** is the honest rule, but offer a clearly-marked **Practice** replay of yesterday's Daily
  so losing doesn't feel punitive.
- **Daily rules are fixed** (one rule set for everyone) — otherwise the comparison is meaningless. Reuse
  `gameRuleRows()` data to render the fixed set read-only.
- **Codes are read-only imports**: an imported shoe never merges into your career stats. It produces a
  comparison, not progress. Same rule as forks in #1.

### Implementation sketch
- **Changed** `Deck` — add `void shuffleWithSeed(unsigned long long seed)` and make `shuffle()` a thin wrapper
  that draws from `random_device` and calls it. One reseed path, not two (today there are two copies of the
  reseed logic, `src/Deck.cpp:38-41` and `:94-97`).
- **New** `include/ShoeCode.hpp` / `src/ShoeCode.cpp` — `encode(const ShoeRecord&)` / `decode(std::string_view)`
  with a checksum. Pure, trivially testable, no SFML.
- **New** `DailyChallenge` — date → seed, and the streak bookkeeping. Pure.
- **Changed** `SessionLifecycle` — a `startImportedShoe(const ShoeRecord&)` entry point alongside the existing
  `startNewGame` / `startTournamentSession` / `startTrainerSession` / `startStatsSession`.
- **Data** profile gains `lastDailyDate`, `dailyStreak`, `dailyBest` — all optional with defaults, per the
  additive-schema contract.

### Headless test surface
- `encode` → `decode` round-trips a shoe to the identical card order, including a 1-deck and an 8-deck shoe.
- A malformed or truncated code is **rejected**, not silently accepted as a different shoe. The checksum earns
  its keep here; test it deliberately.
- The same date always yields the same seed, and the streak advances / resets correctly across a simulated
  date boundary (including the "played yesterday, skipped a day" case).
- An imported shoe leaves career stats unchanged.

### Edge cases & gotchas
- **Deck size changes the code length.** A 1-deck code and an 8-deck code are different sizes; encode the deck
  count in the header so a code can't be misread as another shoe size.
- **The cut card position** is part of the shoe order and must round-trip, or the imported shoe ends at a
  different point than the original and the comparison is a lie.
- **`reshuffleWithRemaining()` breaks continuity** — a shoe that continued mid-deck isn't one order. Either
  encode the continuation too or refuse to generate a code for `isContinuation` shoes. Refusing is fine for v1.
- **Time zones.** "Today's" Daily depends on the local clock; a player crossing a midnight boundary mid-shoe
  should finish the shoe they started. Store the date the shoe *started*.

### Dependencies
B14/B15 (you are adding profile fields and touching persistence), and it should land before #2 and #9, both of
which want seed injection.

### Open questions
- Is a Daily worth it with no leaderboard? Arguably yes — a streak is its own hook — but it is the weakest
  justification here. The **code** half is unambiguously valuable; the **Daily** half is a separate decision.
  Split them if you want the foundation without the retention mechanic.

### Where it grows
This is the cheapest possible on-ramp to Feature 10 (Multiplayer): two players, same code, compare results, no
networking. It also gives #9 its shared shoe and #1 its shareable puzzle.

---

## 5. Bet Spread Designer

**Effort L · the capstone of everything the project is about · requires #2**

### Pitch
Draw your own betting ramp — true count in, wager out — then race it against flat betting and against Kelly over
50,000 hands. The app's entire thesis is that counting and bet sizing are the game; this is the screen where a
player can *test the thesis themselves*.

### Why this app, why now
`BasicStrategy::calculateRekoBet(trueCount, baseBet, minBet, maxBet)` and
`calculateKellyBet(trueCount, kellyFraction, bankroll, minBet, maxBet)` are two hand-designed spreads that already
ship, and `estimateEdgePercent(trueCount)` plus `Constants::EDGE_PERCENT_PER_TRUE_COUNT` already convert count to
edge. The app has opinions about bet sizing; it has never let a player disagree with them. Once #2 exists, the
Designer is just a third `BetPolicy` plus an editor.

### The experience
1. Dashboard → **Spreads** → New.
2. A graph: true count on x (−4 … +8), bet on y. Click to add a point, drag to move it, right-click to delete.
   Presets load the existing Reko and Kelly curves so you can start from something sane.
3. Live readouts while you edit: average bet, max bet, and — the one that teaches — **how many hands you'd be
   playing at max bet**.
4. **Run it** → #2 runs your spread, flat, and Kelly on identical seeds. Three convergence lines, three sets of
   numbers, ruin risk for each.
5. Save named spreads to the profile; pick one as your live betting policy in a normal game.

### Design detail
- **Monotonic by construction.** Force the curve non-decreasing — a spread that bets *less* on a richer count is
  almost certainly a mistake, and letting players draw one makes the results confusing rather than interesting.
- **Clamp to the table.** Every point is bounded by `MINIMUM_BET` / `MAXIMUM_BET` and, once B5 lands, by
  affordability.
- **The honest warning.** A spread tuned on a simulation is overfitted to that simulation. Print the out-of-sample
  result too: run the tuning on one seed range and report performance on a different one. This is the difference
  between a teaching tool and a slot machine with a graph.
- **Using your own spread in a real game** is the feature that makes it matter, and it needs the B5 bet model
  settled first, because "affordability" is exactly what B5 leaves ambiguous.

### Implementation sketch
- **New** `struct BetSpread { std::vector<std::pair<int,int>> points; int betFor(int trueCount) const; }` with
  piecewise-linear interpolation, plus `flattenToTable()` producing the same `min..max` lookup the existing Reko
  path uses — so a spread drops into `BettingHandler` with no special-casing.
- **New** `CustomSpreadPolicy : BetPolicy` for #2.
- **Changed** `BettingHandler` to take a `BetPolicy` instead of branching on `CountingStyle` inline. That refactor
  is what makes #2, #5 and any future style cheap, and it is most of the real work here.
- **Data** `betSpreads` as an optional profile array — additive.
- **UI** the graph editor is the expensive part: hit-testing, dragging, and it must reflow. Do not repeat U3 on a
  brand-new screen.

### Headless test surface
- `betFor` at exact points, between points, below the first point, above the last.
- Monotonicity enforced by the editor *and* by a validator, so a hand-edited profile can't smuggle in a
  decreasing curve.
- A spread equal to the Reko preset reproduces `calculateRekoBet` exactly — the regression test proving the
  refactor didn't change existing behaviour.

### Edge cases & gotchas
- **The count you edit against must match the count you play.** Reko's true count is not Hi-Lo's
  (`rekoStartingOffset`; and `Game.cpp:483` surfaces the running count as the true count under Reko). A spread
  designed on one scale and played on the other is silently wrong. Label the axis with the counting style and
  re-label when it changes. Depends on B4 — until that is fixed the scale itself is off by up to 19.
- **Negative true counts** need a defined bet (the table minimum), not an interpolated value below it.
- **Bankroll-dependent spreads** (Kelly) can't be expressed as pure TC→bet. Keep Kelly a separate policy rather
  than bending the spread model to accommodate it.

### Dependencies
#2 (hard), B4 (count scale), B5 (affordability), U3 (layout).

### Open questions
- Does a custom spread disable the advisor's bet hint, or does the advisor start quoting *your* spread?
- Is "optimal" measured in dollars or dollars-per-unit-risk? Kelly exists precisely because those disagree; the
  results screen has to pick one and say which.

### Where it grows
The natural home for insurance as a toggleable policy and for count-playing deviations in #2. Together that is a
full "build your own player" lab — quietly the most interesting version of this application.

---
## 6. Bankroll Chart tab

**Effort S · do this first if you want a win between bug fixes · near-zero risk**

### Pitch
The app already computes a per-round bankroll curve for the current shoe and already stores a bankroll at every
shoe end. Nothing draws them. A progression line with drawdown shading is a pure presentation win.

### Why this app, why now
`HUDState::bankrollSeries` is "your cumulative net profit per round this shoe" and `edgeSeries` samples the edge
once per round — both already populated, both under-used. `ShoeRecord::bankrollAtShoeEnd` gives the career points,
and `StatsDashboard::ComputedStats` already computes `peakBankroll`, `maxDrawdown` and `greatestHandProfit`.
There is no new logic here at all. This is the rarest kind of proposal: the data is already correct, it just
isn't on screen.

### The experience
- A fourth dashboard tab, **Progress**.
- Top chart: career bankroll across shoes, with the in-progress shoe appended live. Drawdown shaded below the
  running peak.
- Bottom chart: this shoe's per-round curve from `bankrollSeries`, with `edgeSeries` overlaid faintly — so you
  can see the moment the count turned and what you did about it.
- Hover a point → that round's cards; click → jump into that round's Replay.

### Design detail
- **Downsample long careers.** With T2 unresolved `longHistory` grows forever. Bucket to a fixed pixel budget
  using max-per-bucket so spikes survive, rather than plotting raw points.
- **Two different x-axes** (shoes vs rounds) — label them, or the two charts read as one continuous story.
- **Drawdown measured from the running peak**, matching `ComputedStats::maxDrawdown`, or the chart and the number
  disagree on screen and one of them is visibly wrong.

### Implementation sketch
- **New** `include/ChartPrimitives.hpp` / `src/ChartPrimitives.cpp` — `axisFrame`, `polyline`, `shadedRegion`,
  `bucketMax`. Pure geometry returning `sf::Vertex` vectors, so the maths is testable without a window.
- **Changed** `StatsDashboard` — add `Tab::Progress` and `drawProgressTab()`.
- **Data** none. Read `bankrollSeries`, `edgeSeries`, `longHistory`.

### Headless test surface
- `bucketMax` preserves the true max and min for arbitrary input, including empty and single-point.
- Shaded-region depth equals `peak − value` at every sample.
- A career series built from a synthetic `longHistory` matches hand-computed values.

### Edge cases & gotchas
- **`bankrollAtShoeEnd` defaults to 0 for pre-existing profiles** — the loader falls back to 0 meaning *unknown*,
  not *broke*. Treat 0 as a gap and break the line, or every old profile renders as a crash to zero followed by a
  recovery. This is the one real trap in an otherwise trivial feature.
- **`bankrollSeries` resets at every shoe boundary** — it is not a career curve. Concatenating it with the career
  series without an offset produces a sawtooth.
- **A one-point chart** must not divide by zero when scaling.

### Dependencies
B13 — the history this reads is currently lossy for the in-progress shoe. Fix that first or the live tail of the
chart will occasionally step backwards, which looks like a chart bug and isn't.

### Open questions
Log or linear y-axis? Drawdown reads better on log; linear shows "you lost half your roll" honestly.

### Where it grows
Every later feature here wants to draw a line — #2's convergence, #5's three-way race, #9's race. Build the
primitives once, here, and four features inherit them for free. Best first task on the list.

---
## 7. Achievements → Challenges framework

**Effort M · generalises a table that is already half-generalised · lands naturally on the B2 fix**

### Pitch
Twelve fixed achievements become a data-driven challenge system with progress bars, hidden entries, and per-shoe
modifiers that change the rules of the shoe you're playing. "This shoe: surrender is off, and you may only split
aces."

### Why this app, why now
`AchievementManager` is already almost a framework: a static table built once in the constructor, a `HandEvent`
struct fed per hand (`isBlackjack`, `followedStrategy`, `busted`, `splitHand`, `wonAny`, `kellyFull`,
`positiveTrueCount`), an `onNewShoe()` hook, per-shoe accumulators, and `resetPerShoe()`. That *is* a challenge
evaluator — it just only answers yes/no, and only ever once.

Adding progress ("4 of 5 pairs split") needs one field. Adding a modifier needs rules the game already models in
`GameRules`. And a toast with a real lifetime is *required* the moment two challenges can complete together, so
this feature and the B2 fix are the same conversation rather than a collision.

### The experience
- The Achievements screen becomes **Challenges**, in three groups: Career (always on), Shoe (one active at a
  time), and Daily (from #4).
- Each row shows a progress bar and, where relevant, the reward. Locked-but-visible vs hidden (`???`), so some
  things stay a surprise.
- At shoe start a banner names the active Shoe challenge; at shoe end the summary says whether you made it and by
  how much you missed.
- Modifiers are **opt-in** and tagged on the shoe summary, so a player can always tell whether a recorded result
  was achieved under modified rules.

### Design detail
- **Challenge = predicate + progress + optional modifier.** Express the predicate over the existing `HandEvent`
  and a per-shoe accumulator, so nothing new is plumbed into the round path.
- **Derive progress where you can.** Store only the counter you genuinely can't recompute — same instinct as the
  profile's "record is the source of truth".
- **Modifiers reuse `GameRules`** rather than inventing parallel config: double only on 9–11, no surrender, dealer
  hits soft 17, forced flat betting, one hand only. All already expressible as existing fields.
- **Never re-order the existing 12.** Their ids are persisted through `Achievement::toInt/fromInt`; new ids
  append. Renumbering silently rewrites everyone's unlocks — a data bug with no visible symptom.
- **A challenge must never be un-completable by accident.** If a modifier disables surrender, a challenge that
  requires surrender must not be selectable alongside it. Validate the pair, or generate challenges from the
  active rule set.

### Implementation sketch
- **New** `struct Challenge { int id; std::string name, description; ProgressKind kind; int target; bool hidden;
  std::optional<GameRules> modifier; }` and a `ChallengeDef` table replacing the hard-coded vector.
- **Changed** `AchievementManager` → evaluate a list of challenges against `HandEvent` + accumulators, returning
  `{newlyCompleted, progressUpdates}` instead of just newly-unlocked ids.
- **Changed** `AchievementFeeder` — it already owns the game→manager seam and the toast; it now feeds a queue,
  which is exactly what B2's timer needs.
- **Data** `unlockedAchievementIds` stays as-is; add an optional `challengeProgress` map for in-progress counters.
  Additive, and remember B15 before touching the schema.

### Headless test surface
- A synthetic `HandEvent` stream drives a challenge from 0 → target and completes it exactly once.
- Progress survives a save/reload round-trip mid-challenge.
- A legacy profile with only `unlockedAchievementIds` loads and shows all 12 in the right state.
- Modifier + challenge pairs are validated: no un-completable combination is constructible.

### Edge cases & gotchas
- **Toast queueing.** Two completions in one hand is the B2 bug's natural successor — design the queue and the
  timer together, in one change, or you will fix B2 and immediately reintroduce it as a different symptom.
- **Modifiers change the house edge**, so a modified shoe's result must not be comparable to a normal one in any
  chart or streak. Tag it in the record.
- **Shoe-scoped challenges and mid-shoe reshuffles** (`isContinuation`) — decide whether a continuation resets the
  per-shoe accumulators. Today `resetPerShoe()` is called from `onNewShoe()`; make sure a continuation is treated
  consistently with that, not redefined here.

### Dependencies
B2 (hard — same code), B15 (schema), and #4 if Daily challenges are wanted.

### Open questions
- Do challenges pay out chips? Paying out means the economy matters, which drags in U8's bankroll question.
  Recommend cosmetic-only for v1.
- How many Shoe challenges at once — one, or a choice of three? A choice is more fun and much more UI.

### Where it grows
Daily challenges from #4, modifier presets as a "gauntlet" mode, and the natural home for a tutorial's objectives
under #10.

---
## 8. Count-Only Drill Mode

**Effort S · the smallest real feature here · a HUD filter, not a mode rewrite**

### Pitch
Hide the bankroll, hide the totals, hide the strategy hint. Show only the running count and the true count. Bet
the count and get graded on how closely your wager tracked the edge. It is the one skill a blackjack student
actually has to build, and nothing in the app isolates it today.

### Why this app, why now
`HUDState` already carries every displayed number as a separate field — `bankroll`, `currentBet`, `runningCount`,
`trueCount`, `edgePercent`, `basicStrategyHint`, `cardsRemaining`, `countingStyleName`. A drill mode is therefore
a *mask* over `HUDState`, not a new render path: build the state with a `visibleFields` bitmask and let the
existing renderer draw what it's told. That's a day, not a week.

It also exercises B4 directly — a drill built on a seed that ignores deck count teaches the wrong number — so this
feature doubles as a regression harness for that fix.

### The experience
1. Main menu → **Drill** → choose Running count / True count / Bet-to-edge.
2. Cards deal dimmed; the count box is the only bright thing on screen.
3. You bet. On confirm, the screen shows the ideal bet for that count beside yours, with a signed error.
4. End of shoe: a scatter of your bets against true count and your correlation. One number that says *how well
   you bet the count*, independent of whether you won.

### Design detail
- **Three drills, one mechanism.** Running count (does the count match the cards?), true count (penetration
  awareness), bet-to-edge (sizing discipline). Same mask, different reveals.
- **Score on correlation, not profit.** Profit over 20 hands is noise; how tightly your bets track the edge is
  measurable in 20 hands and is the actual skill.
- **Reveal on demand** (hold a key) so it teaches rather than tests — and count the peeks, because "peeked 14
  times" is honest feedback.
- **Drills never touch career stats.** Same rule as forks and imported shoes.

### Implementation sketch
- **New** `struct HudMask { uint32_t visible; }`, applied where `HUDState` is populated rather than inside the
  renderer — keep the renderer dumb and the decision testable.
- **New** `DrillScorer` — takes `(trueCount, wager)` pairs, returns mean absolute error and correlation. Pure.
- **Changed** `AppMode` gains `Drill`; `SessionLifecycle` gains `startDrillSession()` beside the existing four
  entry points.
- **Data** optional `drillBest` per drill type. Additive.

### Headless test surface
- With a given mask, `HUDState` has exactly the expected fields populated and the rest at defaults.
- `DrillScorer` on a synthetic series returns hand-computed error and correlation, including the
  all-bets-identical case (correlation is undefined — decide what you return, and test it).
- A drill leaves the profile unchanged.

### Edge cases & gotchas
- **Faces hidden but totals visible defeats the drill** — and vice versa. Author the mask as coherent *groups*,
  not per-field, or someone toggles one field and produces a drill that teaches nothing.
- **The count must stay correct while other things are hidden.** Don't let "hide" become "don't compute".
- **Reko and Hi-Lo scales differ** (see #5). Label which count is on screen or the drill teaches a number that
  won't match the player's normal game.

### Dependencies
B4 — the seed must be right or the drill teaches wrong. Nothing else.

### Open questions
A dedicated mode, or a toggle inside a normal game? A toggle is cheaper and likelier to be used; a mode is more
focused. Ship the toggle, promote it later if it earns it.

### Where it grows
Feed it from #3's leaks and #4's daily ("today's drill"), and reuse its scoring in #10 to show a newcomer their
count is right before letting them bet on it.

---
## 9. Ghost Rival

**Effort M · makes the NPC seats mean something · needs #4's shared shoe**

### Pitch
Race a ghost — your own best recorded shoe, or a bot that plays perfect basic strategy and flat-bets — over the
same shoe order. Two bankroll lines, one finish. The NPC seats at your table currently do nothing for you; this is
the cheapest way to make them matter.

### Why this app, why now
`ShoeRecord` already contains a complete, replayable game, and `HUDState::playerBankrolls` already exists because
the table already renders multiple seats. A ghost is a second bankroll line driven by a recorded or scripted
policy instead of a live player — the rendering plumbing is there and unused.

The teaching value is specific: a perfect-basic-strategy ghost on the *same shoe* shows, concretely and without
lecturing, exactly where you and the bot diverged and what each divergence cost. That is #3's leak report
experienced as a race instead of a table.

### The experience
1. Before a shoe: **Race** → choose a ghost (past self / basic-strategy bot / a friend's imported score from #4).
2. A second, dimmed seat plays the same shoe from the ghost's recorded actions.
3. A thin two-line race strip above the table; the ghost's card flips face-up only after you've acted, so it never
   contaminates your decision.
4. End of shoe: your line vs the ghost's with divergence points marked. Clicking one opens that round in Replay —
   which is #1, for free.

### Design detail
- **A ghost is a recorded action stream, not a live player.** Store `(roundIndex, handIndex, action)` and replay
  it. Deterministic, cheap, and structurally unable to cheat.
- **Bot ghosts are generated once and stored identically** — run a headless game with a basic-strategy policy over
  the shoe, record its actions, replay them. That keeps any second AI out of the live path and makes every ghost
  the same kind of object.
- **Ghosts never affect your outcome** — no card consumption, no penetration change. Say so on screen, because a
  ghost drawn in a seat will read as a player.
- **Same shoe or nothing.** A race across different shoes is meaningless; require #4.

### Implementation sketch
- **New** `struct GhostRun { std::vector<GhostAction> actions; int finalNet; std::vector<int> bankrollPerRound; }`
  plus `recordGhost(const ShoeRecord&, BetPolicy&, PlayPolicy&)` — headless, reusing #2's policies.
- **Changed** `GameRenderer` — a race strip and a dimmed ghost seat, reusing the `playerBankrolls` layout math.
- **Changed** `Replay` — divergence markers, which are just the set difference of two action streams.
- **Data** optional `ghosts` array on `ShoeRecord`, additive and capped (same reasoning as #1's fork budget).

### Headless test surface
- A ghost replayed against its own shoe reproduces its recorded `finalNet` exactly.
- Divergence detection returns exactly the rounds where actions differ, on a hand-built pair of streams.
- A ghost changes no live state — profile and shoe record byte-identical with and without.

### Edge cases & gotchas
- **Divergent card counts.** If the ghost took a different action it consumed different cards, so from that point
  the shoe state genuinely differs. Re-executing its later actions inside *your* game will eventually ask for an
  illegal move. The clean answer: a ghost is computed on its own headless run and drawn as a parallel line — never
  re-executed inside the live game.
- **Illegal stored actions** (hand-edited profile, or a ghost from another rule set) must be skipped with a log,
  not crash the race.
- **Rule-set mismatch makes the comparison unfair.** Store the rules alongside the ghost and refuse to race across
  different rule sets.

### Dependencies
#4 (shared shoe), #2's policy abstraction (to generate bot ghosts), #6's chart primitives (the race strip).

### Open questions
Is it a *race* (competitive) or a *comparison* (study)? Identical mechanics; the copy decides which it is, and the
study framing is more honest about what's being measured.

### Where it grows
Friend ghosts via #4's codes are the first genuinely social thing in the app and need no networking. A "house
ghost" playing the dealer's side would make #2's variance lessons visceral.

---
## 10. Onboarding Campaign + name entry

**Effort L · the only feature here that is about retention rather than capability · absorbs U7**

### Pitch
A six-hand guided first run: deal → count → edge → bet the edge → win or lose on purpose → here's what the
dashboard means. It ends by asking your name, which fixes the fact that the player is hard-coded to "William" and
`profile.name` ships as an empty string.

### Why this app, why now
This application is unusually demanding for a first-time user: it opens a table with a running count seeded to
**−20**, a "True Count" that is actually the running count under Reko, an Edge % readout, a Kelly fraction, and a
strategy hint — and it explains none of them. U5 in the defect list ("True Count −20.0 looks broken") is not
really a UI bug; it is an onboarding gap wearing a UI costume, and no amount of relabeling fully fixes it.

The mechanics for a scripted run already exist: `StrategyTrainer` is a question-driven session with its own
timer, streaks and accuracy, and `Deck::setCards()` installs an exact card order. A tutorial is those two things
with a fixed script — the deck is stacked so the lesson lands on the beat where it's supposed to.

### The experience
1. First launch only (a `tutorialCompleted` flag on the profile). Skippable in one keystroke, always.
2. Six scripted hands, each with one line of copy and one required action:
   - **H1** hit/stand, no numbers shown at all.
   - **H2** the count appears; you say what it should be before you act.
   - **H3** the count turns positive; the edge readout lights up and explains itself once.
   - **H4** you bet — and the screen shows why the Kelly number is what it is.
   - **H5** a hand where the right play loses. Deliberately. The lesson is that correct ≠ profitable.
   - **H6** a hand where the wrong play wins, and the Leak Report tells you so.
3. **Name entry**, then the dashboard, briefly narrated.
4. Total time under four minutes.

### Design detail
- **Stacked decks, not luck.** Every scripted hand uses `setCards()` with a hand-authored order. If a lesson can
  fail to trigger, it will, on the first run, for the person you most need it to work for.
- **Teach the number the player will actually see.** Under Reko the "True Count" box shows the running count, so
  the tutorial must explain *that* box. This is why the feature depends on B4 and U5 being settled — teaching a
  number that's about to change is wasted work.
- **H5 is the most important screen in the app** and it costs nothing. Almost every blackjack teaching tool stops
  at "here's the right play"; the reason players quit is that the right play loses often, and nobody tells them.
- **Skip must be genuinely one keystroke** and must never re-ask. A tutorial that traps a returning player is
  worse than no tutorial.
- **Name entry is the cheapest part and the one that closes a shipped defect.** Type it, or accept the default;
  store it in the `name` field that already round-trips through the save file.

### Implementation sketch
- **New** `include/Tutorial.hpp` / `src/Tutorial.cpp` — a scripted step list: `{deckOrder, prompt, requiredAction,
  onAdvance}`. Data-driven so lessons can be reordered without touching the engine.
- **Changed** `AppMode` gains `Tutorial` with its own tick/draw/input path; `SessionLifecycle` gains
  `startTutorial()`.
- **Changed** the main menu — a name field, and a "Replay tutorial" entry so it's discoverable later.
- **Data** `name` (already exists, finally populated), `tutorialCompleted` bool, optional `tutorialStep` so a quit
  mid-tutorial resumes rather than restarts. All additive.

### Headless test surface
- Each scripted step's deck order produces the exact cards and count the copy claims — this is a data
  assertion, and it is the test that keeps the copy honest as the game changes.
- Completing step *n* advances to *n+1*; the required action is enforced; skipping sets the flag and never
  re-triggers.
- A name round-trips through save/load, including an empty name and a name with quotes and unicode.
- Quitting mid-tutorial resumes at the same step.

### Edge cases & gotchas
- **The tutorial must not pollute career stats or achievements** — "First Deal" firing during a scripted hand is
  technically correct and emotionally wrong. Same non-persisting gate as #1, #2, #4, #8; by the fourth one, build
  the shared flag.
- **Copy rots.** The tutorial describes the UI. Every future UI change can make a line wrong, and nobody will
  notice. Keep the copy in one table next to the step definitions, and let the deck-order test reference the same
  constants the copy prints.
- **A stacked deck is not a random deck.** Don't let a tutorial shoe leak into `longHistory` or into #4's codes.

### Dependencies
U7 (name entry — this is where it belongs), B4 and U5 (settle what the count box means before explaining it).

### Open questions
- Six hands or twelve? Six is a feature; twelve is a campaign, and campaigns need art and writing you don't have
  yet. Start at six.
- Should the tutorial be reachable from the main menu forever, or only on first run? Forever — it's the best demo
  of what the app does.

### Where it grows
Objectives from #7's challenges, a "count check" moment from #8's scoring, and a first-run Simulation Lab teaser
from #2 that finally answers "so does this actually work?" in the player's own time rather than yours.

---
## Runners-up

Not developed, but worth keeping on the list:

- **Hand-history search.** "Show me every hand where I split 8s." `longHistory` already has everything; it's a
  filter plus a list, and every hit deep-links into Replay. S.
- **Theme / accessibility pass.** Card skins, colourblind-safe suits, font scaling, high-contrast felt. Pairs with
  U3 and U6, and unlike most of this list it's the one thing that broadens who can use the app at all. M.
- **Reactive ambience.** `SoundManager` already has independent sound/music volume and mute; a bed that tightens
  on a losing streak or opens up on a hot shoe is a few hours of work and disproportionately atmospheric. S.
- **Insurance as its own drill.** The most commonly misplayed decision in the game, and the engine already models
  it end to end. S, and it slots straight into #8.
- **A "count check" habit builder.** Prompt the player to state the count at random intervals and track accuracy
  over a shoe — the real-world skill that separates people who know how to count from people who can count while
  playing. S–M, and it is really just #8 with a timer.

## Deliberately not proposed

- **Additional counting systems** (KO, Wong Halves, Omega II). Feature 6 is frozen in the 0.3 notes and the reason
  still holds: the app's identity is Reko + Kelly, and a fourth counting style multiplies the surface of every
  count-dependent readout, including the ones that are currently wrong (B4, B16).
- **Real-time multiplayer networking.** Feature 10 stays Planned. #4 gets most of the social value at a fraction
  of the cost, and doing it in that order means that if networking ever happens, it has a proven thing to network.
- **Anything that widens `GameRules`.** The 0.3 constraint on narrow `const GameRules&` threading is good and the
  ideas above deliberately reuse existing fields as challenge modifiers rather than adding new ones.
- **A second currency, shops, or cosmetics.** It would make #7's payouts meaningful, and it would move the app
  from "a tool that teaches" to "a game that retains". That's a legitimate choice, but it should be a *choice*,
  made on purpose, not an accident of a feature list.

## If you only build three

**#6 → #3 → #1.** The chart primitives from #6 are reused by four other features, and it's a day. #3 makes the
deviation tracking you already have finally mean something. #1 is the feature nobody else in this genre has, and
the app is already storing every byte it needs.

## The pattern worth noticing

Five of these ten (#1, #2, #4, #8, #10) need the same thing: **a game that runs without writing to the profile.**
Today that gate exists in exactly one place, imperfectly (B1), and `HandEvaluator` saves unconditionally at
`:297`. Build that flag once, properly, as part of the B1 fix — one `Game::persistsProgress()` predicate that
every save, achievement and stats path consults — and five of these features get their hardest correctness
requirement for free. Build them one at a time and you will write that conditional five times and get one of them
wrong.

The same goes for the two smaller shared seams: **chart primitives** (#6, reused by #2/#5/#9) and **the
extract-the-string-then-test-it pattern** from B3 (needed by #3, #5, #7, #10). Both are cheap. Both get cheaper
the earlier they exist, and both are the difference between these features being testable and them becoming the
next 0.4 defect list.

---

*Proposals only — nothing here is scheduled. If one of these gets picked up, give it its own section in the next
phase's `NEXT_STEPS.md` with the same fix/test/done-when structure the defects use, and re-verify every file:line
reference above against the tree at that time.*












