You are continuing work on Blackjack2 (C++17 game / C++20 tests, SFML 3, CMake, Catch2 v3.6.0), a desktop blackjack teaching simulator. Phase 0.4 is a QA-hardening pass: defect removal, not feature work.

## State right now (read before Step 0)

- Test suite is **345/345 green** (~11s serial; clean at `-j4` and `-j8`), code HEAD `25cd750` plus the B22/shoeCardCount fix; configure + build clean (warnings only — the pre-existing implicit-this deprecation in GameRenderer.cpp's modeTable() lambdas).
- **B20 — the one `-j8` flake is FIXED.** Test **#175** ("U12: the session accumulator spans shoes…") failed 1 of the first 10 `-j8` runs. It passes in isolation, and a clean `HEAD` worktree with no U12 part-1 code showed the **same rate**, so it was **pre-existing, not a U12 regression**. Root cause = the **B18 class**: `startNewShoeForTest()` → `startNewShoe()` throws the test's seeded neutral deck away and shuffles from the **OS RNG**, so the rounds driven after the boundary varied per process. Re-seed after the boundary. **Measured: 120 single-test runs = 12/120 failing before, 0/120 after.** If you add a test that crosses a shoe boundary, it must re-seed the deck afterwards for the same reason.
- §11 defect count: the **B-series is essentially done**. Only B9 remains, and it is ⚠️ **Partly** — Logger flush + summary-overlay bounds are fixed (#292–#294, #296/#297), but one unreproducible R-time crash still has no test.
- UI/UX backlog (U1–U13): **CLOSED as of 2026-11-12.** **U12 is Resolved — all three parts** (per-graph view toggle #241–#244, session view #175/#176, axis bounds #301–#304), after §10 #8/#9 were decided on 2026-11-11. Note the accepted semantics: part 1 is a per-graph **view** toggle (click a pane → that pane flips per-shoe ↔ per-session; `G` = all three), **not** the per-series **visibility** toggle the original write-up proposed — see the §5 decision of 2026-11-12. U4/U5/U7/U8/U9/U10 landed earlier; **U1, U2, U3, U6, U11** were fixed on 2026-11-09 (`e7677fe`, tests #317–#325) and **verified on screen**, and **U13** (non-ASCII text reaching SFML → mojibake) was found by that screenshot pass and fixed too (#326).
- **B17 was NOT complete and is now closed.** The five-file HOME redirect left 68 round-driving `Game(true)` cases unguarded; a serial bisect found Test #99 (`tests/test_settlement.cpp`) rewriting the developer's real 9.3 MB career profile. Isolation is now **process-scope** (`tests/test_process_isolation.cpp` + `test_support::ProcessHomeIsolation`), locked by #327, and the re-bisect shows zero writers. **Never** add a per-test HOME guard again; the process guard covers everything. **The md5 gate has a false-positive direction:** if the developer runs the real app in the same window (a manual playtest), it legitimately writes that file and is indistinguishable from a leak by md5/mtime alone — check for a manual run before bisecting. Size discriminates: a real playtest leaves the file at full size (9.3 MB, 100 shoes at the T2 cap), whereas the B17 writer truncated it to 1.34 MB of test data.
- **The presentation seam is now closed as a tooling problem.** `tools/visual_check/` runs the real binary under Xvfb, drives it with synthetic X11 input (python-xlib — xdotool/scrot are not installed) and captures PNGs. Read `tools/visual_check/README.md` before writing any new scenario: it records four traps that cost real time (arrow-key navigation wraps; Escape at the main menu closes the window; a 0 bet cannot be confirmed and OfferInsurance swallows H/S; SFML decodes `std::string` as ANSI, not UTF-8).

## What is left — choose Step 0 from here

Phase 0.4 is **complete**: the B-series, the U-series and T1/T2 are all Resolved, and §11 has nothing Open. The suite
is hermetic and flake-free. The productive Step 0 options are now: **open phase 0.5**, take a UX refinement that
resists headless locking (see the visual-check note below), or verify the new save schema on screen.

  T1 — **RESOLVED 2026-11-12.** Scoped by measurement, which **overturned the write-up**. The real 9,591,391-byte profile was **64.6% indentation whitespace** (`PlayerProfile.cpp:190` wrote `dump(2)`; 427,145 lines), 31.8% `longHistory` rounds, 8.3% stored deck arrays, and only **0.2% `currentHistory` (22,888 bytes)** — so the per-round `RoundRecord` append at `HandEvaluator.cpp:295` that T1 was written as was **never the cost**, and it was left alone deliberately. Shipped: `dump()` (compact), and a shoe stored as **`shoeDecks` + `shuffleSeed`** — two integers that rebuild it exactly — replacing the 313-card array whose *sole* consumer (`Replay.cpp`) needed only the shoe's size. Real file: **9,591,391 → 2,724,930 bytes (71.6%)**. Locked by **#257–#261**.

  B21 — **RESOLVED 2026-11-12, in both directions.** `shuffleSeed` was declared, serialized and documented as a real seed, and **never assigned** (100/100 shoes carried 0). *Gameplay* now records it: `Deck` already stored `lastShuffleSeed` on every reorder path and simply had **no accessor**, so `Deck::shuffleSeed()`/`Deck::shoeDecks()` were added and all four shoe-creation sites write the real seed and deck count — production keeps the OS seed, so real shoe order stays unpredictable. *Replay* reads it: `Replay::buildSteps()` rebuilds the shoe from the record and takes `totalCards` from it. A latent second bug fell out: `lastShuffleSeed` was an **uninitialized member**, so a Deck filled by `create()`/`setCards()` without a shuffle would have serialized indeterminate garbage as a plausible seed; now `= 0`. Locked by **#13–#16** and **#258/#259**.

  B9 — **RESOLVED 2026-11-12.** The intermittent `R`-time crash was the developer's **own manual playtesting**, which is exactly why it never produced a clean record. No product-code change was made for it; see the scaffold note below before reopening.

  B22 — **RESOLVED 2026-11-12, and it caught a B21 regression.** `ShoeRecord::isContinuation` was declared, set at one site, had **zero readers**, and was **not serialized in either direction** — a saved continuation shoe loaded back as an ordinary one. Writing the B21 continuation test is what found it, and the same test exposed that `reshuffleWithRemaining()` shuffles the full deck **minus every card already dealt**, so a continuation shoe is smaller than `shoeDecks*52+1` and the seed rebuilds a shoe that was never dealt. The pre-B21 array captured the real shoe and got that for free; the seed schema had silently inflated Replay's `cardsRemaining`/`trueCount` for every continuation shoe. Fixed by storing `shoeCardCount`; both fields now serialized; locked by **#17** `[Continuation]`. **Read §13's third entry before touching `ShoeRecord`.**

  **IDEAS.md has been reviewed → `IDEAS-REVIEWED.md`.** Net effect of this phase on the backlog: every hard dependency the list cites (B1, B2, B4, B5, B8, B13, B14, B15, B16, U3, U5, U7, T2) is **Resolved**; **#4 Shoe Codes is half built** because B21 closed its stated core gap; **#1 Decision Fork's** hardest part (exact deck reconstruction) now exists and is tested; **#6 Bankroll Chart** lost its only trap after the history reset. Stale in the doc: "two live copies of `Serialization.hpp`", the instruction to delete `reconstructDeckAtRound()`, and the additive-schema guidance. Two things to act on: **`Game::persistsProgress()` is unbuilt and blocks five features**, and the **shuffle-portability fix is nearly free while history is empty** (`std::shuffle` draws through an implementation-defined distribution, so shoe codes are not portable across standard libraries) — a now-or-never window that today's wipe opened.

  Dead constant — **DELETED 2026-11-12.** `Constants::BET_BUCKET_WIDTH` had zero users; a comment at the site records why it cannot exist (U12's axis grows per view, so the bucket width is computed at runtime). `BET_BUCKET_COUNT` is live (HUD + tests) and stays.

  History reset — the developer's 9.3 MB profile was **wiped** and legacy-schema support removed, by the developer's decision. Backup: `~/.local/share/blackjack/pre-t1-backup/player_profile.pre-t1-2026-11-12.json.bak` (outside the saves dir, so the app never loads it). Verified: that backup **still loads** under the new schema (100 shoes, 1,688 rounds) and resaves to 2,724,930 bytes — `rounds` is unchanged and the removed key is ignored. Pre-T1 shoes carry seed 0, so their replay rebuilds a valid 313-card shoe rather than the historical order.

**Rule learned from B20, apply it to new tests:** a test that crosses a shoe boundary must **control the deck after the boundary** — `startNewShoe()` discards the seeded shoe and shuffles from the OS RNG. Choose the remedy by the assertion: `startNewShoeWithSeedForTest`/`shuffleWithSeed` for band/span assertions sampled from a distribution (the B18 case), but a **controlled injected deck** where the test asserts **exact round counts** — a seeded real shuffle is reproducible and can still fail deterministically on a natural. Gate it the way B18 was gated: a 100+ run single-test soak, never one green run.

**Rule learned from T1, apply it to any size/perf item: measure the artifact before scoping the fix.** T1's write-up named a specific line (`HandEvaluator.cpp:295`) and a specific symptom (9.3 MB). Profiling the file showed the named structure was **0.2%** of it, while **64.6% was indentation** from one `dump(2)` and 8.3% was a deck array whose only reader needs its length. Fixing what the write-up named would have spent the effort and moved the number by 0.2%. Decompose the artifact first, then pick the lever. Two corollaries that fell out, both now closed by B21: (1) **a field's comment is not evidence** — `shuffleSeed` was documented as a real seed and was always 0, so verify a field is *assigned* before building on it; (2) **a member with no initializer is a silent data-corruption channel** — `lastShuffleSeed` was uninitialized, so any record written from a Deck that had never been shuffled would have serialized indeterminate garbage as a plausible-looking seed. Also: **when a `from_json` key stops being written, it must stop being required** — `j.at(...)` throws, so an old and a new save are only both loadable if the old key becomes `j.contains(...)`-guarded. (This project no longer needs that guard: history was reset and legacy support was removed by decision — but the rule stands for any future schema change that keeps compatibility.)

Recorded, not fixed (do not re-derive): `Card::getSuitSymbol()` returned `u8"♠"[0]` — a truncated byte that can never be a suit glyph; removed as dead code in `667cd16`.

The one case that resisted a fix, now closed by attribution:
  B9  **Resolved 2026-11-12.** Logger flush and the overlay bounds guard were fixed with tests; the intermittent `R`-time crash was the developer's **own manual playtesting**, which is why it never yielded a clean record (one playtest hit, three failed replays, one clean 60-cycle soak). No product-code change was made for it. **A repro scaffold remains in place**: `tests/test_analytics.cpp:575`, `TEST_CASE("REPRO stats flow: play -> quit-to-menu -> open dashboard", "[StatsRepro][TEMP]")` — it drives the reported flow with an injected 12-card shoe, is B17-isolated, and prints `[REPRO]` diagnostics to stderr. It **passes headless**. Start there rather than writing a new harness if the crash ever reappears from ordinary play; the `[TEMP]` tag is deliberate (scaffolding, not a locked regression) — do not silently promote it to a permanent test, since it asserts nothing about the crash.

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