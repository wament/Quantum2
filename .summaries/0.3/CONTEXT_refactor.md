# CONTEXT — Blackjack2 REFACTOR (reference for the next person)

**Purpose:** Self-contained audit + state for the *refinement* pass over the feature work described in `.summaries/0.3/CONTEXT.md`, `ACTION_PLAN.md`, and `FEATURES.md`. It establishes which files are too big, where code is repeated or duplicated, what is already centralized (so we do NOT touch it), and the hard conventions a refactor must preserve. This supersedes earlier refactor notes for this refinement; read first at the start of every window that edits code here.

**Last updated:** 2026-10-31.
**Owner of truth for feature behavior:** `.summaries/0.3/CONTEXT.md` (the live project context). This file is the *refactor-specific* overlay only — it does not re-explain game rules; it narrows "what may change" and points at concrete duplication.

---

## 0. How to use this file (read before acting)

1. Read `.summaries/0.3/CONTEXT.md` first for architecture + testing contract; then `ACTION_PLAN_refactor.md` for the per-file execution order.
2. Confirm any §6 decision affects your scope — see §3 below.
3. After finishing an increment, update §3 findings plus §4/§5 status so the next window inherits accurate state.

### Truncation workaround — do this from the beginning
The file reader truncates at ~47k characters / 2000 lines **regardless of line count**. Files with long single-line entries drop their tail once the char cap is hit (that is why numbered items in `CONTEXT.md` §5 vanished mid-sentence). Workarounds used to build this audit, and what the next window should reuse:

- **Page with bounded ranges.** Always read large files via `start_line`/`end_line` chunks so no important section lands past the cap. Prefer a handful of tight ranges over one full-file dump.
- **Prefer searches over dumps.** For structural questions (function layout, duplicated blocks) use `search_codebase` / `grep -nE` and read only matched spans — never pull whole files just to eyeball them.
- **Write short lines in these docs themselves.** Keep bullet/comment lines under ~100 chars so char-based truncation here cannot drop critical content either. This file follows that rule on purpose.

### Conventions a refactor MUST preserve (non-negotiable)
Inherited from `CONTEXT.md` §3-§4 and re-stated because they constrain *how* we shrink code:

