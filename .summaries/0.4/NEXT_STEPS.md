# NEXT STEPS — Blackjack2 (Phase 0.4: QA hardening)

**Purpose:** Implementation-ready bug ledger + sequenced plan for the next context window. Every item below was
found by *playing the actual game* (headless Xvfb + synthetic X11 input, 139 screenshots) and was traced to a
specific file:line. Companion document: `.summaries/0.4/CONTEXT.md` — read it first for methodology, evidence,
architecture and guardrails.

**Posture change from 0.3:** feature work stays frozen. This phase is defect removal + the missing test seam.
The suite is **303/303 green**. Every defect (B1–B16) has a dedicated fix commit + passing regression test — see the
Status column below for each commit hash and test ID. The only genuinely still-OPEN work now is B9's unreproducible
`R`-time crash (never reproducibly captured, so no deterministic test can close it) plus the UI/UX backlog (U1–U12), of
which U4, U5, U7, U8, U9 and U10 are Resolved, and **U1, U2, U3, U6 and U11 were Resolved on 2026-11-09 and verified on screen with an Xvfb screenshot harness**. **U12** (side-panel graph toggles + session view + bet-axis bounds) was the single remaining UI item; it was **Resolved on 2026-11-12** once §10 #8/#9 were decided, so the UI/UX backlog U1–U13 is **closed**. That gap — everything
remaining lives in the presentation/lifecycle seam the headless-first contract
deliberately leaves untested — is still the theme of this phase; see §11 for the full open list.

