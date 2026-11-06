# CONTEXT — Blackjack2 Phase 0.4 (QA hardening)

**Purpose:** Self-contained background for the phase-0.4 defect-removal pass. Read this **before**
`.summaries/0.4/NEXT_STEPS.md`, which is the actionable ledger. This file explains *how* the findings were
established, what evidence backs them, what was investigated and cleared, and which project rules constrain the
fixes. It supersedes nothing in `.summaries/0.3/` — 0.3 remains the authority on architecture and feature
history; this file is the authority on current defects.

**Provenance:** every finding below came from *playing the built game*, not from reading it. Source reading was
used only afterwards to explain what was observed. Where a claim is code-only (not reproduced on screen) it is
labelled as such.

**Status: audited 2026-11-06 against `96fd416` on branch `3.1`.** Baseline re-measured at 264/264 passing; every
`file:line` citation re-checked and correct; the B13 proof re-run and reproducing. Several *structural* claims in
the first revision were wrong and have been corrected in place — the audit log in §13 lists exactly which, so you
can tell a verified claim from an inherited one. Read §13 before treating any statement here as settled.

---

## 1. Project snapshot

- **What:** Desktop blackjack simulator — Kelly betting, Hi-Lo/Reko counting, basic-strategy advisor, per-shoe
  analytics, replay/shoe history, player profiles, side bets, strategy trainer, tournament, achievements.
- **Stack:** C++17 for the game, C++20 for the tests (`CMakeLists.txt:4` and `:198`); SFML 3.x — `CMakeLists.txt`
  requires ≥3.0 and this machine has **3.1.0** (`find_package` reports "Found SFML 3.1.0"); CMake ≥3.10; Catch2
  **v3.6.0** pulled by `FetchContent` (`CMakeLists.txt:124-130`); vendored nlohmann/json in `include/nlohmann`.
- **Build:** `cmake --preset "GCC 13.3.0 x86_64-linux-gnu"` → `out/build/GCC 13.3.0 x86_64-linux-gnu/`.
- **Test:** `ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu'`. Baseline **264/264 pass**.
- **Entry:** `main.cpp` → `Game::run()` (`src/Game.cpp:185`).
- **Save file:** `$HOME/.local/share/blackjack/saves/player_profile.json`
  (`getSavePath()`, `src/Player.cpp:14-25`; falls back to `saves/player_profile.json` if `HOME` is unset).
- **Window:** 1600×900 default, resizable (`Constants::WINDOW_WIDTH/HEIGHT`).

**The working tree is clean, and every line number in these two docs is accurate against `HEAD`.** An earlier
revision of this file warned that 11 files carried uncommitted Tournament-refactor changes and that line numbers
referred to the working tree rather than `HEAD`. That work has since landed as **`96fd416 "new horizons"`** on
branch `3.1`, together with these three documents. On **2026-11-06** every `file:line` citation in this file and
in `NEXT_STEPS.md` was re-checked against that commit; the citations held, including the fiddly ones
(`Game.cpp:483`, `Game.cpp:766`, `Game.cpp:1340`/`:1499`, `RoundDealer.cpp:35`/`:45`/`:46`,
`GameRenderer.cpp:1020-1023`, `GameRecord.hpp:307`, `PlayerProfile.hpp:66`, `Constants.hpp:270`). Trust the line
numbers. What did *not* hold were several structural claims, and those are corrected in place — see §13.

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
falls back to `Constants::PLAYER_FILE_FALLBACK` = `saves/player_profile.json` when `HOME` is unset,
`include/Constants.hpp:239`). Written by `PlayerProfile::save()` (`src/PlayerProfile.cpp:50-95`), read by
`PlayerProfile::load()` (`:28-48`), shaped by `to_json`/`from_json` (`include/Serialization.hpp:191-231`).

**⚠ There are two copies of this header, and both are live.** `src/Serialization.hpp` (236 lines) and
`include/Serialization.hpp` (231 lines) are code-identical today — strip whole-line `//` comments
(`grep -v '^\s*//'`) and the two files diff **empty** at 211 lines each. So nothing is broken, which is precisely
why this is dangerous: the divergence is invisible until someone edits one copy.

The split is **worse than the first draft of this document said.** It is not "the game vs. one achievement test" —
the two tests that actually lock the save schema both read the `include/` copy:

