# ACTION PLAN — Blackjack2 REFACTOR execution

Grounded on `.summaries/0.3/CONTEXT_refactor.md`. Phases are sequenced by risk + ROI; every phase is **refactor-only** (no behavior change) and re-verified headless-first, then with the full suite. Statuses mirror `ACTION_PLAN.md` phrasing so it slots into the same workflow.

---

## Shared setup before every phase

- Confirm §6 decisions affect scope (`CONTEXT_refactor.md` §1).
- Read each target file in bounded ranges (truncation workaround) — never full-file dumps.
- Keep a clean working tree and commit **per phase** so any change can be reverted or inspected in isolation.
- Treat every phase as behavior-preserving: timing, betting, settlement, counting, and side-bet results must stay identical.

## Guardrails for every phase (inherited)

- Register new tests in `tests/*.cpp` AND under the `Blackjack2Tests` target in `CMakeLists.txt`, or they won't run.
- Keep the JSON schema additive; re-run the "old profile loads" check if the profile/serialization path is touched.
- Keep pure logic SFML-Graphics-only; gate Audio/Network behind headless-safe paths.

## Verification — run after EVERY phase (the gate)

- Build: `cmake --preset "GCC 13.3.0 x86_64-linux-gnu"` -> binary dir `out/build/GCC 13.3.0 x86_64-linux-gnu/`.
- Test: `ctest --test-dir out/build/GCC 13.3.0 x86_64-linux-gnu --output-on-failure`.
- Read note: under the ASan+LSan preset ~75 SFML-linked test processes report "Failed" at exit from environmental libdbus teardown (not a real error). Run ctest with `ASAN_OPTIONS=detect_leaks=0` for a clean pass/fail read (`CONTEXT.md` §5).

---

## Phase 0 — InputHandler Escape helper (P0, ~1h, highest ROI / lowest risk)

- **Modify:** `src/InputHandler.cpp`, `include/InputHandler.hpp`.
- **Add:** `void returnToMainMenu(Game& game, const char* fromMode);` declared near the other `process*` methods in `InputHandler.hpp`. Body:

```cpp
void InputHandler::returnToMainMenu(Game& game, const char* fromMode)
{
    Logger::info() << "Returned to main menu from the " << fromMode << ".";
    game.appMode = AppMode::MainMenu;
    game.menuSelection = MenuOption::PlayGame;
}
```

- **Replace** the 5 identical blocks (InputHandler.cpp lines 336-337, 358-359, 384-385, 400-401, 442-444) with a single `returnToMainMenu(game, "<mode>");` call each. Keep the mode string per call so log lines still differ.
- **Target:** InputHandler.cpp ~736 -> under 700; collapse ~15 duplicated lines to one helper.
- **Test:** new `tests/test_input_return_to_main_menu.cpp`, registered in CMakeLists.txt. Headless `Game(bool)`; drive an Escape event through each mode handler and assert `appMode == AppMode::MainMenu` + `menuSelection == MenuOption::PlayGame`. No UI needed.
- **Gotcha:** do NOT touch the shared list-menu helpers (`applyMenuRowSelection`, `getHoveredMenuItemIndex`, `activateMainMenuOption`) — they already work and are single-source. Only add the Escape helper.

## Phase 1 — Game::update() dispatch table (P0, ~1-2h)

- **Modify:** `src/Game.cpp` update() region (569-620).
- **Approach:** replace the six `if (appMode == ...) { ...; return; }` guards with a small table mapping `AppMode -> std::function<void(float)>` (or one clean switch) so each mode is a row rather than a new guard. GameRules' inline held-key logic becomes its own updater entry.
- **Keep** enum-driven dispatch — no ad-hoc booleans, no bypass of `appMode`.
- **Target:** remove ~30 repetitive guard lines; push Game.cpp below 1700.
- **Test:** headless — set each mode via the accessor path and assert the correct handler advanced (Trainer -> `trainer->update`, StatsDashboard -> `statsDashboard->update`). Stay behind `*ForTest()` so no UI is required. Re-run timing tests to confirm the Playing/RoundEngine path is unchanged.

## Phase 2 — updateHUDState() split (P1, ~1-2h)

- **Modify:** `src/Game.cpp` updateHUDState() region (638+).
- **Approach:** extract per-concern helpers the caller reads naturally — e.g. `syncBetAndSideBet()`, `syncCounting()`, `shoeAnalyticsReadout()` (plus any other self-contained readout block). The public method becomes a clear composition.
- **Refactor ONLY.** Every `hudState` field must keep the same value and type; no new fields, no dropped logic.
- **Target:** each helper <= 40 lines; updateHUDState reads as an index of concerns.
- **Test:** existing `test_hud_layout.cpp` consumers (`hudStateForTest()`) must produce identical values — add/adjust assertions to lock the split. No new file needed unless a concern deserves its own suite.