> **Ledger synced 2026-11-08.** The triage table below previously listed B1, B2, B3, B4, B7, B13, B14, B15 (and B5,
> B8) as Open; all now carry dedicated fix commits and passing tests. U1–U10 remain the true open list. Keep §11 in
> CONTEXT.md in sync with this table.
>
> **Ledger synced 2026-11-09.** U7, U8 and U10 now Resolved — bankroll parity (#70), deck pluralisation + menu-branding
> consolidation + NPC illegal-action-warning suppression (#281/#282). Open UI were then U1–U6 and U9; U4/U5/U9 now also
> Resolved (f8f15cc), plus two new vision-critical items U11, U12 — the current open set is in the note below.
> **2026-11-09 (U-series): U1, U2, U3, U6, U11 Resolved**, nine new tests #317–#325, suite 316 → 325/325, 10-run -j8 soak 10 clean / 0 failing.
>
> **Ledger synced 2026-11-09.** U4/U5/U9 also Resolved (`f8f15cc`; betting-hint consolidation #307–#308, count-box label
> #309–#311, side-bet paytable #312–#315) — fixed+committed but not yet marked here. Two new vision-critical backlog items
> added this pass: U11 (window-resize menu hover/click drift on main + pause menus), U12 (side-panel graph toggles +
> per-shoe/session view switch + Bets Placed axis 10–100 capped at MAXIMUM_BET). Open UI now: U1, U2, U3, U6, U11, U12.
> **Superseded 2026-11-09: open UI is now only U12.** U1/U2/U3/U6/U11 are Resolved; the screenshot harness (`/tmp/bj_visual.py`, `/tmp/bj_u6.py`, `/tmp/bj_menu.py`) closed the "not headless-testable" gap and is what verified them.

> **Handoff prompt moved to `handoff_prompt.md`. Paste that file into a fresh window; it is regenerated from the same audit log (CONTEXT §13) as this document.**

---

## Verification gate (run after EVERY fix)

> **Hermeticity check (added 2026-11-09, B17 follow-up).** Capture `md5sum ~/.local/share/blackjack/saves/player_profile.json`
> **before** the suite runs and **after** it, against a baseline taken outside the suite. "md5 stable across a soak" is not
> sufficient: a soak compares runs to each other, so a writer that emits identical content every run looks stable from
> run 1 onward — that is exactly how B17's writer survived. If it moves, bisect it: md5, `ctest -I i,i`, md5, for each
> index. Process-scope isolation lives in `tests/test_process_isolation.cpp`; a new test never needs its own guard.

```bash
cmake --preset "GCC 13.3.0 x86_64-linux-gnu"
cmake --build 'out/build/GCC 13.3.0 x86_64-linux-gnu' --parallel
ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu' -j8 --output-on-failure
```

Baseline: **316/316 pass**, HEAD `7a19af3` + this round's work. Configure and build are clean — warnings only (the
pre-existing `=` implicit-`this` deprecation in `GameRenderer.cpp`'s `modeTable()` lambdas). Never commit with a lower
count than you started.

> **A single `-j8` run proves almost nothing. Run the soak.** Until B17 (2026-11-09) the suite was **not hermetic**:
> five test files redirected `HOME` to a *fixed* `/tmp` directory, and ctest + Catch2 run each `TEST_CASE` in its own
> process, so concurrently-scheduled tests shared one profile directory. The documented "315/315 green (`-j8`)" was
> **false** — measured 3 of 4 runs, and 3 of 10 in a soak, had ≥1 failure (#290, #259, #306, #169). Serial was always
> green, which is exactly why nobody noticed. So gate like this:
>
> ```bash
> # 1) serial is the authoritative correctness gate
> ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu' -j1 --output-on-failure
> # 2) 10x -j8 is the hermeticity gate: a flake is a DEFECT, not noise
> for i in $(seq 1 10); do ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu' -j8 > /tmp/soak_$i.log 2>&1 \
>     || echo "run $i FAILED: $(grep -oE '[0-9]+ - .*\(Failed\)' /tmp/soak_$i.log)"; done
> ```
>
> Post-B17/B18 the soak is **12 clean / 0 failing**. Any new fixed-name temp dir, unseeded `Deck::shuffle()`, or
> statistical assertion over an unseeded shoe re-introduces the flake — use `test_support::IsolatedHome`,
> `Deck::shuffleWithSeed()` / `Game::startNewShoeWithSeedForTest()` / `StrategyTrainer::startSessionWithSeedForTest()`,
> and assert against quantities the code actually counts. See CONTEXT.md §13 (2026-11-09) and §11's B17/B18/B19 rows.
>
> **Never rebuild while a soak is running.** `cmake --build` during a `-j8` run replaces the test binary under the
> runner and ctest reports **`BAD_COMMAND`** for a large slice of the suite at once (observed: 106 of 316 in 7.5s).
> That is the runner race CONTEXT §13 records, not a regression — build once, up front, then soak.

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

| ID | Sev | Title | Primary site | Phase | Status |
|----|-----|-------|--------------|-------|--------|
| B1 | 🔴 Critical | Tournament mode overwrites the saved career profile | `src/SessionLifecycle.cpp:35` | 0 | ✅ Resolved `5107aa7` (#230) |
| B2 | 🔴 Critical | Achievement toast never clears (permanent "NEW!" banner) | `src/AchievementFeeder.cpp:48` / `src/GameRenderer.cpp:154` | 0 | ✅ Resolved `f3aeb1f` (#215) |
| B3 | 🔴 Critical | Side-bet result always prints "WON" (LOST branch is dead) | `src/GameRenderer.cpp:997-1000` | 0 | ✅ Resolved `c7978f1` (#147) |
| B4 | 🔴 Critical | Reko count seeded with compile-time deck count, not the rule | `src/Game.cpp:277` (+ `src/Deck.cpp:73`) | 0 | ✅ Resolved `3924b56` (#80/#81) |
| B13 | 🔴 Critical | Quitting mid-shoe silently deletes those hands from career stats | `include/Serialization.hpp:193-210` + `src/PlayerProfile.cpp:68` | 0 | ✅ Resolved `387a3e0` (#279) |
| B5 | 🟠 High | Main bet never escrowed → bankroll can go negative | `src/Player.cpp:89` | 1 | ✅ Resolved `4f85d37` (#113–115) |
| B6 | 🟠 High | Rules panel keyboard edit is unthrottled (multi-step per tap) | `src/Game.cpp:418-433` | 1 | ✅ Resolved `584aa4b` (#291) |
| B7 | 🟠 High | Escape-to-quit from the main menu skips the profile save | `src/Game.cpp:189` + `src/InputHandler.cpp:251-258` | 1 | ✅ Resolved `11b187b` (#290) |
| B8 | 🟠 High | `Deck::draw()` has no empty-shoe guard (pre-existing, known) | `src/Deck.cpp:123-137` | 1 | ✅ Resolved `11f6f50` (#13/#14) |
| B14 | 🟠 High | Save is non-atomic and a failed load silently resets to a fresh profile | `src/PlayerProfile.cpp:50-61` + `:28-48` | 1 | ✅ Resolved `9d1d125` (#238/#239) |
| B15 | 🟠 High | Two live copies of `Serialization.hpp`; game and tests compile different files | `src/Serialization.hpp` vs `include/Serialization.hpp` | 1 | ✅ Resolved `9a89185` (#240) |
| B9 | 🟠 High | One unreproducible crash; logger loses all output on abort | `src/Logger.cpp` (stdout buffering) | 2 | ⚠️ Partly: flush + overlay bounds fixed (`d6d6436`,`facea14`, #292–294,#296/#297); unreproducible `R`-time crash still open |
| B10 | 🟡 Medium | `maxSplitHands` is dead config (no UI row, not serialized) | `src/GameRules.cpp:15-27` / `include/GameRules.hpp` `to_json` | 3 | ✅ Resolved `69cd9e8` (#280/#282) |
| B11 | 🟡 Medium | Startup control banner mislabels Left/Right | `src/Game.cpp:162-163` | 3 | ✅ Resolved `140e9b5` (#295) |
| B12 | 🟡 Medium | Save-schema typo `resplitTentsAllowed` ("Tens") | `include/GameRules.hpp` | 3 | ✅ Resolved `31a3703` (#241–243) |
| U1–U12 | 🟡 Medium | UI/UX defects (see Phase 4) | various | 4 | ✅ Resolved this phase: U4 (#307–#308), U5 (#309–#311), U7 (#287 + #289–#291), U8 (#70), U9 (#312–#315), U10 (#282/#283), **U1 (#317), U2 (#318), U3 (#319–#320), U6 (#321–#322), U11 (#323–#325)** — the last five verified on screen with the Xvfb harness — and **U12 (#301–#304, #175/#176, #241–#244) on 2026-11-12**, closing the UI set. **U13** (non-ASCII → SFML mojibake, found by screenshot) #326 Resolved. **Open UI: none.** |
| T1–T2 | 🟢 Low | Save-write amplification; unbounded history | `src/HandEvaluator.cpp:297` | 5 | ✅ Resolved `1abda8f`,`fbaea91` (#302/#303 / #298) |
| B16 | 🟡 Medium | `reconstructDeckAtRound()` is dead code and reconstructs from the wrong end | `include/GameRecord.hpp:307-329` | 5 | ✅ Resolved `5f81d48` (dead-code deletion; no dedicated regression test) |
| B20 | 🟡 Medium | Test **#175** flaked under `ctest -j8`: `startNewShoeForTest()` → `startNewShoe()` discards the test's seeded deck and shuffles from the **OS RNG**, so post-boundary rounds varied per process | `tests/test_analytics.cpp` (U12 session-accumulator case) | 2 | ✅ Resolved 2026-11-12 — B18 pattern: re-seed the deterministic deck after the shoe boundary. Measured **12/120 → 0/120** over 120 single-test runs; baseline confirmed pre-existing on a clean `HEAD` worktree. Test-only |

**Every "Primary site" above was re-checked against `96fd416` on 2026-11-06 and is correct**, as is every
`file:line` citation in the body of this document — the citations are the reliable part of these docs. What the
audit did find were errors in the *structural* claims around those citations: a stale dirty-tree warning, B1
naming three save sites where there are six, B15 prescribing a CMake edit for a file CMake never referenced, and
B15 understating which translation units read which header. Those are corrected in place; the running list is in
CONTEXT.md §13.

**Not a bug (investigated and cleared):** `handActions` flat-indexing looks wrong when `split()`
(`src/Game.cpp:810`) appends at the end, but is safe because seats act strictly in order, so later slots are still
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
profile, and the window-close handler (`src/InputHandler.cpp:251-258`) saves `players[0].getProfile()` over the
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
| `src/InputHandler.cpp:108` | pause → **Restart** | the draft mislabelled this one as Quit-to-Menu |
| `src/InputHandler.cpp:123` | pause → **QuitToMenu** | |
| `src/InputHandler.cpp:258` | **window close** (`Event::Closed`) | the B1 repro route; draft said `:252` |
| `src/Game.cpp:1386` | shoe boundary | |
| `src/Game.cpp:1545` | shoe boundary (second path) | |
| `src/SessionLifecycle.cpp:293` | shoe/lifecycle boundary | **missing from the original six; added 2026-11-06 — an unguarded saveProfile that B1's tournament-time gate must also cover** |

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
`syncBettingAndSideBet()` copies it faithfully (`src/Game.cpp:485`).

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

**Root cause.** `src/Game.cpp:277`:

```cpp
const int rekoSeed = BasicStrategy::rekoStartingOffset(Constants::NUM_DECKS);
```

`Constants::NUM_DECKS` is the compile-time 6, so the seed is always -20 regardless of `gameRules_.numDecks`.
Error by shoe: 1 deck off by 19 (-20 vs -1), 2 off by 15, 4 off by 8, 8 off by 7 (-20 vs -27). Because Reko's
"True Count" *is* the running count (`src/Game.cpp:1012`), this corrupts the True Count readout, Edge % and
Kelly / Auto-Kelly sizing from the first hand.

**Same defect, second site.** `Deck::reshuffleWithRemaining()` (`src/Deck.cpp:73`) rebuilds with
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
is called at the shoe boundary (`src/Game.cpp:1384`, `:1543`) and on **Quit to Menu** (`src/InputHandler.cpp:106`,
`:119`) — but **not** in the window-close handler (`src/InputHandler.cpp:251-258`), which jumps straight to
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
- Call the existing shoe-flush in the window-close handler (`src/InputHandler.cpp:251-258`) so it matches the
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
2. **Over-commitment.** `Game::placeSideBet()` (`src/Game.cpp:1174`, affordability check at `:1180`) checks
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

**Root cause (pre-fix).** The polling block was at `src/Game.cpp:383-388`; it polled held keys every frame with no edge detection. **Fixed** — the rules case is now edge-gated (`&& !rulesRightPrev_` / `&& !rulesLeftPrev_`) at `src/Game.cpp:420-433`.

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
(`src/InputHandler.cpp:251-258`). Clicking the title-bar X queues a Closed event that *does* get polled, so that
route saves. But main-menu Escape calls `game.window.close()` directly (`src/InputHandler.cpp:524-525`); on the
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

`src/Deck.cpp:123-137` calls `cards.back()` / `pop_back()` with no size check, and a **second** unguarded
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
an out-of-bounds read. A bounds guard was added (`facea14` / `tests/test_summary_overlay_bounds.cpp`, #296/#297): the loop now iterates to `min(playerSummaries.size(), players.size())` via the shared `summaryOverlayRowCount()` helper and the fan-width uses `summaryRowFanWidth()`, so both halves are provably in-bounds.

**Done when.** A crash leaves a complete log (flush fixed `d6d6436`, #284/#293/#294) **and** the summary-overlay indexing is provably in-bounds (`facea14`, #296/#297). Only the original non-deterministic `R`-time crash remains unaddressed — it has never been reproducibly captured, so no deterministic test can close it; see CONTEXT.md §13.

---

# PHASE 3 — Config completeness (B10–B12)

## B10 — `maxSplitHands` is dead config 🟡

`GameRules` declares `int maxSplitHands = Constants::MAX_SPLIT_HANDS;` (`include/GameRules.hpp:36`) and
`Game::split()` enforces it (`src/Game.cpp:821`), but:
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
**Status — RESOLVED 2026-11-09:** root cause was two layers deep. `InputHandler::processMenuEvents` called `applyMenuRowSelection()` on `MouseMoved`, so hovering **moved the keyboard selection**; `drawMenuItem` then rendered `hovered || selected` as the same gold (bold-only difference). Hover is now purely visual — background pill + leading marker, never mutating `menuSelection` — gold is reserved for the keyboard selection, and a click activates. Marker is ASCII `>`, **not** `›`: the shipped `NotoSans-Regular.ttf` has no U+203A glyph and rendered a tofu box (caught on screenshot). Verified on screen: gold "Review History" (selected) and pill+marker "Achievements" (hovered) simultaneously. Regression #317.

**U2 — Pause menu has no panel.** Bare text over the felt; "Quit to Menu" collides with the seat-name labels
(`GameRenderer.cpp:1274`). *Fix:* a centred rounded rect with padding behind the items (reuse the side-panel
style), and suppress the play-phase hint strip while paused.
**Status — RESOLVED 2026-11-09:** `drawPauseMenu` painted a full-window alpha-128 overlay with no panel, so "Quit to Menu" collided with the seat-name label over the live table. Added a centred panel (fill + outline, sized from the row bounds with padding) behind title, rows and hint; `RectangleShape::setRadius` is not usable here under SFML 3.1, so it is a plain fill + outline. Verified at 1600×900 and 900×560 — the "William" label is behind the panel, not under the text. Regression #318.

**U3 — Two screens do not reflow.** Achievements (`GameRenderer.cpp:108-141`) and Game Rules (`:1236`) use
hard-coded pixel offsets; at 900×560 the footer hint overlaps list rows. The **table reflows correctly**, so copy
that pattern: derive start-Y and row spacing from `window.getSize()`. Add a `*ForTest()` layout accessor and
assert no rect overlap at both 1600×900 and 900×560.
**Status — RESOLVED 2026-11-09:** Achievements and Game Rules now derive their geometry from `GameRenderer::worldSizeOf(game)` instead of hard-coded offsets, so the list can no longer run under the footer at 900×560. Note for the record: `drawGameRules`'s `startY = h*0.30f` was already fractional and its 7 rows **do** fit at 900×560 — **Achievements is the screen that provably overlaps** (`y=96`, `rowH=60`, footer at `h-28`). Verified on screen at both sizes. Regressions #319–#320.

**U4 — Hint clutter.** Five hint strings in three rows at ~10px, in two competing formats
(`Up/Down: set bet` vs `[Up/Down | Adjust Bet]`), and every action is shown even when irrelevant — H/S/D/P/Y are
displayed during PlaceBets. *Fix:* show only the current `RoundState`'s controls, one format, legible size; move
the rest to the Controls screen from B11.
**Status — RESOLVED 2026-11-09 (`f8f15cc`):** betting-phase hints consolidated into single-source `Game::bettingControlsHint()`/`sideBetControlsHint()`, rendered in one consistent format (13.f) and never showing irrelevant actions during PlaceBets. Regression #307–#308.

**U5 — "True Count" shows the running count under Reko** (`Game.cpp:1012`). Deliberate, but a fresh 6-deck shoe
displaying `True Count -20.0` reads as broken, and a learner has no way to know it is intended. *Fix:* relabel the
box to "Betting Count (Reko)", or show RC and TC side by side.
**Status — RESOLVED 2026-11-09 (`f8f15cc`):** `Game::trueCountStatusLabel()` returns "True Count" (Hi-Lo) or "Betting Count (Reko)" (Reko); HUDState.countingBoxLabel syncs each frame and HUD.cpp applies it via StatBox::setTitle. Regression #309–#311.

**U6 — Card overlap and total legibility.** With 3+ cards the lower cards are almost fully covered, and hand
totals are small white text drawn straight onto the felt arc. *Fix:* increase the per-card offset; draw totals in
a small filled pill with a dark backing (`drawTotal`, `GameRenderer.cpp:468`).
**Status — RESOLVED 2026-11-09:** cards advanced 16px, so 3+ card hands showed only a sliver of each card; introduced `cardFanCenterShift` (wider step, fan re-centred on the seat) and a dark pill behind every hand total. Verified on screen with a **7-card hand**: the fan is centred on the chip/label and the dealer **7** / player **28** totals stay legible over the felt arc text. Regressions #321–#322.

**U7 — The human seat is hard-coded to "William"** (`SessionLifecycle.cpp:35` and `:182`); there is no name entry
anywhere and the profile's `name` field ships as `""`. Ship-blocking for anything public. *Status — RESOLVED 2026-11-07 (`e157052`): added the automatic "enter your name before hitting main menu" screen; auto-prompt fires when profile.name is empty (Game::needsNameEntry gate at run()), editing goes through a clamped Game `nameEntry*` buffer, commitPlayerName persists seat 0's display + profile, and Player Stats reuses Statistics→StatsDashboard. Headless tests #286–#288.*

**U8 — 10× bankroll mismatch.** The human starts at **$1,000** (`PlayerProfile.hpp:72`) while NPCs start at
**$10,000** (`Constants.hpp:285` `STARTING_BANKROLL`) — visible side by side on the table, and `MAXIMUM_BET` is
1000, i.e. the human's entire stack. *Fix:* reconcile to one number; if $1,000 is intended, drop the NPC stacks.
**Status — RESOLVED 2026-11-09 (`63c0ee9`):** the Player load ctor now reseeds a brand-new / recovered profile's
in-memory bankroll to `GameConfig::STARTING_BANKROLL` (only the bankroll; `profile.name` stays empty so U7's
`needsNameEntry()` prompt is preserved). Headless regression #70 in tests/test_player.cpp.

**U9 — No side-bet paytable anywhere.** Perfect Pairs (10/25/30:1) and 21+3 (2/5/9/30/40:1) are offered with no
odds shown. *Fix:* render the active bet's paytable in the right panel during PlaceBets.
**Status — RESOLVED 2026-11-09 (`f8f15cc`):** `Side BetEvaluator::paytableText()` is the single source for the odds; drawPlaySession renders it during PlaceBets when a non-None kind is active via `Game::activeSideBetKind()`. Regression #312–#315.

**U10 — Minor.** "1 decks" pluralisation (`GameRules.cpp:34`); branding drift (window "Blackjack" / menu
"BLACKJACK" / felt "QUANTUM BLACKJACK" / project "Blackjack2"); NPC illegal-action WARNs such as
`Alice tried to double down but hand cannot double` — NPC logic should pre-check preconditions so the log stays
clean.
**Status — RESOLVED 2026-11-09 (`63c0ee9`):** (a) pluralisation corrected in `gameRuleDescription`, regression #281;
(b) the menu banner now renders from the single canonical `Constants::WINDOW_TITLE`, so it can't drift from the OS window
title — felt "QUANTUM BLACKJACK" and the repo name "Blackjack2" are distinct contexts (table art vs project name) and
were left as-is; (c) action-validation warns routed through a new `Game::logInvalidAction()` that logs only for the human
seat, so NPC mis-attempts during auto-play no longer clutter the console. Regression #282 in tests/test_game_rules.cpp.

---

**U11 — Menu hover/click target drifts from the drawn row on window resize (main menu + pause screen).** 🟡 Vision-critical.
Resizing makes where you move the mouse no longer correspond to where an item is actually drawn. Drawing is in absolute-pixel
space: `drawMenuItem` centres at `(window.width/2, y)` (`GameRenderer.cpp:1071`); pause rows sit at `pauseRowY(game,i) = height *
yFrac` (line 1134). The three hover hit-tests use **different coordinate bases**: `getHoveredMenuItemIndex` converts with
`mapPixelToCoords(sf::Mouse::getPosition(...))` (line 1078) yet compares against absolute-pixel `item.y` (1086);
`getHoveredPauseMenuItemIndex` uses raw `sf::Mouse::getPosition(game.window)` as a Vector2i (1144) vs `pauseRowY`;
`getHoveredGameRulesRowIndex` again uses mapPixelToCoords (1104) against an absolute-pixel layout (1108–1115). Draw and
hit-test diverging in coordinate basis makes the cursor→item mapping break at window sizes where resolved space differs from
logical size — exactly what changes while resizing. *Fix:* unify so every menu hover/click path resolves the cursor in the
same space its row positions are computed in, and confirm resize reflows consistently. Vision-critical: only manifests across
a range of window sizes with live mouse input; no headless unit test asserts "cursor X → drawn row Y" without an SDL view + a
real mouse position — needs on-screen verification while resizing.
**Status — RESOLVED 2026-11-09:** the three hit-tests ran on three coordinate bases (the pause one used desktop-space `sf::Mouse::getPosition()`), while drawing used `gameView` **world** units — equal to window pixels only at exactly 1600×900. Unified through one accessor, `GameRenderer::worldSizeOf(const Game&)`, used by **both** drawing and hit-testing, so every hover/click resolves the row the renderer draws. Verified on screen: after 1600×900 → 900×560 the pill+marker land on the drawn row, and hovering the *old* pixel position highlights nothing. Regressions #323–#325 (the last asserts the renderer's accessors agree with the pure helpers).

**U13 — Non-ASCII text passed to SFML as `std::string` renders as mojibake/tofu.** 🟡 Found by screenshot on 2026-11-09.
`sf::String(const std::string&, const std::locale& = {})` is an **ANSI** string converted through the locale
(`SFML/include/SFML/System/String.hpp:173`), so a UTF-8 source literal decodes to its individual bytes and the
0x80-range byte among them has no glyph. Two live sites: `Game::bettingControlsHint()` (U+2191/2193/2190/2192, rendered
as `□□□ Increase Bet` in the play band — these arrows are genuinely absent from the Noto cmap) and the name-entry
hint's U+2022 bullets. *Fix:* ASCII labels, or build `sf::String(U"…")`/`std::u32string` when a real glyph is wanted.
**Status — RESOLVED 2026-11-09:** both sites replaced with ASCII; the name-entry line extracted to the pure
`PresentationLayout::nameEntryHintText()`; `isSfmlSafeAscii()` added and asserted over every hint helper in
`tests/test_presentation_layout.cpp` **#326** (red verified against the exact pre-fix byte sequences). Recorded, not
fixed: `Card::getSuitSymbol()` returns `u8"♠"[0]` — a single truncated byte, dead code, no callers.

**U12 — RESOLVED 2026-11-12, all three parts.** It was the last open UI item of the phase; it unblocked when §10 #8
(histogram bounds → option a) and #9 (session = a NEW dataset → option a) were decided on 2026-11-11. What shipped is
summarised immediately below, and the original finding is kept underneath it as written.

**U12 — Side-panel graphs: no per-graph visibility toggle, only a per-shoe view (no session view), and Bets Placed X-axis is
**What shipped** (side panel; `HUD::drawCharts`, `HUD::cycleGraphScope`, `Game::cycleGraphView`, `SessionAnalytics`):

- **(3) Axis** — `HUD::computeAxisMax` spans `[MINIMUM_BET, 100]` and grows to the largest wager placed in the active
  view window, hard-capped at `MAXIMUM_BET`. Tests #301–#304.
- **(2) Session view** — a NEW `SessionAnalytics` accumulator (`GameRecord.hpp:127`) covering one open play window: it
  runs across every shoe in it and resets only on open/close, so it is distinct from both per-shoe `ShoeAnalytics` and
  career `longHistory`, exactly as §10 #9a required. `G` cycles the window's view. Tests #175, #176.
- **(1) Per-graph control** — each pane is its own **per-shoe ↔ per-session** switch. A left click inside a pane moves
  ONLY that pane (`HUD::cycleGraphScope`, hit-tested through `chartPaneBounds()`); `G` applies one view to all three at
  once (`HUD::setAllGraphScope`), and a click still refines any single pane afterwards. Tests **#241–#244**.
- **Deviation from the original plan, decided and recorded (CONTEXT §5, 2026-11-12):** part (1) was specified as a
  per-series **visibility** toggle (hide a graph). What shipped is a per-graph **view** toggle instead, and that is the
  accepted semantics: the session view is what needed per-graph control, a hideable pane leaves a hole in a 240px panel
  and teaches nothing, and the per-pane axis only makes sense with a per-pane view. Do not "restore" the visibility
  reading without a new decision.
- **Verification, both ways:** headless **#243/#244** assert the *dataset selection* through the same
  `HUD::activeBankrollSeries()/activeEdgeSeries()/activeBetHistogram()/activeBetBucketWidth()/activeBetAxisLo()`
  accessors `drawCharts()` draws from (verified red by mutating the selection — the flag-only tests #241/#242 stayed
  green, which is why the accessors exist), and `tools/visual_check/check_u12_click.py` proves on screen by ImageMagick
  `compare` AE that one click changes exactly one pane's title strip (bank=947, edge=0, bets=0) and that `G` changes the
  other two while leaving the already-session pane at 0. Screenshots read "Session Bankroll Curve" /
  "Session Edge Over Time" / "N bets (session)".


the full [0, MAXIMUM_BET] instead of the intended 10–100 scaling range.** 🟡 Vision-critical. Three parts, all on the side panel
(`StatsDashboard` drawn at `GameRenderer.cpp:74`; HUDState series in `HUDState.hpp:55-56`, Bets Placed histogram per-shoe):
(1) **Toggle each graph** — `bankrollSeries`/`edgeSeries` + the histogram are always drawn; add per-series visibility flags to
StatsDashboard/HUDState and gate drawing. (2) **Per-shoe ↔ per-session view** — the data model is binary: per-shoe accumulators
reset at the shoe boundary via the graph-recalibration signal (`HUDState.hpp:36-37`) vs career `longHistory`/RollupCarry
(`Constants.hpp:179`, `GameRecord.hpp:169`). Neither is session-scoped; a session view needs a NEW accumulator accumulating
across shoes within one session and resetting at session end — distinct from longhistory (the user flagged this explicitly).
(3) **Bets Placed axis bounds** should be 10–100, scaling up as larger bets are placed, capped at table max `MAXIMUM_BET` (=1000;
`MINIMUM_BET`=10, `Constants.hpp:194-200`). Currently the chart folds wagers into BET_BUCKET_COUNT ranges spanning [0,
MAXIMUM_BET] — a fixed 0–1000 axis. *Fix:* (1) per-series visibility + draw gating; (2) introduce the session-scoped dataset +
UI mode switch with correct labeling; (3) reconfigure the axis to the 10–100 base growing up to MAXIMUM_BET. Vision-critical:
correctness of session-vs-shoe data, toggle visuals, and axis labels/ticks must be verified by eye across real gameplay and
window sizes — not headless-testable in isolation.

---

# PHASE 5 — Tech debt (T1–T2)

**T1 — The profile is rewritten to disk every round.** `HandEvaluator::evaluateHands()` ends with
`players[0].saveProfile()` (`src/HandEvaluator.cpp:297`), observed as a full JSON serialise + write every 6–8
seconds. Correct today, but O(total history) per round. *Fix:* debounce — write at round end only if dirty and at
most every N seconds — while keeping the forced writes at shoe boundary, pause-quit and exit.

**T2 — History grows without bound.** `longHistory` accumulates every shoe forever and is re-serialised on every
save, compounding T1. *Fix:* cap it (last N shoes) or roll older shoes into aggregate counters, keeping the schema
additive.

**B16 — `reconstructDeckAtRound()` was dead code, and wrong.** 🟡 **Resolved 2026-11-07 (`5f81d48`): removed.**
It was the only occurrence of the symbol in the tree (no caller, no test). It rebuilt a shoe by popping the **front**
of a deque built from `initialDeck`, while `Deck::draw()` deals from the **back**, so it would have removed cards that
had *not* been dealt and ignored the cut card's extra draw and mid-shoe reshuffles. No runtime impact today because
nothing called it — but a landmine for any replay analysis, decision-forking or "what if" work (it looks like the right
tool and is not. Deleted cleanly; no schema migration needed since nothing referenced it.

---

# Testing strategy — close the seam that let all this through

The suite is **303/303 green**. Nine of the 15 logic/data defects are already Resolved (B2,B3,B4,B6,B7,B12,B13,B14,
B15); the six open ones — B1,B5,B8,B9,B10,B11 — each sit in the presentation/lifecycle seam that the headless-first
contract deliberately leaves untested. Two cheap additions would have
caught the top three:

1. **Persistence round-trip integration test.** Play → save → enter mode → save → reload → assert invariants.
   Catches B1 and B7. Belongs in `tests/test_session_lifecycle.cpp`.
2. **Renderer-string extraction.** Move display strings out of `draw*()` into pure helpers and assert on the
   strings. Catches B3, and now covers U8 (#70) and U10 (#281/#282) headlessly; U4/U5/U9 remain to be extracted this way (U3 a layout-overlap assertion).
   `gameRuleRows()` / `mainMenuItems()` — one source of truth shared by draw and test.

Also worth adding: a per-screen layout-overlap assertion (catches U3), and a sweep for "state field written but
never cleared" (catches B2 — grep every `hudState.` field for a reset path).

---

# Definition of done (phase 0.4)

- [ ] B1–B4 and B13 fixed, each with a named regression test that fails without the fix.
- [ ] B5–B8, B14 and B15 fixed, or explicitly deferred with a written rationale in CONTEXT.md §5.
      (B15 must land **before** B12/B13 — they edit the header it duplicates.)
- [x] B9 overlay indexing guarded (`facea14` / #296/#297) and logger flush fixed (`d6d6436`) — both clauses of the overlay+flush DoD met. The unreproducible `R`-time crash itself remains Open (see PHASE 2 note).
- [ ] B10–B12 resolved; legacy profiles verified to still load.
- [ ] U1, U2, U3, U7 addressed — the four a new player hits within five minutes. (U7 done 2026-11-07 `e157052`; U1/U2/U3 still Open.)
- [ ] Persistence round-trip + renderer-string extraction tests exist.
- [ ] Full suite green (currently **277/277**) with a test count at or above the current baseline, growing as each fix adds
      its regression test.
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

**2026-11-06 — audited against `96fd416` (branch `3.1`).** Baseline at that time: **264/264 in 8.15 s**; every
`file:line` citation in this ledger re-checked and correct; B13's standalone proof re-run and reproducing. That audit is
now historical — nine of the defects it catalogued have since been Resolved (see §5, §6.10) and the suite is at **277/277**
on HEAD `356fc31`; use §11 for what remains open.

Four structural claims did **not** survive the audit and are fixed above: the "working tree is not clean" warning
(stale — that work landed as `96fd416`); B1's "three save sites" (there are six, and two line numbers were wrong);
B15's instruction to edit `CMakeLists.txt` (that file never mentioned `Serialization.hpp`); and B15's consumer
table, which named one `include/`-copy test where there are three — including the two that lock the save schema.
The verification gate also dropped its no-op `ASAN_OPTIONS` prefix.

The detailed changelog, and the list of claims confirmed unchanged, live in **CONTEXT.md §13**. Update both files
together; they contradict each other more cheaply than either contradicts the code.
