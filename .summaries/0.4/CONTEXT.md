# CONTEXT — Blackjack2 Phase 0.4 (QA hardening)

**Purpose:** Self-contained background for the phase-0.4 defect-removal pass. Read this **before**
`.summaries/0.4/NEXT_STEPS.md`, which is the actionable ledger. This file explains *how* the findings were
established, what evidence backs them, what was investigated and cleared, and which project rules constrain the
fixes. It supersedes nothing in `.summaries/0.3/` — 0.3 remains the authority on architecture and feature
history; this file is the authority on current defects.

**Provenance:** every finding below came from *playing the built game*, not from reading it. Source reading was
used only afterwards to explain what was observed. Where a claim is code-only (not reproduced on screen) it is
labelled as such.

**Status: audited 2026-11-06 on branch `3.1` (HEAD `356fc31`).** Baseline is now **277/277 passing**; every
`file:line` citation re-checked and correct against HEAD, including the fiddly ones (`Game.cpp:1012`, `810`, …) in
§1. Several *structural* claims in the original revision were wrong and have been corrected in place — §13 lists
exactly which, so you can tell a verified claim from an inherited one. Nine defects are already **Resolved**
(B2,B3,B4,B6,B7,B12,B13,B14,B15); see the tracker in §11. Read §13 before treating any statement here as settled.

---

## 1. Project snapshot

- **What:** Desktop blackjack simulator — Kelly betting, Hi-Lo/Reko counting, basic-strategy advisor, per-shoe
  analytics, replay/shoe history, player profiles, side bets, strategy trainer, tournament, achievements.
- **Stack:** C++17 for the game, C++20 for the tests (`CMakeLists.txt:4` and `:198`); SFML 3.x — `CMakeLists.txt`
  requires ≥3.0 and this machine has **3.1.0** (`find_package` reports "Found SFML 3.1.0"); CMake ≥3.10; Catch2
  **v3.6.0** pulled by `FetchContent` (`CMakeLists.txt:124-130`); vendored nlohmann/json in `include/nlohmann`.
- **Build:** `cmake --preset "GCC 13.3.0 x86_64-linux-gnu"` → `out/build/GCC 13.3.0 x86_64-linux-gnu/`.
- **Test:** `ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu'`. Baseline **277/277 pass** (HEAD `356fc31`).
- **Entry:** `main.cpp` → `Game::run()` (`src/Game.cpp:185`).
- **Save file:** `$HOME/.local/share/blackjack/saves/player_profile.json`
  (`getSavePath()`, `src/Player.cpp:14-25`; falls back to `saves/player_profile.json` if `HOME` is unset).
- **Window:** 1600×900 default, resizable (`Constants::WINDOW_WIDTH/HEIGHT`).

**The working tree is clean, and every line number in these two docs is accurate against `HEAD`.** An earlier
revision of this file warned that 11 files carried uncommitted Tournament-refactor changes and that line numbers
referred to the working tree rather than `HEAD`. That work has since landed as **`96fd416 "new horizons"`** on
branch `3.1`, together with these three documents. On **2026-11-06** every `file:line` citation in this file and
in `NEXT_STEPS.md` was re-checked against that commit; the citations held, including the fiddly ones
(`Game.cpp:1012`, `Game.cpp:810`, `Game.cpp:1340`/`:1499`, `RoundDealer.cpp:35`/`:45`/`:46`,
`GameRenderer.cpp:1020-1023`, `GameRecord.hpp:307`, `PlayerProfile.hpp:66`, `Constants.hpp:273`). Trust the line
numbers. Two of these shifted a few commits later (B4/B6 moved `activeTrueCount()` 483→1012 and `Game::split()`/its
enforcement to :810–:821); §13 records them, so re-check if you are editing around there. What did *not* hold were
several structural claims, and those are corrected in place — see §13.

## 2. Posture change entering 0.4

Phases 0.0–0.3 were feature delivery then refactor. 0.4 is **defect removal**. Feature work stays frozen.

The defining fact of this phase — **the test suite is fully green while the game carries user-visible defects the
headless contract never exercises** — still holds, but the composition has changed: 9 of the original defects are now
**Resolved** (B2, B3, B4, B6, B7, B12, B13, B14, B15) each with a dedicated regression test. The open ones left are
B1, B5, B8, B9, B10, B11 — plus the UI/UX set U1–U10 and the tech-debt items T1, T2, B16 (all in §11). Of those,
**B1 is the last red defect a player meets in a normal session**; it is the natural next target once this phase's
lower-severity items are triaged. See §3 for where the remaining defects live.

Working rule for the phase: **a fix is not done until a test exists that fails without it.** Several of these
bugs are one-line changes that would otherwise be trivially re-broken.

## 3. The testing blind spot (read this first)

The 0.3 testing contract is: *test pure logic headlessly through `Game(bool headless)` + `setDeck(...)` and read
results via `*ForTest()` accessors; keep presentation out of the test binary.* That contract is good and has
produced 277 meaningful tests. But it deliberately excludes exactly the layers where these bugs live:

| Defect | Layer the contract excludes |
|---|---|
| B1 Tournament overwrites the profile | mode lifecycle + persistence integration |
| B2 Toast never clears | HUD state lifetime / presentation |
| B3 Side bet always "WON" | string formatting inside `draw*()` |
| B7 Escape-quit skips the save | the `run()` loop and event ordering |
| U1–U6, U9 layout/label issues | rendering |

The logic underneath is correct in every case. B3 is the clearest example: `resolveSideBets()` computes the right
payout, `syncBettingAndSideBet()` copies it faithfully, and the tests assert those numbers — then the renderer
throws the sign away. The tests pass and the player is lied to.

**Two structural additions close most of the gap:**

1. **Persistence round-trip integration tests.** The current tests exercise save and load separately; none plays
   → changes mode → saves → reloads and asserts invariants. B1 and B7 both live in that gap.
2. **Renderer-string extraction.** Move display strings out of `draw*()` into pure helpers and assert on the
   strings. The project already does exactly this for menu/rule content (`mainMenuItems()`, `gameRuleRows()` —
   "one source of truth shared by drawing and hover"), so it is an established idiom, not a new invention.

## 4. Architecture map (unchanged from 0.3, re-verified)

State is owned by `Game`; presentation and settlement are delegated friends.

| Concern | Owner |
|---|---|
| Loop, mode dispatch | `Game::run()` / `update()`, `AppMode` enum (`include/Game.hpp:7`) |
| Round/turn state machine | `RoundEngine::tick(dt, Game&)` |
| Settlement / blackjack check | `HandEvaluator` |
| Deal + side-bet resolution | `RoundDealer::deal()` / `resolveSideBets()` |
| Betting sizing | `BettingHandler` (composed, pure) |
| Session entry points | `SessionLifecycle` (composed) — `startNewGame`, `startTournamentSession`, `startTrainerSession`, `startStatsSession` |
| Achievements | `AchievementManager` (rules) + `AchievementFeeder` (game → manager + toast) |
| Presentation | `GameRenderer` (single `modeTable()` dispatch), `HUD`, `StatBox`, `FontManager`, `Graphics` |
| Input | `InputHandler` — event-driven for menus/play; **polled** for the rules panel (see B6) |
| Persistence | `PlayerProfile` + `Serialization.hpp` (additive JSON) |

Two things worth internalising before touching this code:

- **`Game::resetShoeAnalytics()` is the single shoe-boundary choke point.** Anything per-shoe must reset there.
  B4 is a bug *inside* that choke point, not a missing one.
