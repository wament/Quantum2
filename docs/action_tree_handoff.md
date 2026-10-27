# Action Tree — Handoff & Roadmap Map

**What this is:** an operational map for handing the Action Tree tool across multiple fresh context windows. Each window should be *prefilled* with specific docs; this file tells you which ones and why, records the timeline of design decisions, and lays out the implementation roadmap with a clear done/next split so nothing is re-derivable-lost on reload.

**Golden preload rule:**
- **Every continuation window starts with `docs/action_tree_revisions.md`** — it is the as-built source of truth (exact formats + code anchors + deviations + remaining work).
- Load `docs/action_tree_spec.md` **only** when you need original design rationale or are about to change a decision marked "locked." It explains *why*; revisions explains *what shipped*.
- Load the specific source/test file only when editing that area (see per-phase recipes below).

## Document index — preload by need
| File | Role | Preload it when… |
|---|---|---|
| `docs/action_tree_revisions.md` | **Primary as-built map** (formats, anchors, deviations, remaining work) | Almost every implementation window. Default first load. |
| `docs/action_tree_spec.md` | Original plan & locked design decisions (§0–§12) | Need "why we chose X"; about to change a locked decision; onboarding the idea from scratch. |
| `include/ActionTree.hpp` + `src/ActionTree.cpp` | The implementation (157 / 1079 lines) | Editing model, generator, or a backend. Anchor lines are in revisions. |
| `tests/test_action_tree.cpp` | §8 test expectations (incl. real-`Game` cross-check) | Phase 1 (test execution gate). |
| `CMakeLists.txt` | Build wiring (ActionTree.cpp at lines 101 & 160; Catch auto-discovery) | Changing build targets/sources. |
| `docs/action_tree_handoff.md` | This file (timeline + phases + preload recipes) | Orienting into the program of work / deciding next phase. |

## Timeline of design decisions
Project: Blackjack simulator in C++/SFML that already autoplay-shoes with basic strategy. New tool: enumerate the *full action tree* of a shoe for every player, with basic-strategy paths labeled; designed now for text output, later for a visualizer.

- **D1 — Scope & root shape.** One chunk = one round. Root node = initial-deal state (all seats + dealer hold two cards). Decided because a "round" is the natural unit and every seat's starting pair should be fixed across branches.
- **D2 — Faithful deal order + shared monotonically-advancing deck cursor.** Shoe = one immutable card sequence; cursor advances only on cards drawn during play and is *shared across seats on a path*. So a branch where seat 0 draws more shifts what seat 1 / the dealer receive downstream, while starting pairs stay put. Chosen so outcomes correspond to real rounds (confirmed as recommended over exhaustive enumeration).
- **D3 — Enumerate all legal actions at each node.** Every edge = an action (H/S/D/P/R); continuing-hit branch plus complementary stand/double/surrender branches. Basic-strategy edge flagged `isBasicStrategy`. After the first hit only H/S remain; splits fan sibling hands (re-split up to MAX_SPLIT_HANDS=4, split aces stand after one card unless re-splittable).
- **D4 — Seat play serialized into the path.** Seat 0 subtree resolves → seat 1 "initiates the next branch" → … → dealer completes each line. Root-to-leaf = one full round where each seat independently chose an action line; leaf records per-seat result for that combination.
- **D5 — Whole shoe = forest of chunk-trees.** Each chunk seeded on the immutable tail left by the previous round's *actual (basic-strategy)* resolution, so outcomes match real play.
- **D6 — Headless output; visualizer later.** No graphics today. Three backends read one canonical model: flat pre-order text (primary save), ASCII preview, JSON export. Model is viz-ready (stable ids, edge flag, outcomes) but stores no geometry.
- **D7 — Efficient text now, universal share later.** Chose option 1 (flat pre-order records) as primary; also asked for ASCII methods + JSON (§7).


## Implementation roadmap — overview & status
Status key: ✅ done · 🟡 in progress / pending execution · ⬜ backlog. "Done" means code written & wired; test execution is a separate gate (see P1).

| Phase | Objective | Status | Preload (from index) |
|---|---|---|---|
| P0 | Orient into the program of work | ✅ done (this file) | handoff.md + revisions.md |
| P1 | Execute §8 tests; pass fidelity gate (real-`Game` cross-check) | 🟡 written+wired, **not yet confirmed passing** | revisions.md + spec §8 + test_action_tree.cpp |
| P2 | Optional: refactor `toJson()` to nlohmann for maintainability | ⬜ backlog | ActionTree.cpp :1040/982; revisions §dev-2 |
| P3 | Optional: replay an actual played shoe via `Deck` records (vs injected seq) | ⬜ backlog (§12) | spec §6/§12; Deck.hpp/Game record accessors |
| P4 | Optional: opt-in tree limits (leaf cap / sampling / max-depth / single-round mode) | ⬜ backlog (§8.3/§12); `summarize()` exists at :818 | revisions §Remaining; spec §12 |
| P5 | Interactive SFML visualizer w/ basic-strategy spine highlight | ⬜ big backlog (§12/§5 viz-readiness) | spec §5/§12; revisions JSON schema |
| Px | Cleanup: resolve `tests/probe_round5.cpp`, nlohmann presence, stray flags | ⬜ sporadic | handoff index |

**Critical path = P1.** Everything else is optional enhancement. Do not move to P2–P5 until P1's tests pass; the cross-check against a real headless `Game` (spec §8.2) is the fidelity proof for the whole tool.