## Phase 3 — GameRenderer.draw() unified table (P1, ~2-3h)

- **Modify:** `src/GameRenderer.cpp` top-level draw().
- **Approach:** build one `AppMode -> {sf::Color backdrop, draw_fn}` table; replace the backdrop if/else-if chain (lines 19-27) and the dispatch switch (line 35+) with a single lookup. Keep per-mode modules calling their own `->draw(window)`.
- **Target:** collapse two mode switches into one; keep all 17 `draw*` methods intact.
- **Test:** graphics-only smoke — assert each known mode clears to its documented backdrop color and reaches its draw fn. GameRenderer.cpp compiles into `Blackjack2Tests`, so stay Graphics-only (no Audio/Network).

## Phase 4 — Achievement.hpp counter sub-structs (P2, ~1h)

- **Modify:** `include/Achievement.hpp` private members (lines 102-121).
- **Approach:** group the flat counters into a `CareerProgress { hands, blackjacks, splitWins, perfectStreak }` struct and a `ShoeProgress { ... }` struct; update references in `recordHand()` / `onNewShoe()` / `resetPerShoe()`. Keep all public accessor names unchanged (`careerHands()`, etc.).
- **Refactor ONLY.** No new conditions; the persistence encoding (`toInt`/`fromInt`) stays identical.
- **Test:** existing `test_achievements.cpp` must stay green — it already drives `recordHand`/`onNewShoe` unlock logic. Verify `persistUnlockedTo`/`loadUnlockedFrom` still round-trips (same int encoding).

## Phase 5 — Constants.hpp tidy + Serialization comment trim (P3, ~1h)

- **Modify:** `include/Constants.hpp` (merge the two timing namespaces into one; trim verbose per-constant prose to short notes); `include/Serialization.hpp` (standardize the 8 additive-schema comments to one short inline note or a single file-header line).
- **Behavior-preserving.** No value moves, no field drops, no changes to how fields are read.
- **Test:** timing tests (`test_gamer_speed.cpp`, `test_deal_animation_speed.cpp`) plus the profile regression (`test_profile_settings.cpp`).

## Phase 6 — HUD panel-geometry builder (P2, ~1h)

- **Modify:** `src/HUD.cpp`.
- **Approach:** add one `struct Panels { sf::Rect<float> left, right, bottom; } computePanels() const;` that builds all three panels (and derives the chart-pane bounds from them). Replace the bodies of `chartPaneBounds()`(692), `leftPanelBounds()`(721), `rightPanelBounds()`(726), `bottomPanelBounds()`(731) to delegate to it. Keep the public getter names for API stability.
- **Refactor ONLY.** Returned Rect values must be byte-for-byte identical (lock them in `test_hud_layout.cpp`).
- **Target:** fold ~4 scattered small getters into one builder; trend HUD.cpp below 755.
- **Test:** graphics-only — assert panel bounds unchanged (`test_hud_layout.cpp`).

## Phase 7 — HandEvaluator::evaluateHands() case+logger collapse (P2, ~1-2h, moderate)

- **Modify:** `src/HandEvaluator.cpp` evaluateHands() switch region (lines 68-92).
- **Approach:** the `switch(result)` has three near-identical cases (Surrender/Lose/Win) each doing a stats counter increment, an `if(pIdx == 0){roundLost|roundWon = true;}` flag, and a nearly identical logger line. Consolidate by building ONE outcome log string from `result` after the switch (a small result->verb mapping), keeping the per-result counter/flag deltas minimal or also data-driven, then emit one `if(pIdx == 0){flag}` + one Logger call. This collapses ~3 repeated case+logger blocks into a single post-switch logger.
- **Refactor ONLY.** Identical stats counters, identical roundWon/roundLost flags, and byte-for-byte identical log output — settlement behavior and logs unchanged.
- **Target:** remove the ~9-12 duplicated lines across the three cases.
- **Test:** existing `test_blackjack_settlement.cpp` MUST stay green (it locks outcomes/payouts). Keep behind `*ForTest()` where practical; do not over-collapse to a point that changes counter/flag semantics (Surrender/Lose -> roundLost, Win -> roundWon).
- **Gotcha:** each case sets a different counter and flag, so route those via the result value rather than deleting them — collapse only the repeated logging structure.

## Phase 8 — StatsDashboard::toCsv() tidy (P3, ~30m, trivial)

