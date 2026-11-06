# NEXT STEPS — Blackjack2 (Phase 0.4: QA hardening)

**Purpose:** Implementation-ready bug ledger + sequenced plan for the next context window. Every item below was
found by *playing the actual game* (headless Xvfb + synthetic X11 input, 139 screenshots) and was traced to a
specific file:line. Companion document: `.summaries/0.4/CONTEXT.md` — read it first for methodology, evidence,
architecture and guardrails.

**Posture change from 0.3:** feature work stays frozen. This phase is defect removal + the missing test seam.
The suite is **264/264 green** while **15 logic/data defects (B1–B15) and 10 UI issues (U1–U10)** exist, plus
three tech-debt items (T1, T2, B16). That gap is the theme of this phase: every bug lives in the
presentation/lifecycle seam that the headless-first contract deliberately leaves untested.

**Handoff prompt — paste into a fresh window** (regenerated 2026-11-06 to match the audited docs; keep it in sync
with CONTEXT.md §13 whenever the ledger changes):

````markdown
You are continuing work on Blackjack2 (C++17 game / C++20 tests, SFML 3, CMake, Catch2 v3.6.0), a desktop
blackjack teaching simulator. Phase 0.4 is a QA-hardening pass: defect removal, not feature work.

## Step 0 — this iteration
Branch:            3.1   <- edit if different
Focus this round:  <e.g. "B15 + B13 only">
Already done:      <e.g. "nothing" / "B15 landed">
Anything I should know: <optional>

