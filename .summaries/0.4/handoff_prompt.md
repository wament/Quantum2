You are continuing work on Blackjack2 (C++17 game / C++20 tests, SFML 3, CMake, Catch2 v3.6.0), a desktop blackjack teaching simulator. Phase 0.4 is a QA-hardening pass: defect removal, not feature work.

## State right now (read before Step 0)

- Test suite is **315/315 green** (~10s, `-j8`), code HEAD `f8f15cc`; configure + build clean (warnings only — the pre-existing implicit-this deprecation in GameRenderer.cpp's modeTable() lambdas). Working tree is clean.
- §11 defect count: the **B-series is essentially done**. Only B9 remains, and it is ⚠️ **Partly** — Logger flush + summary-overlay bounds are fixed (#292–#294, #296/#297), but one unreproducible R-time crash still has no test.
- UI/UX backlog (U1–U12): U4/U5/U7/U8/U9/U10 Resolved with tests. **What's left open are the vision-critical items** — they need an SDL window + Xvfb/screenshot to verify, so no pure headless test can lock them; that is what Phase 0.4 has left to finish (see below).

## The vision-critical backlog — this is what Phase 0.4 still owes (choose Step 0 from here)

Everything left open lives in the presentation/lifecycle seam and cannot be proven without looking at it on screen. Work these next; each needs real verification:

  U1  Two rows look selected at once. Hover + keyboard both render gold (`GameRenderer.cpp:1034-1046`), so Enter is genuinely ambiguous about what a click does. Fix: a distinct hover treatment (background pill or `›` marker) and reserve gold for the selected row.
  U2  Pause menu has no panel — bare text over the felt, "Quit to Menu" collides with seat-name labels (`GameRenderer.cpp:1274`). Fix: a centred rounded rect with padding behind the items (reuse the side-panel style); suppress the play-phase hint strip while paused.
  U3  Two screens do not reflow. Achievements (`:108-141`) + Game Rules (`:1236`) use hard-coded pixel offsets and at 900×560 the footer overlaps the list rows — while the table itself reflows correctly. Fix: derive start-Y/row-spacing from `window.getSize()`; add a `*ForTest()` layout accessor, assert no rect overlap at both 1600×900 and 900×560.
  U6  Card overlap hides the lower cards (3+), and hand totals are tiny white text straight on the felt arc. Fix: more per-card offset; draw totals in a small filled pill with dark backing (`drawTotal`, `GameRenderer.cpp:468`).
  U11 Menu hover/click target drifts from the drawn row on window resize (main menu + pause screen). Drawing is absolute-pixel (`drawMenuItem` centres at `(window.width/2, y)` `:1071`; pause rows `pauseRowY = height*yFrac` `:1134`) but the three hover hit-tests use DIFFERENT coordinate bases — `getHoveredMenuItemIndex` uses mapPixelToCoords vs an absolute-pixel `item.y`, `getHoveredPauseMenuItemIndex` uses raw getPosition, `getHoveredGameRulesRowIndex` uses mapPixelToCoords against an absolute layout. Fix: unify the cursor's resolution space with each row's basis; verify reflow while resizing on screen (needs SDL view + real mouse — only manifests across window sizes).
  U12 Side-panel graphs (`StatsDashboard` `GameRenderer.cpp:74`; HUDState series `HUDState.hpp:55-56`): no per-graph visibility toggle; only a per-shoe view with NO session view; Bets Placed X-axis is the full [0, MAXIMUM_BET] instead of the intended 10–100 scaling range. Fix: (1) per-series visibility flags + draw gating; (2) a NEW session-scoped dataset distinct from longhistory with a UI mode switch; (3) reconfigure the axis to 10–100 growing up to MAXIMUM_BET (`Constants.hpp:194-200`). Correctness of the session-vs-shoe data, toggle visuals and axis labels/ticks must be verified by eye across real gameplay and window sizes — not headless-testable in isolation.

And one inherent non-determinism case (not really fixable by design):
  B9  Logger flush + overlay bounds are fixed; the unreproducible R-time crash is still open and carries no test — it stays Partly, not Resolved, on that basis.

Why these resist tests: every one needs an SDL window context and a screenshot/Xvfb image diff to assert anything visual; no pure headless unit test can lock pixel-level rendering (CONTEXT §3 testing blind spot / §11 verdicts). Do NOT paper them over with headless tests — that is exactly the mistake 0.4 kept making until it started extracting strings/layout into pure helpers.

## Step 0 — this iteration
Branch:            3.1   <- edit if different
Focus this round:  <e.g. "U-series vision-critical (U3 reflow)" / "the B9 crash">
Already done:      <e.g. "nothing" / "B15 landed">
Anything I should know: <optional>

## Step 1 — read these, in this order, before touching code
All three live in `.summaries/0.4/` on branch `3.1`. (`master` is a frozen checkpoint at 380da26 and does NOT contain them — if they're missing, you are on the wrong branch.)

1. `.summaries/0.4/CONTEXT.md` **§13 (Document audit log)** — what has actually been verified, and how. Read first; it tells you which claims in these docs are load-bearing and which were inherited from a playtest.
2. `.summaries/0.4/CONTEXT.md` — the rest, fully. Background, evidence, guardrails.
3. `.summaries/0.4/NEXT_STEPS.md` — the actionable ledger.
4. `.summaries/0.4/IDEAS.md` — DO NOT read or implement unless I explicitly ask this round. Not a plan.

Key sections: CONTEXT §3 testing blind spot · §4 architecture map · §5 persistence & data model · §6 evidence log (§6.9 "cleared — not bugs", §6.10 B13 proof) · §7 conventions · §8 environment (incl. the `-M` trap) · §10 open decisions · §11 status tracker. NEXT_STEPS: Verification gate · Triage table · Phases 0–5 · Testing strategy · Definition of done · Explicitly out of scope.

## Step 2 — establish real state, do not trust the docs blindly
The docs were written at a point in time and may be stale. Before coding:

  git status && git --no-pager log --oneline -3
  cmake --preset "GCC 13.3.0 x86_64-linux-gnu"
  cmake --build 'out/build/GCC 13.3.0 x86_64-linux-gnu' --parallel
  ctest --test-dir 'out/build/GCC 13.3.0 x86_64-linux-gnu' -j8 --output-on-failure

Then report back, BEFORE writing any code:
  - current test count and pass/fail (baseline now 315/315; never finish with a lower count than you started)
  - which §11 IDs are actually still open — verify against the source, don't trust the table
  - any doc claim that no longer matches the code
  - your proposed plan for this round's focus, and any open §10 decision that blocks it

Calibration: every `file:line` citation has been re-checked against the current source (the U-series ones as recently as 2026-11-09) and holds. The suite grew from the old 277/277 baseline to 315/315 as tests were added with each fix, and the whole B-series is essentially done except B9 — so the "open reds are B1,B5,B8,B9,B10,B11" line in these docs is stale. Citations stay trustworthy until the **code** moves, not until the docs do — so if `git status` is clean and `git log --oneline -- include src tests CMakeLists.txt` shows no code change since `f8f15cc`, trust the citations and spend your effort on the §11 **Evidence** column instead: several rows are marked code-only or "not re-run this pass", and those are the ones worth re-testing. If any code has landed since, re-verify the sites you're about to touch. Note that some historical §13 entries cite test IDs from before a +3 numbering shift (B13 was #270; it is now #273) — use the current IDs in §5, §6.10 and §11.

Wait for my go-ahead.

## Step 3 — working rules
- One bug = one commit. Test first, then fix: several items specify a test that must FAIL before the fix. If it passes immediately, you edited the wrong file (see B15).
- Re-run the full suite after every fix. Never finish with a lower test count than you started.
- Headless-first tests only: `Game(true)` + `setDeck(...)` + `*ForTest()` accessors. No display — **except for the vision-critical U-series items (U1,U2,U3,U6,U11,U12)**, whose nature needs an SDL window/Xvfb and a rendered screenshot; they are exactly why Phase 0.4 has not yet closed the presentation seam.
- New JSON fields must be optional with a default. Never break a legacy profile.
- `Player::getProfile()` returns BY VALUE — never bind it to `const auto&`.
- If a doc is wrong, FIX THE DOC in the same commit. Don't work around it silently.
- **B15 lands before B12/B13/B14.** There are two live copies of `Serialization.hpp`: the shipped binary compiles `src/`'s copy via `src/PlayerProfile.cpp`, while `test_profile_settings.cpp` / `test_game_rules.cpp` / `test_achievements.cpp` compile `include/`'s. A schema fix in the wrong copy goes green with an unchanged binary.
- **B1 has SEVEN `saveProfile()` call sites**, not three — the table is in the B1 entry (the seventh, `SessionLifecycle.cpp:293`, was added after the six were enumerated). Gating three leaves the bug.
- `.summaries/0.4/hist_proof.cpp` is the checked-in B13 proof; it exits **0** once B13 is fixed (it exited **1** while open) and is the fastest way to confirm a B13 fix reached the *shipped* schema rather than just the tested one.
- Never append `-M` to a command copied from `compile_commands.json` — it overwrites the `.o` with the dependency list. See CONTEXT §8.

## Step 4 — before you finish
- Update the §11 status tracker in CONTEXT.md.
- Append any decision made to the decision list in CONTEXT.md §5.
- Add a dated entry to CONTEXT.md §13 if you corrected any claim in these docs.
- Tell me: what landed, what's still open, what you'd do next — and, for any vision-critical U-item, how you verified it on screen (Xvfb/screenshot).

## Hard guardrails
- No new features. IDEAS.md is not a plan.
- Do not revive Feature 6 (KO / Wong Halves / Omega II counting).
- Do not widen `GameRules` beyond exposing `maxSplitHands` (B10); U12's "session view" is a NEW dataset, not a GameRules change.
- Do not change the `AppMode` enum except by adding a value with its own state machine.
- Do not "fix" the `handActions` indexing — it is correct; see CONTEXT §6.9.
- Do not re-litigate anything already cleared in §6.9.
- Do not break the additive-JSON contract: every new field optional, with a default.