- **Modify:** `src/StatsDashboard.cpp` toCsv().
- **Approach:** toCsv repeats one `os << "metric," << computed_.metric << "\n"` line per field (~14 times). Because values are mixed types, replace the hand-written lines with a small lambda list (`{name, [](){ return value; }}`) or typed helpers. Export format only.
- **Refactor ONLY.** CSV text output must be byte-for-byte identical — same headers, same row order, same `round_index,net_profit,cumulative_bankroll` detail block.
- **Target:** remove ~14 hand-written metric lines into one data structure + loop.
- **Test:** lock the exact CSV (assert header rows + a known metric value) in a new or existing test so any drift is caught. No logic change.

---

## Feature-overlap review queue (run after phases; judgment calls)

Do these as separate small changes OR leave them as explicit TODOs — each needs a deliberate trade-off, not an automatic merge:

- **HUD.drawCharts() vs StatsDashboard charts** -> share one chart renderer, or clearly separate "in-game micro readout (HUD)" from "full analytics dashboard (StatsDashboard)".
- **Replay.cpp(901) vs ReplayView.cpp(583)** -> extract shared layout/time helpers if both duplicate them; otherwise keep the clean state-vs-presentation split.
- **Achievements vs StatsDashboard** -> confirm a single data accessor for round-history aggregates instead of re-deriving twice.
- **Stat-aggregation unification (broadens the item above — REVIEW, larger change).** Four subsystems derive tallies from round history independently: `PlayerStats` (persisted career), `ShoeAnalytics` (live per-shoe, already centralized at `resetShoeAnalytics()` — do NOT re-tally), `AchievementManager` career counters, and `StatsDashboard::ComputedStats` (recomputed via `feed()`). Target: one aggregation function over round history that all four read. Cross-cutting + touches persisted/live data -> a deliberate follow-up like the Game.cpp god-class split, not a shrink-now phase. Sequence after the phases; guard behavior with the full suite.
- **Already clean — do NOT re-scan (verified):** `SoundManager` uses a data-driven event->file table (`knownEvents()`, no if/else); `Tournament` delegates betting/settling to `Player::placeBet` / `settle` + RoundEngine; `BasicStrategy` Hi-Lo vs Reko are compact switch branches and Feature 6 is frozen; `Player::calculateNextBet` is a strategy switch where each case is substantive per-algorithm logic (Martingale double+cap, Fibonacci sequence walk) — not just variable assignment. Nothing to gain re-examining these.

## Do NOT expand scope into (explicit guardrails)

- Reviving Feature 6 counting systems (§6 #1: keep Hi-Lo + Reko only).
- Widening GameRules threading (§6 #2: keep it narrowly-scoped `const GameRules&`).
- Changing betting, settlement, counting, or side-bet behavior.
- Adding new `AppMode` values or changing the enum (mode-dispatch pattern stays as-is).

## Follow-up / beyond scope (do AFTER the behavior-preserving phases)

These are larger structural changes, not shrink-now steps. Sequence them deliberately once every phase is green:

- **Game.cpp god-class split (biggest remaining win).** Even after Phases 0-8, Game.cpp stays large because it owns dispatch + betting + settlement + analytics + session lifecycle + persistence hooks. Stage 1 extracted `Betting` into composed BettingHandler; Stage 2 moved Settlement/deal() into composed RoundDealer (deal + resolveSideBets, value-member+friend pattern like HandEvaluator — see tests/test_round_dealer.cpp); Stage 3 moved the session entry points + table-settings persistence into composed SessionLifecycle (startNewGame/startTrainerSession/startTournamentSession/startStatsSession/applySavedTableSettings/rememberTableSettings), same composition pattern; Game keeps thin delegators. This alone drops Game.cpp from 1727 to ~1493.
- **Test-scaffolding review.** `startTestRound()` is called ~60× across `tests/*.cpp`; many tests repeat an identical
  `Game(bool)` + `setDeck(...)` preamble (plus a per-file local tickOnce/settleRound frame driver). STATUS: consolidated
  into header-only shared fixture `tests/test_support.hpp` (`make_card`, `seed_deck`, `start_round`, plus frame-drivers
  `tickFrame`/`advanceToDealerTurn`/`settleRound`). Readability > DRY — generic helpers only; bespoke setups
  (card_animation/tournament/unit files) left in place. Settlement/deal() is now Stage 2 (RoundDealer); the remaining
  future stage SessionLifecycle builds on this shared scaffolding instead of re-deriving it each time.

## Definition of done for the whole refactor

- Every target file is at/under 800 lines where feasible; no file regressed in size (Game.cpp, GameRenderer.cpp, Replay.cpp are the three to get under 800). HUD.cpp and InputHandler.cpp trend down too.
- Each phase green under build + `ctest` (applying the `ASAN_OPTIONS=detect_leaks=0` read note).
- No behavior change: existing test expectations unchanged except any new lock assertions added to freeze refactored output.
- `CONTEXT_refactor.md` §3/§7 findings, §4/§5 status updated; git commit per phase with a message naming the file and change.
