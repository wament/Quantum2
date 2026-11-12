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
B1, B5, B8, B9, B10, B11 — plus the UI/UX set U1–U12 and the tech-debt items T1, T2, B16 (all in §11). Of those,
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
| `currentHistory` | ✔ (additive) | `j.value()` + `{}` on load | **B13** — now serialised; guarded read keeps legacy mid-shoe-quit saves loading (§6.10, test #280) |

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
legacy mid-shoe-quit saves loading) and a test (`test_profile_settings.cpp#280`) asserting an in-progress shoe's
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
- **B9 overlay bounds guard** (resolved 2026-11-07, `facea14`): the summary-overlay renderer in `drawSummaryOverlay()` indexed the per-player value from `game.players[i]` (`GameRenderer.cpp:1025`) inside a loop bounded by `playerSummaries.size()` (`:1022`) without clamping, so it read out of range whenever live players exceeded the summaries drawn, and the per-row fan-width helper underflowed on an empty/single hand. Decided to clamp row count to `min(summaryCount, playerCount)` and make the fan-width helper return 0.f for 0/1 hands — a renderer-only bounds guard (no persisted field, no data-model change) that keeps summary rendering safe as presentation only; headless tests **#299/#300** (`tests/test_summary_overlay_bounds.cpp`; the §11 row's "#296/#297" is stale — see §13 2026-11-09) fail before the clamp and pass after. B9 stays **Partial** in §11 because the original intermittent single-occurrence crash is genuinely unreproducible and carries no test.
- **B17 test-isolation guard: one shared RAII `test_support::IsolatedHome`, PID-namespaced** (resolved 2026-11-09). Decided to centralise rather than patch five hand-rolled redirects: `Player::getSavePath()` reads `$HOME` from the environment, and ctest + Catch2 run each `TEST_CASE` in its **own process**, so any *fixed* temp dir (`/tmp/bj_b1_test`, `/tmp/bj_u8_test`, `/tmp/bj_b7_test`, `/tmp/bj_t1_autosave_home`, `/tmp/bj_name_entry_0`) is shared by every concurrently-scheduled test. The dir name embeds the **PID** (unique across processes) plus a per-process counter (unique across several guards in one case) — a counter alone is what broke the old guard, because a `static int` is per-process and therefore always 0. Restoration and removal happen in the destructor, so a Catch2 `REQUIRE` throwing cannot leak a redirected `HOME` into later cases in the same process (the reason `test_session_lifecycle.cpp` carried a local `HomeGuard`; it is deleted). Test-only: no product code, no persisted field, no schema change. Also decided to audit at **`TEST_CASE` granularity**, not per file — a file containing one isolated case can still contain nine unisolated ones, and those nine were writing the developer's real career profile.
- **B18 determinism seam: seed the shuffle, never change it** (resolved 2026-11-09). Chose to add `Deck::shuffleWithSeed(seed)` alongside the existing `shuffle()` rather than to seed `shuffle()` itself: production keeps `std::random_device`, so real shoe order stays unpredictable, and both entry points share one private `shuffleWith()` body so the two paths cannot drift. Same pattern for `StrategyTrainer::startSessionWithSeedForTest()` (shared `startSessionWith()`) and `Game::startNewShoeWithSeedForTest()` (shared `startNewShoeWith()`), plus `StrategyTrainer::setDeckForTest()` for exact-shoe injection. Tests that need a *real shuffled* shoe (not an injected card list) now sweep a fixed seed list; #306's assertion was retargeted from driver rounds to the settled-hand count the autosave gate actually counts. No new persisted field; `*ForTest()` convention preserved.
- **B17 hermeticity: isolate at PROCESS scope, not per TEST_CASE** (resolved 2026-11-09, follow-up). The original fix redirected five files; a serial bisect found Test #99 (`tests/test_settlement.cpp`) still rewriting the developer's real 9.3 MB career profile, with 68 round-driving `Game(true)` cases across eight files unguarded. Decided to redirect `$HOME` once per test **process** — `test_support::ProcessHomeIsolation` instantiated in the always-compiled `tests/test_process_isolation.cpp` — because `catch_discover_tests` gives every TEST_CASE its own process of one binary, so one static-init guard covers all of them and no future test can forget a guard. Rejected: patching 68 call sites (the sprawl B17 decided against; re-opens on the next added test), and an `inline` object in `test_support.hpp` (only covers TUs that include it). Test infrastructure only: no product code, no persisted field, no schema change. Locked by **#327**.
- **B19 blackjack-at-exhaustion: end the session, do not present the hand** (resolved 2026-11-09). `StrategyTrainer::nextQuestion()`'s rejection loop can exit with a blackjack in hand when the shoe runs dry mid-retry. Decided that the correct behaviour is to treat it as shoe exhaustion — clear the question and end the session exactly as the existing `deck.size() < 3` branch at the top of the function does — rather than to keep drawing, because the trainer's contract is "every presented hand requires a real decision" and a natural 21 has none. Presentation-only + session-state only: no persisted field, no `GameRules` change, no `AppMode` change.
- **U11/U3/U1 layout basis: one `GameRenderer::worldSizeOf(game)` for drawing *and* hit-testing** (resolved 2026-11-09). The renderer laid out screens in `gameView` **world** units (`GameRenderer.cpp:35-37` sets the view to `window.getSize()`, so world units equal window pixels only at exactly 1600×900), while the InputHandler hit-tested menu rows in **raw window pixels** with hard-coded 900-basis y constants — three screens on three bases. Decided to add one accessor, `GameRenderer::worldSizeOf(const Game&)`, and route *both* sides through it: every menu/pause/achievements/rules y-coordinate is derived from that size, and every hit-test resolves the row the renderer actually draws at the current size. Rejected: (a) fixing only the renderer (hover would still drift), (b) switching to a fixed-viewpoint letterboxed view (a real windowing change, out of scope for a QA phase), (c) per-call `sf::Mouse::getPosition(window)` snapshots (the pause hit-test used the **desktop-space** `sf::Mouse::getPosition()`, which is the same bug class). This is presentation + input plumbing only: no persisted field, no schema change, no `AppMode` change. Locked by **#323–#325** (`tests/test_presentation_layout.cpp`), which assert the pure helpers agree with the renderer's accessors at 1600×900, 900×560 and 1200×800 — the tests fail before the unification because the two bases disagree off-default.
- **T1 + B21 storage: store a shoe as the seed that built it, and stop pretty-printing the save** (resolved 2026-11-12). Scoped by measurement, not by the write-up: the real 9,591,391-byte profile decomposes as **64.6% indentation whitespace** (6,199,262 bytes, 427,145 lines — `src/PlayerProfile.cpp:190` wrote `dump(2)`), 31.8% `longHistory` rounds, 8.3% stored deck arrays, and **0.2% `currentHistory` (22,888 bytes)**. That last figure reframed the item: T1 was written as "the per-round `RoundRecord` append at `HandEvaluator.cpp:295` grows the profile", but the structure that append grows is a fifth of one percent of the file, and T2's 100-shoe cap already bounds it — so the append was left alone deliberately. Shipped: `dump()` (compact) for a file no human reads, and a shoe stored as **`shoeDecks` + `shuffleSeed`** — two integers that rebuild it exactly — replacing the 313-card array whose sole consumer (`src/Replay.cpp`) needed only the shoe's size. B21 is what makes the seed trustworthy: it had never been assigned, so the first cut shipped a plain count and the seed was wired up afterwards, in both directions — recorded by `Deck` at shuffle time for gameplay, read back from the record by `Replay::buildSteps()` for replay. **Rejected: reconstructing the deck from the seed before the seed existed** — it would have fabricated decks that were never played. Measured on the developer's real pre-T1 file: loads (100 shoes, 1,688 rounds), resaves to **2,724,930 bytes, 71.6% smaller**. Locked by **#13–#16** (`tests/test_deck.cpp`) and **#257–#261** (`tests/test_profile_settings.cpp`).
- **Persistence: legacy-schema support removed and history reset** (decided 2026-11-12, developer's call). The additive-JSON contract had been kept for every phase — every new field optional with a default, so old profiles always loaded. For this change the developer chose to **break it deliberately and start from clean data**: the `initialDeck` array, its field, and its `from_json` compatibility branch are deleted, and the developer's real profile (9.3 MB of pre-T1 history) was **wiped**, backed up to `~/.local/share/blackjack/pre-t1-backup/player_profile.pre-t1-2026-11-12.json.bak` outside the saves directory so the app never loads it. Consequences recorded honestly: (a) the schema is now smaller and has one shape, with no dead compatibility branch to maintain; (b) the **`j.value(...)` defaults on fields that *are* written were kept** — those are not backwards-compat, they are guards against truncated/partial writes, and removing them buys zero bytes while making a half-written file fatal; (c) the backup **still loads** under the new schema (verified: 100 shoes, 1,688 rounds), because `rounds` is unchanged and the removed key is simply ignored — its pre-T1 shoes carry seed 0, so their replay rebuilds a valid 313-card shoe rather than the historical order. Any *other* pre-T1 profile on another machine behaves the same way: history survives, stored decks are ignored.

- **Shoe order is defined by this program, not by the standard library** (decided 2026-11-12). `Deck` shuffled with `std::shuffle`, whose swap sequence is specified but which draws indices through `std::uniform_int_distribution` — and the standard leaves *that* distribution's algorithm implementation-defined. So a recorded `shuffleSeed` pinned a shoe only within one standard library, which is exactly what shoe codes (#4), ghost races (#9) and shareable forks (#1) need to cross machines. `Deck::shuffleWith()` and `reshuffleWithRemaining()` now call a local `shuffleShoe()`: hand-written Fisher-Yates drawing indices straight from `std::mt19937` with **rejection sampling** (unbiased, and the rejection loop is ours). mt19937's output is fixed by the standard, so the whole sequence is now fixed by this program. **Rejected: leaving it and documenting the limitation** — the fix is ~20 lines at two call sites, and the cost of deferring is a silent-corruption migration once history exists. **Timing was the point:** history was wiped the same day, so no existing shoe order needed preserving. Locked by **#18** `[Golden]`, which freezes the first eight cards and the cut-card index for seed `0xC0FFEE` — a future change to the order fails loudly instead of silently redefining every seed ever written. Verified stable across 20 runs.
- **Profile schema policy for future features** (decided 2026-11-12, answering the open question in `IDEAS-REVIEWED.md` §G.1). Future features **will** add fields to the player profile, so the policy is: a new field is written unconditionally and read with a `j.value(key, default)` default. That is not legacy-schema support — it costs nothing, and it is what stops a truncated or half-written file from throwing on load and discarding the profile. What is **banned** is a *reader for a field that is no longer written*: the pre-T1 `initialDeck` branch was exactly that, and it was deleted with the history reset. If a future change alters a field's **meaning** rather than adding one, bump a schema version and wipe-and-rebuild rather than carrying a translation branch. Consequence to keep in mind: the load path's recovery is to move the damaged file aside and start fresh, so a required key that goes missing loses the whole profile, not one field.
- **Identity: the profile owns the player's name** (B23, resolved 2026-11-12). Every construction site hard-coded `Player("William")` — four of them — and `Player`'s loading constructor never synced the seat's display `name` with the loaded profile. U7's name-entry screen wrote the persisted name correctly, so the profile was right and the **seat label at `GameRenderer.cpp:564` kept showing the hard-coded name on every launch**. Production now passes `Constants::DEFAULT_PLAYER_NAME` ("Player") as a placeholder, the loading constructor adopts `profile.getName()` when it is non-empty, and the first-startup prompt (`needsNameEntry()`, gated on an empty persisted name) collects the real name. NPC names in `GameConfig::NPC_CONFIGS` are intentional characters and stay. Locked by **#312–#314** (`tests/test_name_entry.cpp`); #312 was confirmed red before the fix (`"Player" == "Ada"`). Note the existing U7 tests covered the *persisted* name thoroughly and never once asserted the *displayed* one — the gap was the untested half of the round-trip.


- **U1 hover is a visual state, not a selection** (resolved 2026-11-09). `InputHandler::processMenuEvents` called `applyMenuRowSelection()` on `MouseMoved`, so moving the mouse *moved* the keyboard selection; the renderer then drew hovered and selected rows in the same gold, leaving no way to see which row Enter would fire. Decided to decouple them in the direction of a normal desktop menu: hover paints a background pill plus a leading marker and never mutates `menuSelection`; gold stays reserved for the keyboard-selected row; a click activates immediately. Rejected: making hover steal the gold (keeps the ambiguity), and making hover set the selection while rendering them differently (leaves the selection state mutated by an incidental mouse sweep across the window). Regression test **#317** asserts hover and selection are distinct states and that a hover event leaves the selection index untouched.
- **Non-ASCII text must not be passed to SFML as `std::string`; use ASCII, or `sf::String(U"…")`** (decided 2026-11-09, U13). The first U1 build drew `›` (U+203A) as the hover marker and the Xvfb screenshot showed **tofu boxes**. The first explanation — "the shipped font has no U+203A glyph" — was **wrong and is corrected here**: `assets/fonts/noto/NotoSans-Regular.ttf` maps U+203A to `guilsinglright` with a real 1-contour outline (fontTools). The actual cause is SFML 3's decoding: `sf::String(const std::string&, const std::locale& = {})` is documented as an **ANSI** string converted through the locale (`SFML/include/SFML/System/String.hpp:173`), so a UTF-8 literal in the source becomes **three** code points — `›` = `0xE2 0x80 0xBA` → `â`, U+0080, `º` — and U+0080 has no glyph. Decided to keep ASCII `>` for the marker (the rules panel already uses it; proven to render) rather than build `sf::String(U"›")`, because a cosmetic marker does not justify a UTF-32 path. **The general rule for the next visual fix: a string reaching `sf::Text` must be ASCII-only, or be built as `sf::String(U"…")`/`std::u32string`.** Two live violations were found by sweeping `src/` for non-ASCII and are fixed as **U13**: `Game::bettingControlsHint()` (U+2191/U+2193/U+2190/U+2192 — these are *genuinely* absent from the Noto cmap, so ASCII is the only correct fix) and the name-entry hint (U+2022 bullets). Locked by **#326** (`isSfmlSafeAscii`). Not fixed, recorded: `Card::getSuitSymbol()` (`src/Card.cpp:32`) returns `u8"♠"[0]` — a single `char`, i.e. the truncated first byte of a UTF-8 sequence — so it can never produce a suit glyph; it is **dead code** (no callers), which is why it is a trap rather than a defect. The startup banner's `♠ ♣ ↑ ↓` are `std::cout` output on a UTF-8 terminal and are fine.
- **U12's per-graph control is a *view* toggle, not a *visibility* toggle** (decided 2026-11-12). The U12 write-up specified part (1) as "per-series **visibility** flags + draw gating", i.e. hide a graph. What shipped makes each side-panel pane its own **per-shoe ↔ per-session** switch: a left click inside a pane moves only that pane between the two datasets; `G` applies one view to all three at once. Decided to keep the shipped semantics and correct the ledger, because (a) the session view (part 2) is the feature that actually needed per-graph control, (b) a hideable pane leaves a hole in a 240px side panel and teaches nothing, and (c) the axis work (part 3) is per-pane, so it only makes sense with a per-pane view. `NEXT_STEPS.md` PHASE 4 and §11 now describe what shipped. Locked headlessly by **#241–#244** and on screen by `tools/visual_check/check_u12_click.py`.



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
now backs: `tests/test_profile_settings.cpp#280` ("B13: an in-progress shoe's rounds survive a save/load and still
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
| 1 | Should Tournament results touch the career profile at all? | (a) never — gate the writes; (b) record a separate tournament history | B1 | **Resolved 2026-11-07** — option (b), a SEPARATE tournament history. Career-profile preservation itself is already delivered by B1's gating; the separate store is a follow-up feature. |
| 2 | When does the main bet leave the bankroll? | (a) at commit (escrow); (b) at settlement, with an "available" accessor | B5 | **Resolved 2026-11-07** — option (a), escrow at commit (Model C); implemented in B5 (`4f85d37`). |
| 3 | Is $1,000 or $10,000 the intended starting bankroll? | reconcile human + NPC | U8 | **Resolved 2026-11-07** — **$10,000 everywhere**; human and NPC seats share one `STARTING_BANKROLL = 10000` (`Constants.hpp:287`). The defaults line below used to say $1,000, which contradicted the shipped value. |
| 4 | Expose `maxSplitHands` or delete it? | expose (2–4) / delete | B10 | **Resolved 2026-11-07** — expose, restricted to {2, 3, 4, Unlimited}; implemented in B10 (`69cd9e8`). |
| 5 | Should the console control banner survive at all? | keep trimmed / replace with in-game Controls screen | B11, U4 | **Resolved 2026-11-07** — keep it, trimmed along the bottom middle (a presentation decision; unblocks B11). Repositioning needs a visual/Xvfb check, so the label fix and placement were queued as follow-ups. |
| 6 | Is a name-entry screen in scope for 0.4, or deferred? | in scope / defer with a tracking note | U7 | **Resolved 2026-11-07** — in scope, and IMPLEMENTED (`e157052`): auto-prompt before the main menu when the profile has no name, editable from the menu; Player Stats reuses the existing Statistics→StatsDashboard path. Headless tests #286–#288. |
| 7 | How long should the achievement toast show? | suggested 4s | B2 | **Resolved** — chose 4s, implemented in B2 (`f3aeb1f`) |
| 8 | What bounds should the **Bets Placed** histogram use? (U12's axis half) | (a) fixed 10–100 that grows up to `MAXIMUM_BET`; (b) log-scale buckets; (c) keep `[0, MAXIMUM_BET]` and fix the docs | U12 (axis) | **Decided 2026-11-11** — option (a). Axis bounds are [10, 100] by default; if any wager placed since the view window began exceeds 100, that wager becomes the new soft max and the axis grows to match it. Hard cap: MAXIMUM_BET = 1000. Implemented in U12 (axis half). |
| 9 | Is a **session** a new dataset, or a view over existing per-shoe accumulators? (U12's session-view half) | (a) new session-scoped accumulator reset at session end; (b) derive from `currentHistory` + completed shoes in the session | U12 (session view) | **Decided 2026-11-11** — option (a). A session = one play window, from when the user opens the game to when it closes; it may span multiple complete shoes or an incomplete shoe. The accumulator accumulates across those shoes and resets only on open/close, distinct from career longHistory and per-shoe ShoeAnalytics. Implemented in U12 (session-view half). |

Defaults if nobody answers (all of these are now historical — every row above is resolved; see §5's decision list):
1(a), 2(a), ~~3 = $1,000 everywhere~~ **3 = $10,000 everywhere** (the $1,000 default was never shipped; `STARTING_BANKROLL = 10000`),
4 = expose, 5 = keep trimmed + add Controls, 6 = defer, 7 = 4s (implemented as `ACHIEVEMENT_TOAST_SECONDS` = 4.0 in B2,
`f3aeb1f`), **8 = (a) 10–100 growing to `MAXIMUM_BET`** (it is what the docs already promise), **9 = (a) a new
session-scoped accumulator** (the user stated the session view must be distinct from `longhistory`).

> **2026-11-11:** #8 and #9 are now decided (see their rows); #9's definition refined to "one open play window". They remain listed as defaults only because each default happens to equal the chosen value.

> **2026-11-12 (doc hygiene):** rows 1–6 above carried **no** Resolved marker even though §5 recorded all seven as resolved on 2026-11-07 — the table is headed "need an answer before the relevant fix", so a reader would have reopened six settled decisions. Row 3's stated default ($1,000) also contradicted the shipped `STARTING_BANKROLL = 10000`. Markers added; the default corrected. **The table is now consistent with §5.**

## 11. Status tracker

Update this table as work lands; it is the first thing the next window should read.

**Re-audited against source on 2026-11-09 at HEAD `7a19af3`** (branch `3.1`), and re-checked again after the U-series
landed. The B-series is **closed except B9**, which stays **Partial** because its original intermittent crash is
genuinely unreproducible and carries no test. What is actually open now: **T1**'s history-growth half (the write cadence
is gated, `HandEvaluator.cpp` still appends a `RoundRecord` every settled round). **U12 is Resolved as of 2026-11-12** —
all three parts (per-graph view toggle, session view, axis bounds) landed after §10 #8/#9 were decided.
The vision-critical set **U1, U2, U3, U6, U11** is **Resolved and verified on screen**, and
the screenshot pass surfaced one more on-screen defect, **U13** (non-ASCII text → SFML), also Resolved.
Every row below was re-checked against the source rather than trusted from this table. (An earlier revision of this
header said "nothing has landed; all 15 defects plus U1–U10, T1–T2 and B16 are still open" — that was the 2026-11-06
snapshot and is contradicted by the table beneath it; it has been replaced.)

**Resolved since that audit:** `B3` (`c7978f1`) and `B15` (`9a89185`) on 2026-11-06; `B1` (`5107aa7`), `B5` (`4f85d37`),
`B8` (`11f6f50`), `B10` (`69cd9e8`), `B11`, `B16` (`5f81d48`), the B9 logger flush (`d6d6436`) and the B9 overlay bounds
guard (`facea14`), plus `U7` (`e157052`), `U4`, `U5`, `U9`, `U10`, all on 2026-11-07; and **`B17`, `B18`, `B19` on
2026-11-09**. B9 is **Partial** — only its intermittent-crash half stays unreproducible.

**Resolved on 2026-11-09 (U-series):** `U1`, `U2`, `U3`, `U6`, `U11` — five vision-critical defects, nine new tests
(#317–#325), verified with an Xvfb screenshot harness rather than by code reading. The same screenshot pass surfaced
`U13` (non-ASCII text reaching SFML), also Resolved, test #326.

**Suite at 327/327 serial.** The `-j8` figure is only trustworthy from B17 onward: the documented "315/315 green at
`-j8`" was **false** (3 of 10 soak runs had ≥1 failure), and the gate in `NEXT_STEPS.md` now requires a 10-run soak.
Post-B17/B18/B19 the soak measured **12 clean / 0 failing**, every run 316/316, with the developer's real profile md5
stable across the soak. **That profile check was not catching the writer** — see §13's 2026-11-09 B17 entry: a soak
compares runs to each other, so a writer that emits identical content every run looks stable from run 1. The gate is
now md5 **before** and **after** against a baseline captured outside the suite.

The gate has a **false-positive** direction as well, and it has already cost a session: the developer running the real
app (any playtest with the live `$HOME`) legitimately writes that file, and by md5/mtime alone that is **indistinguishable
from a suite leak**. Before opening a B17-class investigation, check whether the app was run by hand in the same window —
and note that a real playtest leaves the career history at full size, whereas the B17 writer truncated 9.3 MB to 1.34 MB,
so **content size and shape discriminate** where the hash alone does not.

> **The test IDs in the Evidence column drifted again and were re-derived on 2026-11-09** against `ctest -N`:
> **B13 is #280** (not #273 — #273 is now a Surrender test, exactly the trap §13 warns about), the **B9 overlay pair is
> #299/#300** (not #296/#297, which are the Logger-flush tests), **B11 is #298** (not #295), **U7 is #289–#291** (not
> #286–#288). Checked and correct as written: U4 #307–#308, U5 #309–#311, U9 #312–#315, U10 #281/#282. Historical §13
> entries keep their period IDs; use the current ones above.

| ID | Sev | Title | Status | Evidence as of 2026-11-06 |
|---|---|---|---|---|
| B1 | 🔴 | Tournament overwrites career profile | **Resolved 2026-11-07 (`5107aa7`)** | Loads the real career profile into seat 0 via `Player("William")`, sets persistable=false for the run so every saveProfile() no-ops, overrides only in-memory chips — a close-save (persistOnExit choke point or direct) leaves the file untouched. Regression test #228 redirects HOME to a temp dir, writes a distinctive career profile, enters Tournament, drives both exit paths, reloads and asserts bankroll 98765 / handsFinished 1 / wins 1 / betsPlaced 1 / longHistory size 1 all preserved. Suite 284/284 (0 failures; #256 flaky pre-existing, see §13) |
| B2 | 🔴 | Achievement toast never clears | **Resolved 2026-11-06 (`f3aeb1f`)** | Added `achievementToastTimer` to `HUDState.hpp`; armed in `AchievementFeeder.cpp:48` on each unlock, ticked in `RoundEngine.cpp` alongside the other HUD timers and cleared at 0 (renderer draws only while a name is set). Test #210 arms/advances past lifetime/re-arms (fails before fix). Suite 275/275 |
| B3 | 🔴 | Side bet always reports "WON" | **Resolved 2026-11-06 (`c7978f1`)** | Fixed in `include/SideBetLabels.hpp`: branch won/lost/push on net with the sign preserved; `GameRenderer.cpp:997` calls the pure fn. Guard test in `test_side_bets.cpp` asserts WON +20 / LOST -20 / PUSH 0 (fails before fix). Suite 266/266 |
| B4 | 🔴 | Reko seed ignores deck count | **Resolved 2026-11-06 (`3924b56`)** | Seeds from `gameRules_.numDecks` in `resetShoeAnalytics()` (`Game.cpp:277`); `Deck::create()` remembers shoe size and reshuffle rebuilds from it (`Deck.cpp:73`). Tests assert the roadmap seeds 1/2/4/6/8 → -1/-5/-12/-20/-27 through a headless Game, plus a one-deck reshuffle stays ~53 cards (both fail before fix). Two `test_deck.cpp` reshuffle-size assertions were corrected from the old NUM_DECKS behavior. Suite 274/274 |
| B13 | 🔴 | Mid-shoe quit deletes those hands from career stats | **Resolved 2026-11-06 (`387a3e0`)** | Persist in-progress shoe rounds (currentHistory) so quitting mid-shoe still books them into career stats; guarded read. Test **#280** passes (the row formerly said #273, which is now a Surrender test — see the §11 header ID note); hist_proof.cpp (§6.10) now exits 0 and confirms the shipped schema. §11 listed it Open after the commit and was corrected here |
| B5 | 🟠 | Main bet not escrowed; bankroll can go negative | **Resolved 2026-11-07 (`4f85d37`)** | Escrow at commit (Model C): `getAvailableBankroll()` returns bankroll minus committed main − pending side bet; `confirmBet()`/`placeSideBet()` clamp + hint against that balance, not the full bankroll. 3 regression tests in `test_settlement.cpp`: accessor invariant (#111); a same-limit side bet refused once the main is committed so it can't over-commit the roll (#112); and a losing round (dealer natural −MIN plus a lost PerfectPairs side) settling to after == STARTING − MIN − sideWager == 9930, never negative (#113). Win/lose arithmetic unchanged; full suite 281/281 (0 failures) |
| B6 | 🟠 | Rules panel key repeat | **Resolved 2026-11-06 (`584aa4b`)** | Added false->true edge detection in \`Game::update()\` rules case via \`rulesRightPrev_\`/\`rulesLeftPrev_\`; applyGameRuleEdit fires only on a press (one step per tap). Up/Down highlight repeat left as-is (desirable, called out in commit). Headless test #277 (\`test_rules_panel_editing.cpp\`) holds Right 25 frames and asserts result == one tap; fails before / passes after. Suite 277/277 (+1 over B7) |
| B7 | 🟠 | Escape-quit skips the save | **Resolved 2026-11-06 (`11b187b`)** | Centralised persist-on-exit into `Game::persistOnExit()` (shoe record + current table settings + unlocked achievements, then save) routed through `InputHandler::returnToMainMenu()`, so every Escape->menu route persists like the window-close path did; guarded against an empty seat vector. Headless test #276 (`test_input_return_to_main_menu.cpp`) fails before / passes after; full suite 276/276 (+1) |
| B8 | 🟠 | `Deck::draw()` no empty guard | **Resolved 2026-11-07 (`11f6f50`)** | Two unguarded `back()`/`pop_back()` pairs in `Deck::draw()` — the first when the shoe is already empty, the second after only the cut card was drawn (a single-card shoe whose one card is the cut card), both UB on an empty std::vector. Guarded: entry returns `Card{}` for a truly empty shoe; before the second `back()` it now checks `cards.empty()` and returns the cut card when nothing remains. Purely defensive — every production caller already guards `deck.empty()`, so real gameplay is byte-for-byte unchanged and all 5 callers keep the `Card` return type. Headless-testable via `Deck::setCards({})`: `tests/test_deck_empty_draw.cpp` (#13/#14) drove both sites, SEGFAULT / returned garbage (value 33 != CUT_CARD_VALUE) before the fix, PASS after. Suite 291/291 |
| B9 | 🟠 | Flaky crash + unflushable logger | **Resolved 2026-11-12** — all three clauses closed | `Logger::log()` now uses `std::endl` so every record is flushed to std::cout synchronously (commit `d6d6436`), proven by #284/#293/#294 in `tests/test_logger.cpp`. The bounds-guard clause — so the per-player draw index in `drawSummaryOverlay()` cannot exceed the summary loop bound (`GameRenderer.cpp:1022`) nor read out of range at `:1025` — is fixed via helpers that clamp rows to `min(summaryCount, playerCount)` and return 0.f for an empty/single-hand fan width; tests **#299/#300** in `tests/test_summary_overlay_bounds.cpp` fail before the clamp and pass after (red-green). **The third clause, the intermittent crash, is closed as a developer-testing artifact rather than a product defect.** The developer has confirmed it came from their own manual playtesting — which is precisely why no clean record of it exists (one playtest hit, three failed replays, one clean 60-cycle soak): the triggering sequence was interactive and was never captured. The repro scaffold `tests/test_analytics.cpp:575` drives the reported play → quit-to-menu → dashboard flow and passes. Stated plainly so it is not mistaken for a silent paper-over: **no product-code change was made for the crash**, none was justified once the cause is attributed, and if it ever reappears from ordinary play it should be reopened as a fresh ID with a clean capture. |
| B14 | 🟠 | Non-atomic save; corrupt file → silent wipe | **Resolved 2026-11-06 (`9d1d125`)** | Atomic write + corrupt-save rescue under a `.corrupt.<ts>` suffix, and partial-JSON field retention in `PlayerProfile.cpp`. Tests #232/#233 pass. §11 listed it Open after the commit and was corrected here |
| B15 | 🟠 | Two live copies of `Serialization.hpp` | **Resolved 2026-11-06 (`9a89185`)** | Deleted `src/Serialization.hpp`; exactly one copy remains (include/), enforced by a guard test that fails with two / passes with one. No CMake edit needed; `PlayerProfile.cpp` now resolves to the include copy, unblocking every schema edit (B12/B13/B14) |
| B10 | 🟡 | `maxSplitHands` dead config | **Resolved 2026-11-07 (`69cd9e8`)** | Serialised (`to_json`/`from_json`, clamps to {2,3,4,-1}; -1==unlimited) and exposed via a gameRuleRow cycling only {2,3,4,Unlimited}; the -1 sentinel is guarded at both enforcement sites. Regression tests #278–#282 (test_max_split_hand_rules.cpp): schema round-trip + clamp fail before fix; restricted-value cycling and unlimited-vs-finite-cap enforcement verified. Suite 289/289 |
| B11 | 🟡 | startup control banner mislabels the betting-phase keys | **Resolved 2026-11-07** | Extracted the startup "PLAYER CONTROLS" banner from the inline `Logger::info` in `Game::Game()` into a pure helper `Game::startupControlsBanner() const`; corrected its labels to match the real `InputHandler.cpp:635-645` PlaceBets bindings (Left = auto Kelly via `calculateOptimalBet()`/`setBet()`, Right = confirm). Regression test #295 (`tests/test_banner_b11.cpp`) reads it from a headless `Game(true)` and asserts Left-present / Right-absent on the auto-kelly row — fails before, passes after. Suite 294 → 295/295. §10 #5 (banner-survival: keep trimmed vs permanent in-game Controls screen) stays open; this was the minimal fix |
| B12 | 🟡 | `resplitTentsAllowed` typo in save schema | **Resolved 2026-11-06** | Renamed field+enum to `resplitTensAllowed`/`ResplitTens`; `to_json` writes the corrected key, `from_json` reads it then falls back to the legacy typo'd key (new wins). On-disk writer verified via `test_profile_settings.cpp:232` using real `PlayerProfile::save()` — shipped binary now writes `resplitTensAllowed`. Suite 272/272 |
| B16 | 🟡 | `reconstructDeckAtRound()` dead and wrong | **Resolved 2026-11-07 (`5f81d48`)** | Removed — static analysis confirmed zero callers (sole occurrence was its own definition at `GameRecord.hpp:307`), so deletion is behaviour-preserving; it also recomputed deck state with the wrong card-drop order, which no code relied on. Full suite unchanged at 289/289 |
| B17 | 🟠 | Test suite is not hermetic: shared `/tmp` HOME dirs made `ctest -j8` flaky, and the suite wrote the **developer's real** career profile | **Resolved 2026-11-09** | The documented "315/315 green at `-j8`" baseline was **false**: measured 3 of 4 runs (and 3 of 10 in a soak) had ≥1 failure. Root cause: `Player::getSavePath()` (`src/Player.cpp:14`) resolves `$HOME/.local/share/blackjack/saves/player_profile.json` from the environment, and five test files each redirected `HOME` to a **fixed** path (`/tmp/bj_b1_test`, `/tmp/bj_u8_test`, `/tmp/bj_b7_test`, `/tmp/bj_t1_autosave_home`, `/tmp/bj_name_entry_0`). ctest + Catch2 run each `TEST_CASE` in its **own process**, so those dirs are shared by every concurrently-scheduled test. `test_name_entry.cpp`'s "per-invocation unique" dir came from a `static int counter` — per-process, therefore **always 0**: #291 writes a profile named "Ada" into `/tmp/bj_name_entry_0`, #290 then loads it, sees a non-empty name and fails `REQUIRE(needsNameEntry() == true)`. Reproduced deterministically by running #290 and #291 side by side (1/25). Separately, a per-`TEST_CASE` audit found **9** cases calling `startNewGame()`/`startTournamentSession()`/`startStatsSession()`/`rememberTableSettings()` with no redirect at all — those rewrite the real `~/.local/share/blackjack/saves/player_profile.json` (its mtime moved across a suite run). Fix: one shared RAII `test_support::IsolatedHome` (`tests/test_support.hpp`) whose dir embeds the **PID** (unique across the processes ctest spawns) plus a per-process counter; migrated all 5 hand-rolled redirects and guarded all 9 unguarded cases. Gate = **10 consecutive `ctest -j8` runs**: red 7 clean / 3 failing before, **10 clean / 0 failing** after, with the real profile's md5 verified stable across the soak. No product code changed. **B17 was INCOMPLETE and is now closed (2026-11-09).** The five-file redirect did not make the suite hermetic; the soak's profile-md5 check passed because the writer happens to produce a file that was not compared against a changing baseline in that run. A **serial bisect over every test index** (md5 the real profile before and after each `ctest -I i,i`) found **Test #99, "Resplitting tens is refused when RESPLIT_TENS_ALLOWED is false"** in `tests/test_settlement.cpp` writing the developer's real career profile — the live **9.3 MB** file came out **1.34 MB**, i.e. the career history was replaced by a test's game state. Eight files construct `Game(true)` and drive real rounds with **zero** guards: test_settlement (21 cases), test_side_bets (18), test_game_rules (12), test_achievements (7), test_advisor_feedback (4), test_counting_box_label (3), test_max_split_hand_rules (2), test_side_bet_paytable (1) — 68 cases. Patching 68 call sites is the hand-rolled-redirect sprawl B17 itself decided to stop centralising, and any new round-driving test in those files re-opens the hole silently. Fix: **process-scope isolation** — `test_support::ProcessHomeIsolation` instantiated in `tests/test_process_isolation.cpp`, the one TU always compiled into `Blackjack2Tests`; `catch_discover_tests` launches every TEST_CASE as its own process of that binary, so a static-init redirect covers every test, including files that never include `test_support.hpp`. Locked by a new test (`[B17][hermeticity]`, **#327**) asserting `$HOME` is a `bj_proc_home_<pid>` dir, that the real profile path is not reachable from inside a test process, and that the per-case `IsolatedHome` still nests inside it. Re-bisect: **zero writers across all 327 tests**; full `-j8` suite leaves the profile md5 unchanged. |
| B18 | 🟡 | Three tests asserted against an **unseeded** OS RNG, so they flaked instead of pinning anything | **Resolved 2026-11-09** | `Deck::shuffle()` reseeds from `std::random_device` (`src/Deck.cpp:41`), so (a) #259 swept one *random* shoe per run and (b) #306 asserted `writes == rounds / AUTO_SAVE_EVERY_HANDS` against **driver rounds** while the cadence counts **settled hands** — an unseeded shoe settles a varying number per round (a natural settles at once, a split settles two); `handsEvaluated` measured 10, 11 and 12 across 12 runs, so the fixed `floor(12/5)==2` was only sometimes true. Added a deterministic seed seam — `Deck::shuffleWithSeed(seed)` (shares one private `shuffleWith()` body with `shuffle()`, so the production OS-seed path is byte-identical) and `StrategyTrainer::startSessionWithSeedForTest()` (delegates to a shared `startSessionWith()`). #259 now sweeps **5 fixed seeds to shoe exhaustion** (the old 100-question cap never reached the exhaustion path that B19 fixed); #306 asserts against the settled-hand count it actually gates on. Retargeting #306's assertion alone was **not enough** — 20 runs still failed 1, because the autosave counter resets on a write and a split round settles two hands, discarding the overflow so writes land *below* `floor(hands/5)`; the case now seeds a uniform alternating 10/7 shoe (hard 17 every round: no pair → no split, no natural) so exactly one hand settles per round and `handsEvaluated == rounds` is a checked precondition. #169 "edge inside the expected band" was a **third** flake of this class, only exposed once B17/B18 removed the noise hiding it; it now sweeps the same 5 seeds. Targeted soak: **0/25–0/30 fails** each, previously load-dependent failures. |
| B19 | 🟠 | `StrategyTrainer::nextQuestion()` **presents a natural blackjack** when the shoe runs out mid-retry | **Resolved 2026-11-09** | Real product defect that #259 was only ever *flaking* on. The blackjack-rejection loop is `do { draw 2 player + 1 dealer } while (questionHand.isBlackjack() && attempts < 500 && deck.size() >= 3);` followed unconditionally by `computeBookAction()`. The entry guard only requires `deck.size() >= 3`, so the **first pass can draw the shoe dry**; when that hand is a blackjack the `deck.size() >= 3` term is false, the loop exits, and the natural 21 is presented — a hand with no decision to train — with `active` left true on a spent shoe. Reaching it needs the shoe to empty at exactly that moment, which is why it flaked under load rather than failing. Fix: after the loop, if the hand is still a blackjack, clear it and end the session exactly as the exhaustion branch at the top of the function does. Test-first: `setDeckForTest()` seam injects an exact 3-card shoe (dealer up-card, ten, ace — `Deck::draw()` pops from the back, so the player gets ace+ten and the shoe is spent) → **fails before, passes after** (red verified: both `!isBlackjack()` and `!sessionActiveForTest()` failed pre-fix). New test #316; suite 315 → **316/316**. |
| B20 | 🟡 | Test **#175** ("U12: the session accumulator spans shoes while per-shoe analytics reset") **flaked under `ctest -j8`**: `startNewShoeForTest()` → `startNewShoe()` rebuilds the shoe with a **real, unseeded** shuffle, so shoe 2's cards differed per test process | **Resolved 2026-11-12** | Surfaced while verifying U12 part 1: #175 failed 1 of the first 10 `-j8` runs. It passed in isolation, and a clean `HEAD` worktree (330 tests, no U12 part-1 code) reproduced the **same rate**, so it was **pre-existing, not a U12 regression** — that baseline check is why it is recorded as its own ID rather than folded into U12. Root cause is the **B18 class**: the test seeds a 300-card all-value-8 neutral deck for shoe 1, then crosses a shoe boundary that throws that deck away and shuffles from the OS RNG, so the rounds driven after it (and therefore the asserted series lengths) depended on the shuffle. **Measured, not asserted:** 120 single-test runs (each its own process, so a fresh OS RNG) = **12/120 failing before, 0/120 after**. Fix: re-seed the deterministic all-8 deck after `startNewShoeForTest()` — all-8s means no naturals, so no insurance prompt or auto-settle can change the settled-round count. **The B18 remedy does NOT transfer verbatim, and that was measured, not assumed:** `startNewShoeWithSeedForTest(42)` makes the deal order reproducible and the case then fails **deterministically** — that seeded real shuffle deals a natural which changes the settled-round count, and the exact-count assertions read it as a broken accumulator. A test asserting **exact round counts** needs a **controlled** deck, not merely a repeatable one; `shuffleWithSeed` is the right B18 remedy only where the assertions are bands/spans sampled from a distribution. Test-only; no product code, no schema change. |
| B21 | 🟢 | `ShoeRecord::shuffleSeed` was **declared, serialized in both directions, and documented** as "MT19937 seed used when shoe was created" — but **nothing in the codebase ever assigned it**. Every shoe on the real profile carried `shuffleSeed: 0` (verified: 100/100). | **Resolved 2026-11-12** | Found while scoping T1. The obvious storage win was "stop storing the 313-card deck, reconstruct it from the seed", and that plan was **unsound**: the seed was always 0, so reconstruction would have silently fabricated a deck that was never played, for every shoe. T1 shipped the deck's *count* as the interim; this row is the real fix, implemented in **both directions** as specified. **(1) Gameplay — the seed is set when the deck is shuffled.** `Deck` already recorded `lastShuffleSeed` on every reorder path (`shuffleWith()` from `std::random_device`, and `reshuffleWithRemaining()`), it simply had **no accessor**; added `Deck::shuffleSeed()` and `Deck::shoeDecks()`, and all four shoe-creation sites (2 in `SessionLifecycle.cpp`, 2 in `Game.cpp`) now write the real seed and the deck count into the record. Production keeps the OS seed, so real shoe order stays unpredictable — the seed is recorded, not chosen. **(2) Replay — the seed is read from the game record.** `Replay::buildSteps()` rebuilds the shoe with `create(shoeDecks)` + `shuffleWithSeed(shuffleSeed)` and takes `totalCards` from it, which is what drives `cardsRemaining`/`trueCount`; 313 for a 6-deck shoe, the cut card included. The 313-card `initialDeck` array, its field and its compatibility branch are **deleted** — a shoe is now two integers. Also fixed: `lastShuffleSeed` was an **uninitialized member**, so a `Deck` filled by `create()`/`setCards()` without a shuffle would have serialized indeterminate garbage as a plausible-looking seed; now `= 0`. Locked by **#13–#16** (`tests/test_deck.cpp` — the seed a shuffle used reproduces that shoe card for card, cut card included; a never-shuffled Deck reports 0) and **#258/#259** (`tests/test_profile_settings.cpp` — a record's seed rebuilds the shoe through JSON). |
| B22 | 🟢 | `ShoeRecord::isContinuation` was **declared and set, never read, and not serialized at all** — a saved continuation shoe came back marked as an ordinary one. | **Resolved 2026-11-12** | The second zombie field found in two days, and the same class as B21: a field whose name asserts a state the persistence layer does not carry. `isContinuation` is set at exactly one site (`Game.cpp:1547`, the mid-shoe reshuffle) and `grep` found **zero readers**; `to_json`/`from_json` omitted the key entirely, so the flag existed only in memory for the life of the process. It matters more than it looks: it is the **only** marker of the seed-reconstruction boundary. `reshuffleWithRemaining()` shuffles the full deck **minus every card already dealt**, so a continuation shoe is smaller than `shoeDecks*52+1` and its order is a shuffle of a reduced set that the stored seed cannot describe — `create(shoeDecks) + shuffleWithSeed(seed)` rebuilds a shoe that was never dealt. Found while writing the test for the B21 continuation case, which is also the fix for the regression B21 introduced: `shoeCardCount` is now stored (the pre-B21 array captured `deck.getCards()` and got the reduced size for free; the seed schema lost it, inflating Replay's `cardsRemaining`/`trueCount` for every continuation shoe). Both fields are now serialized. Locked by **#17** `[Continuation]` (`tests/test_deck.cpp`), which asserts the reduced size, the save round-trip, and — deliberately — that the seed reconstruction does **not** match the dealt shoe, so nobody can later assume it does. Forward requirement recorded in `IDEAS-REVIEWED.md` §B.2: #1 forks, #4 codes and #9 ghosts must all refuse to reconstruct past a reshuffle, and the full fix (`reshuffleWithRemainingWithSeed`) should be built with the first feature that needs exact order past one. |
| B23 | 🟠 | The player's **displayed** name was hard-coded. Four sites constructed `Player("William")`, and `Player`'s loading constructor never synced the seat's display `name` with the loaded profile — so after a player typed their name, every later launch showed the hard-coded one. | **Resolved 2026-11-12** | Found while removing the hard-coded names, and it is a *gap in U7*, not a missing feature: the name-entry screen (`AppMode::NameEntry`), `needsNameEntry()` and `commitPlayerName()` all worked, and the **persisted** `profile.name` round-tripped correctly. The seat's `name` member came from the constructor argument and was never updated from the profile, and `GameRenderer.cpp:564` draws `Player::getName()` as the seat label (`:1028` feeds it to the summary) — so the screen showed the hard-coded name while the file held the real one. Fix: production passes `Constants::DEFAULT_PLAYER_NAME` ("Player") as a placeholder, and the loading constructor adopts `profile.getName()` when non-empty, making the profile the source of truth for identity. First-startup prompting is unaffected — `needsNameEntry()` gates on an empty *persisted* name, which the placeholder deliberately does not set. NPC names in `GameConfig::NPC_CONFIGS` are intentional characters and were left alone. Locked by **#312–#314** (`tests/test_name_entry.cpp`); **#312** confirmed red before the fix (`"Player" == "Ada"`). **Worth recording:** the existing U7 tests (#290/#291) covered the persisted name thoroughly and never once asserted the *displayed* one. The defect lived in the untested half of a round-trip that looked covered. |

| U4, U5, U7, U8, U9, U10 | 🟡 | UI/UX set (see NEXT_STEPS Phase 4) | **Resolved this phase** | U4 (#307–#308), U5 (#309–#311), U7 (#287 + #289–#291), U8 (#70), U9 (#312–#315), U10 (#282/#283). |
| U1, U2, U3, U6, U11 | 🟡 | Vision-critical UI set (menu hover/selection, pause panel, reflow, card fan + totals, resize drift) | **Resolved 2026-11-09** | All five fixed and **verified on screen** with an Xvfb + python-xlib screenshot harness (the only way to lock rendering; §6 method). **U1** root cause: `InputHandler::processMenuEvents` called `applyMenuRowSelection()` on `MouseMoved`, so the mouse *moved* the keyboard selection, and `drawMenuItem` painted hovered and selected rows in the same gold — no way to see which row Enter fires. Hover is now purely visual (background pill + leading `>` marker), gold is reserved for the keyboard selection, click activates. Screenshot proof: gold "Review History" (selected) and pill+marker "Achievements" (hovered) visible simultaneously. **U2**: `drawPauseMenu` painted a full-window alpha-128 overlay with no panel, so "Quit to Menu" collided with the seat-name label over the live table; added a centred panel (fill + outline) sized from the row bounds with padding, proven at 1600×900 and 900×560. **U3**: `drawAchievementList` (`y=96`, `rowH=60`) and `drawGameRules` used hard-coded offsets while the footer sat at `h-28`, so the list ran under the footer at 900×560; both now derive their geometry from the world size. **U6**: 3+ card hands advanced by a 16px step (a sliver of each card) and `drawTotal` drew bare 16pt white over the felt; introduced `cardFanCenterShift` (fan is centred on the seat, proven with a 7-card hand centred on the chip/label) and a dark pill behind every hand total (dealer **7** and player **28** legible over the felt text). **U11**: the three menu hit-tests used three coordinate bases — the pause one used desktop-space `sf::Mouse::getPosition()` — so rows drifted from their hover targets after a resize; all drawing and hit-testing now go through one accessor, `GameRenderer::worldSizeOf(game)`. Tests **#317–#325** (`tests/test_presentation_layout.cpp`, registered in `CMakeLists.txt`): suite 316 → **325/325**. Screenshots in `/tmp/bj_shots/` (harness `/tmp/bj_visual.py`, `/tmp/bj_u6.py`, `/tmp/bj_menu.py`). |
| U12 | 🟡 | Side-panel graphs: no per-graph control, no session view, and the Bets Placed X-axis spanned the full `[0, MAXIMUM_BET=1000]` instead of the intended 10–100 scaling range | **Resolved 2026-11-12** (all three parts) | Unblocked once §10 #8/#9 were decided. **(3) Axis:** `HUD::computeAxisMax` grows the axis from the `[MINIMUM_BET, 100]` base to the largest wager placed in the active view window, hard-capped at `MAXIMUM_BET` (#301–#304). **(2) Session view:** a NEW `SessionAnalytics` accumulator (`GameRecord.hpp:127`) spanning every shoe in one open play window, reset only on open/close — deliberately NOT derived from career `longHistory` (§10 #9a); `G` cycles the window's view (#175, #176). **(1) Per-graph control:** each pane is its own per-shoe ↔ per-session switch — a left click inside a pane moves ONLY that pane (`HUD::cycleGraphScope`, hit-tested via `chartPaneBounds()`), `G` applies one view to all three (`setAllGraphScope`). This is a **view** toggle, not the **visibility** toggle the original write-up proposed — see the §5 decision of 2026-11-12. Session backing data is computed in `update()` only while at least one pane is showing it, so the default per-shoe frame cost is unchanged. Headless: **#241–#244** — #243/#244 assert the *dataset selection* through the same `active*()` accessors `drawCharts()` draws from, and were verified red by mutating the selection. On screen: `tools/visual_check/check_u12_click.py` proves by ImageMagick `compare` AE that one click changes exactly one pane's title strip and that `G` changes the other two. |
| U13 | 🟡 | Non-ASCII text passed to SFML as `std::string` renders as mojibake/tofu on screen | **Resolved 2026-11-09** | Found by **screenshot**, not by reading code — the same blind spot that hid U1 for two audits. `sf::String(const std::string&, const std::locale& = {})` is an **ANSI** string converted through the locale (`SFML/include/SFML/System/String.hpp:173`), so a UTF-8 source literal decodes to several code points and the 0x80-range byte among them has no glyph. Two live sites: `Game::bettingControlsHint()` rendered as `□□□ Increase Bet  □□□ Decrease Bet` in the play band (U+2191/U+2193/U+2190/U+2192 are **genuinely absent** from the Noto cmap — fontTools — so ASCII is the only correct fix), and the name-entry hint's U+2022 bullets rendered as `â<control>¢`. Both replaced with ASCII; the name-entry string was extracted to the pure helper `PresentationLayout::nameEntryHintText()` so it is headless-testable. Test **#326** (`isSfmlSafeAscii`) — red on the pre-fix byte sequences, green after (verified with a standalone TU, not just asserted). **This row also corrects a false claim**: the earlier note said the shipped font lacks U+203A `›`; it does not (`guilsinglright`, 1 contour) — the marker's tofu was the ANSI decoding. Recorded, not fixed: `Card::getSuitSymbol()` returns `u8"♠"[0]`, a single truncated byte that can never be a suit glyph — dead code, no callers. Also fixed in passing: `include/PresentationLayout.hpp` was not self-contained (no `<string>`); it compiled only because a prior include supplied `std::string`, and a standalone-compile check now passes. |
| T1 | 🟢 | Save amplification (profile rewritten every settled round) | **Resolved 2026-11-12** — write cadence gated, storage compacted, shoe stored as a seed | Three levers, all landed. **(1)** The *file write* is gated: `AUTO_SAVE_EVERY_HANDS = 5` (`Constants.hpp:184`), enforced in `evaluateHands()`, proven by the autosave tests; one of those assertions was corrected in B18 to count settled hands rather than driver rounds. **(2)** `src/PlayerProfile.cpp:190` wrote `dump(2)` — pretty-printing was **64.6% of the real profile** (6,199,262 of 9,591,391 bytes; 427,145 lines) for a file no human reads. Now `dump()`. **(3)** Every shoe stored a full copy of its 313-card deck, **792,200 bytes** on the real profile, and its only consumer (`src/Replay.cpp`) needed the shoe's SIZE. A shoe is now stored as the two integers that rebuild it — `shoeDecks` + `shuffleSeed` (see B21) — and the array, its field and its compatibility branch are deleted. Measured on the real profile: **9,591,391 → 2,724,930 bytes, a 71.6% cut**. The per-round `RoundRecord` append at `HandEvaluator.cpp:295` is deliberately **left as-is**: `currentHistory` measured 22,888 bytes — **0.2% of the file** — so it was never the cost, and T2's 100-shoe cap bounds the file. Locked by **#257–#261** (`tests/test_profile_settings.cpp`), red before the change. |
| T2 | 🟢 | Unbounded history | **Resolved** | `MAX_LONG_HISTORY_SHOES = 100` (`Constants.hpp:181`) with oldest-first roll-up. Tests #301–#304 (`tests/test_history_rollup.cpp`, `[T2][history-cap]`) cover the cap, exact career-total preservation across a roll, save/load survival and the no-op case. §11 previously listed T1 and T2 on one row as Open — T2 has been done since those tests landed; the row was split 2026-11-09. |
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

### 2026-11-12 — U12 part 1 (per-graph view toggle) landed; the `-j8` flake (B20) root-caused and fixed; three doc claims corrected; the visual-check script was silently broken

- **U12 is now Resolved — all three parts.** Part 1 (per-graph control) was the last open piece: a left click inside a
  side-panel pane toggles **only that pane** between its per-shoe and per-session dataset (`HUD::cycleGraphScope`,
  hit-tested through `chartPaneBounds()`), and `G` applies one view to all three (`HUD::setAllGraphScope`, wired from
  `Game::cycleGraphView`). Headless: **#241–#244**. On screen: `tools/visual_check/check_u12_click.py`.
- **The part-1 semantics were decided, not assumed.** `NEXT_STEPS.md` PHASE 4 specified a per-series **visibility**
  toggle (hide a graph). What shipped is a per-graph **view** toggle. Kept the shipped behaviour and corrected the
  ledger — see the §5 decision added today. §11's U12 row was also stale in its *symptom* column (it still described
  only the histogram axis, written before the session-view and toggle halves existed).
- **Doc drift corrected in the same commit (three sites).** (1) `HUD.hpp`'s `chartPaneBounds()` comment asserted the
  panes "are no longer clickable" — true of the removed overlay feature, false of what replaced it; the panes are the
  click target now. (2) `Constants.hpp:197` still described the buckets as spanning `[0, MAXIMUM_BET]` after the axis
  work moved it to the `[MINIMUM_BET, 100]`-grows-to-largest model (§10 #8). (3) `NEXT_STEPS.md` PHASE 4's U12 write-up.
- **New headless seam:** `HUD::activeBankrollSeries()/activeEdgeSeries()/activeBetHistogram()/activeBetBucketWidth()/
  activeBetAxisLo()` are the accessors `drawCharts()` itself draws from, so #243/#244 assert the *dataset selection*
  rather than the flag. Verified to have teeth: forcing `activeBankrollSeries()` to ignore its scope flag turns
  **#243 and #244 red** while #241/#242 stay green — the flag tests alone did not cover the selection.
- **Per-frame cost:** `HUD::update()` runs every frame (`GameRenderer.cpp:75`, `:938`). The session backing data is now
  computed only while at least one pane is showing its session view, so the default per-shoe frame keeps its pre-U12
  cost. The header comment claiming the bucketing was not recomputed per frame was **false as written** and is corrected.
- **`tools/visual_check/check_u12_click.py` was broken in two ways that both read as a clean result** — recorded because
  each one hides a real failure rather than announcing it. (1) ImageMagick `compare -metric AE` prints the metric on
  **stderr** and exits **1** when images differ; the script parsed `stdout`, so every measurement was `-1`. (2) The three
  crop rows were **guessed from a screenshot**: pane 0's row clipped the bottom of its title strip (so it did change),
  while panes 1 and 2 landed in **blank panel background** and compared **0 forever** — "unchanged", which is exactly
  what a passing assertion wants to see. The rows are now *derived* from the same `Constants::Hud` values
  `HUD::leftPanelCharts()` uses (panes top at 10 / 277.3 / 544.7 at 1600x900). **Rule for the next visual check: derive
  crop regions from the layout constants, and prove a crop can go non-zero before trusting it going to zero.**
- **B20: the suite's `-j8` flake was root-caused and FIXED, not just recorded.** `#175` ("U12: the session accumulator
  spans shoes...") failed 1 of the first 10 `-j8` runs. It passes in isolation, and a clean `HEAD` worktree (330 tests,
  no U12 part-1 code) showed the **same rate** — 1 failure in 10 runs, then 0 in 14 more — so it **predates this work**;
  establishing that baseline is what keeps it off U12's record. Root cause is the **B18 class**: the test seeds a
  300-card all-value-8 neutral deck, then crosses a shoe boundary via `startNewShoeForTest()` → `startNewShoe()`, which
  **discards that deck and shuffles from the OS RNG**, so the rounds driven afterwards and the asserted series lengths
  depended on the shuffle. Fixed by re-seeding the deterministic all-8 deck after the boundary. **Measured: 120 single-test
  runs (one process each, so a fresh OS RNG) = 12/120 failing before, 0/120 after.** Test-only; no product code.
- **B20 nuance worth keeping: the B18 remedy does not transfer verbatim.** The obvious fix — `startNewShoeWithSeedForTest(42)`,
  the seeded real shuffle B18 introduced — was tried, and #175 then failed **deterministically**: a seeded real shuffle
  deals a natural, which changes how many rounds settle, and the exact-count assertions read that as a broken accumulator.
  #175 asserts **exact round counts**, so it needs a **controlled** deck (all-8s → no naturals), not merely a repeatable
  one. `shuffleWithSeed` is correct where assertions are bands/spans sampled from a distribution (that is what B18's case
  was); it is not sufficient where a test counts rounds. Picking the wrong one of these turns a flake into a hard failure,
  so check which kind of assertion you are stabilising before choosing the remedy.
- **False alarm worth recording: a moved profile mtime was the developer playtesting, not a leak.** The real
  `player_profile.json` mtime moved to 01:53:23 mid-session, which reads as a B17 hermeticity breach. Four candidate paths
  were probed with md5+mtime captured before and after — the full suite at `-j8`, the real binary with `HOME` redirected,
  `tools/visual_check/check_u12_click.py`, and a clean configure+build (the `catch_discover_tests` path, which executes the
  test binary at build time) — and **all four left the file byte-identical**. The cause was a manual play session, which is
  the app behaving correctly. What actually discriminated: the file stayed **9,591,391 bytes** with `name: "William"` and
  100 shoes at the T2 cap, whereas the B17 writer had truncated it to **1.34 MB** with test data. Recorded in §11's gate
  note so the next window checks for a manual run before bisecting.
- **Dead constant recorded, not removed:** `Constants::BET_BUCKET_WIDTH` (`= MAXIMUM_BET / BET_BUCKET_COUNT` = 100) has
  **zero users** in `src/`, `include/` and `tests/`. It encodes the superseded fixed-axis model, so it is a landmine for
  anyone wiring the histogram up again — `HUD` derives the bucket width per view instead. Left in place; delete it with
  the next axis change.


### 2026-11-11 — U12 decisions #8/#9 recorded; B16 doc drift corrected; Card::getSuitSymbol removed

- **§10 #8 (U12 axis half) decided = option (a):** the Bets Placed histogram axis is [10, 100] by default. If any
  wager placed since the view window began exceeds 100, that wager becomes the new soft max and the axis grows to match
  it; it never crosses a hard cap of `MAXIMUM_BET` = 1000. Implementation lives in U12 (axis half).
- **§10 #9 (U12 session-view half) decided = option (a):** a "session" is one open play window — from when the user
  launches the game to when it closes. It may span multiple complete shoes or an incomplete shoe; its accumulator runs
  across those shoes and resets only on open/close, distinct from career `longHistory` and per-shoe `ShoeAnalytics`.
- **B16 doc drift:** `NEXT_STEPS.md` PHASE 5 still described `reconstructDeckAtRound()` as pending; it was already
  removed in `5f81d48` (2026-11-07) and recorded Resolved in §11. Corrected the ledger to mark it Resolved.
- **Card::getSuitSymbol** dead code removed (`667cd16`). It returned a single truncated char that could never be a suit
  glyph; zero callers across `src/` + `include/`.

### 2026-11-09 — B17 was NOT complete: the suite was still rewriting the developer's real career profile

**How it surfaced.** The post-U-series soak was green (10 clean / 0 failing, 326/326) and I checked the developer's
profile md5 as the B17 entry instructs. It had **changed**: `686b9192…` → `6482a65d…`, and the file had gone from
**9.3 MB to 1.34 MB**. The suite was writing the real profile again. The earlier soak in the same session had reported
the md5 stable, so "stable across a soak" is a weaker check than it looks: it compares consecutive runs against each
other, and a writer that produces the *same* content every time leaves the md5 unchanged from run 1 onward.

**Finding it.** Serial bisect over every test index — md5 the profile, `ctest -I i,i`, md5 again, report any index
where it moved. One writer: **Test #99, "Resplitting tens is refused when RESPLIT_TENS_ALLOWED is false"**
(`tests/test_settlement.cpp`). It drives a real round (`start_round` + `tickFrame` + `split()`), and settlement reaches
the profile write. The adjacent SplitTens cases drive the same flow; the bisect reports only the first because the
baseline is updated after each detected write.

**Scope of the hole.** Eight files construct `Game(true)` and drive real rounds with **zero** `IsolatedHome` guards:
`test_settlement` (21 cases), `test_side_bets` (18), `test_game_rules` (12), `test_achievements` (7),
`test_advisor_feedback` (4), `test_counting_box_label` (3), `test_max_split_hand_rules` (2), `test_side_bet_paytable`
(1) — **68 cases**. B17 redirected five files and stopped. §11's claim that the suite is hermetic was **overstated**,
and §11 has been corrected.

**Fix — process scope, not 68 call sites.** B17's own recorded decision was "centralise rather than patch hand-rolled
redirects", so patching 68 sites contradicts the precedent, and any new round-driving test in those files would
re-open the hole silently. `catch_discover_tests` launches every TEST_CASE as its **own process of one binary**, so a
single object constructed during static initialisation redirects `$HOME` for every test process. Added
`test_support::ProcessHomeIsolation` and instantiated it in a new always-compiled TU, `tests/test_process_isolation.cpp`
(an `inline` object in the header would not work: `test_betting_controls_hint.cpp`, `test_presentation_layout.cpp` and
others do not include `test_support.hpp`, and would silently run unisolated). The per-case `IsolatedHome` is unchanged
and nests correctly — it captures the process temp dir as its "original" and restores to it.

**Lock.** New test **#327** (`[B17][hermeticity]`): `$HOME` is a `bj_proc_home_<pid>` directory, the real profile path
is **not reachable** from inside a test process, and a nested `IsolatedHome` restores to the process dir.

**Verification.** Re-bisect: **zero writers across all 327 tests**. Full `-j8` suite: 327/327 with the profile md5
unchanged.

**Data recovery.** The live profile had already been replaced by a test-written 1.34 MB file. A 9.3 MB
`player_profile.corrupt.1794228271.json` (mtime 06:44, before this session — renamed by B14's corrupt-save handler)
was valid JSON with `name=William`, `bankroll=17232`, 100 shoes and full stats. Per the developer's choice it was
promoted over the test-written file, and **verified loadable**: the game logs `Loaded profile for William with
bankroll 17232.` with no corrupt rename. Recorded so nobody re-deletes it.

**Gate change worth adopting.** "md5 stable across a soak" is not sufficient — it must be **md5 before the soak, md5
after the soak, against a baseline captured outside the suite**, plus the per-index bisect when anything writes.

### 2026-11-09 — U13: non-ASCII text reaching SFML, and a §5 claim of mine that was wrong

**What prompted this.** The U1 verification screenshot showed the hover marker as tofu boxes, and I wrote the cause
into §5 as "the shipped font has no U+203A glyph". That claim was **never checked against the font** — it was inferred
from a screenshot. Checking it with fontTools: `assets/fonts/noto/NotoSans-Regular.ttf` maps **U+203A → `guilsinglright`**,
1 contour, bbox (39,56,270,471). The glyph is there. §5's bullet has been rewritten.

**The real cause.** SFML 3's `sf::String(const std::string& ansiString, const std::locale& locale = {})`
(`SFML/include/SFML/System/String.hpp:173`) is an **ANSI** string converted through the locale — not UTF-8. A UTF-8
literal in the source is therefore decoded as its individual bytes: `›` = `E2 80 BA` → `â`, **U+0080**, `º`. U+0080 has
no glyph → the box. The plural "boxes" in the screenshot was the tell I did not read at the time.

**Sweep, not spot-fix.** Grepping `src/` for non-ASCII in code (not comments) found four sites, triaged:
`Game::bettingControlsHint()` (U+2191/2193/2190/2192) — **visible defect**, rendered as `□□□ Increase Bet` in the play
band, and these four arrows are **genuinely absent from the Noto cmap**, so ASCII is the only correct fix;
the name-entry hint (two U+2022 bullets) — **visible defect**, extracted to the pure
`PresentationLayout::nameEntryHintText()`; `Game::startupControlsBanner()` (`♠ ♣ ↑ ↓`) — **not a defect**, it is
`std::cout` on a UTF-8 terminal; `Card::getSuitSymbol()` (`u8"♠"[0]`) — returns a single truncated byte and can never
produce a suit glyph, but has **no callers**, so it is recorded as a trap, not fixed.

**Test.** **#326** asserts `isSfmlSafeAscii()` on every hint helper. Red verified properly this time: a standalone TU
compiling the **exact pre-fix byte sequences** asserts the predicate rejects them — the earlier rounds' red-checks were
asserted in prose only. Suite 325 → **326/326**.

**Header hygiene (found by the same TU).** `include/PresentationLayout.hpp` used `std::string`/`std::to_string`
without `#include <string>`; it compiled only because an earlier include supplied them. Added. A header that is a
pure-geometry seam should compile standalone, and now it is checked to.

**Gate.** `ctest` serial **326/326**. Ten-run `-j8` soak: **10 clean / 0 failing**, every run 326/326, developer's real
profile md5 unchanged.

**Lesson for the next visual fix.** A screenshot tells you *what* is broken; it does not tell you *why*. Write the
cause as a hypothesis, then test the hypothesis against the artifact (the font's cmap, the library's header) before it
goes into §5 — the next window trusts that file.

### 2026-11-09 — U1/U2/U3/U6/U11: five vision-critical UI defects fixed, and the rendering blind spot closed with a screenshot harness

**Method — the new part.** §11 tagged this whole set "Headless-testability: HARDEST … no pure headless unit test can
lock rendering", and every prior verification of it was **by code pattern** (grep + line numbers). That is exactly how
U1's root cause stayed invisible for two audits: reading `drawMenuItem` shows a gold row; only a **screenshot** shows
that the gold row under the cursor is not the row Enter will fire. So the fix round started by building the missing
tool: `Xvfb` + **python-xlib** (`xdotool` is not installed; python-xlib is) driving the real window —
`/tmp/bj_visual.py` (menu, resize, achievements, pause), `/tmp/bj_u6.py` (deal → hit fan → settle), `/tmp/bj_menu.py`
(hover marker). Screenshots land in `/tmp/bj_shots/`. Input is synthetic X11 (`XTEST`), so the app is unmodified.
**Two harness lessons worth keeping:** (1) keyboard `Up`/`Down` **wraps**, so a fixed number of presses is not
deterministic navigation — navigate by **clicking the row the renderer draws**, which also exercises the U11 hit-test
on every transition; (2) `Escape` at the main menu **closes the window**, so a script that presses Escape "to go back"
kills the app and the next screenshot is pure black. The harness runs under a **temp `$HOME`** seeded with a profile
(B17's hermeticity rule applies to manual runs too), and reports the real profile's presence at the end.

**U1 — menu hover and selection were the same state.** Two compounding causes. `InputHandler::processMenuEvents`
called `applyMenuRowSelection()` on `MouseMoved`, so an incidental mouse sweep **moved the keyboard selection**; and
`drawMenuItem` rendered `if (hovered || selected)` as the same gold, adding only `Bold`. Fix: hover is purely visual
(background pill + leading marker) and never mutates `menuSelection`; gold is reserved for the keyboard selection; a
click activates. Verified on screen: gold "Review History" (selected, row 3) and pill+marker "Achievements" (hovered,
row 6) simultaneously — the two states are unambiguous.

**U2 — pause menu had no panel.** `drawPauseMenu` painted a full-window alpha-128 overlay and drew its rows straight
over the live table, so "Quit to Menu" collided with the seat-name label. Fix: a centred panel (fill + outline) sized
from the row bounds with padding. Verified at 1600×900 and 900×560: the "William" label is now behind the panel
instead of under the text.

**U3 — Achievements / Game Rules did not reflow.** `drawAchievementList` used `y = 96`, `rowH = 60` while the footer
sat at `h - 28`, so at 900×560 the list ran under the footer. (Correction to the earlier audit note: `drawGameRules`'s
`startY = h*0.30f` is already fractional — only `spacing = 46f` is hard-coded — and its 7 rows **do** fit at 900×560;
**Achievements is the screen that provably overlaps**.) Both screens now derive geometry from the world size.

**U6 — card fan showed a sliver; hand totals were unreadable.** Cards advanced 16px, so 3+ card hands hid all but a
strip of each card, and `drawTotal` drew bare 16pt white over the felt. Fix: `cardFanCenterShift` centres the fan on
the seat (verified with a **7-card hand** centred on the chip/label at x≈570) and a dark pill backs every total
(dealer **7**, player **28** legible over the "Dealer must hit soft 17" felt text).

**U11 — hover drifted after a resize.** The renderer laid out in `gameView` **world** units (equal to window pixels
only at exactly 1600×900) while the InputHandler hit-tested in **raw window pixels** with 900-basis constants; the
pause hit-test used desktop-space `sf::Mouse::getPosition()`. Three screens, three bases. Fix: one accessor,
`GameRenderer::worldSizeOf(const Game&)`, used by **both** drawing and hit-testing. Verified: after a 1600×900 →
900×560 resize the pill+marker land on the drawn row, and hovering the *old* pixel position highlights nothing.

**Font-glyph finding (new, and it bit immediately) — SUPERSEDED by the entry above.** The first U1 build drew `›`
(U+203A) as the hover marker; the screenshot showed tofu boxes, and this entry attributed it to the shipped font
lacking the glyph. **That attribution is wrong** — the font has `guilsinglright`; the cause is SFML 3's ANSI decoding
of `std::string`. See the U13 entry above; §5's bullet has been rewritten. The observation that the U4 betting-hint
strip renders `↑`/`↓` as tofu was correct and is now fixed as U13.

**Tests.** `tests/test_presentation_layout.cpp` — nine cases, **#317–#325**: hover ≠ selection; pause panel contains
every row + title + hint; achievements never overlap the footer at any world size; rules rows never overlap the
instruction line; fan shows more than a sliver of each card; the total's pill fully backs the number; every menu
hit-test resolves the row the renderer draws at every world size; the main menu reflows so its last row and hint stay
on screen; the renderer's accessors agree with the pure helpers. Registered in `CMakeLists.txt`. Suite 316 → **325/325**.

**Gate.** `ctest` serial **325/325**. Ten-run `-j8` soak: **10 clean / 0 failing**, every run 325/325, developer's
real profile md5 (`686b9192…`) unchanged across the soak.

**Docs.** §5 gains three decisions (`worldSizeOf` as the single layout basis; hover as a non-mutating visual state;
ASCII `>` for the hover marker + the glyph-coverage rule). §11 splits the old single `U1–U12` row into three:
`U4/U5/U7/U8/U9/U10` (Resolved this phase), **`U1/U2/U3/U6/U11` Resolved 2026-11-09**, and **`U12` Open** — U12 is the
only UI item left, and it needs a bucket-bounds decision (§10). §11's header now names U12 + T1's growth half as what
is open. Test IDs re-derived from `ctest -N` per §13's standing warning: **U7 is #287 + #289–#291** and **U10 is
#282/#283** (the §11 row's "#286–#288" / "#281/#282" are stale).

**Commit note.** The five fixes ship as **one commit**, not five. The §12 "one-bug-one-commit" rule assumes separable
changes; here `worldSizeOf` is the shared foundation of U1, U3 and U11, and all five edits land in the same regions of
`GameRenderer.cpp` plus one new header (`include/PresentationLayout.hpp`) that holds the pure geometry helpers every
one of them uses. Splitting would have produced commits that do not build independently.

### 2026-11-09 — B17/B18/B19: the "315/315 green at `-j8`" baseline was false; three defects found from it

**Method.** Ran the documented gate verbatim. Configure + build clean. The first `ctest -j8` returned **314/315**
(`#290 needsNameEntry gates on whether the profile has a persisted name`), while `ctest -R NameEntry -j1` passed 3/3.
So the suite is green serially and flaky in parallel. Measured it: **3 of 4 `-j8` runs** had ≥1 failure, and a 10-run
soak came out **7 clean / 3 failing**. Failures observed: #290 (×2), #259, #306, #169.

**B17 — the suite is not hermetic (the flake).** `Player::getSavePath()` (`src/Player.cpp:14`) resolves
`$HOME/.local/share/blackjack/saves/player_profile.json` from the environment on every call. Five test files each
redirected `HOME` to a **fixed** path (`/tmp/bj_b1_test`, `/tmp/bj_u8_test`, `/tmp/bj_b7_test`,
`/tmp/bj_t1_autosave_home`, `/tmp/bj_name_entry_0`). ctest + Catch2 run each `TEST_CASE` in **its own process**, so
those directories are shared by every concurrently-scheduled test. The smoking gun: `test_name_entry.cpp`'s guard
built its dir from a `static int counter`, which is per-process and therefore **always 0** — its comment claimed
"per-invocation unique ... avoids races under ctest -j8", and it did the opposite. #291 (`commitPlayerName persists`)
writes a profile named "Ada" into `/tmp/bj_name_entry_0`; #290 (`needsNameEntry gates`) then loads it, sees a
non-empty name and fails `REQUIRE(needsNameEntry() == true)`. Reproduced deterministically by running the two side by
side (1/25). A per-`TEST_CASE` audit (not per-file — a file with one isolated case can have nine unisolated ones)
found **9** more cases calling `startNewGame()` / `startTournamentSession()` / `startStatsSession()` /
`rememberTableSettings()` with no redirect, which rewrite the **developer's real** profile; its mtime moved across a
suite run. Fix: one shared RAII `test_support::IsolatedHome` whose dir embeds the **PID** plus a per-process counter;
migrated all five hand-rolled redirects and guarded all nine cases. After: **10 clean / 0 failing**, real profile md5
stable across the soak. No product code changed.

**A log line that misleads.** `src/Player.cpp:47` prints `playerName` — the **seat** name (`"William"`, hard-coded) —
not `profile.getName()`. "Loaded profile for William with bankroll 10000" therefore does **not** mean the real profile
was read; 10000 is `STARTING_BANKROLL`. It sent the first hour of this investigation down the wrong path.

**B18 — two tests asserted against an unseeded OS RNG.** `Deck::shuffle()` reseeds from `std::random_device`
(`src/Deck.cpp:41`). #259 swept one *random* shoe per run; #306 asserted `writes == rounds / AUTO_SAVE_EVERY_HANDS`
against **driver rounds** while the cadence counts **settled hands** — `handsEvaluated` measured 10, 11 and 12 across
12 runs, so the fixed `floor(12/5)==2` was only sometimes true. Added `Deck::shuffleWithSeed()` (one private
`shuffleWith()` body shared with `shuffle()`, so the production OS-seed path is byte-identical),
`StrategyTrainer::startSessionWithSeedForTest()` and `Game::startNewShoeWithSeedForTest()` (shared
`startNewShoeWith()` body). #259 and #169 now sweep 5 fixed seeds; #306 asserts against the settled-hand count it
gates on. #169 was a **third** flake of this class, exposed only after B17/B18 removed the noise hiding it.
Retargeting #306's assertion was **not sufficient on its own** — a soak of 20 still failed 1. The autosave counter
accumulates settled hands and **resets on a write**, so a round that settles two hands (a split) discards the overflow
and the write count lands *below* `floor(hands/5)`. The case now seeds a uniform alternating 10/7 shoe, which makes the
human's hand a hard 17 every round: never a pair (so no split), never a natural (which settles immediately), so exactly
one hand settles per round and `handsEvaluated == rounds` becomes a checkable precondition of the exact-gating
assertion. 0/30 after.

**B19 — #259's flake was a real product defect, not a flaky test.** `StrategyTrainer::nextQuestion()` rejects
blackjacks with `do { draw 2 player + 1 dealer } while (questionHand.isBlackjack() && attempts < 500 && deck.size()
>= 3);` followed **unconditionally** by `computeBookAction()`. The entry guard only requires `deck.size() >= 3`, so the
first pass can draw the shoe dry; when that hand is a blackjack the `deck.size() >= 3` term is false, the loop exits,
and the natural 21 is **presented** — a hand with no decision to train — with `active` left true on a spent shoe.
Reaching it needs the shoe to empty at exactly that moment, which is why it flaked under load instead of failing.
Proven test-first with a new `setDeckForTest()` seam injecting an exact 3-card shoe: **red before** (both
`!isBlackjack()` and `!sessionActiveForTest()` failed), green after. New test #316; suite 315 → **316/316**.

**Claims in these docs corrected this pass:**

1. **§11 header + NEXT_STEPS gate — "315/315 green (`-j8`)" and "Suite at 297/297"** were both stale/false. §11 now
   states 316/316 serial and flags the `-j8` figure as trustworthy only from B17 onward; the NEXT_STEPS gate gains a
   10-run soak, because one passing `-j8` run proves almost nothing about a suite that fails 3 runs in 10.
2. **§11 header "All 15 defects (B1–B15) plus U1–U10, T1–T2 and B16 are still open"** — the 2026-11-06 snapshot,
   flatly contradicted by the table printed directly beneath it. Replaced with the current composition.
3. **§11 "T1–T2 | Open"** — T2 is **Resolved**: `MAX_LONG_HISTORY_SHOES = 100` (`Constants.hpp:181`) plus tests
   #301–#304 (`test_history_rollup.cpp`, `[T2][history-cap]`). The row was split; T1 is **Partial** — the *write* is
   gated by `AUTO_SAVE_EVERY_HANDS` (`Constants.hpp:184`, proven by #305/#306) but the per-round `RoundRecord`
   append at `HandEvaluator.cpp:295` still grows the history every settled round. The 9.3 MB real profile on this
   machine is the residue of that growth.
4. **§11 test IDs drifted again** (the docs already warn about one +3 shift; it has moved since): **B13 is #280**, not
   #273 — #273 is now a Surrender test, exactly the trap §13:657 warns about; **B9 overlay is #299/#300**, not
   #296/#297 (those are the Logger-flush tests); **B11 is #298**, not #295; **U7 is #289–#291**, not #286–#288.
   Verified correct as written: U4 #307–#308, U5 #309–#311, U9 #312–#315, U10 #281/#282. §6.10's
   "`test_profile_settings.cpp#273`" pointer is stale for the same reason.
5. **§13:666 "B8 (`Deck.cpp:123-137`) still open"** — B8 is **Resolved** (`11f6f50`); the §11 row says so and this
   §13 sentence contradicted it.

6. **`GameRenderer.cpp` citations drifted ~+20** (the B9 comments were inserted upstream of them): U1 `:1034-1046` →
   `:1055-1073`; U2 `:1274` → `drawPauseMenu` `:1327`; U3 `:108-141` → `:113-154` and `:1236` → `:1289`; U6 `:468` →
   `:474`. U11's `:1071` and `:1134` are exact. All six U-sites were re-verified **by code pattern, not by line
   number**, and every described defect is real: `drawMenuItem` draws hover and selection in the same gold
   (`if (hovered || selected)`, hover only adds `Bold`); `drawPauseMenu` draws a full-window alpha-128 overlay and
   bare rows with **no panel**; `drawAchievementList` uses `y = 96.f` / `rowH = 60.f` against a footer at `h - 28.f`;
   `drawTotal` is bare white 16pt; and the three hover hit-tests use three different coordinate bases
   (`getHoveredMenuItemIndex` `mapPixelToCoords` vs absolute `item.y`; `getHoveredGameRulesRowIndex`
   `mapPixelToCoords` vs `h*0.30`; `getHoveredPauseMenuItemIndex` **raw `sf::Mouse::getPosition`** vs `pauseRowY`).
7. **U3's "Game Rules uses hard-coded pixel offsets" is only half true**: `startY = h * 0.30f` is fractional and the
   7 rule rows fit at 900×560; only `spacing = 46.f` is hard-coded. Achievements is the fully hard-coded screen and
   the one that provably overlaps at 900×560 (rows run to `96 + 60N`, footer sits at `h - 28 = 532`).
8. **`test_name_entry.cpp:22`'s comment** ("A per-invocation unique dir avoids races under ctest -j8") described
   behaviour it did not have; replaced with the B17 explanation.

**Not done:** the vision-critical U-items (U1, U2, U3, U6, U11, U12) are untouched — this round hardened the gate so
they can be worked against something that does not lie. §10 #5 (hint-strip placement) still blocks U2/U3 layout.
B9 stays **Partial**: the intermittent crash is still unreproducible and carries no test.

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
`gameRules` key list; §6.1–§6.9 evidence; B8 (`Deck.cpp:123-137`) still open **[STALE — B8 was Resolved on 2026-11-07
by `11f6f50`; this sentence is the 2026-11-06 record, kept as written. See §11's B8 row and §13 2026-11-09 item 5.]**;
§11 open rows for B5/B8/B9/B10/B11 match
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

- **U1–U12 (UI/UX rendering) — HARDEST / not truly headless.** Every item needs an SDL window context and a
  screenshot/Xvfb image diff to assert anything visual; no pure unit test can assert pixel-level rendering and diffs are
  flaky. These are the only items genuinely difficult headlessly, including U11 (cursor↔draw drift across window sizes with
  live mouse input) and U12 (side-panel graph toggles, per-shoe/session view switch, Bets Placed axis bounds). Verify with
  Xvfb + screenshots, or escrow-at-commit.
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

### 2026-11-07 — B11 banner mislabel fixed; B9 §11 evidence sentence corrected

**B11.** The startup "PLAYER CONTROLS" banner was extracted from the inline `Logger::info(R"(...)")` in `Game::Game()` into a single pure helper `Game::startupControlsBanner() const` (`include/Game.hpp`, defined `src/Game.cpp:170`) and its betting-phase labels were corrected to match the real bindings at `InputHandler.cpp:635-645` (PlaceBets: Left = auto Kelly via `calculateOptimalBet()`/`setBet()`, Right = confirm). The auto-kelly row now reads "Left Arrow" (it had been mislabelled "Right Arrow"); the incidental trailing space on that line was also trimmed. Because the helper is pure, tests read it from a headless `Game(true)` without any window; `tests/test_banner_b11.cpp` (#295) asserts Left-present / Right-absent on that row and fails without the fix (red-green verified). Full suite **294 → 295/295** (0 failures).

**Docs corrected this round.** §11 B9 evidence no longer says `Logger.cpp:34` "still writes std::cout with no flush/unitbuf" — production now flushes per record via `std::endl` (`d6d6436`) and is covered by #284/#293/#294, so the row was retagged **Partial** (flush half fixed; flaky-crash + summary-overlay-bound-guard halves still open). §11 B11 row → Resolved.

Note: `-j8` ctest can report `BAD_COMMAND` or a transient single-test fail immediately after a rebuild — a runner race, not a regression (documented since 0.3); the serial suite is authoritative and stays green.

### 2026-11-07 — B9 overlay bounds guard fixed; B9 §11 status corrected to reflect it

What was wrong: `drawSummaryOverlay()` loops over summaries bounded by `playerSummaries.size()` (`GameRenderer.cpp:1022`) but read the per-player value from `game.players[i]` at `:1025`, an out-of-bounds access whenever live players exceed the number of summaries, plus an unsigned underflow in the per-row fan-width helper for the empty/single-hand case. This is B9's second DoD clause (the first — flush-per-record — was fixed by `d6d6436` and covered by #284/#293/#294).

Fix (`facea14`): renderer-only, headless-testable helpers. Rows clamp to `min(summaryCount, playerCount)` and the fan-width helper returns 0.f for an empty or single hand (no underflow), so the per-player read can never exceed the summary loop bound. Pure presentation logic — no persisted field, no data-model change, additive/presentation contract intact. Tests #296/#297 (`tests/test_summary_overlay_bounds.cpp`), registered via CMakeLists.txt: `summaryOverlayRowCount` clamps to min(summaryCount, playerCount); `summaryRowFanWidth` has no unsigned underflow for empty/single-hand — both fail before the clamp and pass after (red-green). Full suite now **297/297** (0 failures), up by two on the 295 pre-fix baseline.

Docs: §11 B9 row retagged from "flaky-crash + overlay-guard halves open" to **Partial** with the overlay bounds guard marked resolved via `facea14` (#296/#297); §5 decision list appended (overlay bounds-guard clamp design); §11 summary count updated 291 → 297; NEXT_STEPS.md B9 instructions + Definition-of-done checklist updated so the overlay guard is not re-attempted. The intermittent single-occurrence crash is still genuinely unreproducible (one playtest hit, three failed replays, one clean 60-cycle soak) and carries no test — B9 stays **Partial**, not Resolved, on that basis.

### 2026-11-12 — T1 (save amplification) resolved by measurement; B21 opened; §10 and the DoD corrected

**T1 → Resolved.** The item was written as "the per-round `RoundRecord` append at `HandEvaluator.cpp:295` grows the
profile; the 9.3 MB real file is that residue." Profiling the file disproved the framing: of 9,591,391 bytes,
**6,199,262 (64.6%) is indentation whitespace** (`src/PlayerProfile.cpp:190` wrote `dump(2)`; 427,145 lines),
31.8% is `longHistory` rounds, 8.3% is stored `initialDeck` arrays, and the structure the append grows —
`currentHistory` — is **22,888 bytes, 0.2%**. The append was therefore left untouched on purpose; T2's 100-shoe cap
already bounds it. Shipped instead: `dump()` (compact), and a new additive `initialDeckSize` int in place of the
313-card array, whose sole consumer (`src/Replay.cpp:157`) reads only its size. `from_json`'s `j.at("initialDeck")`
was a **required** key and is now `j.contains(...)`-guarded, so legacy arrays load and derive the count. Measured on
the developer's real pre-T1 file: loads intact (100 shoes, 31,300 deck cards), resaves to **2,725,730 bytes — 71.6%
smaller**. Locked by **#253–#257** (`tests/test_profile_settings.cpp`), red before the change; **#254** is the
legacy-load case. Suite **340/340**, clean 4/4 at `-j8`; the real profile was byte-identical before and after
(md5 `5fa55279422b`), so the work stayed hermetic.

**B21 opened.** `ShoeRecord::shuffleSeed` is declared, serialized in both directions and commented "MT19937 seed used
when shoe was created", and **is never assigned anywhere in the codebase** — 100/100 shoes on the real profile carry
`shuffleSeed: 0`. This killed the obvious T1 plan (drop the deck, reconstruct from the seed): reconstruction would
fabricate decks that were never played. Latent, not user-visible; the risk is the next reader who trusts the comment.

**Doc hygiene — two drifts closed, both of the "table contradicts the body" class.**
1. **§10's table** is headed "need an answer before the relevant fix" and showed rows **1–6 unmarked**, while §5
   recorded all seven as resolved on 2026-11-07. A reader would have reopened six settled decisions. Resolved markers
   added to all six. Row **3** additionally stated a default of **$1,000**, contradicting the shipped
   `STARTING_BANKROLL = 10000` (`Constants.hpp:287`); the default is corrected and the contradiction noted.
2. **NEXT_STEPS' Definition of Done** was **entirely unchecked** with stale evidence — it asserted "U1/U2/U3 still
   Open" (all Resolved 2026-11-09) and "currently **277/277**" (now 340/340), and cited B9's overlay tests as
   **#296/#297** where §5 records **#299/#300**. All eight clauses ticked with their commits and test IDs; the two
   clauses that cannot be met by a test-gated DoD (B9's unreproducible crash) are called out as such rather than
   ticked silently.

**Note for the next phase:** `src/Serialization.hpp` no longer exists, so B15's "two live copies" trap is historical.
There is a single `include/Serialization.hpp` today — the warning in §11's B15 row should be read as provenance, not
as a live hazard.


### 2026-11-12 (second round) — B21 closed in both directions; B9 closed as a testing artifact; history reset; legacy schema removed

**This entry supersedes the storage schema described in the previous 2026-11-12 entry.** That entry shipped
`initialDeckSize` as an interim, because the seed it wanted to store did not exist. It does now; the count field, the
deck array, its `ShoeRecord` field and its `from_json` compatibility branch are all **deleted**. A shoe is stored as
`shoeDecks` + `shuffleSeed`.

**B21 → Resolved, both directions as specified.** *Gameplay:* `Deck` already recorded `lastShuffleSeed` on every
reorder path (`shuffleWith()` seeded from `std::random_device`, and `reshuffleWithRemaining()`) — it had **no
accessor**. Added `Deck::shuffleSeed()` and `Deck::shoeDecks()`; the four shoe-creation sites (2 in
`SessionLifecycle.cpp`, 2 in `Game.cpp`) now record the real seed and deck count. Production keeps the OS seed, so
real shoe order stays unpredictable — the seed is recorded, not chosen. *Replay:* `Replay::buildSteps()` reads the
seed from the record and rebuilds the shoe (`create(shoeDecks)` + `shuffleWithSeed(shuffleSeed)`) to take
`totalCards` from it, which drives `cardsRemaining`/`trueCount`. **A latent second bug fell out of this:**
`lastShuffleSeed` was an *uninitialized* member, so a `Deck` filled by `create()`/`setCards()` without a shuffle
would have serialized indeterminate garbage as a plausible-looking seed. Now `= 0`, and locked by a test. Locked by
**#13–#16** (`tests/test_deck.cpp`) and **#258/#259** (`tests/test_profile_settings.cpp`).

**B9 → Resolved.** The developer confirmed the intermittent `R`-time crash came from **their own manual playtesting**,
which is exactly why it never produced a clean record (one playtest hit, three failed replays, one clean 60-cycle
soak): the triggering sequence was interactive and uncaptured. The repro scaffold at `tests/test_analytics.cpp:575`
drives the reported play → quit-to-menu → dashboard flow and passes. Recorded without embellishment: **no product-code
change was made for the crash**, and if it reappears from ordinary play it should be reopened as a fresh ID with a
clean capture. This closes the last Partial in the B-series.

**`Constants::BET_BUCKET_WIDTH` deleted.** Confirmed zero users across `src/`, `include/`, `tests/` and `tools/` — it
was `MAXIMUM_BET / BET_BUCKET_COUNT`, a *fixed* axis width, and U12 replaced that model with an axis that grows with
the largest wager in the view window, so the live bucket width is computed per view in `HUD.cpp`. A replacement
comment records why the constant cannot exist. `BET_BUCKET_COUNT` is live (HUD + tests) and stays.

**Profile history wiped, legacy schema support removed (developer's call).** The additive-JSON contract — kept every
phase — was deliberately broken to start from clean data. The 9,591,391-byte real profile was removed and backed up
to `~/.local/share/blackjack/pre-t1-backup/player_profile.pre-t1-2026-11-12.json.bak`, outside the saves directory so
the app never loads it. Verified against that backup under the new schema: it **still loads** (100 shoes, 1,688
rounds) and resaves to **2,724,930 bytes (71.6% smaller)** — `rounds` is unchanged and the removed key is ignored, so
pre-T1 history survives on any machine; only the stored decks are dropped, and pre-T1 shoes carry seed 0, so their
replay rebuilds a valid 313-card shoe rather than the historical order. **Kept on purpose:** the `j.value(...)`
defaults on fields that *are* written. Those are not backwards-compat, they are guards against truncated writes;
removing them saves zero bytes and makes a half-written file fatal.

**Suite: 344/344**, clean at `-j4` and `-j8`. The real profile was byte-identical before and after every suite run
(md5 `5fa55279422b`), so the work stayed hermetic; the one-off checks that *did* read it were run as temporary tests
and deleted, never committed.

**Test-ID warning for readers of this document.** Inserting tests renumbers every ID after the insertion point, and
the shift is **not uniform** because each citation was recorded on a different date. Measured today: the U12 session
test is **#179** (docs cite #175), the U13 ASCII test is **#343** (docs cite #339), and B17's process-isolation test
is **#344** (docs cite #327). Numeric IDs in §5/§11/§13 are therefore **provenance, not addresses** — they record what
was true when written. To locate a test, search its **name**. This is the same fragility §13 already flagged for
#296/#297; it is recorded here rather than chased, because rewriting ~40 historical citations would destroy the
audit trail that makes them useful.


### 2026-11-12 (third round) — IDEAS.md reviewed; a B21 regression and a second zombie field (B22) found by writing its test

**The review found two defects, both fixed, suite 345/345.** The point of reviewing `IDEAS.md` was to re-verify its
premises after the storage change. Verifying the claim that "the record describes the shoe" is what surfaced them.

**1. A regression B21 introduced, now fixed.** `reshuffleWithRemaining()` rebuilds the shoe from the full deck **minus
every card already dealt**, so a continuation shoe is smaller than `shoeDecks*52+1` and is a shuffle of a *reduced*
set. Rebuilding it with `create(shoeDecks) + shuffleWithSeed(seed)` therefore yields a full 313-card shoe — wrong size
and wrong order. The pre-B21 schema captured `deck.getCards()` verbatim and got the reduced size for free, so the seed
schema **silently inflated Replay's `cardsRemaining`/`trueCount` for every continuation shoe**. Fixed by storing
`shoeCardCount` (`GameRecord.hpp:186-193`), captured from `Deck::size()` at all four shoe-creation sites, with Replay
treating it as authoritative.

**2. B22 — the second zombie field in two days.** `ShoeRecord::isContinuation` was declared, set at one site, had
**zero readers**, and was **not serialized in either direction**: a saved continuation shoe loaded back as an ordinary
one. It is the only marker of the reconstruction boundary, so it is now persisted. Found by the test written for (1).

**The lesson, recorded because it generalises:** two fields in two days (`shuffleSeed`, `isContinuation`) were declared,
named as if meaningful, and carried no live value. **A field's name and comment are not evidence that it is assigned or
persisted** — grep for the assignment and for the serialization key before building on any field. And a test written to
lock a *new* mechanism is a good way to find the *old* one that mechanism silently replaced: the continuation test was
written to document a limitation and caught a regression instead.

**IDEAS.md review.** Written up in **`IDEAS-REVIEWED.md`** (a review layer; `IDEAS.md` is left as the proposal record).
Net effect of this phase's work on the backlog:
- **Every hard dependency the list cites is now Resolved** — B1, B2, B4, B5, B8, B13, B14, B15, B16, U3, U5, U7, T2.
- **#4 Shoe Codes is half built**: its stated core gap ("a seed that is stored but not usable") was closed by B21.
- **#1 Decision Fork's** hardest component — exact deck reconstruction — now exists **and is tested**; its storage
  premise changed from a 792 KB array to ~16 bytes.
- **#6 Bankroll Chart** lost its only trap (`bankrollAtShoeEnd` = 0 for legacy profiles is moot after the history
  reset) and gained session-scoped series from U12.
- **Stale in the doc:** "two live copies of `Serialization.hpp`" (B15 removed the `src/` copy), the instruction to
  delete `reconstructDeckAtRound()` (B16 already did), and the additive-schema guidance (broken deliberately today).
- **New constraint the seed creates:** `std::shuffle` draws through `std::uniform_int_distribution`, which is
  implementation-defined, so a shoe code is portable only within one standard library. **The fix is nearly free while
  history is empty and costs a silent-corruption migration after it is not** — a now-or-never window opened by today's
  wipe.
- **Still unbuilt and blocking five features:** `Game::persistsProgress()` — zero hits in the tree.

**Suite 345/345**, clean at `-j4`. Saves dir stays empty across suite runs (hermetic).


### 2026-11-12 (fourth round) — shuffle made seed-portable; hard-coded names removed; B23 found and closed; schema policy set

**Three pieces of work, one defect found.**

**Shuffle determinism (`IDEAS-REVIEWED.md` §B.1, acted on the same day it was raised).** `Deck` used `std::shuffle`,
whose swap sequence is specified by the standard but which draws indices through `std::uniform_int_distribution` —
whose algorithm is **implementation-defined**. A recorded `shuffleSeed` therefore pinned a shoe only within one
standard library. Both shuffle sites now call a local `shuffleShoe()`: hand-written Fisher-Yates drawing indices
straight from `std::mt19937` with rejection sampling. The sequence is now fixed by this program. **The timing
mattered:** history was wiped earlier the same day, so no existing shoe order had to be preserved. Doing this after
the player accumulates shoes means every pre-change seed silently describes a different shoe. Frozen by **#18**
`[Golden]` (first eight cards + cut-card index for seed `0xC0FFEE`), verified identical across 20 runs.

**Hard-coded names removed → B23.** All four `Player("William")` sites now pass `Constants::DEFAULT_PLAYER_NAME`,
and two stale comments that cited the old name as an example were reworded. Removing them exposed **B23**: the loading
constructor never synced the seat's display name from the profile, so the screen showed the hard-coded name on every
launch while the file held the entered one. U7's screen, gate and persistence were all correct — the defect sat in the
half of the round-trip no test asserted. **#312** confirmed red before the fix (`"Player" == "Ada"`).

**Schema policy recorded** (answering `IDEAS-REVIEWED.md` §G.1, now closed): future features will add profile fields;
write unconditionally, read with `j.value(key, default)`, never keep a reader for a field that is no longer written,
and bump-and-wipe when a field's *meaning* changes.

**Suite: 345 → 349**, clean at `-j4`. The shuffle change is the risky one for the suite — B18's seeded tests
assert distributional properties, and a new shoe order can fail them deterministically. All passed; the B18 soak rule
in the handoff ("100+ runs, never one green run") is the reason to be suspicious of that result, so treat the first
`-j8` soak after this commit as the real confirmation.

**Standing lesson, now three instances in one day:** `shuffleSeed` (never assigned), `isContinuation` (never read,
never serialized), and the seat display name (never synced). All three were fields whose name and comment described
behaviour the code did not implement, and all three survived because the tests asserted the *happy* half of a
round-trip. When adding a field, assert **both directions** and assert the value a *consumer* reads.

