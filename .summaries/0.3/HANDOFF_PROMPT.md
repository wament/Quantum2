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
