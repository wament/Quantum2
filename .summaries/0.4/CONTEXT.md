# CONTEXT — Blackjack2 Phase 0.4 (QA hardening)

**Purpose:** Self-contained background for the phase-0.4 defect-removal pass. Read this **before**
`.summaries/0.4/NEXT_STEPS.md`, which is the actionable ledger. This file explains *how* the findings were
established, what evidence backs them, what was investigated and cleared, and which project rules constrain the
fixes. It supersedes nothing in `.summaries/0.3/` — 0.3 remains the authority on architecture and feature
history; this file is the authority on current defects.

**Provenance:** every finding below came from *playing the built game*, not from reading it. Source reading was
used only afterwards to explain what was observed. Where a claim is code-only (not reproduced on screen) it is
labelled as such.

---

## 1. Project snapshot

- **What:** Desktop blackjack simulator — Kelly betting, Hi-Lo/Reko counting, basic-strategy advisor, per-shoe
  analytics, replay/shoe history, player profiles, side bets, strategy trainer, tournament, achievements.
- **Stack:** C++17 (tests C++20), SFML 3.0 (Graphics/Window/System/Audio/Network), CMake ≥3.10, Catch2 v3.6,
  vendored nlohmann/json in `include/nlohmann`.
- **Build:** `cmake --preset "GCC 13.3.0 x86_64-linux-gnu"` → `out/build/GCC 13.3.0 x86_64-linux-gnu/`.
- **Test:** `ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu'`. Baseline **264/264 pass**.
- **Entry:** `main.cpp` → `Game::run()` (`src/Game.cpp:185`).
- **Save file:** `$HOME/.local/share/blackjack/saves/player_profile.json`
  (`getSavePath()`, `src/Player.cpp:14-25`; falls back to `saves/player_profile.json` if `HOME` is unset).
- **Window:** 1600×900 default, resizable (`Constants::WINDOW_WIDTH/HEIGHT`).

**Working tree is dirty — every line number in these two docs refers to the working tree, not `HEAD`.** At the
time of writing, 11 files carry uncommitted changes (584 insertions / 465 deletions), mostly the Tournament
refactor: `include/{Game,GameRenderer,Tournament}.hpp`, `src/{Game,GameRenderer,InputHandler,RoundEngine,
SessionLifecycle,Tournament}.cpp`, `tests/test_{rendering,tournament_mode}.cpp`. Two of the files this phase must
change — `src/SessionLifecycle.cpp` (B1) and `src/GameRenderer.cpp` (B2/B3/U1–U3) — are in that set. Commit or
stash that work before starting 0.4, or the diffs will be hard to read.

## 2. Posture change entering 0.4

Phases 0.0–0.3 were feature delivery then refactor. 0.4 is **defect removal**. Feature work stays frozen.

The defining fact of this phase: **the test suite is fully green while the game has at least eight user-visible
defects.** Four of them (B1–B4) are things a player meets in a normal session. That is not a criticism of the
tests — it is a statement about *where* they look. See §3.

Working rule for the phase: **a fix is not done until a test exists that fails without it.** Several of these
bugs are one-line changes that would otherwise be trivially re-broken.

## 3. The testing blind spot (read this first)

The 0.3 testing contract is: *test pure logic headlessly through `Game(bool headless)` + `setDeck(...)` and read
results via `*ForTest()` accessors; keep presentation out of the test binary.* That contract is good and has
produced 264 meaningful tests. But it deliberately excludes exactly the layers where these bugs live:

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
falls back to `saves/player_profile.json` when `HOME` is unset). Written by `PlayerProfile::save()`
(`src/PlayerProfile.cpp:50-95`), read by `PlayerProfile::load()` (`:28-48`), shaped by
`to_json`/`from_json` (`include/Serialization.hpp:193-231`).

