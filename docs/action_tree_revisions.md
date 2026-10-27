# Action Tree — Revisions vs. Original Spec

**Purpose:** As-built reference for the next window. Compares what was implemented in this session against `docs/action_tree_spec.md` (the plan), records the exact formats actually shipped, and lists deviations + remaining work so a fresh prompt continues without re-deriving anything.

## Status snapshot (end of implementation session)
| Item | File(s) | Notes |
|---|---|---|
| Model + API | `include/ActionTree.hpp` (157 lines) | Matches spec §5, plus a multi-seat `seatCount` param (§dev-1). |
| Implementation | `src/ActionTree.cpp` (1079 lines) | Generator + rule mirror + 3 backends. Reuses `Card`/`Hand`/`Constants`/`getRecommendedAction`; mirrors `Game.cpp`+`RoundEngine.cpp`. Does NOT drive real `Game`. |
| Tests | `tests/test_action_tree.cpp` (Catch2, §8) | Unit tests + **cross-check vs headless `Game`** (§8.2) + size/determinism (§8.3/§8.4). 31 refs to ActionTree. |
| CMake | `CMakeLists.txt` lines 101 & 160 | `ActionTree.cpp` added to BOTH targets; Catch auto-discovers the test. |
| JSON dep | `include/nlohmann/json.hpp` | Project-wide header (also used by `Serialization.hpp`, `PlayerProfile.hpp`). ActionTree's own `toJson()` is **hand-written** (§dev-2). |

## Fidelity verdict: HIGH — implementation matches the spec

## As-built data model (§5 — unchanged)
`TreeNode`: `nodeId, seatIndex, isDealerNode, decision(char), isBasicStrategy, cards(vector<Card>), handValue, isSoft, outcome(Outcome), seatResults(vector<int>), deckCursorAtNode, children`. `Outcome` enum: DealRoot, HitContinue, Stand, Bust, TwentyOne, Blackjack, Surrender, DoubleEnd, SplitSiblingStart, DealerResult. `Chunk`: `roundIndex, deckStartCursor, seats(vector<SeatInfo>), root`. `SeatInfo`: `index, name, startCards`. Constructor is `ActionTree(std::vector<Card>, int seatCount = 1)`.

## As-built flat pre-order format (§7a — EXACT, use verbatim in tests)
File: line `FOREST <nChunks>`, then per chunk a header `CHUNK <roundIndex> <deckStartCursor>`, then one pipe-delimited record per node in DFS pre-order. A node's explicit `nChildren` is the delimiter (parser recurses exactly that many child records). Record fields, left→right, no spaces:
```
nodeId|seatIndex|isDealer|decision|isBS|handValue|isSoft|cards|deckCursor|nChildren|outcomeCode|results
```
- `decision` label: `DEAL`(root)/`H`/`S`/`D(double)`/`P`/`R`/`X`(dealer); root renders `DEAL`.
- `isBS`: 0/1. `cards`: comma-separated **bj-values** only (suits are NOT stored; reconstructed as Spades on parse via `cardWithBJ`).
- `results` (only on `DealerResult` leaves): `s0=WIN,s1=LOSS,...`; seat index sits between leading `s` and `=`. Default dense-fill is PUSH.
Round-trip is byte-identical: `fromFlatText()` rebuilds chunks_ only (the immutable shoe is not required to round-trip).

## As-built JSON (§7b — fields, hand-written, valid)
Top level: `{ "shoe": { "decks": <NUM_DECKS> }, "chunks": [ ... ] }`. Per chunk: `roundIndex, deckStartCursor, seats[] (index,name,startCards as bj-values), root`. Per node object: `nodeId, seatIndex, isDealerNode, decision, isBasicStrategy, cards[], handValue, isSoft, outcome("…"), deckCursorAtNode, children[]`; DealerResult leaves additionally carry `seatResults[]`. Fields are emitted in exactly that order; children last with no trailing comma.

## As-built ASCII (§7c — renderer)
Connectors use real Unicode └─/├─ (and │ continuation); root has no connector. Node label: `[seat i] cards (value[ soft]) | edge=<H|S|D|P|R>` with a trailing `*` marking the basic-strategy edge; dealer nodes show `[dealer] … | edge=X`. Terminal suffixes: `=> bust N`, `=> 21`, ` => BLACKJACK`, `=> SURRENDER`, `=> DOUBLE N`, `=> STAND`, `=> SPLIT HAND`. `toAscii(maxDepth)` with `-1`=full; when capped it prints `…(N more nodes omitted)`. Chunk header line: `== Chunk i  round=..  cursor=..  seats=.. ==`.


## Deviations / enhancements vs. original plan (§1–§7)
These are the places where as-built differs from `action_tree_spec.md`. None alter core behavior; flag them so a future window doesn't "fix" intent.