- **Mode dispatch stays enum-driven.** Extend `AppMode`; each mode keeps its own state machine mirroring Replay/Pause wiring. A refactor must NOT replace this with ad-hoc booleans or free switches that bypass it.
- **Headless-first testing.** New/changed *logic* is tested headless via `Game(bool headless)` + `setDeck(...)` and read through the `*ForTest()` accessors before any UI touches it. Presentation-only changes follow the same Graphics-only rule (test binary links only `SFML::Graphics`).
- **Tests must be registered.** New test files go in `tests/*.cpp` AND are added to the `Blackjack2Tests` target in `CMakeLists.txt`, or they never run. This applies to new helpers extracted during a refactor too.
- **Additive JSON schema.** Persistence (`Serialization.hpp`) keeps every field optional with a default; old profiles must still load. Refactors touching the profile must keep this true and re-run the "old profile loads" check.
- **Narrow custom-rules threading (§6 #2).** `GameRules` stays a narrowly-scoped `const GameRules&` into advisor/dealer/UI. Do not widen its blast radius while refactoring.

---

## 1. §6 decisions — do they affect this refactor? (yes, all three)

`CONTEXT.md` §6 lists three standing decisions. All three constrain this refinement:

- **§6 #1 Counting approach is deferred/skipped (Feature 6 frozen).** Hi-Lo + Reko only; KO / WongHalves / Omega II were never implemented and must stay unimplemented. If a refactor touches `BasicStrategy.{hpp,cpp}` or any counting path, the Hi-Lo/Reko dispatch and Hi-Lo true-count calibration are **behavior-preserving** — do not revive Feature 6.
- **§6 #2 Custom-rules scope is narrowly-scoped.** `GameRules` stays a narrow `const GameRules&` into advisor/dealer/UI. Refactors that reorder rule access must preserve the narrow threading, not widen it.
- **§6 #3 Deliverable shape = new `include/` + `src/` files with mirrored `tests/*.cpp`, existing module naming.** For a refactor this means: keep extracted code inside the same module/naming layout where possible; if extraction creates new files, follow naming conventions and register their tests under `Blackjack2Tests`. No ad-hoc modules.

---

## 2. Audit baseline — file sizes vs the "under 800 lines" target

Measured with `wc -l`; sorted by size (largest first). **Three source files exceed 800 lines** and are the primary targets; two are close under it and worth trimming.

| File | Lines | Over 800? | Why it is big / what to look at |
|---|---|---|---|
| `src/Game.cpp` | 1727 | YES (way over) | Mode dispatch guards + `updateHUDState()` field-only body + betting/settlement/analytics all in one class. Largest single target. |
| `src/GameRenderer.cpp` | 1202 | YES | Dual mode-dispatch (backdrop if/else-if **and** draw switch); 17 `draw*` methods; per-mode overlays. |
| `src/Replay.cpp` | 901 | YES | Split from `ReplayView.cpp`; shared layout/time helpers unextracted. See §5 review queue. |
| `src/HUD.cpp` | 755 | near | `draw()` alone is ~368 lines (288-656); many tiny Rect getters + StatBox building. |
| `src/InputHandler.cpp` | 736 | near | 8 per-mode `process*Events` methods; repeated "Escape -> main menu" block. **See §4 #1.** |
| `src/ReplayView.cpp` | 583 | — | Presentation half of Replay (see §5 review queue). |
| `include/Game.hpp` | 461 | — | Mostly `*ForTest()` accessors + includes; structural, not a priority. Review only. |
| `include/Constants.hpp` | 301 | — | Dual timing namespaces + verbose per-constant prose. **See §4 #5.** |
| `include/Serialization.hpp` | 236 | — | Repeated additive-schema comment boilerplate. **See §4 #6.** |

Other notable sizes (review, not primary targets): `src/StatsDashboard.cpp` 342, `src/Player.cpp` 289, `src/BasicStrategy.cpp` 281, `src/HandEvaluator.cpp` 355, `include/Replay.hpp` 256, `include/GameRecord.hpp` 220, `tests/test_game.cpp` 513.

### Targets by the three lenses you named
- **Newest classes (git add date 2026-10-29 / 30):** `Achievement.{hpp,cpp}`, `SoundManager.{hpp,cpp}`, `CardAnimation.{hpp,cpp}`, `StatsDashboard.{hpp,cpp}`, `GameRules.{hpp,cpp}`, `Tournament.cpp`. These are the "new features with overlap" — review for shared responsibility (see §5).
- **Largest classes:** Game(1727), GameRenderer(1202), Replay(901), HUD(755), InputHandler(736) — all in §4.
- **Oldest classes (git add date 2026-07-28):** `Card.{hpp,cpp}`, `Deck.{hpp,cpp}`, `Player.cpp`, plus `HUDState.hpp` (10-30), `HandEvaluator.cpp`. Review for dead code left behind as features were layered on top.

---

## 3. Concrete duplication / refactor findings (the per-file detail)

Each item gives before -> after, a test strategy, and gotchas. Line numbers are grounded from this audit.

### #1 InputHandler — repeated "Escape -> main menu" + 8 near-identical process* methods
`InputHandler.cpp` defines one method per mode: `processTrainerEvents`(300), `processTournamentEvents`(346), `processStatsDashboardEvents`(369), `processAchievementEvents`(394), `processRulesEvents`(414), `processMenuEvents`(450), `processGameEvents`(520), `processPauseEvents`(692). Each follows the same skeleton (extract event type -> guard -> switch on `pressed->code` -> Escape handling).

**The duplication to kill:** the "Escape returns to menu" block appears literally **5 times**, identical except for one log string — at 336-337, 358-359, 384-385, 400-401, 442-444. `grep -rn "Returned to main menu" src/` returns **5**. Each pair is:

```cpp
Logger::info() << "Returned to main menu from the <mode>.";   // only this line differs
game.appMode = AppMode::MainMenu;
game.menuSelection = MenuOption::PlayGame;
```

- **Before:** 5 copies of those three statements sprinkled across handlers.
- **After:** one `InputHandler::returnToMainMenu(game, "<mode>")` helper (declared near the other `process*` methods in `InputHandler.hpp`) that each handler calls on Escape. Collapses ~15 lines to a single call; keeps the mode name via the string arg so the log line still differs per mode.
- **Test:** headless — drive each mode's handler with an Escape event and assert `appMode == AppMode::MainMenu` + `menuSelection == MenuOption::PlayGame`. Pure logic, no UI needed.
- **Status: DONE (Phase 0).** Collapsed to one `InputHandler::returnToMainMenu(game, "<mode>")` call per handler; kept the mode string arg so log lines still differ. Rules keeps its separate `rememberTableSettings()` persist call. Added `Game::getAppModeForTest()` + exposed the five Escape-handlers publicly (navigateMenu precedent) for testing. New test registered in CMakeLists.txt. All 231 tests green; commit `567ad71`.

**Note the strength to preserve:** list-style menus already share single-source helpers — `applyMenuRowSelection` is a template in `InputHandler.hpp:44`, and `getHoveredMenuItemIndex` / `activateMainMenuOption` are defined once. The mode handlers *reuse* these rather than re-implementing them. Do not "fix" that working pattern; only add the Escape helper.

### #2 Game::update() — 6 repetitive early-return mode guards
`src/Game.cpp:569-620` contains six `if (appMode == ...) { ...; return; }` blocks: MainMenu returns; ReplayPlaying/History hands off to `replay->update(dt)`; and Trainer / Tournament / StatsDashboard each do the identical `mode->update(dt); return;`; GameRules runs inline held-key logic.

- **Before:** a growing wall of `if (appMode == X)` guards, one per added mode. This is exactly the anti-pattern §6 warns against when it drifts toward ad-hoc booleans — but here the fix is a *clean* dispatch table that keeps enum-driven dispatch.
- **After:** a small table mapping `AppMode -> std::function<void(float)>` (or a single clean switch) so adding a mode is one row, not a new guard. GameRules' inline logic stays as its own updater entry.
- **Test:** headless — set each mode via the accessor path and assert the right handler ran (Trainer advances `trainer`, StatsDashboard advances `statsDashboard`). Keep it behind `*ForTest()` so no UI is required.
- **Status: DONE (Phase 1).** Collapsed the six `if(appMode==...)` guards in update() into one exhaustive AppMode dispatch switch — enum-driven, no ad-hoc booleans. GameRules inline nav kept as its own case then falls through to the Playing guard; Paused/Achievements fall through that same guard exactly as before; Playing is the sole mode advancing RoundEngine.tick. Added `setAppModeForTest(AppMode)`, out-of-line `trainerElapsedSecondsForTest()`, and startTrainerSession now builds its trainer headless (like Tournament/StatsDashboard). New test tests/test_game_mode_dispatch.cpp: Playing→RoundEngine advances past PlayerTurn; every non-Playing mode routes without advancing the round; trainer case genuinely advances. Game.cpp 1727 lines = baseline (no regression); <1700 needs the god-class split. Commit `e9268cc`.

### #3 Game::updateHUDState() — a field-only body (~170 lines, starts at line 638)
Lines 638 through ~720+ are almost entirely `hudState.field = value;` assignments with little logic — exactly the "large variable assignment, not much logic" pattern. It covers betting/side-bet, all-player bankrolls, counting, card-remaining, advisor hint, and shoe-analytics readouts.

- **Before:** one ~170-line method of flat field assignments (plus an `if/else` for side-bet and Reko true-count).
- **After:** split into per-concern helpers the caller reads naturally — e.g. `syncBetAndSideBet()`, `syncCounting()`, `shoeAnalyticsReadout()` — each ~15-30 lines of assignments with a clear header. The public method becomes obvious composition; reviewers/tests can target one concern at a time. Refactor only (no value changes).
- **Test:** existing `hudStateForTest()` consumers must still produce identical values; add/adjust assertions in `test_hud_layout.cpp` to lock the new split.
- **Status: DONE (Phase 2).** Split updateHUDState() into six per-concern helpers — syncBettingAndSideBet, syncPlayerBankrolls, syncCounting, syncSoundState, syncBookAdvisor, syncShoeAnalytics — each a single readout concern; the public method is now an index of concerns. Every hudState field keeps its value/type (concerns set disjoint fields), unchanged and locked by test_hud_layout.cpp (hudStateForTest); full suite 234/234. Game.cpp modestly larger (+28) from split overhead; the <1700/<800 targets come from follow-up #8's god-class split. Commit `fb81bcb`.

### #4 GameRenderer::draw() — dual mode-dispatch collapses to one table
The top-level draw has a backdrop if/else-if chain (lines 19-27) **and** a separate dispatch switch (line 35+) calling `drawMainMenu`, `drawGameTable`, `drawInsurance`, `drawSideBet`, `drawSoundStatus`, `drawSummaryOverlay`, `drawPendingAchievement`, etc. GameRenderer owns **17** `draw*` methods total (`grep -cE '::draw[A-Z]' src/GameRenderer.cpp` = 17).

- **Before:** two separate mode switches that must both stay in sync whenever a mode's backdrop color or draw fn changes — adding a mode means editing *both* chains (this is how backdrops drift out of sync).
- **After:** one `AppMode -> {sf::Color backdrop, draw_fn}` table consumed by draw(). One place per mode; adding a mode is one row. Keeps the "renderer consumes a presentation module" pattern (Trainer/Tournament/StatsDashboard/Achievements still call their own `->draw(window)`).
- **Test:** graphics-only smoke — assert each known mode clears to its documented backdrop color and reaches its draw fn. GameRenderer.cpp compiles into `Blackjack2Tests`, so keep it Graphics-only (no Audio/Network).
- **Status: DONE (Phase 3).** Collapsed the backdrop if/else-if chain + separate dispatch switch in draw() into one unified AppMode->{sf::Color backdrop, render} table (modeTable()), consumed by draw() as a single lookup; all 17 draw* methods intact and still call their own ->draw(window), so a mode's color and its draw fn now live on one row and can't drift apart. Playing/Paused bodies preserved byte-for-byte inside their render lambdas. Behavior identical: 236/236 pass, locked by new tests/test_game_renderer_modes.cpp (modeEntryForTest — each known mode clears to its documented color + routes to a non-empty render; unknown mode -> default entry). GameRenderer.cpp net +13 (81 added / 68 removed) from the table + accessor; <800 comes from follow-up #8's god-class split. Commit `308488f`.

### #5 Constants.hpp — dual timing namespaces + verbose per-constant prose
`Constants.hpp:301`. There is a top-level `namespace GameTiming{...}` (lines 12-99+, NPC delays, deal pacing, summary durations, refresh-rate "Gamer" logic) nested inside/near the main `Constants` block with a further `namespace Hud`. Each constant carries a multi-line rationale comment.

- **Before:** two timing namespaces doing overlapping jobs plus long explanatory comments per line (some ~10 lines each).
- **After:** one timing namespace; keep all functional values unchanged, trim prose to one short sentence, and move deep rationale into a trailing `// why` note only where non-obvious. Behavior-preserving: no value moves or drops.
- **Test:** nothing changes in behavior; rely on existing timing tests (`test_gamer_speed.cpp`, `test_deal_animation_speed.cpp`) as the regression guard.
- **Status: DONE (Phase 5).** Trimmed verbose per-constant prose to short notes across the single `GameTiming` namespace (NPC delays, deal pacing, summary durations, refresh-rate "Gamer" logic) and the Kelly/edge-over-time + Feature-8 deal-timing constants; every value unchanged, non-obvious rationale kept in brief trailing notes. Standardized Serialization.hpp's additive-schema comments to short `// additive: missing key ...` notes (the `j.value(...)` read path is untouched). Behavior-preserving: full suite 236/236 green under build + ctest; timing tests and profile-load regression as guards. Commit `33c524c`.

### #6 Serialization.hpp — repeated additive-schema comment boilerplate
`Serialization.hpp:236`. The additive-schema pattern (`j.value("field", <default>)`) is already *functionally* correct and must stay (old profiles keep loading). But there are **8** near-identical explanatory comments ("additive — older profiles lack these fields and fall back to the struct default") next to field loads.

- **Before:** 8 repetitive comment blocks repeating the same schema rule per field.
- **After:** standardize to a short `// additive: defaults keep old profiles loading` (or fold once into a file-header note) and trim each occurrence. Documentation bloat, not logic — do NOT change how fields are read.
- **Test:** re-run the profile-load regression (`test_profile_settings.cpp`) to prove the schema stayed additive.
- **Status: DONE (Phase 5).** Standardized the ~11 additive-schema comment occurrences to short `// additive: missing key falls back to default ...` notes, preserving each field/feature context; the functional `j.value(...)` read path is byte-for-byte unchanged so old profiles still load exactly as before. Guarded by test_profile_settings.cpp (profile-load regression); full suite 236/236 green. Commit `33c524c`.

### #12 Game.cpp — betting core math (true-count + Kelly/Reko sizing) owned by the god-class
`src/Game.cpp` held `activeTrueCount()` (~1173), `hiLoTrueCount()` (~1183) and `calculateOptimalBet()` (~1193): the pure money-in decisions. They read only shared state (`countingStyle`, `runningCount`, `backgroundRunningCount`, shoe depth via `deck.size()`) and are called externally through Game's public API by RoundEngine.cpp, InputHandler.cpp and HandEvaluator.cpp — so they keep identical Game signatures as thin delegators.

- **Before:** three ~8-line methods doing Reko raw-count vs Hi-Lo deck-normalized true-count derivation and Kelly/Reko optimal-bet dispatch, living inside the 1755-line god-class.
- **After (stage 1 of follow-up #8):** extract them into a new pure-logic `BettingHandler` (`include/BettingHandler.hpp`, no SFML deps) that Game owns via `std::unique_ptr<BettingHandler>` and delegates to; shared game state is passed in as parameters. `Deck` stays owned by Game — the handler takes `const Deck&` and calls `deck.size()` exactly as before, so betting semantics are byte-for-byte identical.
- **Test:** new `tests/test_betting_handler.cpp` drives BettingHandler directly and asserts each method returns exactly what BasicStrategy computes — the same calls the old Game methods made — locking behavior preservation at unit level; full suite guards integration.
- **Status: STAGE 1 of follow-up #8 (god-class split).** Pure-math core extracted into composed BettingHandler with a committed test; establishes the composition pattern (unique_ptr owner + thin Game delegators, shared state passed in) for the remaining stages — betting-state methods (`confirmBet`, `placeSideBet`, `resolveSideBets`), then Settlement/`deal()`, then SessionLifecycle — which need bigger test scaffolding. Net ~30 lines out of Game.cpp; 241/241 green under build + ctest. Commit `8d9d995`.

---

## 4. Already centralized — DO NOT touch (and verify no bypass exists)

Not everything big is a duplication target. Two things are already correct; a refactor should confirm they stay that way:

- **`Game::resetShoeAnalytics()` (`src/Game.cpp:432`) is the single shoe-boundary choke point.** It bumps `shoeGeneration`, zeroes `ShoeAnalytics shoe_`, and re-seeds counts — only bankroll carries across a shoe. Do NOT introduce a second reset path. Verify no new code zeroes shoe state without going through this function (CONTEXT §5 already confirms this is the one choke point).
- **List-style menu sharing** (`applyMenuRowSelection` template, `getHoveredMenuItemIndex`, `activateMainMenuOption`) — a single source reused by every mode handler (see #1). Preserve; extend via it.

---

## 5. Feature-overlap review queue (not forced consolidations)

These are real overlaps to look at, but each needs judgment before changing — listed explicitly so the next window does the trade-off deliberately instead of guessing:

- **HUD.drawCharts() vs StatsDashboard chart rendering.** `HUD::drawCharts()` (657) + anonymous-namespace `drawLineChart` (524) render a bankroll curve + edge-over-time from `hudState.bankrollSeries` / `shoeAnalytics`. `StatsDashboard` also computes and renders the same kind of bankroll/edge series. Decide: share one chart renderer, or clearly separate "in-game micro readout (HUD)" from "full analytics dashboard (StatsDashboard)". Review item — do not merge blindly.
- **SoundManager vs CardAnimation.** Both are "renderer-injected presentation modules" per CONTEXT §5, but they are structurally different (SoundManager = pImpl + event queue; CardAnimation = pure tween math). Only unify behind a base if a common shape genuinely emerges — do not force one.
- **Replay.cpp(894) vs ReplayView.cpp(583).** State machine + serialization vs presentation. DONE this pass: the shared history-list row geometry (topStart=120.f, lineHeight=40.f, the per-row top formula `topStart+i*lineHeight-scroll*lineHeight` + header clip, and the x hit-test band [50,1250]) appeared literally in both files with cross-referencing comments proving drift-risk (drawn rows vs hoverable rows). Extracted into one focused, header-only, Graphics-free helper `include/ReplayLayout.hpp` (`historyRowTop`, `inSelectableXBand`, `visibleHistoryRows`, + constants) that both call; Replay keeps the thin query delegators. No TIME-formatting duplication existed (both only emit integer "Step X / Y" counts and store raw timestamps), so this was purely a layout helper. Behavior byte-identical: grid test re-derives original formulas and locks each branch. Line counts: Replay 901->894 (state-machine logic dominates, not layout; the remaining <800 gap stays but trends down), ReplayView steady at 583. Full suite still 257/257 green. Clean state-vs-presentation split preserved — one source of truth for geometry, no forced merge.
- **Achievements vs StatsDashboard.** Both derive aggregates from round history / `PlayerStats` (win-rate vs win streaks). Confirm they read the same underlying data through one accessor rather than re-deriving it twice.
- **Stat-aggregation unification (broadens the item above — REVIEW, larger change).** Four subsystems independently derive tallies from round history / `RoundRecord`: (1) `PlayerStats` in `GameRecord.hpp`, persisted career stats; (2) `ShoeAnalytics`, live per-shoe counters centralized at `resetShoeAnalytics()` (already correct — do NOT re-tally here); (3) `AchievementManager` career counters (`careerHands_`, `careerBlackjacks_`, ...); (4) `StatsDashboard::ComputedStats` (win-rate / bj-bust frequency / per-round profit / bankroll series), recomputed from round history via `feed()`. The consolidation target is a **single aggregation function over round history** that all four read, instead of each re-deriving its own. This is cross-cutting and touches persisted + live data, so it is a deliberate follow-up (like §7 #8), not a shrink-now phase — sequence it after the phases with the full suite guarding behavior.
- **Already clean — do NOT re-scan these (verified this pass):** `SoundManager.cpp` maps events to files via a data-driven table (`knownEvents()` vector at line 45, looped at 67-70/113-114) — no if/else chain. `Tournament.cpp` delegates betting/settling to `Player::placeBet` / `Player::settle` + RoundEngine ("RoundEngine's same rule", "game's own settlement primitive") — it does NOT duplicate the core path. `BasicStrategy` handles Hi-Lo vs Reko via compact switch branches (lines 247-249) and Feature 6 is frozen anyway. Nothing to gain by re-examining these.

---

## 6. Status / how to use this file

**Completed refactor phases (verified green before each commit):**
- **Phase 0 — InputHandler `returnToMainMenu` helper.** Finding #1 done. Collapsed the 5 duplicated "Escape -> main menu" blocks into one helper; added a `getAppModeForTest()` accessor + new test in `tests/test_input_return_to_main_menu.cpp`. Commit `567ad71`.
- **Phase 2 — Game::updateHUDState() per-concern helpers.** Finding #3 done. Split the ~107-line flat-assignment method into six self-contained sync helpers (syncBettingAndSideBet, syncPlayerBankrolls, syncCounting, syncSoundState, syncBookAdvisor, syncShoeAnalytics); each a single readout concern with clear headers and updateHUDState reads as an index of concerns. Every hudState field keeps its value/type; locked unchanged by test_hud_layout.cpp (hudStateForTest). Full suite 234/234. Game.cpp modestly larger (+28) from split overhead; <1700/<800 come from follow-up #8's god-class split. Commit `fb81bcb`.
- **Phase 1 — Game::update() AppMode dispatch switch.** Finding #2 done. Collapsed the six `if(appMode==...)` early-return guards in update() into one exhaustive switch (enum-driven; no ad-hoc booleans). GameRules inline nav kept as its own case + falls through to the Playing guard; Paused/Achievements fall through that same guard exactly as before; Playing is the sole mode advancing RoundEngine.tick. Added `setAppModeForTest(AppMode)`, out-of-line `trainerElapsedSecondsForTest()`, and startTrainerSession now builds its trainer headless (like Tournament/StatsDashboard). New test tests/test_game_mode_dispatch.cpp (3 cases, all green). Game.cpp 1727 lines = baseline; <1700 needs the god-class split. Commit `e9268cc`.
- **Phase 3 — GameRenderer::draw() unified mode-dispatch table.** Finding #4 done. Replaced draw()'s backdrop if/else-if chain + separate dispatch switch with one AppMode->{sf::Color backdrop, render} table (modeTable()); draw() is now a single lookup and every mode's color + draw fn sit on one row so they can't drift apart. All 17 draw* methods intact; Playing/Paused bodies preserved byte-for-byte inside their render lambdas. Exposed ModeEntry + modeEntryForTest() (*ForTest pattern); new tests/test_game_renderer_modes.cpp locks each known mode's documented backdrop color and non-empty render fn (236/236). GameRenderer.cpp net +13 (81 added / 68 removed) from the table + accessor; <800 comes from follow-up #8's god-class split. Commit `308488f`.
- **Phase 6 — HUD panel-geometry builder.** Finding #7 done. Folded the three near-identical `<panel>.bounds()` getters into one `computePanels()` lookup (`struct Panels{left,right,bottom}`); named getters stay as thin delegators. Commit `0d45de5`.
- **Phase 4 — AchievementManager counter sub-structs.** Grouped scattered career/shoe counters into `CareerProgress{hands,blackjacks splitWins,perfectStreak}` + `ShoeProgress{...}`; public accessor names/types/values unchanged; bankroll swing tracking kept flat for cross-shoe persistence. Persistence (Id round-trip) unaffected. Commit `44ea266`.
- **Phase 8 — StatsDashboard::toCsv() data-driven table.** Finding #11 done. Replaced the 17 near-identical `os << name,value` lines with one `std::array<std::string,17>` of pre-formatted rows (mirroring toJson()), plus a small `csvValue<T>()` helper streaming mixed int/float fields through one path. Output byte-for-byte identical; detail block unchanged. Added exact-header lock to test_stats_dashboard.cpp. Commit `80b1022`.
- **Phase 5 — Constants.hpp prose tidy + Serialization.hpp additive-schema comment standardize.** Findings #5 + #6 done. Trimmed verbose per-constant prose to short notes across the single `GameTiming` namespace (NPC delays, deal pacing, summary durations, refresh-rate "Gamer" logic) plus Kelly/edge-over-time and Feature-8 deal-timing constants; every value unchanged, non-obvious rationale kept in brief trailing notes. Standardized Serialization.hpp's ~11 additive-schema comments to short `// additive: missing key ...` notes (the `j.value(...)` read path is untouched). Behavior-preserving; full suite 236/236 green under build + ctest, with timing tests and the profile-load regression as guards. Commit `33c524c`.
- **Phase 7 — HandEvaluator::evaluateHands() case+logger collapse.** Finding #10 done. Collapsed the three near-identical Surrender/Lose/Win cases by routing the per-result counter and human-seat streak flag by outcome (Surrender+Lose -> losses/roundLost; Win -> wins/roundWon) via result-based if/else, then one byte-for-byte-identical log line through a small result->verb map (Push has none). Counters, roundWon/roundLost flags and settlement semantics unchanged. Existing test_blackjack_settlement.cpp green; full suite 236/236 green under build + ctest. Commit `6787e13`.
- **Follow-up #8 — Stage 1: Betting core extracted into composed BettingHandler.** Finding #12 (stage 1 of follow-up #8) done. Moved Game::activeTrueCount()/hiLoTrueCount()/calculateOptimalBet() out of the god-class into a new pure-logic `BettingHandler` (`include/BettingHandler.hpp`, no SFML deps) that Game owns via unique_ptr and delegates to; shared state (countingStyle, running/background counts, deck depth via const Deck&) passed in as parameters — Deck stays owned by Game so betting semantics are byte-for-byte identical. Constructed in both ctors (like AchievementManager). New tests/test_betting_handler.cpp asserts each method returns exactly what BasicStrategy computes (unit-level behavior lock); full suite 241/241 green under build + ctest. Establishes the composition pattern for the remaining #8 stages (betting-state methods → Settlement/deal() path → SessionLifecycle), which need bigger test scaffolding. Commit `8d9d995`.

- **Follow-up #8 — Stage 2: Settlement/deal() extracted into composed RoundDealer.** Finding #8 + #12 (stage 2). Moved Game::deal() + Game::resolveSideBets() out of the god-class into a new `RoundDealer` helper composed like HandEvaluator (value member holding Game&, declared friend; methods moved verbatim). Game keeps thin delegating deal()/resolveSideBets() and all mutable state stays in Game, so deal + side-bet settlement are byte-for-byte identical. RoundDealer::deal() drives card dealing + evaluateBlackjack through Game helpers; resolveSideBets() settles each seat's side bet BEFORE main-hand settlement (win/lose branches preserved). New tests/test_round_dealer.cpp drives the delegation path end-to-end through a real headless startTestRound() deal — winning Perfect Pairs (30:1), losing, PlayerTurn advance, and insurance-offer branch. CMake registers RoundDealer.cpp in both source lists; full suite 244/245 green under build + ctest (only pre-existing SoundManager CWD artifact #132 fails). Commit `360ed81`.

- **Follow-up #8 — Stage 3: session lifecycle + table-settings persistence extracted into composed SessionLifecycle.** Moved startNewGame()/startTrainerSession()/startTournamentSession()/startStatsSession() and applySavedTableSettings()/rememberTableSettings() out of the god class into a new `SessionLifecycle` helper composed like RoundDealer/HandEvaluator (value member holding Game&, declared friend; methods moved verbatim with members qualified as game.*). Game keeps one thin delegator per method, all mutable state stays in Game, so each entry point + persist/restore round-trip are byte-for-byte identical. Deck/shoe reset still routes through untouched resetShoeAnalytics(); the settle-aftermath stats/analytics feed stays in Game; recordHandAchievements() moved out in Stage 4 (see below). New tests/test_session_lifecycle.cpp locks startNewGame reset+playable seat, trainer/tournament/stats route-and-advance, and table-settings persistence round-trip (remember→apply restores Hyper from clobbered Slow); registered in both source lists + Blackjack2Tests. Full suite 250/250 green under build + ctest (was 244/245; SoundManager CWD artifact #132 unchanged). Commit `13dbfc0`.

At the start of each window: read this file, then `ACTION_PLAN_refactor.md`, and confirm any §6 decision affects scope (see §1). After an increment, update §3/§7 findings with what changed plus §4-§5 status so the next window inherits accurate state. Verification gate for every phase is in `ACTION_PLAN_refactor.md` ("Verification").

---

## 7. Second-pass missed opportunities (additional findings)

A follow-up scan for items not yet in §3. Each is listed with before -> after and a test note; the actionable ones become phases below.

### #7 HUD panel-geometry getters — three near-identical Rect builders
`HUD.cpp` exposes `chartPaneBounds()(692)`, `leftPanelBounds()(721)`, `rightPanelBounds()(726)`, `bottomPanelBounds()(731)` and `layoutLeftPanels()(746)`. Each recomputes a tiny `sf::Rect<float>` from the same shared panel constants; together they are scattered geometry helpers that inflate HUD.cpp (755 lines).

- **Before:** four public getters + a layout method, each a ~5-line Rect builder spread across the bottom of the file.
- **After:** one `computePanels()` helper returning a small `struct Panels { left, right, bottom; }` with chart-pane bounds derived from it. The three getters become thin one-liners delegating to it (public names kept for API stability).
- **Test:** graphics-only — assert the returned panel Rects still match pre-refactor values (`test_hud_layout.cpp` already reads these bounds).
- **Status: DONE (Phase 6).** Real code had thin `m_XPanel.bounds()` delegations (not constant-based builders); folded all three into one `computePanels()` returning `struct Panels{left,right,bottom}`, with left/right/bottomPanelBounds as thin one-liners. chartPaneBounds kept separate (distinct derived-chart-rect concern). Public getter names unchanged; test_hud_layout.cpp still green. Commit `0d45de5`.

### #8 Game.cpp is a splittable god-class (FOLLOW-UP, beyond this refactor)
Beyond `update()` and `updateHUDState()`, `Game.cpp:1727` owns clearly separable concerns that could each become its own class later. Confirmed regions from the function inventory:

- Session lifecycle: `startNewGame()(320)`, `startTrainerSession()(210)`, `startTournamentSession()(218)`, `startStatsSession()(228)`.
- Table-settings persistence: `applySavedTableSettings()(366)`, `rememberTableSettings()(411)`.
- Settlement/aftermath: `recordHandAchievements()(477)` and the settle path that feeds stats/analytics.

- **Before:** one class owns dispatch + betting + settlement + analytics + session lifecycle + persistence hooks. It grows by ~20 lines per feature (hence 1727 baseline).
- **Progress (stages done):** Stage 1 extracted `Betting` into composed BettingHandler (#12); Stage 2 moved Settlement/deal() into composed RoundDealer; Stage 3 moved the session entry points + table-settings persistence into composed SessionLifecycle; Stage 4 moved settlement/aftermath recordHandAchievements() into composed AchievementFeeder. All keep behavior byte-for-byte via thin Game delegators.
- **After (future):** the documented startNewGame/start*/persistence region is fully extracted; remaining Game.cpp size comes from dispatch + betting-state methods + analytics + settlement-aftermath (recordHandAchievements), candidates for a later follow-up only if it ever regresses.
- **Why SessionLifecycle was phased:** it's a large structural change requiring new test scaffolding and careful ownership moves. Done after stages 1-2, guarded by the full suite staying green — see Commit `13dbfc0`.
- **Status: DONE (Stage 4, Follow-up #8).** Extracted `recordHandAchievements()` and its shared just-unlocked popup lookup into new composed AchievementFeeder (`include/AchievementFeeder.hpp`, `src/AchievementFeeder.cpp`), value-member+friend pattern like RoundDealer/SessionLifecycle.
- The file-local setPendingUnlocked() (used by recordHandAchievements AND resetShoeAnalytics()) was lifted once into a shared public surfaceNewlyUnlocked() member; resetShoeAnalytics reroutes its onNewShoe() through the feeder. Game keeps thin delegators; all mutable state stays in Game, so behavior is byte-for-byte identical.
- New headless tests/test_achievement_feeder.cpp lock human-seat aggregation + split/natural detection, a live deal->settle cycle, and the shoe-boundary popup path. Full suite 253/253 green under build + ctest. Commit `7a232a6`.

### #9 Repeated test scaffolding — NOW CONSOLIDATED (was "REVIEW, do not force")
`startTestRound()` was called ~60× across `tests/*.cpp`; many tests also repeated an identical
`Game(bool)` + `setDeck(...)` + `startTestRound()` preamble **and** a per-file local `tickOnce`/`settleRound`
frame driver. This was the last class of duplicated scaffolding in the suite.
- **Decision (done):** extracted a header-only, inline shared fixture `tests/test_support.hpp`; CMakeLists.txt adds
  `tests/` to the test include path only. Guiding rule stayed "readability > DRY": only genuinely-generic helpers went
  in; bespoke per-file setups were deliberately left alone.
- **Helpers now centralized:** `make_card(v,suit)` (replaces each file's local makeCard/card()); `seed_deck(game,cards)`;
  `start_round(game,cards,seats)` (= seed + startTestRound); frame-drivers `tickFrame(game,deltaTime)`,
  `advanceToDealerTurn(game)` (drive to DealerTurn), and `settleRound(game,settleDelta=100.0f)` (drive to Cleanup; sets
  the game member deltaTime so reveal pacing elapses — exactly what settlement tests did locally).
- **Migrated:** deck+frame setup now routes through `test_support::*` in ~24 files (was 19); net −~210 lines of dup
  scaffolding across tests. Every migration was a rename/wrap so deal/settle semantics are byte-for-byte identical
  (Deck stays owned by Game). Build EXIT=0; all cases green except the pre-existing SoundManager CWD artifact.
- **Left bespoke on purpose:** `card_animation` needs configurable-seat setup (`startTestRound(3)`); `tournament_mode`
  has a bespoke `rotatedDeck()`; pure unit-test files (Card/Hand/Player/Deck/BettingHandler/assets/menus/renderer) have
  no Game+deck preamble. None fit the generic fixture — merging them would mix strategies and hurt navigation.
- **Status: DONE.** This unblocks ACTION_PLAN follow-up #8's Settlement/deal() + SessionLifecycle stages, which now build
  on this shared scaffolding instead of re-deriving it each time.

### #9b Test-file THEMING — third pass (condensing per-feature files into themed TUs)
The suite had grown to ~42 tiny feature-specific `test_*.cpp` files, many categorically identical (e.g. four separate
settlement files: `blackjack_settlement` / `resplit_tens` / `split_aces` / `dealer_blackjack`; three side-bet files; three
analytics files; three speed/deal-animation files). Each reused the shared `test_support.hpp` fixture but lived as its own
translation unit. This pass folded each categorical group into ONE themed TU (readability > DRY, same rule as #9).
- **Merge method:** for every member file, everything before its first `TEST_CASE(` was parsed out — includes and all forms
  of `using` (`namespace X;`, `using a::b;`, `alias = ...`) go to the unified header at global scope; the rest (anonymous-ns
  blocks + any file-scope helpers) were brace-matched into individual top-level functions, de-duplicated by comment-stripped
  identity, and hoisted once into a single shared anonymous namespace per merged file. TEST_CASE bodies are concatenated
  verbatim (names already unique — all 256 were distinct). This declares every helper before any test uses it and collapses
  each member's separate anon namespaces, so names must be globally unique within the group; identical copies collapse to one.
- **Safety verified before build:** no file-scope symbol collisions except two real dupes the script de-duped — analytics'
  `tickUntilSettled` (identical in `analytics_charts` + `shoe_analytics`) and achievements' `twoCards` (achievements keeps it
  file-scope global; feeder's is anon-ns to coexist — both now one definition). A stray `using Catch::Approx;` single-name
  using-declaration initially got hoisted *inside* the anon ns (breaking every `Approx` call) and was fixed by keeping such
  declarations in the global header.
- **Result:** 20 member files → 6 themed TUs (`test_settlement`, `test_side_bets`, `test_analytics`, `test_deal_timing`,
  `test_achievements`, `test_rendering`); CMakeLists.txt source list updated (492 insertions / −2831 deletions tracked).
  Build EXIT=0; **ctest 257/257 green** (no sanitizer preset, clean pass/fail). Net test surface unchanged — same assertions,
  fewer files.
- **Left standalone on purpose:** pure unit tests (Card/Hand/Player/Deck/BettingHandler/assets/reko) and bespoke fixtures
  (`tournament_mode` rotated deck, `card_animation` configurable-seat setup, HUD/layout/profile/sound/tournament/session/
  replay/trainer) don't share a category cleanly; merging them would mix strategies. They stay as their own files.

   (`tournament_mode` rotated deck, `card_animation` configurable-seat setup, HUD/layout/profile/sound/tournament/session/
   replay/trainer) don't share a category cleanly; merging them would mix strategies. They stay as their own files.

### #9c Panel folded into HUD — visualization consolidation (this pass)
The HUD owned three separate `Panel` objects (`m_left/m_right/bottomPanel`), each just a dim background rectangle plus a
`std::vector<StatBox*>`. No class used `Panel` except `HUD` itself, so it was a one-level-of-indirection with no reuse — the
exact "one-off helper that could be folded into another class in a similar category" pattern. Folded it out: added an
internal `HudPanel` struct (same API) to `HUD.hpp`, changed the three member types, and **deleted `include/Panel.hpp` +
`src/Panel.cpp`** (−2 files). CMakeLists source lists updated in both targets; HUD.cpp is byte-for-byte unchanged because
every per-panel call site maps 1:1. All 257 tests green.

### #10 HandEvaluator::evaluateHands() — repeated case+logger block (third pass, exhaustive scan)
### #10 HandEvaluator::evaluateHands() — repeated case+logger block (third pass, exhaustive scan)
`HandEvaluator.cpp:355` evaluateHands() `switch(result)` lines 68-92 has three near-identical cases (Surrender/Lose/Win), each doing a stats counter increment, an `if(pIdx == 0){roundLost|roundWon = true;}` flag assignment, and a nearly identical logger line ("surrendered"/"lost"/"won hand …"). Push is the odd one out.
- **Before:** ~3 duplicated case+logger blocks in the settlement switch.
- **After:** build ONE outcome log string from `result` after the switch (a small result->verb mapping), keep the tiny per-result counter/flag deltas, then emit a single `if(pIdx==0){flag}` + one Logger call. Collapse only the repeated logging structure — never change which counter/flag each result sets.
- **Test:** existing `test_blackjack_settlement.cpp` must stay green (it locks outcomes/payouts); log output byte-for-byte identical. Behavior-preserving; settlement semantics unchanged.
- **Status: DONE (Phase 7).** Collapsed the three near-identical Surrender/Lose/Win cases by routing the per-result counter and human-seat streak flag by outcome (Surrender+Lose -> losses/roundLost; Win -> wins/roundWon) via result-based if/else, then emitting one byte-for-byte-identical log line through a small result->verb map (Push still has none; Surrender/Lose share the "(bet: B)" format and only a Win appends payout). Counters, roundWon/roundLost flags and settlement semantics unchanged. Existing test_blackjack_settlement.cpp green; full suite 236/236 green under build + ctest. Commit `6787e13`.

### #11 StatsDashboard::toCsv() — one os<< line per metric (third pass, exhaustive scan)
`StatsDashboard.cpp:342` toCsv() repeats one `os << "metric," << computed_.field << "\n"` line per field (~14 times). Values are mixed types, so a pure key/value table is awkward but small lambdas work.
- **Before:** ~14 hand-written metric lines.
- **After:** a small lambda list (`{name, [](){ return value; }}`) or typed helpers driving one loop. Export format only — no game behavior impact.
- **Test:** lock the exact CSV (headers + row order + `round_index,net_profit,cumulative_bankroll` detail block) in a new/existing test so drift is caught. Byte-for-byte identical output; no logic change.
- **Status: DONE (Phase 8).** Replaced the ~14 hand-written metric lines with one data-driven `std::array<std::string,17>` of pre-formatted rows plus a small `csvValue<T>()` helper streaming mixed int/float fields through one path (mirrors toJson()). Output byte-for-byte identical; detail block unchanged. Added exact-header lock to test_stats_dashboard.cpp. Commit `80b1022`.