**⚠ There are two copies of this header, and both are live.** `src/Serialization.hpp` (236 lines) and
`include/Serialization.hpp` (231 lines) are code-identical today — `diff` shows comment-only divergence — but
`src/PlayerProfile.cpp` compiles the `src/` copy (quoted includes check the includer's directory first) while
`tests/test_achievements.cpp` compiles the `include/` copy. Editing the wrong one makes a test pass against a
binary that never changed. **Resolve B15 before touching this schema.** See NEXT_STEPS B15.

**Top-level keys actually written today** (verified against a live file):

| Key | Written | Read | Notes |
|---|---|---|---|
| `name` | ✔ | `j.at()` **throws if absent** | always `""` — nothing ever sets it (U7) |
| `bankroll` | ✔ | `j.at()` **throws if absent** | default 1000 (`PlayerProfile.hpp:66`) |
| `stats` | ✔ | `j.at()` **throws if absent** | 24 fields; newer ones use `j.value()` defaults |
| `longHistory` | ✔ | `j.at()` **throws if absent** | completed shoes only; grows forever (T2) |
| `preferredPlayerCount/Speed/CountingStyle` | ✔ | `j.value()` + default | |
| `preferredSoundVolume/MusicVolume/Muted` | ✔ | `j.value()` + default | |
| `unlockedAchievementIds` | **only if non-empty** | `j.value()` + `{}` | |
| `gameRules` | ✔ | guarded `contains()` | nested object, 6 keys |
| `currentHistory` | ✘ **never** | — | **B13** |

**Two-tier read contract.** Four core fields use `j.at()` and therefore *throw*; every additive field uses
`j.value(key, default)` and degrades gracefully. The additive half is deliberate and well documented in the
source comments. The `at()` half is what turns a truncated file into a total wipe (B14).

**"The record is the source of truth."** `save()` does not write the in-memory stats verbatim. When
`longHistory` is non-empty it recomputes every record-derivable stat via `careerStatsFromHistory()`
(`src/PlayerProfile.cpp:68-91`) and overwrites them, keeping only the non-recordable trackers
(`splits`, `doubles`, `betsPlaced`, `betSum`, `betDistribution`, strategy counters) as-is. The intent — a saved
profile can never drift from what was actually played — is good, and `tests/test_stats_batching.cpp` proves the
recompute matches the live per-hand increments.

**Why that design still loses data (B13).** `careerStatsFromHistory()` folds the in-progress shoe
(`currentHistory`) in as a synthetic `completed = false` shoe, so the numbers are right *in session*. But
`currentHistory` is not serialised, so after a reload the recompute runs on `longHistory` alone and the next save
overwrites good numbers with smaller ones. Two individually-correct decisions compose into silent data loss.
Proven in §6.10.

**`gameRules` nested object** — written keys: `numDecks`, `dealerStandsOnSoft17`, `doublesAfterSplitAllowed`,
`resplitAcesAllowed`, `resplitTentsAllowed`, `surrenderType`. `maxSplitHands` is a real field enforced at
`src/Game.cpp:777` but is **absent from `to_json`** and has no UI row — dead config (B10). Note the spelling:
`resplitTentsAllowed`, "Tens" misspelled, baked into every save file (B12).

**Decisions to record here as you work** (add a dated bullet per decision):
- Whether Tournament progress persists at all, and where that gate lives (B1).
- Whether the main bet escrows at commit or at settlement (B5).
- Whether `maxSplitHands` is exposed or deleted (B10).
- The canonical starting bankroll for human and NPC seats (U8).

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

`/tmp/hist_proof.cpp` drives the real `PlayerProfile` with no game, no display and no test framework:

```bash
cd /home/william-ament/Documents/Quantum2
g++ -std=c++17 -I include -I /usr/local/include /tmp/hist_proof.cpp \
    src/PlayerProfile.cpp src/Logger.cpp \
    -o /tmp/hist_proof -L/usr/local/lib -lsfml-graphics -lsfml-system
/tmp/hist_proof
```

Output:

```
reload #1: handsFinished=8  shoes=1   (expected 8)
reload #2: handsFinished=6  shoes=1   (expected 9)
BUG: hands silently lost from career stats
```

The program: add 5 rounds → `addShoeRecord()` → add 3 rounds → `save()` → `load()` → add 1 round → `save()` →
`load()`. The career hand count goes **backwards** across a save that added a hand. Port this verbatim into
`tests/test_profile_settings.cpp` as the regression test.

*Note:* `/tmp` does not survive a reboot. Re-create the file from this section if it is gone — the sequence above
is the whole program's logic.

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

**Verified on this machine:** `Xvfb` ✔, ImageMagick `import` ✔, `xwd` ✔, `convert` ✔, python3 +
`python-xlib 0.33` ✔. **`xdotool` ✘ and `scrot` ✘ are not installed** — don't write a repro recipe around them.

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

---

## 9. What is genuinely good (do not regress it)

A playtest is as much a confirmation as a critique. These all held up under adversarial use and should stay true
after the phase-0.4 changes:

- **Core blackjack logic is correct.** Deal, hit, stand, double, split, surrender, insurance, dealer policy and
  settlement all produced correct outcomes and correct bankroll arithmetic across many hands and multiple seats.
  The B3 and B5 findings are a *display* bug and a *timing* question respectively — neither is a settlement error.
- **The headless test suite is real.** 264 tests, ~8s, no flake observed, and they assert behaviour rather than
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
| 7 | How long should the achievement toast show? | suggested 4s | B2 |

Defaults if nobody answers: 1(a), 2(a), 3 = $1,000 everywhere, 4 = expose, 5 = keep trimmed + add Controls,
6 = defer, 7 = 4s. Record the final choice in §5's decision list.

## 11. Status tracker

Update this table as work lands; it is the first thing the next window should read.

| ID | Sev | Title | Status |
|---|---|---|---|
| B1 | 🔴 | Tournament overwrites career profile | **Open — reproduced** |
| B2 | 🔴 | Achievement toast never clears | **Open — reproduced** |
| B3 | 🔴 | Side bet always reports "WON" | **Open — reproduced** |
| B4 | 🔴 | Reko seed ignores deck count | **Open — reproduced** |
| B13 | 🔴 | Mid-shoe quit deletes those hands from career stats | **Open — proven standalone** |
| B5 | 🟠 | Main bet not escrowed; bankroll can go negative | Open — code-verified |
| B6 | 🟠 | Rules panel key repeat | **Open — reproduced** |
| B7 | 🟠 | Escape-quit skips the save | **Open — reproduced** |
| B8 | 🟠 | `Deck::draw()` no empty guard | Known (0.3 §6) |
| B9 | 🟠 | Flaky crash + unflushable logger | Open — crash unrepro |
| B14 | 🟠 | Non-atomic save; corrupt file → silent wipe | Open — code-verified |
| B15 | 🟠 | Two live copies of `Serialization.hpp` (game vs tests) | **Open — proven via `g++ -M`** |
| B10 | 🟡 | `maxSplitHands` dead config | Open |
| B11 | 🟡 | Control banner mislabels keys | Open |
| B12 | 🟡 | `resplitTentsAllowed` typo in save schema | Open |
| U1–U10 | 🟡 | UI/UX set (see NEXT_STEPS Phase 4) | Open |
| T1–T2 | 🟢 | Save amplification; unbounded history | Open |
| — | ✅ | `handActions` indexing | **Cleared — not a bug** |
| — | ✅ | Settlement math | **Cleared — correct** |

## 12. How to use these two files

- **This file (`CONTEXT.md`)** = why things are the way they are, what the evidence is, what not to touch.
- **`NEXT_STEPS.md`** = what to change, in what order, with the test to write for each.
- **`IDEAS.md`** = ten unscheduled feature propositions. **Not a plan.** Read it only after the defect phases are
  done, or not at all; nothing in it is committed work.

Work top to bottom through NEXT_STEPS' phases. For each fix: write the named test first (several are specified as
tests that *fail today* — that is the point), then the fix, then the verification gate, then update §11 here and
append any decision to §5. Keep commits one-bug-one-commit.

If a fix reveals that a finding here is wrong, **correct this file** rather than quietly working around it — the
next window trusts it.