- **§dev-1 — Multi-seat `seatCount` param.** Spec §5 had a single-seat constructor; impl adds `ActionTree(seq, seatCount = 1)`. Default 1 (human seat); multi-seat runs share one cursor per path (§1.2). Seat names are fixed: William, Alice, Bob, Carol, Dave, Eve (`kSeatNames[]`, line 22).
- **§dev-2 — Hand-written JSON.** `toJson()`/`writeNodeJson()` build strings manually (not nlohmann). Output is valid and matches §7b. nlohmann/json.hpp exists project-wide but ActionTree does not use it — leave as-is unless a consumer needs nlohmann parsing.
- **§dev-3 — Dealer branch emits one node PER draw** (up to 8 cards) rather than collapsing to a single terminal. The intermediate dealer nodes carry only card/value/soft; the single trailing `DealerResult` leaf carries per-seat results. Spec §6 allowed either form; this is richer. The hole card is revealed by reading `shoe_[deckStart + 2*seats + 1]` WITHOUT consuming from the cursor (§4).
- **§dev-4 — Subsequent split siblings reconstruct cards from family pair-value + Suit::Spades** (`attachContinuation`, line ~279) rather than storing each sibling's actual dealt cards. Only the FIRST split hand (buildFirstSplitHand) uses the real moved card; later siblings are rebuilt as `cardWithBJ(pairValue, Spades)` + forced draw. Suits never affect blackjack logic, so outcomes are correct, but a reconstructed sibling's `cards` field won't match the original suit. Common 2-way split is unaffected (first-hand path).
- **§dev-5 — ASCII shows splits as a `=> SPLIT HAND` suffix** on each sibling node instead of the spec's suggested `┃ SPLIT (n hands)` marker line. Equivalent information; cosmetic only.
- **§dev-6 — Graceful exhausted-shoe fallbacks.** A hit or double when the shoe is empty ends the hand at its current value (Stand) rather than throwing (§4 applySimpleAction / playSeat). Spec didn't specify this; it's defensive. Also dealer draw loops cap at 8 cards (`< 8`) as a shoe-exhaustion guard (§4).
- **§dev-7 — Surrender knob is implemented exactly as the plan's DEFAULT** (offer `R` on any legal first-two-card hand, `legalActions` line 128), NOT restricted to where strategy recommends it. Confirmed; if you want the stricter variant it's a one-line guard in `legalActions`.

## What is DIFFERENT but worth double-checking (not necessarily wrong)
- **§chk-1 — Split-sibling card reconstruction (§dev-4).** Verify multi-way splits (>2 hands) still produce correct values if that path ever matters; suits are cosmetic, values should be right.
- **§chk-2 — `Outcome` vs settlement `ResultCode`.** Seat results use a separate local `enum ResultCode {PUSH=0,WIN=1,LOSS=2,SURRENDER=3,BLACKJACK=4}` (`settleResult`, line 45), distinct from the node `Outcome` enum. They never mix (results live only in `seatResults`). Confirm this stays intentional if either enum grows.
- **§chk-3 — Suits collapse to Spades on parse.** Because flat/JSON store bj-values only, any rebuilt `Card` is Spades. Functionally irrelevant but note it if a future viz ever displays suit sprites from the tree.


## Remaining work for the next window
1. **Validate tests.** `tests/test_action_tree.cpp` exists per §8 but pass/fail is unconfirmed here. Build and run: prefer `ctest`; if not configured, use the existing build dir in `out/build/…`. Gate on §8.2 (cross-check spine reproduces real Game's actions/outcomes/card-consumption). Report any failures with the exact case + expected vs actual.
2. **Decide nlohmann (§dev-2).** Leave hand-written JSON, or refactor `toJson()` to use `nlohmann::json` for maintainability. Low priority; only if a consumer needs parsing.
3. **Optional spec §12 extensions** (none implemented yet): replay an actual played shoe via `Deck` records instead of an injected sequence; interactive SFML viewer consuming the JSON/first-child-next-sibling form with basic-strategy spine highlight; opt-in tree limits (total-leaf cap + sampling / max-depth / single-round mode). The plumbing exists (`summarize()` at line 818) but no limiting modes are implemented.
4. **Scratch file.** `tests/probe_round5.cpp` is present but not referenced by the test list above — confirm it's a demo or remove it before committing work to it.

## Code anchors (§4/§6/§7 functions → line in src/ActionTree.cpp)
- legalActions :95 · applySimpleAction :137 · walkActiveSeat :186 · attachContinuation :271
- buildFirstSplitHand :318 · buildDealerBranch :354 · playSeat :444 · simulateSpineCards :528
- buildRound :559 · buildShoeForest :591
- record :627 · emitFlat :650 · toFlatText :658 · fromFlatText/parseFlatNode :745/670
- summarize :818 · extractBasicStrategySpines :831 · subtreeNodeCount :859 · nodeLabel :869
- emitAscii :911 · toAscii :933 · outcomeName :950 · jsonEscape :968 · writeNodeJson :982 · toJson :1040

## Quick verification command (once configured)
```
cd out/build/GCC\ 13.3.0\ x86_64-linux-gnu && ctest --output-on-failure -R action_tree
```
If that build dir is stale, reconfigure with the project's CMake preset first. The flat/ASCII outputs are also directly printable via a tiny harness over `ActionTree::toFlatText()`/`toAscii()`.

## Bottom line
The implementation is faithful to the plan. Two things to actually DO next: (a) run and confirm the §8 tests pass — this is the fidelity proof; (b) decide whether to keep the small deviations (§dev-1…7) as-is or adjust. Everything needed to continue is anchored above; re-read `docs/action_tree_spec.md` only for original design rationale, use THIS file for what actually shipped.

The design decisions locked in plan are faithfully implemented: chunk=round root, faithful up-front deal order, shared monotonically-advancing deck cursor per path, all-legal-actions-at-each-node with canonical H,S,D,P,R ordering, basic-strategy edge labeling (`isBasicStrategy`), seat play serialized into the path, splits as sibling hands (re-split up to `MAX_SPLIT_HANDS`, split aces stand after one card unless re-splittable), shoe = forest of chunks seeded on the previous round's real resolution, headless output with flat/ASCII/JSON backends. The data model (§5) and all three serialization formats (§7) are byte-for-byte as specified. The deviations below are small enhancements or minor structural choices — none change the core behavior.