## Current status summary (what has already been done)
- ✅ Model + API (`ActionTree.hpp`): TreeNode/Chunk/SeatInfo, Outcome enum, all public methods (§5).
- ✅ Generator + rule mirror (`ActionTree.cpp:95–620`): legalActions canonical H,S,D,P,R; applySimpleAction; walkActiveSeat/attachContinuation/buildFirstSplitHand; buildDealerBranch (H17); simulateSpineCards/playSeat spine seeding per §4/§6.
- ✅ Three backends (`ActionTree.cpp:627–1079`): flat pre-order (§7a), ASCII with maxDepth preview (§7c), hand-written JSON (§7b). Plus fromFlatText round-trip, extractBasicStrategySpines, summarize.
- ✅ Tests written (`test_action_tree.cpp`, §8): unit + real-`Game` cross-check + size/determinism — but **not confirmed green**; building/running is the pending gate.
- ✅ CMake wired (both targets; Catch auto-discovery).

## The one thing unconfirmed by static review only
Tests are written and linked, but I have not executed them in this session (that requires a build with SFML+Catch2). Until P1 runs green, treat "implementation complete" as "code present," not "validated." This is exactly what P1 exists to settle.


## Per-phase detail (objective · preload · done · next · exit criteria)

### P1 — Test execution & fidelity gate (CRITICAL PATH, do first)
- **Objective:** build and run the §8 suite; confirm it passes, especially the cross-check that the basic-strategy spine reproduces a real headless `Game`'s prescribed actions, outcomes, and card-consumption exactly. Fix any failures.
- **Preload:** `docs/action_tree_revisions.md` (→ "Remaining work" item 1 + Code anchors) + `docs/action_tree_spec.md` (§8 only) + `tests/test_action_tree.cpp`.
- **Done:** tests written; CMake wired via Catch auto-discovery.
- **Next:** run the suite in a configured build dir; triage failures against §8 expectations (legal-action sets, isBasicStrategy labeling, split fan-out/MAX_SPLIT_HANDS cap, double-after-one-card, surrender-on-two-cards, deal-order mirror, flat byte-identical round-trip, JSON fidelity, determinism, size measurement).
- **Exit criteria:** `ctest -R action_tree` green; report the failing case name + expected-vs-actual if any test fails. Only then proceed to optional phases.

### P2 — nlohmann JSON refactor (optional, low priority)
- **Preload:** `ActionTree.cpp` :1040/982 + revisions §dev-2. **Done:** hand-written valid JSON. **Next:** optionally switch to `nlohmann::json`. **Exit:** byte-for-byte identical output; existing round-trip test still passes.

### P3 — Replay actual played shoe via Deck records (optional)
- **Preload:** spec §6/§12 + `Deck.hpp` + Game record accessors (`getPreviousShoeRecord()`/`activeShoeRecord()`) + revisions. **Done:** injected-sequence forest. **Next:** seed the tree from a real recorded shoe instead of a synthetic sequence. **Exit:** same output contract on a real shoe's card order.

### P4 — Opt-in tree limits (optional)
- **Preload:** spec §12/§8.3 + `ActionTree.cpp` :818 (`summarize()`) + revisions. **Done:** `summarize()` reports node/leaf/max-depth. **Next:** add total-leaf cap/sampling, max-depth preview mode, single-round/single-seat modes (driven by those numbers). **Exit:** configurable limits without changing default full-tree output or existing tests.

### P5 — Interactive SFML visualizer (optional, large)
- **Preload:** spec §5/§12 + revisions JSON schema + `include/Game.hpp`/`GameRenderer.cpp` for rendering infra. **Done:** viz-ready model + JSON export. **Next:** a window that consumes the tree (JSON or first-child/next-sibling), highlights basic-strategy spines (`extractBasicStrategySpines`), expands/collapses nodes. **Exit:** renders a chosen round's tree; still headless-serializable for analysis.

### Px — Cleanup (sporadic)
Resolve `tests/probe_round5.cpp` (confirm demo or remove), decide nlohmann presence, clear stray flags. No dedicated window needed unless it blocks another phase.

## Prefill recipes — copy/paste into the next window
Bootstrap (any continuation):
```
Read docs/action_tree_revisions.md in full and treat it as the as-built source of truth;
read docs/action_tree_spec.md only if I need original design rationale or am about to change a locked decision.
Then continue the Action Tree tool per its roadmap in docs/action_tree_handoff.md, phase <P1/P2/…>.
```
Immediate next window (P1):
```
Implementation gate: build and run the Action Tree tests and make them pass.
Preload docs/action_tree_revisions.md (§Remaining work item 1 + Code anchors),
docs/action_tree_spec.md (§8), and tests/test_action_tree.cpp.
Use the existing configured build dir under out/build/… (reconfigure with the project's CMake preset if stale).
Run `ctest --output-on-failure -R action_tree`. The fidelity proof is §8.2: my basic-strategy spine must reproduce a real headless Game's prescribed actions, outcomes and card-consumption exactly. Report each failing case as name + expected vs actual; do not proceed to optional phases until this is green.
```

Outcomes by date: `docs/action_tree_spec.md` written (Oct 26); `include/ActionTree.hpp`, `src/ActionTree.cpp`, `tests/test_action_tree.cpp` created and CMake wired (Oct 27); `docs/action_tree_revisions.md` written (comparison vs plan). This file (`action_tree_handoff.md`) is the program-of-work map.
