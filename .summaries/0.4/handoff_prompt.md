You are continuing work on Blackjack2 (C++17 game / C++20 tests, SFML 3, CMake, Catch2 v3.6.0), a desktop blackjack teaching simulator. Phase 0.4 is a QA-hardening pass: defect removal, not feature work.

## State right now (read before Step 0)

- Test suite is **327/327 green** (~23s serial; 10-run `-j8` soak 10 clean / 0 failing), code HEAD `3c7236d` (U-series `e7677fe`, harness `7d29767`, B17 follow-up `02eae41`); configure + build clean (warnings only — the pre-existing implicit-this deprecation in GameRenderer.cpp's modeTable() lambdas). Working tree is clean.
- §11 defect count: the **B-series is essentially done**. Only B9 remains, and it is ⚠️ **Partly** — Logger flush + summary-overlay bounds are fixed (#292–#294, #296/#297), but one unreproducible R-time crash still has no test.
- UI/UX backlog (U1–U13): **only U12 is open, and it is DEFERRED by decision on 2026-11-09** — it needs §10 **#8** (histogram bounds) and **#9** (session = new dataset vs. view) answered first. Defaults if nobody answers: 8(a), 9(a). U4/U5/U7/U8/U9/U10 landed earlier; **U1, U2, U3, U6, U11** were fixed on 2026-11-09 (`e7677fe`, tests #317–#325) and **verified on screen**, and **U13** (non-ASCII text reaching SFML → mojibake) was found by that screenshot pass and fixed too (#326).
- **B17 was NOT complete and is now closed.** The five-file HOME redirect left 68 round-driving `Game(true)` cases unguarded; a serial bisect found Test #99 (`tests/test_settlement.cpp`) rewriting the developer's real 9.3 MB career profile. Isolation is now **process-scope** (`tests/test_process_isolation.cpp` + `test_support::ProcessHomeIsolation`), locked by #327, and the re-bisect shows zero writers. **Never** add a per-test HOME guard again; the process guard covers everything.
- **The presentation seam is now closed as a tooling problem.** `tools/visual_check/` runs the real binary under Xvfb, drives it with synthetic X11 input (python-xlib — xdotool/scrot are not installed) and captures PNGs. Read `tools/visual_check/README.md` before writing any new scenario: it records four traps that cost real time (arrow-key navigation wraps; Escape at the main menu closes the window; a 0 bet cannot be confirmed and OfferInsurance swallows H/S; SFML decodes `std::string` as ANSI, not UTF-8).

## What is left — choose Step 0 from here

The U-series is closed and verified on screen; the suite is hermetic. **U12 is deferred by decision**, so the productive
Step 0 options are: answer §10 #8/#9 and then take U12; take **T1**'s history-growth half; or close the remaining
documentation/citation drift. Do not re-audit the B-series — it is done except B9.

  U12 Side-panel graphs (`StatsDashboard` `GameRenderer.cpp:74`; HUDState series `HUDState.hpp:55-56`): no per-graph visibility toggle; only a per-shoe view with NO session view; Bets Placed X-axis is the full [0, MAXIMUM_BET] instead of the intended 10–100 scaling range. Fix: (1) per-series visibility flags + draw gating; (2) a NEW session-scoped dataset distinct from longhistory with a UI mode switch; (3) reconfigure the axis to 10–100 growing up to MAXIMUM_BET (`Constants.hpp:194-200`). Needs a §10 decision on bucket bounds before the axis half is touched. Verify with `tools/visual_check/` across real gameplay and window sizes.
  T1  The write cadence is gated (`AUTO_SAVE_EVERY_HANDS = 5`, #305/#306) but `HandEvaluator.cpp:295` still appends a `RoundRecord` every settled round, so the in-profile history still grows per round — the 9.3 MB real profile on this machine is that residue. T2's cap covers completed shoes, not the current one.

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

Calibration: every `file:line` citation has been re-checked against the current source and holds. The suite grew from the old 277/277 baseline to **326/326** as tests were added with each fix, and the whole B-series is essentially done except B9 — so the "open reds are B1,B5,B8,B9,B10,B11" line in these docs is stale, as is any list that still names U1/U2/U3/U6/U11 as open. Citations stay trustworthy until the **code** moves, not until the docs do — so if `git status` is clean and `git log --oneline -- include src tests CMakeLists.txt` shows no code change since `3c7236d`, trust the citations and spend your effort on the §11 **Evidence** column instead: several rows are marked code-only or "not re-run this pass", and those are the ones worth re-testing. If any code has landed since, re-verify the sites you're about to touch. Note that some historical §13 entries cite test IDs from before a +3 numbering shift (B13 was #270; it is now #273) — use the current IDs in §5, §6.10 and §11.

Wait for my go-ahead.

## Step 3 — working rules
- One bug = one commit. Test first, then fix: several items specify a test that must FAIL before the fix. If it passes immediately, you edited the wrong file (see B15).
- Re-run the full suite after every fix. Never finish with a lower test count than you started.
- Headless-first tests only: `Game(true)` + `setDeck(...)` + `*ForTest()` accessors. No display in the test suite. For anything that only shows up on screen, use `tools/visual_check/` (Xvfb + python-xlib screenshots) as the verification step — the U-series is now locked **both** ways: pure geometry helpers in `include/PresentationLayout.hpp` asserted headlessly (#317–#326), and the drawn result confirmed by screenshot.
- **A string reaching `sf::Text` must be ASCII, or built as `sf::String(U"…")`/`std::u32string`.** SFML 3 decodes `std::string`/`const char*` as ANSI through `std::locale` (`SFML/include/SFML/System/String.hpp:173`), so a UTF-8 source literal arrives as several code points and the 0x80-range one has no glyph. `isSfmlSafeAscii()` + test #326 enforce it on the hint helpers. Do not blame the font before checking its cmap: `assets/fonts/noto/NotoSans-Regular.ttf` **does** have U+203A (`guilsinglright`); it has **no** U+2191/U+2193 arrows.
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
- Tell me: what landed, what's still open, what you'd do next — and, for any vision-critical U-item, how you verified it on screen (`tools/visual_check/`, Xvfb/screenshot). A screenshot tells you *what* is broken, not *why*: test a suspected cause against the artifact (font cmap, library header) before writing it into §5.

## Hard guardrails
- No new features. IDEAS.md is not a plan.
- Do not revive Feature 6 (KO / Wong Halves / Omega II counting).
- Do not widen `GameRules` beyond exposing `maxSplitHands` (B10); U12's "session view" is a NEW dataset, not a GameRules change.
- Do not change the `AppMode` enum except by adding a value with its own state machine.
- Do not "fix" the `handActions` indexing — it is correct; see CONTEXT §6.9.
- Do not re-litigate anything already cleared in §6.9.
- Do not break the additive-JSON contract: every new field optional, with a default.