## Step 1 — read these, in this order, before touching code
All three live in `.summaries/0.4/` on branch `3.1`. (`master` is a frozen checkpoint at 380da26 and does NOT
contain them — if they're missing, you are on the wrong branch.)

1. `.summaries/0.4/CONTEXT.md` **§13 (Document audit log)** — what has actually been verified, and how. Read first;
   it tells you which claims in these docs are load-bearing and which were inherited from a playtest.
2. `.summaries/0.4/CONTEXT.md` — the rest, fully. Background, evidence, guardrails.
3. `.summaries/0.4/NEXT_STEPS.md` — the actionable ledger.
4. `.summaries/0.4/IDEAS.md` — DO NOT read or implement unless I explicitly ask this round. Not a plan.

Key sections: CONTEXT §3 testing blind spot · §4 architecture map · §5 persistence & data model · §6 evidence log
(§6.9 "cleared — not bugs", §6.10 B13 proof) · §7 conventions · §8 environment (incl. the `-M` trap) · §10 open
decisions · §11 status tracker. NEXT_STEPS: Verification gate · Triage table · Phases 0–5 · Testing strategy ·
Definition of done · Explicitly out of scope.

## Step 2 — establish real state, do not trust the docs blindly
The docs were written at a point in time and may be stale. Before coding:

  git status && git --no-pager log --oneline -3
  cmake --preset "GCC 13.3.0 x86_64-linux-gnu"
  cmake --build 'out/build/GCC 13.3.0 x86_64-linux-gnu' --parallel
  ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu' -j8 --output-on-failure

Then report back, BEFORE writing any code:
  - current test count and pass/fail (baseline 264/264)
  - which §11 IDs are actually still open — verify against the source, don't trust the table
  - any doc claim that no longer matches the code
  - your proposed plan for this round's focus, and any open §10 decision that blocks it

Calibration: on 2026-11-06 every `file:line` citation in both docs was re-checked against `96fd416` and held, and
the structural claims were corrected in place (`f3c6ce1`). Those citations stay trustworthy until the **code**
moves, not until the docs do — so if `git status` is clean and `git log --oneline -- include src tests CMakeLists.txt`
still shows `96fd416` as the last code change, trust the citations and spend your effort on the §11 **Evidence**
column instead: several rows are marked code-only or "not re-run this pass", and those are the ones worth
re-testing. If any code has landed since, re-verify the sites you're about to touch.

Wait for my go-ahead.

## Step 3 — working rules
- One bug = one commit. Test first, then fix: several items specify a test that must FAIL before the fix. If it
  passes immediately, you edited the wrong file (see B15).
- Re-run the full suite after every fix. Never finish with a lower test count than you started.
- Headless-first tests only: `Game(true)` + `setDeck(...)` + `*ForTest()` accessors. No display.
- New JSON fields must be optional with a default. Never break a legacy profile.
- `Player::getProfile()` returns BY VALUE — never bind it to `const auto&`.
- If a doc is wrong, FIX THE DOC in the same commit. Don't work around it silently.
- **B15 lands before B12/B13/B14.** There are two live copies of `Serialization.hpp`: the shipped binary compiles
  `src/`'s copy via `src/PlayerProfile.cpp`, while `test_profile_settings.cpp` / `test_game_rules.cpp` /
  `test_achievements.cpp` compile `include/`'s. A schema fix in the wrong copy goes green with an unchanged binary.
- **B1 has SIX `saveProfile()` call sites**, not three — the table is in the B1 entry. Gating three leaves the bug.
- `.summaries/0.4/hist_proof.cpp` is the checked-in B13 proof; it exits 1 today and is the fastest way to confirm
  a B13 fix reached the *shipped* schema rather than just the tested one.
- Never append `-M` to a command copied from `compile_commands.json` — it overwrites the `.o` with the dependency
  list. See CONTEXT §8.

## Step 4 — before you finish
- Update the §11 status tracker in CONTEXT.md.
- Append any decision made to the decision list in CONTEXT.md §5.
- Add a dated entry to CONTEXT.md §13 if you corrected any claim in these docs.
- Tell me: what landed, what's still open, what you'd do next.

## Hard guardrails
- No new features. IDEAS.md is not a plan.
- Do not revive Feature 6 (KO / Wong Halves / Omega II counting).
- Do not widen `GameRules` beyond exposing `maxSplitHands` (B10).
- Do not change the `AppMode` enum except by adding a value with its own state machine.
- Do not "fix" the `handActions` indexing — it is correct; see CONTEXT §6.9.
- Do not re-litigate anything already cleared in §6.9.
- Do not break the additive-JSON contract: every new field optional, with a default.
````


---

## Verification gate (run after EVERY fix)

```bash
cmake --preset "GCC 13.3.0 x86_64-linux-gnu"
cmake --build 'out/build/GCC 13.3.0 x86_64-linux-gnu' --parallel
ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu' -j8 --output-on-failure
```

Baseline: **264/264 pass in ~8s** (`-j8`). Re-verified **2026-11-06** at commit `96fd416`: 264/264 in 8.15 s,
configure and build clean (warnings only — the pre-existing `[=]` implicit-`this` deprecation in
`GameRenderer.cpp`'s `modeTable()` lambdas). Never commit with a lower count than you started.

> **The working tree is clean.** The Tournament refactor this document used to describe as "uncommitted" landed
> as `96fd416 "new horizons"` on branch `3.1`, and it carried the `.summaries/0.4/` docs in with it. Every line
> number in these two docs was re-checked against that commit on 2026-11-06 and is accurate. See CONTEXT.md §1.

> **Do not put `ASAN_OPTIONS` in the gate.** An earlier revision of this gate ran
> `env ASAN_OPTIONS=detect_leaks=0 ctest …`. The configured build dir has **no AddressSanitizer**
> (`CMakeCache.txt` has no `-fsanitize`; `nm Blackjack2 | grep -c asan_report` == 0), so that variable is a no-op
> and only makes the gate look like it does something the build cannot support. The 0.3 note about "~75 SFML tests
> failing at exit from libdbus LeakSanitizer teardown" does **not** apply to this build. If you want ASan — and
> B8/B9 argue you should — configure it explicitly in a separate build dir. See CONTEXT.md §8 (doc drift).

---

## Triage summary

| ID | Sev | Title | Primary site | Phase |
|----|-----|-------|--------------|-------|
| B1 | 🔴 Critical | Tournament mode overwrites the saved career profile | `src/SessionLifecycle.cpp:35` | 0 |
| B2 | 🔴 Critical | Achievement toast never clears (permanent "NEW!" banner) | `src/AchievementFeeder.cpp:48` / `src/GameRenderer.cpp:154` | 0 |
| B3 | 🔴 Critical | Side-bet result always prints "WON" (LOST branch is dead) | `src/GameRenderer.cpp:997-1000` | 0 |
| B4 | 🔴 Critical | Reko count seeded with compile-time deck count, not the rule | `src/Game.cpp:254` (+ `src/Deck.cpp:71`) | 0 |
| B13 | 🔴 Critical | Quitting mid-shoe silently deletes those hands from career stats | `include/Serialization.hpp:193-210` + `src/PlayerProfile.cpp:68` | 0 |
| B5 | 🟠 High | Main bet never escrowed → bankroll can go negative | `src/Player.cpp:89` | 1 |
| B6 | 🟠 High | Rules panel keyboard edit is unthrottled (multi-step per tap) | `src/Game.cpp:383-388` | 1 |
| B7 | 🟠 High | Escape-to-quit from the main menu skips the profile save | `src/Game.cpp:189` + `src/InputHandler.cpp:520` | 1 |
| B8 | 🟠 High | `Deck::draw()` has no empty-shoe guard (pre-existing, known) | `src/Deck.cpp:121-135` | 1 |
| B14 | 🟠 High | Save is non-atomic and a failed load silently resets to a fresh profile | `src/PlayerProfile.cpp:50-61` + `:28-48` | 1 |
| B15 | 🟠 High | Two live copies of `Serialization.hpp`; game and tests compile different files | `src/Serialization.hpp` vs `include/Serialization.hpp` | 1 |
| B9 | 🟠 High | One unreproducible crash; logger loses all output on abort | `src/Logger.cpp` (stdout buffering) | 2 |
| B10 | 🟡 Medium | `maxSplitHands` is dead config (no UI row, not serialized) | `src/GameRules.cpp:15-27` / `include/GameRules.hpp` `to_json` | 3 |
| B11 | 🟡 Medium | Startup control banner mislabels Left/Right | `src/Game.cpp:162-163` | 3 |
| B12 | 🟡 Medium | Save-schema typo `resplitTentsAllowed` ("Tens") | `include/GameRules.hpp` | 3 |
| U1–U10 | 🟡 Medium | UI/UX defects (see Phase 4) | various | 4 |
| T1–T2 | 🟢 Low | Save-write amplification; unbounded history | `src/HandEvaluator.cpp:297` | 5 |
| B16 | 🟡 Medium | `reconstructDeckAtRound()` is dead code and reconstructs from the wrong end | `include/GameRecord.hpp:307-329` | 5 |

**Every "Primary site" above was re-checked against `96fd416` on 2026-11-06 and is correct**, as is every
`file:line` citation in the body of this document — the citations are the reliable part of these docs. What the
audit did find were errors in the *structural* claims around those citations: a stale dirty-tree warning, B1
naming three save sites where there are six, B15 prescribing a CMake edit for a file CMake never referenced, and
B15 understating which translation units read which header. Those are corrected in place; the running list is in
CONTEXT.md §13.

**Not a bug (investigated and cleared):** `handActions` flat-indexing looks wrong when `split()`
(`src/Game.cpp:766`) appends at the end, but is safe because seats act strictly in order, so later slots are still
empty. Do not "fix" this without also changing turn order. See CONTEXT.md §6.9.

---

# PHASE 0 — Stop the bleeding (B1–B4, B13)

These five are the defects a player meets in a normal session. B1 and B13 destroy saved data; B2–B4 lie to the
player on screen. Each is a small, independent change — do them first and commit each separately.

---

## B1 — Tournament mode overwrites the saved career profile 🔴

**Symptom (reproduced).** Play a normal game so stats accumulate, enter Tournament from the main menu, then close
the window with the title-bar X. The save file is replaced by the tournament's throwaway profile.

Measured on a live profile:

| field | before | after X-quit from Tournament |
|---|---|---|
| `bankroll` | 1025 | **1000** |
| `stats.handsFinished` | 1 | **0** |
| `stats.wins` | 1 | **0** |
| `stats.betsPlaced` | 1 | **0** |
| `longHistory` (shoes) | 1 | **0** |

Achievements survive only because `persistUnlockedTo()` re-stamps them from the in-memory manager.

**Second symptom, same cause.** The Statistics Dashboard renders **all zeros** after visiting Tournament, because
`startStatsSession()` flattens history out of that same blank profile.

**Root cause.** `SessionLifecycle::startTournamentSession()` builds the human seat with the *ephemeral* NPC
constructor:

```cpp
// src/SessionLifecycle.cpp:35
game.players.push_back(Player("William", config.startingChips));
```

`Player(name, startingBankroll)` (`src/Player.cpp:47-52`) is the NPC path: it fabricates a fresh empty
`PlayerProfile` and never reads disk. Contrast `startNewGame()` (`src/SessionLifecycle.cpp:182`), which uses the
loading constructor `Player("William")`. So after entering Tournament, `players[0]` is no longer the career
profile, and the window-close handler (`src/InputHandler.cpp:247-257`) saves `players[0].getProfile()` over the
real file.

**Fix.** In `startTournamentSession()`, use the loading constructor and override only the in-memory chips:

```cpp
game.players.push_back(Player("William"));        // loads the real profile
game.players[0].setBankroll(config.startingChips); // tournament chips are in-memory only
```

Then make the tournament genuinely non-persisting. **Preferred:** gate the profile writes behind
`Game::tournamentActive_`.

**There are seven `saveProfile()` call sites, not three.** The first draft of this entry named three and got two of
their line numbers wrong; gating only those three leaves four unguarded and the profile still gets clobbered. The
complete set, verified 2026-11-06:

| Site | Path | Note |
|---|---|---|
| `src/HandEvaluator.cpp:297` | every settled round | also T1's write-amplification site |
| `src/InputHandler.cpp:104` | pause → **Restart** | the draft mislabelled this one as Quit-to-Menu |
| `src/InputHandler.cpp:119` | pause → **QuitToMenu** | |
| `src/InputHandler.cpp:254` | **window close** (`Event::Closed`) | the B1 repro route; draft said `:252` |
| `src/Game.cpp:1342` | shoe boundary | |
| `src/Game.cpp:1501` | shoe boundary (second path) | |
| `src/SessionLifecycle.cpp:287` | shoe/lifecycle boundary | **missing from the original six; added 2026-11-06 — an unguarded saveProfile that B1's tournament-time gate must also cover** |

Note also *when* the damage is done: `startTournamentSession()` calls `game.players.clear()`
(`src/SessionLifecycle.cpp:34`) **before** pushing the ephemeral seat, so the loaded career profile is destroyed
the instant Tournament is entered — not at save time. That is the mechanism behind the second symptom (the Stats
Dashboard reading zeros), and it is why gating the writes is not sufficient on its own: the constructor fix at
`:35` is what preserves the in-memory profile, and the gates are what stop a tournament session writing its
throwaway chips and empty stats over the career file.

Alternative: snapshot the real profile on entry and restore on exit. Prefer gating — it matches the intent that a
tournament is a competition, not career progress, and record the decision in CONTEXT.md §5.

**Test.** New headless case in `tests/test_session_lifecycle.cpp`:
1. Build a profile with a non-zero bankroll + one round of history and save it.
2. `startTournamentSession({})`.
3. Assert `players[0]`'s profile still reports the pre-tournament career stats (not zeros).
4. Drive the close-save path, re-load from disk, assert bankroll / stats / `longHistory` are unchanged.
Also assert the dashboard is non-empty after Tournament via `statsPerRoundCountForTest()`.

**Gotcha.** `Player::getProfile()` returns **by value** (`include/Player.hpp:78`). Never bind its result to
`const auto&` across a statement — that dangles (same bug class as 0.3 §6 #26). Write
`PlayerProfile copy = players[0].getProfile();`.

**Done when.** The repro above leaves the JSON byte-identical, and the dashboard shows real numbers after a
Tournament visit.

---

## B2 — The achievement toast never goes away 🔴

**Symptom (reproduced).** "NEW! First Deal" was painted across the middle of the table and never disappeared —
through later rounds, back to the main menu, and again in Tournament mode minutes later, overlapping a chip.

**Root cause.** The field is written in exactly one place and cleared in none:

```
src/AchievementFeeder.cpp:48   game.hudState.pendingAchievementName = def.name;   // set
src/GameRenderer.cpp:154       if (game.hudState.pendingAchievementName.empty())  // read
                               — nothing ever assigns "" —
```

`drawAchievementToast()` therefore draws it forever. It is only drawn in the Playing/Tournament render path,
which is why it appears to "come back" whenever you return to the table.

**Fix.** Give the toast a lifetime. `HUDState` already has `summaryTimer` as precedent — add
`float achievementToastTimer = 0.0f;` beside `pendingAchievementName` (`include/HUDState.hpp:83`), add
`GameTiming::ACHIEVEMENT_TOAST_SECONDS = 4.0f` to `include/Constants.hpp`, set the timer when the name is set,
decrement it in `Game::tickTimer()` alongside the other timers, and clear the name at 0. Draw only while > 0.

**Test.** Headless, in `tests/test_achievements.cpp`: unlock through the real round path, assert the name is set,
advance the tick past the duration, assert the name is empty, then unlock again and assert it re-arms. This is a
logic assertion on `hudStateForTest()`, not a pixel test — stays Graphics-only.

**Done when.** The toast shows for ~4s, disappears, and can re-arm for the next unlock.

---

## B3 — Side bets always report "WON" 🔴

**Symptom (reproduced).** Held 4♣ + 5♥ (not a pair) with a $20 Perfect Pairs side bet. Bankroll math
(1000 → 980 at deal → 930 at settle) proves the side bet **lost**, yet the table read
`Side Bet: Perfect Pairs $20 WON +20`.

**Root cause.** `src/GameRenderer.cpp:997-1000`:

```cpp
int net = game.hudState.sideBetPayout - game.hudState.sideBetWager;
if (net < 0)
    net = -net;                                   // destroys the sign
label += (net >= 0 ? "  WON +" : "  LOST ") + std::to_string(net);
```

The absolute value runs *before* the sign test, so `net >= 0` is unconditionally true and **"LOST" is dead code**.
Upstream data is correct: `resolveSideBets()` sets payout 0 on a loss (`src/RoundDealer.cpp:46`) and
`syncBettingAndSideBet()` copies it faithfully (`src/Game.cpp:451`).

**Fix.** Drop the `abs()` and branch three ways — a zero net is a push, not a win:

```cpp
const int net = game.hudState.sideBetPayout - game.hudState.sideBetWager;
label += (net > 0 ? "  WON +" : net < 0 ? "  LOST -" : "  PUSH  ") + std::to_string(std::abs(net));
```

**Test.** `tests/test_side_bets.cpp` already locks the payout numbers; add a **label-string** assertion so the
renderer cannot regress. Extract the formatting into a pure free function (e.g.
`sideBetResultLabel(kind, wager, payout)` in a Graphics-only header) and assert win / loss / push. This
extract-the-string-then-test-the-string pattern is the one to reuse throughout Phase 4.

**Done when.** A losing side bet reads `LOST -20`.

---

## B4 — Reko count seeded with the compile-time deck count 🔴

**Symptom (reproduced).** Set Custom Game Rules → Decks = 4, started a game: `Cards Left 209` (correct:
4×52 + cut card) but the fresh, undealt shoe showed **`Count -20`**. The correct Reko seed for 4 decks is
**-12**.

**Root cause.** `src/Game.cpp:254`:

```cpp
const int rekoSeed = BasicStrategy::rekoStartingOffset(Constants::NUM_DECKS);
```

`Constants::NUM_DECKS` is the compile-time 6, so the seed is always -20 regardless of `gameRules_.numDecks`.
Error by shoe: 1 deck off by 19 (-20 vs -1), 2 off by 15, 4 off by 8, 8 off by 7 (-20 vs -27). Because Reko's
"True Count" *is* the running count (`src/Game.cpp:483`), this corrupts the True Count readout, Edge % and
Kelly / Auto-Kelly sizing from the first hand.

**Same defect, second site.** `Deck::reshuffleWithRemaining()` (`src/Deck.cpp:71`) rebuilds with
`for (int deck = 0; deck < Constants::NUM_DECKS; ++deck)` — a mid-shoe reshuffle on a 1-deck table silently
rebuilds a 6-deck shoe.

**Fix.** Thread the live rule into both:
- `resetShoeAnalytics()` → `rekoStartingOffset(gameRules_.numDecks)`.
- `Deck` should remember the count it was created with: add `int shoeDecks_` set in `create()` (default
  `Constants::NUM_DECKS`) and use it in the reshuffle loop. Keeps `Deck` self-contained, no signature churn.

**Test.** Extend `tests/test_reko_counting.cpp`: headless `Game(true)`, set `gameRules_.numDecks` to each of
1/2/4/6/8, `startNewShoe()`, assert `runningCountForTest()` equals the roadmap table (-1/-5/-12/-20/-27). Add a
`Deck` test that `reshuffleWithRemaining()` preserves shoe size for a non-6-deck shoe. Both fail today.

**Done when.** A 1-deck shoe starts at -1, an 8-deck shoe at -27, and Edge % / Auto Kelly track them.

---

## B13 — Quitting mid-shoe deletes those hands from career stats 🔴

**Proven, not inferred — and re-proven 2026-11-06.** A standalone program against the real `PlayerProfile` code
(`.summaries/0.4/hist_proof.cpp`, build in CONTEXT.md §6.10) shows the career hand count moving **backwards**:

```
reload #1: handsFinished=8  shoes=1   (expected 8)   <- correct
reload #2: handsFinished=6  shoes=1   (expected 9)   <- 3 hands silently destroyed
```

**Trigger — the ordinary way to stop playing.** Complete a shoe, play 3 hands into the next one, close the
window. Nothing else required.

**Root cause — two correct-looking halves that don't compose.**

1. `to_json(const PlayerProfile&)` (`include/Serialization.hpp:191-210`; `from_json` is `:212-231`) persists
   `longHistory` but **not `currentHistory`** — the in-progress shoe's rounds are simply never written. Verified
   2026-11-06: `grep -rn currentHistory include/ src/` finds it only in `PlayerProfile.hpp` (the member plus one
   comment), `PlayerProfile.cpp` (the member's four uses plus one comment) and two comments in
   `SessionLifecycle.cpp:101-102`. It appears in **neither** copy of `Serialization.hpp`.
2. `PlayerProfile::save()` (`src/PlayerProfile.cpp:68-91`) treats the record as the source of truth: when
   `longHistory` is non-empty it **overwrites** every record-derivable stat with `careerStatsFromHistory()`.

In-session that is right, because `careerStatsFromHistory()` (`src/PlayerProfile.cpp:99-111`) folds the
in-progress `currentHistory` in as a synthetic shoe — which is exactly why save #1 above is correct. But after a
reload `currentHistory` is empty, so the *next* save recomputes from `longHistory` alone and overwrites the
correct numbers with smaller ones. The loss is invisible when it happens and lands one save later.

**Why the close path is the trigger.** `addShoeRecord()` is what moves `currentHistory` → `longHistory`, and it
is called at the shoe boundary (`src/Game.cpp:1340`, `:1499`) and on **Quit to Menu** (`src/InputHandler.cpp:102`,
`:115`) — but **not** in the window-close handler (`src/InputHandler.cpp:247-257`), which jumps straight to
`saveProfile()`. So quitting with the title-bar X mid-shoe orphans the current shoe's rounds.

**Blast radius.** Every record-derivable stat regresses, not just `handsFinished`: wins/losses/pushes,
`blackjacks`, `greatestHand`, the variance accumulators, streaks, `greatestWin/Loss/SessionWin/SessionLoss`, and
the side-bet tallies. It also corrupts the Replay / shoe-history feature, which reads the same records. The
non-recordable trackers (`splits`, `doubles`, `betsPlaced`, `betSum`, `betDistribution`, strategy counters) are
preserved verbatim, so the dashboard ends up internally inconsistent.

**Fix — persist the in-progress shoe (additive, honours the schema contract).** In `to_json`, write
`currentHistory`; in `from_json`, read it with an empty default so pre-existing profiles load unchanged:

```cpp
// to_json
j["currentHistory"] = p.currentHistory;
// from_json
p.currentHistory = j.value("currentHistory", std::vector<RoundRecord>{});
```

That alone makes the recompute correct, because `careerStatsFromHistory()` already knows how to fold
`currentHistory` in.

**Two hazards the first draft of this entry missed.** Both were found by reading the code on 2026-11-06; neither
is theoretical, and the second one is the reason this fix can *introduce* a bug while fixing one.

- **`addShoeRecord()` ignores the rounds you hand it.** `src/PlayerProfile.cpp:20-26` copies the argument, then
  overwrites `shoe.rounds = currentHistory` and clears `currentHistory`. So a shoe's rounds always come from
  `currentHistory`, never from the caller's `game.currentShoeRecord`. This is what makes the additive fix safe
  (no double count: the move clears the source), but it also means the parameter's `rounds` field is dead — do not
  "improve" the fix by populating it.
- **A round can already be stored twice, and there is a guard for it.** `src/SessionLifecycle.cpp:100-110`
  de-duplicates by fingerprint (`dealerCards` + `playerHands`) because a settled round lands in **both**
  `profile.currentHistory` and `game.currentShoeRecord.rounds`, and the stale mirror survives `addShoeRecord()`.
  Once `currentHistory` round-trips through disk, that mirror relationship survives a reload too. If you also take
  the "call the shoe-flush on window close" option below, re-check this dedupe path — the dashboard is the surface
  where a double count would appear.

Two supporting changes:
- Call the existing shoe-flush in the window-close handler (`src/InputHandler.cpp:247-257`) so it matches the
  Quit-to-Menu path, or accept the additive field as sufficient and note why.
- Add a debug assertion in `save()` that a save never *decreases* `handsFinished` relative to the in-memory
  stats — that invariant is what would have caught this class of bug at development time.

**Test.** `tests/test_profile_settings.cpp`, mirroring `.summaries/0.4/hist_proof.cpp` exactly: complete a shoe, add 3
rounds, save, reload, add 1 round, save, reload, assert `handsFinished == 9`. Assert a legacy profile with no
`currentHistory` key still loads. **This test fails against the current code** — write it first.

**Done when.** The proof program prints `OK`, and a mid-shoe quit preserves every career stat.


---

# PHASE 1 — Money, input and save robustness (B5–B8, B14, B15)

---

## B5 — The main bet is never escrowed; bankroll can go negative 🟠

**Verified in code; not reproduced to a negative display.** `Player::placeBet()` is only an affordability check —
it does not deduct:

```cpp
// src/Player.cpp:89
bool Player::placeBet(int amount) { return amount <= bankroll; }
```

Side bets, by contrast, *are* moved at `resolveSideBets()` (`src/RoundDealer.cpp:35/45`), immediately after the
deal and before the player acts. Two consequences:

1. **Inconsistent mid-hand readout.** Observed live: bankroll 980 with `Bet 50` still shown — the side bet is
   already gone, the main bet is still counted as yours.
2. **Over-commitment.** `Game::placeSideBet()` (`src/Game.cpp:1130`, affordability check at `:1134`) checks
   `players[0].getBankroll() < wager` against a bankroll that still contains the pending main bet. With $1000 you
   can set a $1000 main bet *and* a $1000 side bet, then lose both → `loseBet()` twice → **bankroll −1000**.
   `Player::adjustHandBet()` (`src/Player.cpp:200-211`) clamps only to `MINIMUM_BET`/`MAXIMUM_BET`, never to the
   bankroll, so nothing else stops it.

**Fix.** Pick one model and make both bet types obey it. Recommended: **escrow at commit.**
- Deduct the main bet in `confirmBet()` (after the `placeBet` guard succeeds) rather than at settlement, and
  return the stake on win/push in settlement.
- Make `placeSideBet()` affordability use *available* balance:
  `bankroll - pendingMainBet - pendingSideBet`.
- Add `Player::getAvailableBankroll()` so every clamp and hint reads one predicate.

**Test.** `tests/test_settlement.cpp` + `tests/test_side_bets.cpp`: commit main + side exceeding the bankroll and
assert the second commit is refused; assert a losing round can never drive bankroll below 0. Add a
bankroll-conservation invariant: `bankrollAfter == bankrollBefore - mainWager - sideWager + mainPayout +
sidePayout`.

**Done when.** The mid-hand bankroll excludes both wagers and no sequence produces a negative balance.

---

## B6 — Rules-panel keyboard editing is unthrottled 🟠

**Symptom (reproduced).** A single tap of Left on the Decks row jumped 6 → 1 (three steps). Four taps on Surrender
landed back on "Off" instead of "Early". Sometimes one tap = one step, sometimes several.

**Root cause.** `src/Game.cpp:383-388` polls held-key state every frame with no edge detection:

```cpp
const bool rightHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
const bool leftHeld  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
if (rightHeld || leftHeld)
    applyGameRuleEdit(gameRules_, gameRuleRows()[gameRulesSelectedRow].id, rightHeld);
```

At uncapped framerate a human key-down spans many frames → many edits. The mouse-wheel path in
`InputHandler::processRulesEvents()` *is* rate-limited via `scrollCooldown_` / `SCROLL_RATE_LIMIT_SECONDS`; the
keyboard path was never given the same treatment. The main menu is fine — it is event-driven.

**Fix.** Add edge detection to the rules case in `Game::update()`: keep `rulesLeftPrev_` / `rulesRightPrev_`
members and call `applyGameRuleEdit` only on a false→true transition. Up/Down navigation has the same issue, but
repeat is arguably desirable there — if you change it, call that decision out in the commit message.

**Test.** Headless: set `AppMode::GameRules`, simulate "held for N frames" by calling `update()` repeatedly with
the key logically down, assert exactly one edit. Follow the existing `*ForTest()` accessor pattern.

**Done when.** One tap is always exactly one step, and wheel and keyboard agree.

---

## B7 — Escape-to-quit from the main menu skips the profile save 🟠

**Evidence.** After playing, the save file's mtime was 02:40:57 while the process exited at ~02:50 — the
close-time save never ran.

**Root cause.** The main loop exits on the window flag before the pending event is polled:

```cpp
// src/Game.cpp:189
while (window.isOpen())
{
    deltaTime = clock.restart().asSeconds();
    inputHandler.processEvents();
    ...
}
```

The save lives in the `Event::Closed` branch of `InputHandler::processEvents()`
(`src/InputHandler.cpp:247-257`). Clicking the title-bar X queues a Closed event that *does* get polled, so that
route saves. But main-menu Escape calls `game.window.close()` directly (`src/InputHandler.cpp:520-521`); on the
next iteration `window.isOpen()` is false, the loop exits, and the queued Closed event is never read. The save
block is effectively dead for that route.

**Fix.** Extract the save into one method — `Game::persistOnExit()` — and call it **once after the `while` loop
in `run()`**, which covers every exit route. Prefer that over patching both call sites: one choke point, and it
matches the project's "centralize shared side effects in existing choke points" rule.

**Test.** Headless: mutate the profile, invoke the exit-persist path, reload from disk, assert the change landed.
Confirm double-saving is harmless (idempotent).

**Done when.** Both quit routes persist, verified by file mtime.

---

## B8 — `Deck::draw()` has no empty-shoe guard 🟠 (pre-existing, known)

`src/Deck.cpp:121-135` calls `cards.back()` / `pop_back()` with no size check, and a **second** unguarded
`cards.back()` after the cut-card pop. Already logged in 0.3 §6 as an ASan heap-buffer-overflow reachable when
more hands are drawn than the shoe holds (Tournament / StrategyTrainer with injected decks).

**Fix.** Guard both pops. On an empty shoe either return a sentinel the round engine handles by triggering
`handleEmptyDeck()`, or throw a typed error. Do **not** return a default-constructed `Card` silently — that hides
the bug and corrupts the running count.

**Test.** `tests/test_deck.cpp`: draw past the end and assert the guard fires. Worth running the suite in a real
ASan build for this one — see CONTEXT.md §8 (the current build dir has no sanitizer configured).

---

## B14 — A corrupt save file is silently replaced with a blank one 🟠

Found while reading the two functions that make B13 possible. They combine into a total-wipe path.

**Half 1 — the write is not atomic.** `PlayerProfile::save()` (`src/PlayerProfile.cpp:50-61`) truncates the real
file and streams into it:

```cpp
std::ofstream file(filename);
...
file << j.dump(2);
```

No temp file, no rename, no backup. A crash or power loss mid-write leaves truncated JSON. We already observed one
unreproducible crash this session (B9), and the file grows every round (T2), which widens the window.

**Half 2 — a bad read is treated as "no player".** `PlayerProfile::load()` (`src/PlayerProfile.cpp:28-48`)
catches any parse exception, logs `"Starting fresh."`, and returns a default `PlayerProfile()` — `name = ""`,
`bankroll = 1000`, no history. The four core fields use `j.at()` (`Serialization.hpp:214-217`), which **throws**
on a missing key, so a partially-written file is enough. The game then plays happily and its next save writes the
blank profile over the top of the damaged one. Recovery becomes impossible.

**Fix.**
1. **Atomic write:** serialise to `filename + ".tmp"`, then `fs::rename()` over the target (same filesystem, so
   it is atomic on POSIX). Optionally keep one `.bak`.
2. **Never silently discard:** if parsing fails, rename the bad file to `player_profile.corrupt.<timestamp>.json`
   and continue with a fresh profile *without* immediately overwriting it. Log the path so the user can recover.
3. Prefer `j.value(key, default)` over `j.at(key)` for the core fields too, so a file missing one key degrades
   instead of throwing — consistent with how every additive field in this schema already behaves.

**Test.** `tests/test_profile_settings.cpp`: write truncated JSON, load it, assert the corrupt file still exists
under a recoverable name and that no save has clobbered it; write valid-but-partial JSON and assert defaults fill
in. Add a round-trip test that a save leaves no `.tmp` behind.

**Done when.** No input file can cause the game to overwrite data it could not read.

---

## B15 — Two live copies of `Serialization.hpp` 🟠

**Do this before B12/B13/B14.** They all edit this header, and there are two of them.

**Evidence.** `src/Serialization.hpp` (236 lines) and `include/Serialization.hpp` (231 lines) both exist.
`diff` shows the divergence is **comments only** — `grep -v '^\s*//'` on each leaves 211 identical lines, and the
diff between them is empty. So nothing is broken today, which is exactly why this is dangerous.

Both are live, and different consumers get different files:

| TU | Resolves to | Why |
|---|---|---|
| `src/PlayerProfile.cpp` | **`src/Serialization.hpp`** | quoted includes search the includer's own directory first — proven with `g++ -std=c++17 -I include -M src/PlayerProfile.cpp` |
| `tests/test_profile_settings.cpp` | **`include/Serialization.hpp`** | `tests/` has no `Serialization.hpp`, so it falls through to `-I include` |
| `tests/test_game_rules.cpp` | **`include/Serialization.hpp`** | same |
| `tests/test_achievements.cpp` | **`include/Serialization.hpp`** | same |

**This is worse than the first draft of this entry claimed.** The original table listed only
`test_achievements.cpp` as an `include/`-copy consumer, which made the hazard sound hypothetical. In fact the two
tests that lock the save schema — `test_profile_settings.cpp` (B12, B13, B14 all add cases here) and
`test_game_rules.cpp` (B10) — read the copy the shipped binary **never compiles**. `src/PlayerProfile.cpp` is the
only TU that puts this schema into the game, and it reads the other one.

The shipped game and the test suite are therefore compiling **different copies of the save schema**. The moment
one copy is edited, a regression test can pass while the shipped binary is unchanged — or the binary changes and
the test that is supposed to protect it never sees it. For B13 specifically: editing `include/Serialization.hpp`
alone makes the new test green and leaves the bug fully intact in the game.

**Fix.**
1. Delete `src/Serialization.hpp`; keep `include/Serialization.hpp` (it is on the public include path and carries
   the better comments).
2. ~~Remove `src/Serialization.hpp` from both `add_executable` source lists in `CMakeLists.txt`.~~ **No CMake edit
   is needed** — this step was wrong in the first draft. `Serialization.hpp` appears **nowhere** in
   `CMakeLists.txt`: the game's sources are listed in `target_sources(Blackjack2 PRIVATE …)`
   (`CMakeLists.txt:80`) and no headers are listed there or in `add_executable(Blackjack2Tests …)`
   (`CMakeLists.txt:132`). Confirmed with `grep -n 'Serialization.hpp' CMakeLists.txt` → no matches. Deleting the
   file is the whole build-side change.
3. Verify every consumer now resolves to the same file:
   `for f in src/*.cpp tests/*.cpp; do g++ -std=c++17 -I include -I tests -I /usr/local/include -M "$f" 2>/dev/null | tr ' ' '\n' | grep -H 'Serialization.hpp' && echo "  ^ $f"; done`

   Two ways this sweep lies. Both were hit for real on 2026-11-06:

   - **It silently reports nothing for the Catch2 TUs** unless the generated `catch2/catch_user_config.hpp`
     include dir is added — Catch2 lives in `_deps/catch2-src/src` **and** `_deps/catch2-build/src`. A TU that
     fails to preprocess looks exactly like a TU with no such dependency, so drop `2>/dev/null` at least once and
     read the stderr.
   - **Do not "speed it up" by replaying commands from `compile_commands.json` with `-M` appended.** Those
     commands end in `-o <obj> -c <src>`, and in dependency mode GCC writes the dependency list to the `-o`
     target — overwriting the `.o` with text. The build then fails to link with *"file format not recognized;
     treating as linker script"*, pointing the finger at your last edit rather than at the sweep. If it happens,
     find the non-ELF objects and delete them:
     `python3 -c "import os;[print(p) for r,_,fs in os.walk('CMakeFiles') for f in fs if f.endswith('.o') for p in [os.path.join(r,f)] if open(p,'rb').read(4)!=b'\x7fELF']"`
4. Add a cheap guard so it cannot recur — a test or CI step asserting
   `find . -name Serialization.hpp -not -path './out/*' | wc -l` equals 1.

**Sanity check after the merge:** the B13 regression test must **fail** before the `currentHistory` fix and pass
after. If it passes immediately, you edited the copy the tests weren't using.

**Done when.** Exactly one `Serialization.hpp` exists and every TU resolves to it.

---

# PHASE 2 — Crash diagnostics (B9)

## B9 — One unreproducible crash, and a logger that hides it 🟠

**What happened.** In a fresh profile with 3 seats, after a round containing a side bet and an achievement
unlock, pressing `R` (next round) killed the process — window and process both gone, **zero** output. Three
replay attempts of the same sequence did not reproduce it, and an automated 60-cycle auto-play loop ran clean.
So: real, but non-deterministic.

**Why it is currently undebuggable.** `Logger::log()` writes to `std::cout` (`src/Logger.cpp`). When stdout is
redirected to a file it is block-buffered, so a crash discards the whole buffer — the post-mortem log contained
only two SFML startup warnings. Mid-run logs also truncate mid-line at buffer boundaries, which briefly led me to
suspect a settlement bug that did not exist (see CONTEXT.md §6.9).

**Fix (do this before hunting the crash).**
1. Flush per record: add `std::cout << std::unitbuf;` at logger init, or `std::flush` after each line. Cheap, and
   correct for a desktop game.
2. Optionally mirror to a log file under the existing save dir (`~/.local/share/blackjack/`), so a crash leaves a
   trail even when launched from a desktop icon with no console.
3. Add a `Game::run()` top-level `try/catch` (only `main.cpp` catches today) and log the round state on exit.

**Then hunt.** With flushing in place, run an ASan build through a long randomized session (AutoPlay speed is an
excellent fuzzer — it plays every seat on basic strategy). Prime suspects for a `R`-time crash with multiple
seats + side bets: the reveal/animation clock state in `GameRenderer::advanceRevealClock` (0.3 §6 #22 was a bug
in exactly this area), and `drawSummaryOverlay()` indexing `game.players[i]` while iterating
`hudState.playerSummaries.size()` (`src/GameRenderer.cpp:1020-1023`) — if those two ever differ in length that is
an out-of-bounds read. Add a bounds guard there regardless; it is free.

**Done when.** A crash leaves a complete log, and the summary-overlay indexing is provably in-bounds.

---

# PHASE 3 — Config completeness (B10–B12)

## B10 — `maxSplitHands` is dead config 🟡

`GameRules` declares `int maxSplitHands = Constants::MAX_SPLIT_HANDS;` (`include/GameRules.hpp:36`) and
`Game::split()` enforces it (`src/Game.cpp:777`), but:
- `gameRuleRows()` (`src/GameRules.cpp:15-27`) exposes only 6 rows — there is **no UI row**, and
  `applyGameRuleEdit()` has no case for it.
- `to_json(const GameRules&)` **omits it**, so it could never round-trip even if it were editable.

So it is a rule the player can never reach. Either expose it or delete it.

**Fix (expose it).** Add `GameRuleId::MaxSplitHands`, a `{"Max Split Hands"}` row, a description, and an
`applyGameRuleEdit` case stepping a clamped range (2–4, mirroring the `DECK_BRACKETS` idiom). Add
`{"maxSplitHands", r.maxSplitHands}` to `to_json` / `from_json` with the struct default as the fallback so legacy
profiles still load.

**Test.** `tests/test_game_rules.cpp`: round-trip a non-default value; assert a legacy profile with no key loads
the default; assert `split()` refuses at the configured cap.

## B11 — Startup control banner mislabels the keys 🟡

`src/Game.cpp:162-163` prints:

```
  <-  Right Arrow   Auto Kelly Bet
  ->            Confirm Bet
```

The real bindings are **Left = Auto Kelly, Right = Confirm** (`src/InputHandler.cpp:591-602`). The glyph and the
label disagree, and Confirm has no glyph. Also `P`/`Y` are indented 6 spaces off the H/S/D column, and the banner
omits side bets (Z/X/V), insurance (Up/Down/Left/Right), advice (I), pause (Escape) and the count-style click.

**Fix.** Correct the two lines and complete the table. Then decide the banner's future: a GUI player never sees a
console. Recommended — keep a trimmed console banner for developers and move the authoritative control list
in-game as a Controls screen reachable from the main menu. That closes this permanently.

**Test.** Low headless value; if you extract the string, assert it does not contain `"Right Arrow   Auto Kelly"`.

## B12 — Save-schema typo: `resplitTentsAllowed` 🟡

The field, the JSON key and `GameRuleId::ResplitTents` all spell **"Tents"** for "Tens"
(`include/GameRules.hpp:34/54/66`; `src/GameRules.cpp:22/41/82`). The UI label is correct ("Resplit Tens"), so it
is invisible to players but baked into every save file, and gets more expensive to fix over time.

**Fix (additive, honours the 0.3 schema contract).** Write the new key `resplitTensAllowed` and read **both** on
load, new key winning:

```cpp
r.resplitTentsAllowed = j.value("resplitTensAllowed",
                               j.value("resplitTentsAllowed", r.resplitTentsAllowed));
```

Rename the C++ identifier and enum throughout. Old profiles keep loading; new profiles carry the correct key.

**Test.** `tests/test_profile_settings.cpp`: a legacy profile with only `resplitTentsAllowed` still loads; a new
profile writes `resplitTensAllowed`; both parse to the same value.

---

# PHASE 4 — UI / UX (player-facing polish)

Ordered by how quickly a new player hits each one. For each, prefer extracting the string/layout decision into a
pure helper so it becomes headless-testable.

**U1 — Two rows look selected at once.** Hover and keyboard-selection both render gold; hover only adds bold
(`GameRenderer.cpp:1034-1046`). Observed on the main menu with "Play Game" and "Achievements" both gold — it is
genuinely ambiguous whether Enter starts a game or opens Achievements. *Fix:* give hover a distinct treatment
(background pill or a `›` marker) and reserve gold for keyboard selection.

**U2 — Pause menu has no panel.** Bare text over the felt; "Quit to Menu" collides with the seat-name labels
(`GameRenderer.cpp:1274`). *Fix:* a centred rounded rect with padding behind the items (reuse the side-panel
style), and suppress the play-phase hint strip while paused.

**U3 — Two screens do not reflow.** Achievements (`GameRenderer.cpp:108-141`) and Game Rules (`:1236`) use
hard-coded pixel offsets; at 900×560 the footer hint overlaps list rows. The **table reflows correctly**, so copy
that pattern: derive start-Y and row spacing from `window.getSize()`. Add a `*ForTest()` layout accessor and
assert no rect overlap at both 1600×900 and 900×560.

**U4 — Hint clutter.** Five hint strings in three rows at ~10px, in two competing formats
(`Up/Down: set bet` vs `[Up/Down | Adjust Bet]`), and every action is shown even when irrelevant — H/S/D/P/Y are
displayed during PlaceBets. *Fix:* show only the current `RoundState`'s controls, one format, legible size; move
the rest to the Controls screen from B11.

**U5 — "True Count" shows the running count under Reko** (`Game.cpp:483`). Deliberate, but a fresh 6-deck shoe
displaying `True Count -20.0` reads as broken, and a learner has no way to know it is intended. *Fix:* relabel the
box to "Betting Count (Reko)", or show RC and TC side by side.

**U6 — Card overlap and total legibility.** With 3+ cards the lower cards are almost fully covered, and hand
totals are small white text drawn straight onto the felt arc. *Fix:* increase the per-card offset; draw totals in
a small filled pill with a dark backing (`drawTotal`, `GameRenderer.cpp:468`).

**U7 — The human seat is hard-coded to "William"** (`SessionLifecycle.cpp:35` and `:182`); there is no name entry
anywhere and the profile's `name` field ships as `""`. Ship-blocking for anything public. *Fix:* add a menu name
field backed by the existing profile field.

**U8 — 10× bankroll mismatch.** The human starts at **$1,000** (`PlayerProfile.hpp:66`) while NPCs start at
**$10,000** (`Constants.hpp:270` `STARTING_BANKROLL`) — visible side by side on the table, and `MAXIMUM_BET` is
1000, i.e. the human's entire stack. *Fix:* reconcile to one number; if $1,000 is intended, drop the NPC stacks.

**U9 — No side-bet paytable anywhere.** Perfect Pairs (10/25/30:1) and 21+3 (2/5/9/30/40:1) are offered with no
odds shown. *Fix:* render the active bet's paytable in the right panel during PlaceBets.

**U10 — Minor.** "1 decks" pluralisation (`GameRules.cpp:33`); branding drift (window "Blackjack" / menu
"BLACKJACK" / felt "QUANTUM BLACKJACK" / project "Blackjack2"); NPC illegal-action WARNs such as
`Alice tried to double down but hand cannot double` — NPC logic should pre-check preconditions so the log stays
clean.

---

# PHASE 5 — Tech debt (T1–T2)

**T1 — The profile is rewritten to disk every round.** `HandEvaluator::evaluateHands()` ends with
`players[0].saveProfile()` (`src/HandEvaluator.cpp:297`), observed as a full JSON serialise + write every 6–8
seconds. Correct today, but O(total history) per round. *Fix:* debounce — write at round end only if dirty and at
most every N seconds — while keeping the forced writes at shoe boundary, pause-quit and exit.

**T2 — History grows without bound.** `longHistory` accumulates every shoe forever and is re-serialised on every
save, compounding T1. *Fix:* cap it (last N shoes) or roll older shoes into aggregate counters, keeping the schema
additive.

**B16 — `reconstructDeckAtRound()` is dead code, and wrong.** 🟡
`include/GameRecord.hpp:307-329` is the only occurrence of the symbol in the whole tree — no caller, no test. It
rebuilds a shoe by popping the **front** of a deque built from `initialDeck`, but `Deck::draw()`
(`src/Deck.cpp:121-135`) deals from the **back** (`cards.back()` / `pop_back()`), and `initialDeck` is stored as
`deck.getCards()` verbatim (`src/Game.cpp:276`, `:1349`; `src/SessionLifecycle.cpp:55`, `:198`). So it removes
cards that have *not* been dealt. It also ignores the cut card's extra draw and mid-shoe reshuffles.

No runtime impact today because nothing calls it. It is a landmine for anyone building replay analysis, decision
forking, or "what if" features — it looks like the right tool and is not. *Fix:* delete it, or rewrite it to pop
from the back, account for the cut card, and cover it with a test that reconstructs a known shoe.

---

# Testing strategy — close the seam that let all this through

The suite is **264/264 green** while 15 logic/data defects (B1–B15) exist. Every one sits in the presentation /
lifecycle seam that the headless-first contract deliberately leaves untested. Two cheap additions would have
caught the top three:

1. **Persistence round-trip integration test.** Play → save → enter mode → save → reload → assert invariants.
   Catches B1 and B7. Belongs in `tests/test_session_lifecycle.cpp`.
2. **Renderer-string extraction.** Move display strings out of `draw*()` into pure helpers and assert on the
   strings. Catches B3, and makes U4/U5/U9/U10 testable. This is the pattern the project already uses for
   `gameRuleRows()` / `mainMenuItems()` — one source of truth shared by draw and test.

Also worth adding: a per-screen layout-overlap assertion (catches U3), and a sweep for "state field written but
never cleared" (catches B2 — grep every `hudState.` field for a reset path).

---

# Definition of done (phase 0.4)

- [ ] B1–B4 and B13 fixed, each with a named regression test that fails without the fix.
- [ ] B5–B8, B14 and B15 fixed, or explicitly deferred with a written rationale in CONTEXT.md §5.
      (B15 must land **before** B12/B13 — they edit the header it duplicates.)
- [ ] B9: logger flushes per record; summary-overlay indexing guarded.
- [ ] B10–B12 resolved; legacy profiles verified to still load.
- [ ] U1, U2, U3, U7 addressed — the four a new player hits within five minutes.
- [ ] Persistence round-trip + renderer-string extraction tests exist.
- [ ] Full suite green with a **higher** test count than the 264 baseline.
- [ ] `.summaries/0.4/CONTEXT.md` §11 status table and §10 decisions updated before handoff.

# Explicitly out of scope

- **No new features.** Feature 10 (Multiplayer) and Feature 11 (Casino Simulation) stay Planned.
- **Do not revive Feature 6** (KO / Wong Halves / Omega II counting). Still frozen per 0.3 §6 #1.
- **Do not widen `GameRules`** beyond exposing `maxSplitHands`; keep the narrowly-scoped `const GameRules&`
  threading (0.3 §6 #2).
- **Do not change the `AppMode` enum** except by adding a value with its own state machine — never ad-hoc booleans.
- **Do not break the additive JSON contract** — every new field optional, with a default.
- **No feature work from `.summaries/0.4/IDEAS.md`.** That document is an unscheduled menu of propositions. It is
  not a plan, it is not estimated as commitment, and nothing in it is in scope for this phase. Its only legitimate
  influence on 0.4 is the note at its end: build the shared "this game does not persist progress" gate as part of
  the B1 fix, because five of those ideas depend on it and it costs nothing to do now.

---

# Document audit log

**2026-11-06 — audited against `96fd416` (branch `3.1`).** Baseline re-measured at **264/264 in 8.15 s**; every
`file:line` citation in this ledger re-checked and correct; B13's standalone proof re-run and reproducing.

Four structural claims did **not** survive the audit and are fixed above: the "working tree is not clean" warning
(stale — that work landed as `96fd416`); B1's "three save sites" (there are six, and two line numbers were wrong);
B15's instruction to edit `CMakeLists.txt` (that file never mentioned `Serialization.hpp`); and B15's consumer
table, which named one `include/`-copy test where there are three — including the two that lock the save schema.
The verification gate also dropped its no-op `ASAN_OPTIONS` prefix.

The detailed changelog, and the list of claims confirmed unchanged, live in **CONTEXT.md §13**. Update both files
together; they contradict each other more cheaply than either contradicts the code.