- **`Player::getProfile()` returns by value** (`include/Player.hpp:78`). Binding it to `const auto&` dangles. This
  has already caused one bug (0.3 §6 #26) and B1's fix touches the same call sites — check every one.

---

## 5. Persistence and the data model

**File:** `$HOME/.local/share/blackjack/saves/player_profile.json` (`getSavePath()`, `src/Player.cpp:14-25`;
falls back to `Constants::PLAYER_FILE_FALLBACK` = `saves/player_profile.json` when `HOME` is unset,
`include/Constants.hpp:242`). Written by `PlayerProfile::save()` (`src/PlayerProfile.cpp:50-95`), read by
`PlayerProfile::load()` (`:28-48`), shaped by `to_json`/`from_json` (`include/Serialization.hpp:191-231`).

**Copy unified by B15 (Resolved `9a89185`).** Exactly one copy of the header remains — `include/Serialization.hpp`; `src/Serialization.hpp` was deleted. Verified this pass with the shipped TU: `g++ -I include -M src/PlayerProfile.cpp | tr ' ' '\n' | grep Serialization.hpp` resolves to **`include/Serialization.hpp`**. That is the only TU that puts the schema into the shipped binary, and it is also what every save-schema-reading TU reads — the locking tests `test_profile_settings.cpp` / `test_game_rules.cpp` / `test_achievements.cpp` (via `#include <Serialization.hpp>` falling through to `-I include`) and `.summaries/0.4/hist_proof.cpp` (§6.10, compiles `src/PlayerProfile.cpp` directly).

The two-copy divergence this phase once worried about is gone: the "edit one copy while the other stays stale" trap — which is why B15 landed before any schema work — is **closed**. Any schema edit (B12/B13/B14) now targets `include/Serialization.hpp` alone; no `-M` sweep is needed before touching it.

**Top-level keys actually written today** (verified against a live file):

| Key | Written | Read | Notes |
|---|---|---|---|
| `name` | ✔ | `j.at()` **throws if absent** | always `""` — nothing ever sets it (U7) |
| `bankroll` | ✔ | `j.at()` **throws if absent** | default 1000 (`PlayerProfile.hpp:66`) |
| `stats` | ✔ | `j.at()` **throws if absent** | 26 fields; newer ones use `j.value()` defaults |
| `longHistory` | ✔ | `j.at()` **throws if absent** | completed shoes only; grows forever (T2) |
| `preferredPlayerCount/Speed/CountingStyle` | ✔ | `j.value()` + default | |
| `preferredSoundVolume/MusicVolume/Muted` | ✔ | `j.value()` + default | |
| `unlockedAchievementIds` | **only if non-empty** | `j.value()` + `{}` | |
| `gameRules` | ✔ | guarded `contains()` | nested object, 7 keys (+maxSplitHands; B10) |
| `currentHistory` | ✔ (additive) | `j.value()` + `{}` on load | **B13** — now serialised; guarded read keeps legacy mid-shoe-quit saves loading (§6.10, test #273) |

**Two-tier read contract.** Four core fields use `j.at()` and therefore *throw*; every additive field uses
`j.value(key, default)` and degrades gracefully. The additive half is deliberate and well documented in the
source comments. The `at()` half is what turns a truncated file into a total wipe (B14).

**"The record is the source of truth."** `save()` does not write the in-memory stats verbatim. When
`longHistory` is non-empty it recomputes every record-derivable stat via `careerStatsFromHistory()`
(`src/PlayerProfile.cpp:68-91`) and overwrites them, keeping only the non-recordable trackers
(`splits`, `doubles`, `betsPlaced`, `betSum`, `betDistribution`, strategy counters) as-is. The intent — a saved
profile can never drift from what was actually played — is good, and `tests/test_stats_batching.cpp` proves the
recompute matches the live per-hand increments.

**Why that design *was* losing data (B13).** `careerStatsFromHistory()` folds the in-progress shoe
(`currentHistory`) in as a synthetic `completed = false` shoe, so the numbers are right *in session*. The latent
trap was that `currentHistory` is not serialised, so after a reload the recompute runs on `longHistory` alone and
the next save overwrites good numbers with smaller ones — two individually-correct decisions composing into silent
data loss. Proven in §6.10. **B13 fixed it** by adding `currentHistory` to the write (a guarded read on load keeps
legacy mid-shoe-quit saves loading) and a test (`test_profile_settings.cpp#273`) asserting an in-progress shoe's
rounds survive a save/load round trip. If you touch this recompute again, that test is your guardrail: it fails the
moment the in-progress shoe stops surviving a reload.

**`gameRules` nested object** — written keys: `numDecks`, `dealerStandsOnSoft17`, `doublesAfterSplitAllowed`,
`resplitAcesAllowed`, `resplitTensAllowed`, `surrenderType`, `maxSplitHands`. The per-field hooks live in `include/GameRules.hpp`
(ADL'd from the delegation at `Serialization.hpp:212/238`). `maxSplitHands` is a real field enforced at
`src/Game.cpp:821`; it is now a full schema field. `to_json` writes it, and `from_json` clamps any stored
value into {2,3,4,-1} — -1 == unlimited, falling back to the struct default for absent/invalid values. It has a UI row
(`GameRuleId::MaxSplitHands`) cycling only {2, 3, 4, Unlimited}; the -1 sentinel is guarded explicitly at both enforcement
sites (Game.cpp:821 / RoundEngine.cpp:124) so it never reads as a large finite cap. Spelling: B12 renamed the
written key to `resplitTensAllowed`; on load the corrected key wins, with the legacy typo'd `resplitTentsAllowed`
retained only as an additive migration fallback so pre-B12 saves keep loading.

**Decisions to record here as you work** (add a dated bullet per decision):
- **Whether Tournament progress persists at all, and where that gate lives (B1) — resolved 2026-11-07 to option (b):** record a SEPARATE tournament history rather than touching the career profile. Note on implementation: B1 (`5107aa7`) currently delivers the *career-profile-preserved* guarantee via option (a) — `markPersistable(false)` gates every `saveProfile()` for the run and tournaments leave no separate log. Both choices keep the career file untouched, but (b) additionally wants a tournament trace; materialising that as a dedicated tournament-history store is a small follow-up feature task with its own test seam (new persisted data + UI), i.e. outside this QA-hardening round's defect scope. Left open to implement on request — do not conflate "career profile preserved" (already true) with "tournament history recorded" (still option-b-pending).
- **B5 main-bet escrow point** (resolved 2026-11-07): escrow at COMMIT, not settlement — Model C. A committed main stake is tracked in `Player::pendingMainBet_` and a pending side bet in `pendingSideBet_`; `getAvailableBankroll()` returns bankroll minus both, and `confirmBet()`/`placeSideBet()` clamp + hint against that available balance instead of the full bankroll. This keeps win/lose settlement arithmetic byte-for-byte unchanged (zero regression risk on tests asserting exact post-settlement bankrolls); only over-commit cases change — a $1000 main + $1000 side can no longer both clear and settle negative, because after the main is committed the remaining balance is what any side bet may claim. Bankroll itself only moves at settlement (winBet/loseBet), so escrow excludes committed stakes from availability without touching the recorded bankroll until resolution.
- **B10 split-hand cap exposed** (resolved 2026-11-07): the value now persists and is user-editable via the rules panel,
restricted to exactly `{2, 3, 4, Unlimited}` — cycled by `applyGameRuleEdit()` through
`SPLIT_HAND_LIMITS[] = {2, 3, 4, MAX_SPLIT_HANDS_UNLIMITED(-1)}`; the -1 sentinel is handled at every enforcement site.
Serialisation round-trips it and clamps out-of-range stored values back into the set. Regression tests #278–#282
(test_max_split_hand_rules.cpp) cover the schema key, cycling bounds and unlimited-vs-finite-cap enforcement.
- **The canonical starting bankroll for human and NPC seats (U8) — resolved 2026-11-07 to $10,000 everywhere.** Human and NPC seats share a single `STARTING_BANKROLL = 10000`; the prior $1,000 assumption is reconciled. No new persisted field; the value lives in `Constants.hpp` and both seat types read it on profile load (`PlayerProfile.hpp:66`).
- B12 additive-migration strategy (resolved 2026-11-06): `resplitTentsAllowed` was misspelled in every save file, so the rename to `resplitTensAllowed` had to be additive — `to_json` writes only the corrected key, and `from_json` reads it first then falls back to the legacy typo'd key (new wins when both present) so pre-B12 saves keep loading. Honours the 0.3 schema contract: every existing field stays optional with a default; no profile is ever wiped by the rename.
- B2 achievement-toast lifetime (resolved 2026-11-06; default chosen from §10 #7): toast stays on screen for `GameTiming::ACHIEVEMENT_TOAST_SECONDS` (=4.0s), armed in `AchievementFeeder.cpp:48` on each unlock and cleared at zero by `RoundEngine::tick` alongside the other HUD timers (the renderer already draws only while a name is set, so clearing ends the draw). No new persisted field; purely in-memory HUD state.
- **B7 persist-on-exit choke point** (resolved 2026-11-06): exit-persist is centralised into `Game::persistOnExit()`, called from `InputHandler::returnToMainMenu()` so every Escape→main-menu route persists like the window-close path did. It stamps shoe record + current table settings + unlocked achievements then saveProfile through the existing profile plumbing — no new persisted field, purely re-routing an already-existing save. Guarded against an empty seat vector (headless tests build a bare Game). The single-choke-point placement is what keeps the fix one small method instead of sprinkling another `saveProfile()` onto each exit route.
- **B6 rules-panel keyboard edit edge-triggered** (resolved 2026-11-06): Left/Right rule edits fire only on a false→true key transition (`&& !rulesRightPrev_` / `&& !rulesLeftPrev_`) so one tap = one step at any framerate. Deliberate decision — Up/Down highlight navigation keeps its held-key repeat (moving the highlight fast is the point, so it's desirable) and stays plain held-key polling; only the rule-edit axis was throttled. That split call-out lives in the B6 commit message.
- **§10 open decisions — all resolved 2026-11-07 (recorded here for traceability):** #1 tournament persistence → option (b) record a SEPARATE tournament history; career-profile preservation itself is already delivered by B1 via gating (#see inline bullet). #2 main-bet escrow → at commit (Model C), implemented in B5 (`4f85d37`). #3 starting bankroll → $10,000 everywhere (inline bullet). #4 `maxSplitHands` → expose restricted to {2,3,4,Unlimited}, implemented in B10 (`69cd9e8`). #5 console control banner → keep trimmed along the bottom middle (a UI/presentation decision; unblocks B11 — the banner's Auto-Kelly row is mislabelled "Right Arrow" when the binding is Left — but repositioning it to bottom-middle needs a visual/Xvfb check, so the label fix and placement are queued as follow-ups rather than done this round). #6 name-entry screen → in scope for 0.4: entered automatically before hitting main menu, then reachable from main menu to view player stats (a lifecycle/UI task affecting U7) — IMPLEMENTED 2026-11-07 (`e157052`): auto-prompt before main menu when profile has no name (Game::needsNameEntry gate at run() start); editing via the clamped Game `nameEntry*` buffer + commitPlayerName persisting seat 0's display + profile; Player Stats reuses the existing Statistics→StatsDashboard path (no new view, verified covered by test_menu_navigation.cpp). Headless tests #286–#288. #7 achievement-toast lifetime → 4s (`ACHIEVEMENT_TOAST_SECONDS`), implemented in B2 (`f3aeb1f`).

---

## 6. Evidence log — how each finding was established

**Method.** The game was run under `Xvfb` and driven with synthetic X11 input (python-xlib — `xdotool` is not
installed here), with screenshots captured via ImageMagick `import` at each step (139 images in `/tmp/bj_shots/`).
Every mode was entered and played; the window was resized mid-session; the profile JSON was snapshotted before and
after each lifecycle transition. Source was read *after* an anomaly was seen, to explain it. Recipe in §8.

### 6.1 B1 — Tournament overwrites the career profile
Snapshot the JSON → play a winning hand (bankroll 1025, `handsFinished 1`) → main menu → Tournament → close via
the window manager. Re-read the JSON: bankroll back to 1000, all stats 0, `longHistory` empty. Achievements
survived, which is what pointed at `persistUnlockedTo()` re-stamping from memory and hence at `players[0]` having
been *replaced* rather than merely not updated.

### 6.2 B2 — Toast never clears
Trivially reproducible: unlock anything, watch the toast through a round change, a trip to the main menu and a
Tournament entry. It never left. Confirmed as a missing reset by grepping every assignment to the field — one
write, zero clears.

### 6.3 B3 — Side bet always "WON"
Played a $20 Perfect Pairs side bet on a non-pair. The bankroll arithmetic across the round (1000 → 980 → 930) is
independent evidence that the side bet lost, which is what distinguishes a *display* bug from a *settlement* bug.
Only then was the renderer read, to find the `abs()` before the sign test.

### 6.4 B4 — Reko seed
Set Decks = 4 in the rules panel, started a game, and read two numbers off the same screen: `Cards Left 209`
(proves the rule took effect) and `Count -20` (proves the seed did not follow it). Two readouts on one screen
disagreeing is the whole finding.

### 6.5 B6 — Rules panel key repeat
Sent single key presses and observed multi-step changes (Decks 6 → 1 in one tap). Frame-rate dependent, hence the
inconsistency — sometimes one tap did move exactly one step.

### 6.6 B7 — Escape-quit skips the save
Compared the save file's mtime with the process exit time. The X-button route updated the file; the Escape route
did not. That asymmetry is the signature of an event-driven save sitting behind a `while (window.isOpen())` guard.

### 6.7 B9 — The crash
One occurrence, three failed replays, one clean 60-cycle automated soak. Reported as real-but-flaky rather than as
a bug with a known cause. The *actionable* half is that the logger's output was entirely lost, which is a
diagnosability defect we can fix regardless of whether the crash returns.

### 6.8 B10 / B12 — Config completeness
`grep` for the field name across `GameRules.hpp` / `GameRules.cpp` / `Serialization.hpp`, plus reading the
`gameRules` object out of a live save file. Absence from the **on-disk object** is the proof, not absence from the
serializer source.

### 6.9 Cleared — investigated and *not* bugs

Recorded so the next window does not re-litigate them.

- **`handActions` flat indexing.** `Game::split()` appends the new hand at the end of the seat's vector while
  `handActions` is indexed flat, which looks like an off-by-one. It is safe: seats act strictly in order, so any
  slot after the current one is still empty. Verified with a standalone simulation (`/tmp/handactions_proof.cpp`).
  **Do not "fix" this without also changing turn order.**
- **A suspected settlement bug** during the crash hunt turned out to be **stdout block-buffering** truncating the
  log mid-line at a buffer boundary. The game was right; the log was lying. Direct motivation for the B9 flush
  fix — a misleading log is worse than no log.
- **`handActions` out-of-bounds** was separately checked and is bounded correctly.
- **Pause menu, trainer feedback and tournament UI** all render correctly; **window resizing** works well on the
  main table. Only the Achievements and Game Rules screens fail to reflow (U3).

### 6.10 B13 — the standalone proof (reproducible in 30 seconds)

`.summaries/0.4/hist_proof.cpp` drives the real `PlayerProfile` with no game, no display and no test framework:

```bash
cd /home/william-ament/Documents/Quantum2
g++ -std=c++17 -I include -I /usr/local/include .summaries/0.4/hist_proof.cpp \
    src/PlayerProfile.cpp src/Logger.cpp \
    -o /tmp/hist_proof -L/usr/local/lib -lsfml-graphics -lsfml-system
/tmp/hist_proof   # exits 1 while B13 is open; exits 0 once the fix lands
```

Original finding, reproduced **2026-11-06 at `96fd416`** (before any fix):

```
reload #1: handsFinished=8  shoes=1   (expected 8)
reload #2: handsFinished=6  shoes=1   (expected 9)
BUG: hands silently lost from career stats
```

The program: add 5 rounds → `addShoeRecord()` → add 3 rounds → `save()` → `load()` → add 1 round → `save()` →
`load()`. The career hand count goes **backwards** across a save that added a hand. This is the regression test it
now backs: `tests/test_profile_settings.cpp#273` ("B13: an in-progress shoe's rounds survive a save/load and still
book into career stats"), which fails before the fix and passes after.

*Note:* this proof originally lived at `/tmp/hist_proof.cpp` and did not survive a reboot — that is why it is now
checked in beside these docs. Two things to keep in mind when you run it:

- It compiles `src/PlayerProfile.cpp`, so it exercises the **one** remaining `Serialization.hpp` (`include/`), which
  both the shipped binary and the test suite read today (B15 unified on a single copy, §5). Run it to confirm the
  *shipped* schema changed — but now that B13 is Resolved it should exit **0** ("OK", hands 8 → 9), not 1. If it
  still exits 1 after your fix, you edited the wrong file (see B15's rule).
- It writes `/tmp/hist_proof_profile.json`, never the real save file. Safe to run with a live profile present.

---

## 7. Conventions to preserve

These are load-bearing; several bugs above are deviations from them.

- **Headless-first tests.** `Game(bool headless)` + `setDeck(...)` + `*ForTest()` accessors. No display, no
  audio device, no filesystem outside a temp dir in tests.
- **One source of truth shared by draw and test.** `mainMenuItems()` / `gameRuleRows()` return the same data the
  renderer paints. Extend this to the remaining display strings (it is what makes B3/U4/U5/U9 testable).
- **Additive JSON.** Every new field optional with a default on load. B12's migration and B13's `currentHistory`
  both have to honour this.
- **Centralise shared side effects in existing choke points.** `resetShoeAnalytics()` for per-shoe state;
  `syncBettingAndSideBet()` for HUD sync. B7's fix should follow this rather than adding a second save call.
- **`AppMode` owns its state machine.** Add a mode value with its own tick/draw/input path; never ad-hoc booleans
  scattered through `update()`.
- **Narrow `const GameRules&` threading** — do not hand out mutable game-wide config (0.3 §6 #2).
- **`Player::getProfile()` returns by value.** Never bind to `const auto&`.

## 8. Environment notes and doc drift

**Verified on this machine** (re-checked 2026-11-06): `Xvfb` ✔, ImageMagick `import` ✔, `xwd` ✔, `convert` ✔,
python3 + `python-xlib 0.33` ✔. **`xdotool` ✘ and `scrot` ✘ are not installed** — don't write a repro recipe
around them.

**⚠ Never append `-M` to a command copied out of `compile_commands.json`.** The CMake compile line ends in
`-MD -MT <obj> -MF <dep> -o <obj> -c <src>`. Adding `-M` switches GCC to dependency mode, and in that mode the
dependency list is written to the `-o` **target** — i.e. straight over the `.o` file. The object is silently
replaced with text, the next link dies with `file format not recognized; treating as linker script`, and the
blame lands on whatever you edited last. This was hit for real on 2026-11-06 while auditing B15: four objects were
clobbered and had to be deleted before the build would link again. Run the sweep with a hand-written include list
and no `-o` (so deps go to stdout), and if you must reuse a real compile command, strip `-o`/`-c` first.

Driving the game headlessly:

```bash
Xvfb :99 -screen 0 1600x900x24 &
DISPLAY=:99 ./out/build/'GCC 13.3.0 x86_64-linux-gnu'/Blackjack2 > /tmp/run.log 2>&1 &
# send keys / clicks with python-xlib (Xlib.display().screen().root, fake_input KeyPress/KeyRelease,
# and MotionNotify + ButtonPress for the mouse)
DISPLAY=:99 import -window root /tmp/bj_shots/shot.png
```

Two things that cost time and are worth repeating:
- **Wrap the run in `stdbuf -oL`** (or fix B9) or the log is block-buffered and useless mid-run.
- **`stdbuf`/`unbuffered` is not enough if the process dies** — see B9; flush per record.

**Doc drift in `.summaries/0.3/` — correct these, do not trust them:**

1. **There is no AddressSanitizer in the configured build.** `CMakeCache.txt` contains no `-fsanitize`, and
   `nm Blackjack2 | grep -c asan_report` is 0. The 0.3 caveat about "~75 SFML tests failing at exit from libdbus
   LeakSanitizer teardown" does not apply here; `ASAN_OPTIONS=detect_leaks=0` is a no-op. If you want ASan (and
   B8/B9 argue you should), configure it explicitly.
2. **Six test files named in the 0.3 docs do not exist:** `test_side_bet_placing`, `test_card_animation`,
   `test_deal_animation_speed`, `test_game_renderer_modes`, `test_dealer_blackjack`, `test_stats_dashboard`. They
   were consolidated away during the 0.3 "condense the test suite" passes. The real suite is the 26 `tests/*.cpp`
   files plus `test_support.hpp`.
3. **Tests are one Catch2 binary**, `Blackjack2Tests`, with an **explicit source list** in `CMakeLists.txt:132` —
   not a glob. Adding a new `tests/*.cpp` means editing that list. It links `SFML::Graphics` **and**
   `SFML::Audio` (SoundManager is in the test binary).
4. **The installed SFML is 3.1.0, not 3.0.** `CMakeLists.txt` asks for `find_package(SFML 3.0 …)`, which resolves
   to the 3.1.0 install under `/usr/local`. Nothing in 0.4 depends on the difference; the note is here so nobody
   "fixes" the version or reads a 3.0-specific behaviour into a bug.

Items 1–3 were re-checked on 2026-11-06 and still hold.

---

## 9. What is genuinely good (do not regress it)

A playtest is as much a confirmation as a critique. These all held up under adversarial use and should stay true
after the phase-0.4 changes:

- **Core blackjack logic is correct.** Deal, hit, stand, double, split, surrender, insurance, dealer policy and
  settlement all produced correct outcomes and correct bankroll arithmetic across many hands and multiple seats.
  The B3 and B5 findings are a *display* bug and a *timing* question respectively — neither is a settlement error.
- **The headless test suite is real.** 277 tests, ~8s, no flake observed, and they assert behaviour rather than
  implementation.
- **The rules panel actually works.** Every rule changed took effect in the live game — deck count changed the
  shoe size, surrender type changed the available action, and so on. Only the *seed* (B4) and the *input
  throttling* (B6) are wrong.
- **Window resizing is handled well** on the main table — layout reflows cleanly at both large and small sizes.
- **Achievement unlocking, the trainer's feedback, the tournament's blind structure and the pause menu** all
  behave and render as intended.
- **The additive-schema discipline has genuinely paid off** — profiles from earlier phases still load.

## 10. Open decisions (need an answer before the relevant fix)

| # | Question | Options | Blocks |
|---|---|---|---|
| 1 | Should Tournament results touch the career profile at all? | (a) never — gate the writes; (b) record a separate tournament history | B1 |
| 2 | When does the main bet leave the bankroll? | (a) at commit (escrow); (b) at settlement, with an "available" accessor | B5 |
| 3 | Is $1,000 or $10,000 the intended starting bankroll? | reconcile human + NPC | U8 |
| 4 | Expose `maxSplitHands` or delete it? | expose (2–4) / delete | B10 |
| 5 | Should the console control banner survive at all? | keep trimmed / replace with in-game Controls screen | B11, U4 |
| 6 | Is a name-entry screen in scope for 0.4, or deferred? | in scope / defer with a tracking note | U7 |
| 7 | How long should the achievement toast show? | suggested 4s | B2 | **Resolved** — chose 4s, implemented in B2 (`f3aeb1f`) |

Defaults if nobody answers: 1(a), 2(a), 3 = $1,000 everywhere, 4 = expose, 5 = keep trimmed + add Controls,
6 = defer, 7 = 4s (resolved: implemented as `ACHIEVEMENT_TOAST_SECONDS` = 4.0 in B2, `f3aeb1f`). Record the final choice in §5's decision list.

## 11. Status tracker

Update this table as work lands; it is the first thing the next window should read.

**Re-audited 2026-11-06 at `96fd416`: nothing has landed. All 15 defects (B1–B15) plus U1–U10, T1–T2 and B16 are
still open.** Each row below was re-checked against the source rather than trusted from the previous revision —
the "Evidence" column now says what the 2026-11-06 check actually was, which for several items is weaker than the
word "reproduced" implied.

**Resolved since:** `B3` (`c7978f1`) and `B15` (`9a89185`) landed on 2026-11-06; `B5` (`4f85d37`) on 2026-11-07, `B8` (`11f6f50`) on 2026-11-07;
the rows below for those are updated to **Resolved**. Every other row here is still open — see the §13 audit log.
Suite at 291/291 (0 failures; #256 known flaky, see §13).

| ID | Sev | Title | Status | Evidence as of 2026-11-06 |
|---|---|---|---|---|
| B1 | 🔴 | Tournament overwrites career profile | **Resolved 2026-11-07 (`5107aa7`)** | Loads the real career profile into seat 0 via `Player("William")`, sets persistable=false for the run so every saveProfile() no-ops, overrides only in-memory chips — a close-save (persistOnExit choke point or direct) leaves the file untouched. Regression test #228 redirects HOME to a temp dir, writes a distinctive career profile, enters Tournament, drives both exit paths, reloads and asserts bankroll 98765 / handsFinished 1 / wins 1 / betsPlaced 1 / longHistory size 1 all preserved. Suite 284/284 (0 failures; #256 flaky pre-existing, see §13) |
| B2 | 🔴 | Achievement toast never clears | **Resolved 2026-11-06 (`f3aeb1f`)** | Added `achievementToastTimer` to `HUDState.hpp`; armed in `AchievementFeeder.cpp:48` on each unlock, ticked in `RoundEngine.cpp` alongside the other HUD timers and cleared at 0 (renderer draws only while a name is set). Test #210 arms/advances past lifetime/re-arms (fails before fix). Suite 275/275 |
| B3 | 🔴 | Side bet always reports "WON" | **Resolved 2026-11-06 (`c7978f1`)** | Fixed in `include/SideBetLabels.hpp`: branch won/lost/push on net with the sign preserved; `GameRenderer.cpp:997` calls the pure fn. Guard test in `test_side_bets.cpp` asserts WON +20 / LOST -20 / PUSH 0 (fails before fix). Suite 266/266 |
| B4 | 🔴 | Reko seed ignores deck count | **Resolved 2026-11-06 (`3924b56`)** | Seeds from `gameRules_.numDecks` in `resetShoeAnalytics()` (`Game.cpp:277`); `Deck::create()` remembers shoe size and reshuffle rebuilds from it (`Deck.cpp:73`). Tests assert the roadmap seeds 1/2/4/6/8 → -1/-5/-12/-20/-27 through a headless Game, plus a one-deck reshuffle stays ~53 cards (both fail before fix). Two `test_deck.cpp` reshuffle-size assertions were corrected from the old NUM_DECKS behavior. Suite 274/274 |
| B13 | 🔴 | Mid-shoe quit deletes those hands from career stats | **Resolved 2026-11-06 (`387a3e0`)** | Persist in-progress shoe rounds (currentHistory) so quitting mid-shoe still books them into career stats; guarded read. Test #273 passes; hist_proof.cpp (§6.10) now exits 0 and confirms the shipped schema. §11 listed it Open after the commit and was corrected here |
| B5 | 🟠 | Main bet not escrowed; bankroll can go negative | **Resolved 2026-11-07 (`4f85d37`)** | Escrow at commit (Model C): `getAvailableBankroll()` returns bankroll minus committed main − pending side bet; `confirmBet()`/`placeSideBet()` clamp + hint against that balance, not the full bankroll. 3 regression tests in `test_settlement.cpp`: accessor invariant (#111); a same-limit side bet refused once the main is committed so it can't over-commit the roll (#112); and a losing round (dealer natural −MIN plus a lost PerfectPairs side) settling to after == STARTING − MIN − sideWager == 9930, never negative (#113). Win/lose arithmetic unchanged; full suite 281/281 (0 failures) |
| B6 | 🟠 | Rules panel key repeat | **Resolved 2026-11-06 (`584aa4b`)** | Added false->true edge detection in \`Game::update()\` rules case via \`rulesRightPrev_\`/\`rulesLeftPrev_\`; applyGameRuleEdit fires only on a press (one step per tap). Up/Down highlight repeat left as-is (desirable, called out in commit). Headless test #277 (\`test_rules_panel_editing.cpp\`) holds Right 25 frames and asserts result == one tap; fails before / passes after. Suite 277/277 (+1 over B7) |
| B7 | 🟠 | Escape-quit skips the save | **Resolved 2026-11-06 (`11b187b`)** | Centralised persist-on-exit into `Game::persistOnExit()` (shoe record + current table settings + unlocked achievements, then save) routed through `InputHandler::returnToMainMenu()`, so every Escape->menu route persists like the window-close path did; guarded against an empty seat vector. Headless test #276 (`test_input_return_to_main_menu.cpp`) fails before / passes after; full suite 276/276 (+1) |
| B8 | 🟠 | `Deck::draw()` no empty guard | **Resolved 2026-11-07 (`11f6f50`)** | Two unguarded `back()`/`pop_back()` pairs in `Deck::draw()` — the first when the shoe is already empty, the second after only the cut card was drawn (a single-card shoe whose one card is the cut card), both UB on an empty std::vector. Guarded: entry returns `Card{}` for a truly empty shoe; before the second `back()` it now checks `cards.empty()` and returns the cut card when nothing remains. Purely defensive — every production caller already guards `deck.empty()`, so real gameplay is byte-for-byte unchanged and all 5 callers keep the `Card` return type. Headless-testable via `Deck::setCards({})`: `tests/test_deck_empty_draw.cpp` (#13/#14) drove both sites, SEGFAULT / returned garbage (value 33 != CUT_CARD_VALUE) before the fix, PASS after. Suite 291/291 |
| B9 | 🟠 | Flaky crash + unflushable logger | Open — crash unrepro | `Logger.cpp:34` still writes `std::cout` with no flush/unitbuf. Headless-testability: the reproducible half (logger flush) is EASY and already covered by test #284; the unreproducible flaky-crash half is inherently hard headlessly — needs the exact timing/Xvfb conditions that reproduce it before a guard test can be written |
| B14 | 🟠 | Non-atomic save; corrupt file → silent wipe | **Resolved 2026-11-06 (`9d1d125`)** | Atomic write + corrupt-save rescue under a `.corrupt.<ts>` suffix, and partial-JSON field retention in `PlayerProfile.cpp`. Tests #232/#233 pass. §11 listed it Open after the commit and was corrected here |
| B15 | 🟠 | Two live copies of `Serialization.hpp` | **Resolved 2026-11-06 (`9a89185`)** | Deleted `src/Serialization.hpp`; exactly one copy remains (include/), enforced by a guard test that fails with two / passes with one. No CMake edit needed; `PlayerProfile.cpp` now resolves to the include copy, unblocking every schema edit (B12/B13/B14) |
| B10 | 🟡 | `maxSplitHands` dead config | **Resolved 2026-11-07 (`69cd9e8`)** | Serialised (`to_json`/`from_json`, clamps to {2,3,4,-1}; -1==unlimited) and exposed via a gameRuleRow cycling only {2,3,4,Unlimited}; the -1 sentinel is guarded at both enforcement sites. Regression tests #278–#282 (test_max_split_hand_rules.cpp): schema round-trip + clamp fail before fix; restricted-value cycling and unlimited-vs-finite-cap enforcement verified. Suite 289/289 |
| B11 | 🟡 | Control banner mislabels keys | Open | `Game.cpp:162-163` vs real bindings `InputHandler.cpp:595-606` |
| B12 | 🟡 | `resplitTentsAllowed` typo in save schema | **Resolved 2026-11-06** | Renamed field+enum to `resplitTensAllowed`/`ResplitTens`; `to_json` writes the corrected key, `from_json` reads it then falls back to the legacy typo'd key (new wins). On-disk writer verified via `test_profile_settings.cpp:232` using real `PlayerProfile::save()` — shipped binary now writes `resplitTensAllowed`. Suite 272/272 |
| B16 | 🟡 | `reconstructDeckAtRound()` dead and wrong | **Resolved 2026-11-07 (`5f81d48`)** | Removed — static analysis confirmed zero callers (sole occurrence was its own definition at `GameRecord.hpp:307`), so deletion is behaviour-preserving; it also recomputed deck state with the wrong card-drop order, which no code relied on. Full suite unchanged at 289/289 |
| U1–U10 | 🟡 | UI/UX set (see NEXT_STEPS Phase 4) | Partial — U7 Resolved | Not re-verified on screen this pass; cited renderer lines all resolve to the claimed functions. **U7 Resolved 2026-11-07 (`e157052`): name-entry screen + player-name persistence — headless tests #286–#288 (buffer clamp, gating predicate, commit round-trip); remaining U-items still Open, unverified on screen.** Headless-testability: HARDEST — needs an SDL window context + screenshots/Xvfb for visual assertions; no pure headless unit test can lock rendering.
| T1–T2 | 🟢 | Save amplification; unbounded history | Open | `HandEvaluator.cpp:297` still saves every settled round. Headless-testability: MEDIUM — needs byte-counting output interception (T1) and a chosen history cap before it is assertable (T2).
| — | ✅ | `handActions` indexing | **Cleared — not a bug** | Do not "fix" without also changing turn order (§6.9) |
| — | ✅ | Settlement math | **Cleared — correct** | |

## 12. How to use these two files

- **This file (`CONTEXT.md`)** = why things are the way they are, what the evidence is, what not to touch.
- **`NEXT_STEPS.md`** = what to change, in what order, with the test to write for each.
- **`IDEAS.md`** = ten unscheduled feature propositions. **Not a plan.** Read it only after the defect phases are
  done, or not at all; nothing in it is committed work.
- **The copy-paste handoff prompt** lives at the top of `NEXT_STEPS.md`. It encodes the working contract (read
  order, verification gate, test-first rule, guardrails) and the hard-won gotchas — B15-before-schema, B1's six
  save sites, the `-M` trap. Regenerate it whenever this file's §10–§13 change; a stale prompt reproduces stale
  assumptions in a fresh window that has no way to check them.

Work top to bottom through NEXT_STEPS' phases. For each fix: write the named test first (several are specified as
tests that *fail today* — that is the point), then the fix, then the verification gate, then update §11 here and
append any decision to §5. Keep commits one-bug-one-commit.

If a fix reveals that a finding here is wrong, **correct this file** rather than quietly working around it — the
next window trusts it.

## 13. Document audit log

These documents are load-bearing, so they get audited like code. Each entry records what was checked, against
what, and what changed. If you correct something, add a bullet here — a silent edit is how the next window ends up
trusting a stale claim again.

### 2026-11-07 — B5 main-bet escrow at commit (over-commit → negative bankroll) fixed

What was wrong: `confirmBet()` and `placeSideBet()` affordability checks read the full `bankroll`, so a player could
commit the MAXIMUM_BET main and an equal side bet against one roll; both cleared and settlement then drove the recorded
bankroll negative (the two wagers overlapped).

Fix (Model C): track committed stakes as pending bets — `Player::pendingMainBet_` / `pendingSideBet_`, accessors
`getAvailableBankroll()`/`commitMainBet()`/`commitSideBet()`/`clearPendingBets()`. `resetRound()` clears both; the two
clamps/hints gate on `getAvailableBankroll()` = bankroll − committed main − pending side instead of the full bankroll.
The main bet is escrowed at commit (not settlement), so availability shrinks the moment the main is locked and no side
bet can claim beyond what remains. Bankroll itself only moves at winBet/loseBet, so the win/lose arithmetic that other
tests pin to exact post-settlement values is byte-for-byte unchanged — zero regression; only over-commit cases change.

Docs: §5 records the escrow-at-commit decision; §11 B5 row → Resolved (`4f85d37`); 3 regression tests added in
`test_settlement.cpp` (accessor invariant #111, over-commit refusal #112, losing-round non-negative + conservation #113).
Full suite 281/281 (0 failures), up from the B1 baseline of 278/278.

Note: the very first draft of test #113 asserted an exact delta but never called `seed_deck()`, so it dealt from a
default shoe and produced a non-deterministic win; seeding + a proven natural sequence made it deterministic (after ==
9930). The side-bet loss settles at deal via `resolveSideBets()` (RoundDealer.cpp:84), so the intent must be placed before
`startTestRound()`.

### 2026-11-06 — full claim audit against `96fd416` (branch `3.1`)

Method: re-ran configure/build/`ctest -j8`; re-checked every `file:line` citation in both docs against the source;
re-ran the B13 standalone proof; re-derived the B15 include split with `g++ -M`; re-checked the §8 environment
list. **No code was changed in this pass — it is a documentation-only revision.**

Corrected:

1. **§1 / NEXT_STEPS header — "the working tree is dirty" was false.** The Tournament refactor landed as
   `96fd416`, taking these docs with it. Replaced with a clean-tree statement plus the note that the citations were
   re-verified against that commit.
2. **B15 prescribed a CMake edit for a file CMake never referenced.** The fix said to remove `src/Serialization.hpp`
   "from both `add_executable` source lists (it is listed as a source in each)". It is in neither, and in fact
   appears nowhere in `CMakeLists.txt`. Deleting the file is the whole build-side change.
3. **B15 understated its own blast radius.** It named `test_achievements.cpp` as the only `include/`-copy consumer.
   `test_profile_settings.cpp` and `test_game_rules.cpp` — the two tests that lock the save schema, and the ones
   B10/B12/B13/B14 all extend — also read the `include/` copy. Rewritten in §5 with a full consumer table and the
   proof command. This is what turns B15 from a tidiness item into a prerequisite.
4. **B1 named three save sites where there are six**, and mislabelled two of them (`InputHandler.cpp:117` is
   `:119` QuitToMenu; the window-close save is `:254`, not `:252`; `:104` is **Restart**, not Quit-to-Menu). Added
   the full table, and the `players.clear()` at `SessionLifecycle.cpp:34` that explains the Stats-Dashboard
   symptom.
5. **B13's proof program no longer existed.** `/tmp/hist_proof.cpp` did not survive a reboot. Re-created as
   `.summaries/0.4/hist_proof.cpp`, re-run, output byte-identical to the recorded finding. §6.10 now points at the
   checked-in copy and warns that it exercises the `src/` schema copy.
6. **B13 gained two implementation hazards** the original entry missed: `addShoeRecord()` discards the caller's
   `rounds` (`PlayerProfile.cpp:20-26`), and `SessionLifecycle.cpp:100-110` already de-duplicates rounds that land
   in both `currentHistory` and the stale `currentShoeRecord` mirror — a path the fix can disturb.
7. **Counts and versions.** `stats` is 26 fields, not 24. The defect count is 15 (B1–B15), not 14. SFML is 3.1.0
   installed against a ≥3.0 requirement. Comment-stripped `Serialization.hpp` is 211 lines, not 194. Catch2 pinned
   v3.6.0 at `CMakeLists.txt:124-130`.
8. **NEXT_STEPS verification gate** no longer runs under `env ASAN_OPTIONS=detect_leaks=0`. The doc's own §8 says
   that variable is a no-op in this build, so leaving it in the gate advertised protection the build does not have.
   Gate now uses `-j8`.
9. **§11 tracker** gained an Evidence column stating what each 2026-11-06 check actually established. Several rows
   previously read "reproduced" on the strength of a playtest that this pass did not re-run; they now say so. B16
   was missing from the tracker entirely and has been added.
10. **New §8 warning about `-M` and `compile_commands.json`,** learned the hard way while auditing B15: replaying a
    CMake compile command with `-M` appended overwrites the `.o` with the dependency list, and the resulting link
    failure looks like it was caused by your most recent edit. Four objects were clobbered and deleted during this
    audit; the build and suite are back to 264/264. The recovery one-liner is recorded in NEXT_STEPS B15 step 3.
11. **The handoff prompt at the top of `NEXT_STEPS.md` was rewritten.** The old six-line version still described
    the pre-audit world — it told a fresh window to "work the phases in order" with no mention of the B15
    prerequisite, B1's six save sites, or the two-copy schema hazard, and its "don't trust the docs blindly" step
    had no way to say *which* parts of the docs are now verified. The new version carries the read order (§13
    first), a calibration note so the next window knows what to re-verify versus what to trust, the gotchas as
    working rules, and the §13-update obligation in Step 4. §12 now says to regenerate it whenever §10–§13 change.

### 2026-11-06 — code fixes this round (B15 + B3): the first non-doc-only pass since the audit

Resolved two defects from the ledger. Every `file:line` location below was re-checked against the new HEAD and still
holds; test counts went 264 → 265 (B15) → 266 (B3), all green after each fix.

- **B15 (`9a89185`).** Deleted `src/Serialization.hpp`; exactly one copy remains in `include/`, enforced by a guard
  test in `test_profile_settings.cpp` that fails with two copies and passes with one. No CMake edit was needed — the
  file is referenced nowhere in `CMakeLists.txt`. `PlayerProfile.cpp` now resolves to the include copy, so every schema
  edit (B12/B13/B14) changes both the tested and shipped binary at once.
- **B3 (`c7978f1`).** Fixed the side-bet readout at `GameRenderer.cpp:997`: the old code ran `abs()` before testing the
  sign, so `LOST` was dead code and a lost stake read `WON +20`. Extracted the three-way branch (won/lost/push) into a
  pure SFML-free function `sidebet::resultLabel(wager, payout)` in `include/SideBetLabels.hpp`; `GameRenderer.cpp` calls
  it. Guard test in `test_side_bets.cpp` asserts WON +20 / LOST -20 / PUSH 0 and fails before the fix.
- **Re-checked B1's saveProfile sites.** Source has SEVEN call sites, not the six enumerated in NEXT_STEPS' B1 table —
  `SessionLifecycle.cpp:287` was missing. Flagging for whoever fixes B1 so every unguarded tournament-time save is gated.

Confirmed accurate, no change needed: every `file:line` citation listed in §1; the §3 blind-spot analysis; the §4
architecture map; the §5 two-tier read contract and `gameRules` key list; §6.1–§6.9 evidence; §7 conventions; §8
drift items 1–3; §9; §10.

### 2026-11-06 — code fix this round (B12: resplit-tens schema typo)

Resolved one defect from the ledger and closed a build that a partial B12 rename had broken earlier in the session.
Every location below was re-checked against source; test count went 269 → 272, all green after the fix (serial run —
the transient `-j8` ctest reported `BAD_COMMAND`, which is a runner artifact right after rebuild, not a failure).

- **B12.** The field name, enum value and JSON key all spelled "Tents" for "Tens". Renamed to `resplitTensAllowed`
  (field) / `ResplitTens` (enum) throughout: `include/GameRules.hpp` (struct member + `to_json` key), `src/Game.cpp`
  (re-split gate + comment), `src/GameRules.cpp` (row label, description case, toggle case). Wrote only the corrected
  key; on load read it first then fall back to the legacy typo'd key so pre-B12 saves keep loading (new wins when both
  present) — additive, honours the 0.3 schema contract. Three guard tests in `tests/test_profile_settings.cpp` (#232–
  #234) assert: fresh profile writes the corrected key / legacy-only save loads / both-present corrected-key-wins.
  #232 drives real `PlayerProfile::save()` and reads the on-disk JSON, proving the fix reached the shipped binary (not
  just the tested copy). No other TU referenced `.resplitTentsAllowed` or `ResplitTents` directly — they only failed to
  compile because they include the now-broken header.
- **Doc drift corrected this pass.** §5's gameRules note still named the *written* key as the misspelled
  `resplitTentsAllowed`; updated to show the corrected write key with the legacy key retained only as a migration
  fallback, and noted that GameRules' per-field hooks live in `include/GameRules.hpp` (ADL'd from `Serialization.hpp`).
  §11 B12 row moved Open → Resolved; the B12 decision added to §5's decision list.
- Re-checked: §5 two-tier read contract and gameRules key list (§5:171–176) now match the corrected writer; §5
  consumer table still holds (the one-copy guard test in `test_profile_settings.cpp` is what makes this edit touch both
  binaries at once); B13/B14/B15 rows unaffected. No other claim changed.

### 2026-11-06 — corrected stale status for B13 + B14 in §11

The `B13` (`387a3e0`) and `B14` (`9d1d125`) fixes landed on 2026-11-06 with passing dedicated tests — `test_game_rules.cpp`
#270 for B13, and `test_profile_settings.cpp` #229 + #230 for B14 — but the §11 tracker (last updated at `95a3d32`, which
marked only B3 + B15 resolved) still listed both as Open. Verified each against source and moved both rows to Resolved with
the commit hash plus test IDs as evidence, so the table matches what is actually implemented. No code or other citations
changed — status correction only; the B14 primary-site citation (`src/PlayerProfile.cpp`) was re-checked and still holds.

> **Note (2026-11-06):** the test numbers here have since shifted by +3 as more Phase-0 regression tests were added, so
> they are now historical rather than current — `#270` is no longer a B13 test (it is an advisor-DAS case), and the
> authoritative current IDs are **B13 = #273**, **B14 = #232/#233** (see §5, §6.10). If you grep `ctest` by these old
> numbers you will hit unrelated tests; use the current IDs.

### 2026-11-06 — resolved B4 (Reko seed) + B2 (achievement toast); corrected drifted citations

Two more Phase-0 criticals landed with passing tests, each committed separately (`3924b56`, `f3aeb1f`) and verified via the full gate. Both are in the presentation/lifecycle seam the headless contract misses, so they needed dedicated tests to lock them.

- **B4 (`3924b56`).** `resetShoeAnalytics()` seeds Reko from the live rule now (`Game.cpp:277`, `gameRules_.numDecks`)
  instead of compile-time `NUM_DECKS`; a one-deck shoe starts at -1, an 8-deck at -27. Added `Deck::shoeDecks_` (set in
  `create()`, used by the reshuffle loop) so a mid-shoe reshuffle on a non-default shoe keeps its real card count
  (`Deck.cpp:73`). New tests in `test_reko_counting.cpp` assert the roadmap seeds through a headless Game across 1/2/4/6/8
  decks, plus a one-deck reshuffle stays ~53 cards; both failed before the fix. Two existing `test_deck.cpp` reshuffle-size
  assertions encoded the old NUM_DECKS behavior on a create(1) shoe and were corrected to the single-deck size. Suite
  274/274 (baseline 272, +2).

- **B2 (`f3aeb1f`).** The achievement toast now has a finite lifetime: `achievementToastTimer` added to `HUDState.hpp`,
  armed in `AchievementFeeder.cpp:48` on each unlock, ticked in `RoundEngine.cpp` alongside the other HUD timers and cleared
  at zero (the renderer already draws only while a name is set). New test #210 arms / advances past lifetime / re-arms via
  `advanceTurnState()` (which folds `RoundEngine::tick`) — an inert round state + non-AutoPlay speed makes the frame run only
  the timer countdown, so no session setup is needed. It fails before the fix. Suite 275/275 (+1 over B4).

**Citation drift to re-check (my two edits moved line numbers; nothing else changed).**
- `Deck.hpp` +4 and `Deck.cpp` +2 after B4's `create()` edit: the reshuffle loop is now `Deck.cpp:73`, and the `draw()` guard region `121-135` → `123-137`. Corrected in §11 (B4/B8 rows) and throughout NEXT_STEPS.md.
- `Constants.hpp` +3 after B2's toast constant insertion: `PLAYER_FILE_FALLBACK` is now `:242` (old `:239` → `:242`); the §1 recheck-list entry `:270` → `:273`, and STARTING_BANKROLL in NEXT_STEPS.md U8 likewise. All corrected.

### 2026-11-06 — B7 (Escape-to-main-menu exit skips the save) resolved

Added `Game::persistOnExit()` (`src/Game.cpp:198-219`) as the single persist-on-exit choke point and routed it through
`InputHandler::returnToMainMenu()` (`src/InputHandler.cpp:28`). It stamps shoe-record bookkeeping + current table settings
+ unlocked achievements onto the profile, then saves — identical to what the pause-menu Quit-to-Menu path did before B7,
just centralised so no Escape route can silently drop a session's bankroll / record / settings (the defect: window-close
saved on exit, an Escape quit did not). Guarded against an empty seat vector; pure Game state, headless-testable.

Test `#276` in `test_input_return_to_main_menu.cpp` drives an Escape through a per-mode handler on a headless `Game(true)`
with `HOME` redirected to an isolated temp dir (so `Player::getSavePath()` writes there), then reads the profile back: it
failed before the fix (`fs::exists` false, load fell back to default bankroll 1000 != 98765) and passes after. Full suite
276/276 (+1). §11 B7 row moved Open → Resolved with this hash; no JSON-schema change, no new profile field.

### 2026-11-06 — B6 (rules-panel keyboard edit is unthrottled / multi-step per tap) resolved

Added false->true edge detection to the rules-panel case in `Game::update()` (`src/Game.cpp:418-433`): kept previous-frame
held state (`rulesRightPrev_` / `rulesLeftPrev_`) so `applyGameRuleEdit` fires only on a press, making one tap exactly one
step at any framerate instead of once per frame (the pre-fix `if (rightHeld || leftHeld)` re-edited every uncapped-fps frame).
Up/Down highlight navigation keeps its held-key repeat -- repeating the highlight is desirable there, so only Left/Right editing
was changed and that decision is called out explicitly in the commit. A headless test seam (`setRulesKeyHeldForTest` /
`resetRulesKeyOverrideForTest`) lets update() read injected held-key state instead of real hardware keys (default override off =
unchanged `isKeyPressed()`).

Test `#277` in `tests/test_rules_panel_editing.cpp` holds Right down and drives `update()` for 25 frames, asserting the deck
count equals holding it for ONE frame (held-N == held-1). It failed before the fix (`4 == 6`: 25 forward steps through the
{1,2,4,6,8} brackets wrap back to start) and passes after. CMakeLists.txt registers the new test source. §11 B6 row moved Open
-> Resolved with this hash; no schema change, no new profile field.

### 2026-11-06 — citation drift reconciled against current source (accumulated across recent 0.4 commits)

Recent 0.4 code changes shifted many later `file:line` numbers (`src/Game.cpp` / `src/InputHandler.cpp`) — B4's gameRules
additions, then B6/B7 this pass — and the docs were not re-verified after each commit, so citations drifted. Every actionable
citation below was checked against the current tree; the post-arrow numbers are now correct. The historical recheck-list prose in
§1 (:39) still records its pre-shift audit and is left as that record — only post-audit drift has been corrected here. Verified unchanged (sits before the inserted code): banner controls `Game.cpp:162`,
`run()` while-loop `189`, main→run entry `185`.

- Reko seed / `resetShoeAnalytics()` (B4): `Game.cpp:254` → **277** (`resetShoeAnalytics` moved +23 when
  `persistOnExit()` was inserted before it). `Deck.cpp:73` unchanged — verified.
- B6 rules case: `Game.cpp:383-388` → **418-433** (old polling block gone; now edge-gated at 420-433).
- `activeTrueCount()` / `"True Count == running count"` (B4 §6.5, U5): `Game.cpp:483` → **1012**.
- B5 `placeSideBet()`: def `1130`→**1174**, affordability check `1134`→**1180**.
- B10 `maxSplitHands` enforcement / `Game::split()`: `Game.cpp:777` → **821**; the cleared-item handActions note
  (NEXT_STEPS :156) `split()` `766` → **810**.
- Shoe-boundary save sites: mid-shoe addShoeRecord `1340`→**1384**, saveProfile **1386**; end-of-shoe addShoeRecord
- B16 `deck.getCards()`: `Game.cpp:276`/**1349** → **299**/**1393**.

### 2026-11-06 — progress reconciliation (assessed git tree, HEAD `356fc31`)

The prior §13 entries all describe the *fixes* that landed; none reconciled the ledger against where things stand
now. This pass re-read the tracker + evidence and corrected drift found there. Method: full `ctest -j8`, then verified
each Resolved claim against source (not the table). Baseline is **277/277**.

- **9 defects are now Resolved** with dedicated regression tests — B2 (`f3aeb1f`), B3 (`c7978f1`), B4 (`3924b56`),
  B6 (`584aa4b`), B7 (`11b187b`), B12 (`31a3703`), B13 (`387a3e0`), B14 (`9d1d125`), B15 (`9a89185`). Open:
  **B1, B5, B8, B9, B10, B11** plus the UI set U1–U10 and tech-debt T1, T2, B16. Of the reds left, **B1 is the last
  one a player meets in a normal session** (tournament overwrites the career profile) — natural next target once the
  lower-severity items are triaged.
- **§6.10 (B13 proof) was written as if B13 were still open.** It said `/tmp/hist_proof` "exits 1 while B13 is open"
  and showed a sample output of hands finishing 8 then **6** ("BUG"). Updated: the block now clearly labels that as the
  *pre-fix* finding, states hist_proof exits **0** once B13 is Resolved, drops the stale `src/Serialization.hpp` copy note
  (B15 unified to one `include/` copy), and points at the regression test it now backs — `test_profile_settings.cpp#273`.
  If you run the proof after touching this schema it should exit 0; a lingering exit 1 means you edited the wrong file.
- **§5 `currentHistory` row + paragraph described pre-fix behaviour.** Top-level-key row moved `✘ never` →
  `✔ (additive)`, noting the guarded-load migration and test #273. The "why that design loses data" paragraph is now in
  the past tense with B13's fix and its guardrail (§6.10 / #273) called out, so a reader does not think silent career-data
  loss still happens.
- **§11 test IDs had drifted.** B13 was `#270`; now it is **#273** (a uniform +3 shift — three more Phase-0 regression
  tests were added). B14 was `#229/#230`; now **#232/#233**. §13:547–548 still cite the historical IDs, so a block there
  flags them as stale and names the current ones. If you grep `ctest` by `270` you will hit an advisor-DAS test, not B13.
- **§1 recheck list cited two post-audit-stale numbers.** `Game.cpp:483` (`activeTrueCount`) → now **1012**; `Game.cpp:766`
  (`Game::split()`) → now **810**. Both shifted after the original audit (B4/B6), which §13 already records; the "trust the
  line numbers" callout in §1 is corrected to current values with a one-line pointer back to that record.
- **§1 status + §2 framing were stale.** Status callout now names HEAD `356fc31` and suite 277/277; §2's "at least eight
  defects, four of them B1–B4" is updated to the current composition (nine Resolved, remaining reds named) while keeping
  its core point — the green suite hides defects in the seam the headless contract never exercises.

**Confirmed accurate this pass:** every `file:line` citation listed in §1 against HEAD; §5 two-tier read contract and
`gameRules` key list; §6.1–§6.9 evidence; B8 (`Deck.cpp:123-137`) still open; §11 open rows for B5/B8/B9/B10/B11 match
source (B10 `maxSplitHands` enforced at `Game.cpp:821`, Banner mislabels keys at `Game.cpp:162-163` vs bindings
`InputHandler.cpp:595-606`). No code changed.

  `1499`→**1543**, saveProfile **1545**.
- B3 `syncBettingAndSideBet()`: `Game.cpp:451` → **485**.
- InputHandler window-close `Event::Closed`: `247-257` → **251-258**; Restart addShoeRecord/saveProfile `102/104` →
  **106/108**; QuitToMenu `115/119` → **119/123**.
- B16 `deck.getCards()`: `Game.cpp:276`/**1349** → **299**/**1393**.

### 2026-11-07 — B9 flush fix + mark B1 Resolved; corrected stale §5 Serialization.hpp copy text; noted #256 flake

- **B9 (`d6d6436`).** `Logger::log()` now flushes per record: the bare `"\n"` became `std::endl` (one newline, unchanged format) so each line is pushed to std::cout immediately. Fixes the unflushable-logger diagnosability defect (§6.7/§6.8): under Xvfb a line otherwise sat in the stream buffer and was lost if the process died or never hit a natural flush. New regression test `tests/test_logger.cpp` (registered in CMakeLists §8 rule 3) swaps cout's rdbuf for a sink that releases characters only on sync(), proving the flush is synchronous (fails before, passes after); a below-threshold guard locks the level gate (`level < currentLevel`). Full suite 284/284.
- **B1 → Resolved.** The `5107aa7` fix (load real profile into seat 0, mark non-persistable for the tournament run) is now reflected in §11: B1 row → Resolved with hash + test #228. Test #228 verified passing — a Tournament visit/close no longer overwrites the career profile.
- **§5 drift corrected.** §5's opening said "two copies of Serialization.hpp, both live"; that is pre-B15 text. `src/Serialization.hpp` was deleted by B15 (`9a89185`) and only `include/Serialization.hpp` remains — verified with `g++ -I include -M src/PlayerProfile.cpp | grep Serialization.hpp` → the include copy. Replaced the stale table + verification snippet and stated the "edit the wrong copy" trap is closed.
- **#256 (known flake).** "StrategyTrainer: auto-dealt questions never include a natural blackjack" fails nondeterministically (≈1 in ~15 full `-j8` runs; passes reliably isolated). Neither `src/StrategyTrainer.cpp` nor its test changed since audit (`356fc31`) → not a B9 regression. Cause: expected natural-hand rejections deplete the 6-deck shoe mid-loop in `nextQuestion()`, which can leave a lingering natural while `sessionActive` stays true (also a rare production edge case). Left unfixed this round; read the suite as 284/284 with #256 the one known flake.

### 2026-11-07 — headless-testability classification of the open §11 items

Classified each remaining §11 item by how hard it is to verify without an SDL window, so the next window knows which
need escrow-at-commit (§4 rule 2) vs code-first tests. Grounded against source, not assumed:

- **U1–U10 (UI/UX rendering) — HARDEST / not truly headless.** Every item needs an SDL window context and a
  screenshot/Xvfb image diff to assert anything visual; no pure unit test can assert pixel-level rendering and diffs are
  flaky. These are the only items genuinely difficult headlessly. Verify with Xvfb + screenshots, or escrow-at-commit.
- **B8 (`Deck::draw()` empty-guard) — actually COVERABLE.** Two unguarded `back()`/`pop_back()` pairs at Deck.cpp:125-133,
  reached only when `cards` is empty. `Deck::setCards(std::vector<Card>)` (a public test harness in Deck.hpp) deals an
  arbitrary shoe, so `setCards({})` + draw() reaches the path directly; a defensive unit test asserting no UB/crash covers it.
- **T1 (save amplification) — MEDIUM.** Feasible but needs byte-counting output interception (custom streambuf/ofstream sink)
  to prove serialized size is stable across repeated saves — more involved than a pure unit test.
- **T2 (unbounded history, HandEvaluator.cpp:297) — MEDIUM.** Feasible headlessly but first requires choosing + implementing
  a cap on career-stats/history growth before there is anything to assert; the test then verifies the cap holds after many rounds.
- **B16 (`reconstructDeckAtRound`, GameRecord.hpp:307) — EASY.** Zero callers (sole occurrence is its definition); confirm via
  grep/static-analysis, remove, re-run full suite. Trivial, low-risk, fully CI-verifiable.
- **B10 (`maxSplitHands`) — EASY.** Pure data-model change: round-trip through to_json/from_json, assert the value + schema key
  survive, then assert the restricted gameRules UI values. Cleanly unit-testable.

### 2026-11-07 — B16 (dead reconstructDeckAtRound removal) fixed

Removed `reconstructDeckAtRound()` from GameRecord.hpp:307 after confirming via static analysis that its sole occurrence was
its own definition — zero callers anywhere in include/src/tests. Deletion is behaviour-preserving, and it also removes a
function whose card-drop order (removing dealer cards then player hands per round) was wrong for no caller. Full suite
unchanged at 289/289; the only way to break this would be an external TU calling the inline function, which grep rules out.

### 2026-11-07 — B10 (serialise + expose maxSplitHands) fixed

`maxSplitHands` was a dead config: enforced at Game.cpp:821 / RoundEngine.cpp:124 but absent from `to_json` and with no UI
row. Made it a full schema field and exposed it restricted to exactly {2, 3, 4, Unlimited}:

- Serialization: added `maxSplitHands` to `to_json`; `from_json` reads it then clamps any stored value into the allowed set
  {2,3,4,-1} (-1 == unlimited), falling back to the struct default for absent or invalid values (additive on load).
- UI: new `GameRuleId::MaxSplitHands`, a `gameRuleRows()` entry and a `gameRuleDescription()`. `applyGameRuleEdit` cycles
  `SPLIT_HAND_LIMITS[] = {2, 3, 4, MAX_SPLIT_HANDS_UNLIMITED}` (-1 == unlimited) with no out-of-range value reachable.
- Enforcement: the -1 sentinel is guarded explicitly at both sites (`maxSplitHands > 0 && ...` at Game.cpp:821; a
  `resplitCapOpen` guard at RoundEngine.cpp:124) so an unsigned size_t-vs-int comparison can never read unlimited as a large
  finite cap.

New tests/test_max_split_hand_rules.cpp (#278–#282): schema round-trip + load-time clamp fail before the fix; restricted-value
cycling (visits only {2,3,4,-1}) and unlimited-vs-finite-cap enforcement verified headlessly. CMakeLists registers the new TU.
Full suite 289/289.

### 2026-11-07 — B8 (Deck::draw() empty-shoe guard) fixed + all §10 decisions resolved

What was wrong: `Deck::draw()` had two unguarded `cards.back()`/`pop_back()` pairs — one when the shoe is already
empty, and a second after only the cut card was drawn (a single-card shoe whose one card is the cut card). Both are
undefined behavior on an empty std::vector. Reachable headlessly via `Deck::setCards({})`; every production caller guards
`deck.empty()` before calling draw(), so real gameplay never hit it (hence "pre-existing, known" since 0.3 §6).

Fix: guarded `draw()` — entry returns `Card{}` for a truly empty shoe; before the second `back()` it now checks
`cards.empty()` and returns the cut card when nothing remains. The `Card` return type is unchanged and all 5 callers
(`Game.cpp:1421`,`1437`; three in `StrategyTrainer.cpp`) keep working unchanged; only degenerate empty-draw cases change
from UB to a benign default (purely defensive, no gameplay regression).

Docs: §11 B8 row → Resolved (`11f6f50`); 2 headless tests added in `tests/test_deck_empty_draw.cpp` (#13/#14) registered
via CMakeLists.txt — they SEGFAULT (empty deck) / returned garbage value 33 != CUT_CARD_VALUE(14) before the fix and PASS
after. Suite 289 → 291/291 (0 failures).

Also this round: resolved all seven §10 open decisions in CONTEXT.md §5's decision list — #1 tournament persistence →
option (b) record a SEPARATE tournament history (B1 currently preserves the career profile via gating, i.e. option (a);
the separate store is a follow-up feature), #2 escrow at commit (done in B5, `4f85d37`), #3 starting bankroll $10,000
everywhere, #4 maxSplitHands exposed to {2,3,4,Unlimited} (done in B10, `69cd9e8`), #5 console control banner keep trimmed
along the bottom middle (unblocks B11; label fix + repositioning queued as follow-ups needing a visual/Xvfb check), #6
name-entry screen in scope for 0.4 (a UI/lifecycle task affecting U7 — automatic before main menu, then reachable from main
menu to view player stats), #7 achievement-toast lifetime 4s (done in B2, `f3aeb1f`). Corrected a minor citation drift: the
B1 saveProfile site cited in NEXT_STEPS working rules was `SessionLifecycle.cpp:287`; current line is :293 (+6) — that table
is not yet corrected.

### 2026-11-07 — U7 name-entry screen + player-name persistence fixed

What was wrong: `profile.name` was never set (always ""), so a player's identity never round-tripped through save/load — U7. Per §10 #6 this round adds the "enter your name before hitting main menu" screen and commits the name to seat 0's display + persisted profile.

Fix: core logic in `Game` (headless-testable): `nameEntryAppendChar`/`nameEntryBackspace` edit a buffer clamped to `MAX_PLAYER_NAME_LENGTH` (Constants.hpp), `nameEntryText()` reads it, `needsNameEntry()` gates the one-time startup prompt on whether the profile has no name, and `commitPlayerName` updates seat 0's display + `profile.setName(...)` then `saveProfile()`. Real-mode wiring: new `AppMode::NameEntry` case in the InputHandler dispatch switch + `processNameEntryEvents` (sf::Event::TextEntered capture via `.unicode`, Enter/Space commit, Esc cancel+clear, Backspace), and a `drawNameEntry` row in the GameRenderer dispatch table.

Docs: §5 decision list #6 → implemented; §11 U1-U10 row → Partial (U7 Resolved); new entry here. NEXT_STEPS.md checked for stale "not implemented this round" claims and citation drift (the SessionLifecycle save-site :287→:293 noted in the B8 entry is corrected). Tests `tests/test_name_entry.cpp` (#286–#288) registered via CMakeLists.txt — buffer clamp, gating predicate, commit persistence round-trip all FAIL before impl and PASS after. Full suite 289 → 294/294 (0 failures). Part B ("Player Stats from main menu") satisfied by the existing Statistics→StatsDashboard reuse (no new view; covered by test_menu_navigation.cpp).