| TU | Resolves to | Why |
|---|---|---|
| `src/PlayerProfile.cpp` — the only TU that puts the schema into the **shipped binary** | **`src/Serialization.hpp`** | quoted includes check the includer's own directory first |
| `tests/test_profile_settings.cpp` | **`include/Serialization.hpp`** | `#include <Serialization.hpp>`; no copy in `tests/`, so it falls through to `-I include` |
| `tests/test_game_rules.cpp` | **`include/Serialization.hpp`** | same |
| `tests/test_achievements.cpp` | **`include/Serialization.hpp`** | same |
| `.summaries/0.4/hist_proof.cpp` (§6.10) | **`src/Serialization.hpp`** | it compiles `src/PlayerProfile.cpp` directly |

The practical consequence for this phase: **a B13 or B12 fix written into `include/Serialization.hpp` turns the
suite green while the shipped binary stays byte-for-byte unchanged.** The proof program in §6.10 reads the *other*
copy, so it would keep reporting the bug after a "successful" fix. Verify with:

```bash
g++ -std=c++17 -I include -M src/PlayerProfile.cpp | tr ' ' '\n' | grep Serialization.hpp
# -> src/Serialization.hpp
```

**Resolve B15 before touching this schema.** See NEXT_STEPS B15.

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
`resplitAcesAllowed`, `resplitTensAllowed`, `surrenderType`. The per-field hooks live in `include/GameRules.hpp`
(ADL'd from the delegation at `Serialization.hpp:212/238`). `maxSplitHands` is a real field enforced at
`src/Game.cpp:777` but is **absent from `to_json`** and has no UI row — dead config (B10). Spelling: B12 renamed the
written key to `resplitTensAllowed`; on load the corrected key wins, with the legacy typo'd `resplitTentsAllowed`
retained only as an additive migration fallback so pre-B12 saves keep loading.

**Decisions to record here as you work** (add a dated bullet per decision):
- Whether Tournament progress persists at all, and where that gate lives (B1).
- Whether the main bet escrows at commit or at settlement (B5).
- Whether `maxSplitHands` is exposed or deleted (B10).
- The canonical starting bankroll for human and NPC seats (U8).
- B12 additive-migration strategy (resolved 2026-11-06): `resplitTentsAllowed` was misspelled in every save file, so the rename to `resplitTensAllowed` had to be additive — `to_json` writes only the corrected key, and `from_json` reads it first then falls back to the legacy typo'd key (new wins when both present) so pre-B12 saves keep loading. Honours the 0.3 schema contract: every existing field stays optional with a default; no profile is ever wiped by the rename.

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
/tmp/hist_proof   # exits 1 while B13 is open
```

Re-run **2026-11-06** at `96fd416`; output is byte-identical to the original finding:

```
reload #1: handsFinished=8  shoes=1   (expected 8)
reload #2: handsFinished=6  shoes=1   (expected 9)
BUG: hands silently lost from career stats
```

The program: add 5 rounds → `addShoeRecord()` → add 3 rounds → `save()` → `load()` → add 1 round → `save()` →
`load()`. The career hand count goes **backwards** across a save that added a hand. Port this verbatim into
`tests/test_profile_settings.cpp` as the regression test.

*Note:* this proof originally lived at `/tmp/hist_proof.cpp` and did not survive a reboot — that is why it is now
checked in beside these docs. Two things to keep in mind when you run it:

- It compiles `src/PlayerProfile.cpp`, so it exercises **`src/Serialization.hpp`**, not the `include/` copy the
  test suite reads (§5). After the B13 fix lands, re-run it to confirm the *shipped* schema changed and not just
  the tested one.
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

**Re-audited 2026-11-06 at `96fd416`: nothing has landed. All 15 defects (B1–B15) plus U1–U10, T1–T2 and B16 are
still open.** Each row below was re-checked against the source rather than trusted from the previous revision —
the "Evidence" column now says what the 2026-11-06 check actually was, which for several items is weaker than the
word "reproduced" implied.

**Resolved since:** `B3` (`c7978f1`) and `B15` (`9a89185`) landed on 2026-11-06; their rows below are updated to
**Resolved**. Every other row here is still open — see the §13 audit log for what changed.

| ID | Sev | Title | Status | Evidence as of 2026-11-06 |
|---|---|---|---|---|
| B1 | 🔴 | Tournament overwrites career profile | **Open** | Mechanism re-confirmed in code: `SessionLifecycle.cpp:34` `players.clear()` + `:35` NPC ctor. Playtest repro not re-run this pass |
| B2 | 🔴 | Achievement toast never clears | **Open** | Re-confirmed by grep: one write (`AchievementFeeder.cpp:48`), one read (`GameRenderer.cpp:154`), zero clears; no `achievementToastTimer` symbol exists |
| B3 | 🔴 | Side bet always reports "WON" | **Resolved 2026-11-06 (`c7978f1`)** | Fixed in `include/SideBetLabels.hpp`: branch won/lost/push on net with the sign preserved; `GameRenderer.cpp:997` calls the pure fn. Guard test in `test_side_bets.cpp` asserts WON +20 / LOST -20 / PUSH 0 (fails before fix). Suite 266/266 |
| B4 | 🔴 | Reko seed ignores deck count | **Open** | Re-confirmed: `Game.cpp:254` and `Deck.cpp:71` both use `Constants::NUM_DECKS` (6) |
| B13 | 🔴 | Mid-shoe quit deletes those hands from career stats | **Resolved 2026-11-06 (`387a3e0`)** | Persist in-progress shoe rounds (currentHistory) so quitting mid-shoe still books them into career stats; guarded read. Test #270 passes; hist_proof.cpp (§6.10) confirms shipped schema. §11 listed it Open after the commit and was corrected here |
| B5 | 🟠 | Main bet not escrowed; bankroll can go negative | Open | `Player.cpp:89` still a bare affordability check; no `getAvailableBankroll` in the tree. Negative balance not re-reproduced on screen |
| B6 | 🟠 | Rules panel key repeat | **Open** | `Game.cpp:383-388` still polls held keys with no edge detection |
| B7 | 🟠 | Escape-quit skips the save | **Open** | No `persistOnExit`; save still only in the `Event::Closed` branch (`InputHandler.cpp:247-257`) |
| B8 | 🟠 | `Deck::draw()` no empty guard | Known (0.3 §6) | Two unguarded `back()`/`pop_back()` pairs confirmed at `Deck.cpp:121-135` |
| B9 | 🟠 | Flaky crash + unflushable logger | Open — crash unrepro | `Logger.cpp:34` still writes `std::cout` with no flush/unitbuf |
| B14 | 🟠 | Non-atomic save; corrupt file → silent wipe | **Resolved 2026-11-06 (`9d1d125`)** | Atomic write + corrupt-save rescue under a `.corrupt.<ts>` suffix, and partial-JSON field retention in `PlayerProfile.cpp`. Tests #229/#230 pass. §11 listed it Open after the commit and was corrected here |
| B15 | 🟠 | Two live copies of `Serialization.hpp` | **Resolved 2026-11-06 (`9a89185`)** | Deleted `src/Serialization.hpp`; exactly one copy remains (include/), enforced by a guard test that fails with two / passes with one. No CMake edit needed; `PlayerProfile.cpp` now resolves to the include copy, unblocking every schema edit (B12/B13/B14) |
| B10 | 🟡 | `maxSplitHands` dead config | Open | Enforced at `Game.cpp:777` / `RoundEngine.cpp:121`; absent from `to_json` and `gameRuleRows()` |
| B11 | 🟡 | Control banner mislabels keys | Open | `Game.cpp:162-163` vs real bindings `InputHandler.cpp:591-602` |
| B12 | 🟡 | `resplitTentsAllowed` typo in save schema | **Resolved 2026-11-06** | Renamed field+enum to `resplitTensAllowed`/`ResplitTens`; `to_json` writes the corrected key, `from_json` reads it then falls back to the legacy typo'd key (new wins). On-disk writer verified via `test_profile_settings.cpp:232` using real `PlayerProfile::save()` — shipped binary now writes `resplitTensAllowed`. Suite 272/272 |
| B16 | 🟡 | `reconstructDeckAtRound()` dead and wrong | Open | Still zero callers; sole occurrence is its own definition at `GameRecord.hpp:307` |
| U1–U10 | 🟡 | UI/UX set (see NEXT_STEPS Phase 4) | Open | Not re-verified on screen this pass; cited renderer lines all resolve to the claimed functions |
| T1–T2 | 🟢 | Save amplification; unbounded history | Open | `HandEvaluator.cpp:297` still saves every settled round |
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
