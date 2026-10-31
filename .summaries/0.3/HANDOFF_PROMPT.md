---
# Blackjack2 Refactor — Chat Window Handoff Prompt

Copy everything between the rules below and paste it at the start of a fresh chat window / new context so the next session inherits this refactor pass without re-deriving anything. It references two generated documents in `.summaries/0.3/`.
---

Blackjack2 (C++17 / SFML 3 / CMake / Catch2) — this is a REFACTOR refinement pass over already-completed feature work, NOT new features. Two reference documents govern everything. Read them first, in order:

1. `.summaries/0.3/CONTEXT_refactor.md` — the audit + state: §6 decisions and how they constrain this refactor (§1); file-size baseline vs the 800-line target (§2); every concrete duplication finding with line numbers (#1-#11, in §3 and §7); what is already centralized / DO NOT touch (§4); feature-overlap review queue (§5); plus the truncation workaround.
2. `.summaries/0.3/ACTION_PLAN_refactor.md` — execution order: Phases 0-8 (P0 highest ROI -> P3 trivial), each with modify / add / target / test / gotcha, the verification gate, guardrails, follow-up items, and Definition of done.

Also read `.summaries/0.3/CONTEXT.md` for architecture + testing contract (it is the authority on feature behavior; this refactor doc only narrows what may change).

HARD CONSTRAINTS you MUST honor:
- Refactor ONLY. No behavior changes to betting / settlement / counting / side-bets / timing. Each phase is behavior-preserving; freeze refactored output with new assertions where useful.
- Mode dispatch stays enum-driven: extend AppMode, each mode keeps its own state machine mirroring Replay/Pause wiring (never ad-hoc booleans). Add a mode = new enum value + update() handling.
- Headless-first testing: logic tested via Game(bool headless) + setDeck(...) and read through *ForTest() accessors before any UI. Presentation-only code stays Graphics-only (test binary links only SFML::Graphics).
- Tests live in tests/*.cpp AND are registered under Blackjack2Tests in CMakeLists.txt, or they won't run — including new helpers extracted during a refactor.
- Additive JSON schema: PlayerProfile keeps every field optional with defaults; old profiles must still load. Re-run the "old profile loads" check if you touch persistence.
- Scope guardrails (§6): counting stays Hi-Lo + Reko only (Feature 6 frozen — do not revive); GameRules stays a narrowly-scoped const GameRules& into advisor/dealer/UI (do not widen). Do not add new AppMode values or change the enum.

ALREADY CENTRALIZED / DO NOT TOUCH: Game::resetShoeAnalytics() is the single shoe-boundary choke point; shared list-menu helpers. ALREADY CLEAN / do NOT re-scan these: SoundManager (data-driven event->file table), Tournament (delegates betting/settling to Player + RoundEngine), BasicStrategy (Feature 6 frozen), Player::calculateNextBet (substantive per-strategy logic, not variable assignment).

VERIFICATION GATE — run after EVERY phase:
- Build: cmake --preset "GCC 13.3.0 x86_64-linux-gnu" -> binary dir out/build/GCC 13.3.0 x86_64-linux-gnu/
- Test: ctest --test-dir out/build/GCC 13.3.0 x86_64-linux-gnu --output-on-failure
- Note: under the ASan+LSan preset ~75 SFML-linked test processes report "Failed" at exit from environmental libdbus teardown (not a real error). Run ctest with ASAN_OPTIONS=detect_leaks=0 for a clean pass/fail read.

TRUNCATION WORKAROUND when reading large files: the reader truncates at ~47k chars / 2000 lines regardless of line count. Page via start_line/end_line ranges and prefer grep / search_codebase over full-file dumps. Keep doc lines short (~100 chars) so char-based truncation never drops critical content.

SUGGESTED ORDER: start with Phase 0 (InputHandler Escape helper — highest ROI / lowest risk), then follow ACTION_PLAN_refactor.md phases in order, committing per phase. After each increment update §3/§7 findings and §4/§5 status in CONTEXT_refactor.md so the next window inherits accurate state.
---

## CURRENT STATE — read this first (appended by the most recent session)

Phases DONE + verified green before their commits (full list in §6 of CONTEXT_refactor.md):
- Phase 0 `567ad71`, Phase 1 `e9268cc`, Phase 2 `fb81bcb`, Phase 3 `308488f` (code) + `e9f210d`
  (docs), Phase 4 `44ea266`, Phase 5 `33c524c`, Phase 6 `0d45de5`, Phase 7 `6787e13`, Phase 8 `80b1022`.
  Full suite 236/236, tree clean.

Phases REMAINING in ACTION_PLAN_refactor.md order: all phases 0-8 are done. The only remaining item is
the #8 follow-up: Game.cpp god-class split (>800 lines, biggest structural win — extract Betting /
Settlement / SessionLifecycle helpers; needs new test scaffolding).

This session did Phases 5 + 7 (behavior-preserving; full suite 236/236 green):
- Phase 5 `33c524c`: trimmed verbose per-constant prose in Constants.hpp's single GameTiming namespace
  plus Kelly/edge-over-time and Feature-8 deal-timing constants; standardized Serialization.hpp's ~11
  additive-schema comments to short notes. All values unchanged.
- Phase 7 `6787e13`: collapsed HandEvaluator evaluateHands() Surrender/Lose/Win case+logger blocks by
  routing the per-result counter and human-seat streak flag by outcome, then one byte-for-byte-identical
  log line via a result->verb map. Counters/streak flags/settlement semantics unchanged.

Prior session (Phase 3) — GameRenderer::draw():
- Collapsed the backdrop if/else-if chain + separate dispatch switch into ONE unified table
  AppMode -> {sf::Color backdrop, render} = modeTable() in src/GameRenderer.cpp. draw() is now a single
  lookup; every mode's color + draw fn live on one row so they can't drift apart (Finding #4).
- All 17 draw* methods intact & still call their own ->draw(window); Playing/Paused bodies preserved
  byte-for-byte inside render lambdas. Unknown modes -> {nullopt backdrop + no-op render}.
- Exposed ModeEntry struct + modeEntryForTest(AppMode) in include/GameRenderer.hpp (*ForTest pattern).
- New test: tests/test_game_renderer_modes.cpp, registered under Blackjack2Tests in CMakeLists.txt.

Gotchas learned this session (avoid on the next phase):
- GCC needs an EXPLICIT lambda capture for member calls ([] does NOT implicitly grab `this` here). Use
  [=] or [this]. Do NOT wrap such lambdas inside an immediately-invoked [](){...}() IIFE — nesting one
  extra lambda layer breaks [=]'s reach to the enclosing member's this. Build maps by assigning t[X]=...
  directly inside the member function instead (that is what modeTable() does now).
- GameRenderer.hpp now needs AppMode (modeTable/modeEntryForTest signatures) via #include "Game.hpp".
  Because Game.hpp includes GameRenderer.hpp before it defines AppMode, any TU including GameRenderer.hpp
  MUST include "Game.hpp" FIRST (its `GameRenderer renderer;` member needs the complete type). All TUs
  already do; if you add a new test, keep that include order.
- Net size: GameRenderer.cpp net +13 lines (81 added / 68 removed) from the table + accessor — deliberate
  structure, not bloat; readouts locked identical by tests. <800 comes from follow-up #8's god-class split.

Next concrete step if continuing: pursue #8's Game.cpp god-class split (ACTION_PLAN_refactor.md "Follow-up"),
verify-green, commit in pieces (code then docs), and refresh the §3 finding + §6 list for it.
---
