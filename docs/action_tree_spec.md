# Action Tree Tool — Specification & Implementation Context

**Status:** Design finalized (plan phase). Ready to implement in Act mode.
**Files to create:** `include/ActionTree.hpp`, `src/ActionTree.cpp` (+ a Catch2 test). No new SFML dependency.
**Goal:** A headless analysis tool that, for each round of a shoe, emits the *full action tree* — every legal action at every node is a branch, the basic-strategy edge is labeled/highlighted, and outcomes are grounded in the actual shoe's card order. The whole shoe is a forest of these per-round "chunks". The data model is the single source of truth for a future visualizer; three backends read from it today: flat pre-order text (primary save), ASCII visualization, and JSON export.

---

## 0. How to use this document
Read top-to-bottom. Everything needed to implement correctly is here: locked design decisions, exact game-rule mirror with code anchors, generator pseudocode, data model, three serialization schemas, and the test plan. **Do not re-derive rules** — reuse the real classes below. If a rule is ambiguous, the "Default / knob" callouts are already decided; change only if the user explicitly re-decides.

---

## 1. Locked design decisions (do not revisit without the user)

1. **Chunk = round.** Root node = the initial-deal state where every seat + dealer hold their two cards (dealt up front exactly like `Game::deal()`, see §6). Each seat's starting pair is therefore *fixed across all branches*.
2. **Faithful deal order, shared monotonically-advancing deck cursor.** The shoe is one immutable card sequence. Within a round the cursor advances only on cards *drawn during play*; it is *shared across all seats on a path*. So a branch where seat 0 draws more cards shifts what seat 1 (and the dealer) receive downstream, while each seat's starting pair stays put. Siblings that consume different card counts genuinely diverge below their split point.
3. **Enumerate ALL legal actions at every node.** The continuing-hit branch plus complementary stand/double/surrender branches all exist as edges. After the first hit, only H/S remain (can't double/surrender/split). The edge matching `BasicStrategy::getRecommendedAction(...)` is labeled (`isBasicStrategy=true`).
4. **Seat play is serialized into the path.** Seat 0's subtree resolves, then seat 1 "initiates the next branch" from wherever the cursor left off, … through the last seat, then the dealer completes each line. A root-to-leaf path = one full round where each seat independently chose an action line; the leaf records that combination's per-seat result.
5. **Splits add sibling hands under one node** (RoundEngine semantics: a split deals one forced card to each new hand, then each plays out). Re-splits recurse up to `MAX_SPLIT_HANDS`. Split-aces get exactly one card then stand unless re-split is allowed.
6. **Whole shoe = forest of chunk-trees.** Each chunk is seeded on the immutable tail beginning at the cursor left by the previous round's *actual (basic-strategy) resolution*, so outcomes correspond to real rounds.
7. **Bust / 21 / stand / double-forced-card / surrender end a hand early** — this pruning is what keeps the tree tractable.

---

## 2. Reuse vs. reimplement

**Reuse the real classes** (already compiled into both targets):
- `Card` (`include/Card.hpp`, `src/Card.cpp`): `getBlackjackValue()`, `isAce()`.
- `Hand` (`include/Hand.hpp`, `src/Hand.cpp`): `getValue()`, `isSoft()`, `isBust()`, `isBlackjack()`, `canSplit()`, `canDouble()`, `size()`, `getCard(i)`, `addCard()`, `takeLastCard()`, and flag setters/getters (`setFinished/isFinished`, `setSplit`, `isSplit`, `setDoubledDown`, `isSplitAce`).
- `BasicStrategy::getRecommendedAction(...)` (`include/BasicStrategy.hpp`): returns the single prescribed action letter. **Call this identically to label edges.**
- `Constants` (`include/Constants.hpp`): `NUM_DECKS=6`, `DEALER_STAND=17`, `MAX_SPLIT_HANDS=4`, `DOUBLE_AFTER_SPLIT_ALLOWED=true`, `RESPLIT_ACES_ALLOWED=true`, `RESPLIT_TENS_ALLOWED` (inline var), betting bounds.

**Reimplement a focused simulator** (NOT the real `Game`):
- Do **not** drive `Game`. Its per-round side effects (Kelly betting, Hi-Lo counting, HUD, timers, round-state machine, `activePlayerIndex`) make parallel independent replays messy. Instead build a small simulator reusing only `Card`/`Hand`/`Constants` + the strategy lookup and **mirroring** the action rules from `src/Game.cpp` (hit/stand/doubleDown/split/surrender) and `src/RoundEngine.cpp` (split-hand dealing). See §4.
- Own an immutable `const std::vector<Card>&` shoe sequence and a per-path integer cursor; drawing = `shoe[cursor++]`. This sidesteps `Deck`'s shuffle/cut-card logic. Integration with a real played shoe (via `Deck::getPreviousShoeRecord()`/`activeShoeRecord()`) is optional/later.

---

## 3. Exact strategy-letter → action mapping (for labeling)
From `src/BasicStrategy.cpp:getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue)` — mirror this exact order; it returns ONE of `{H,S,D,P,R}`:

1. If `dealerUpValue` not in [2,11] → `value>=17 ? S : H`.
2. **Soft** (`hasAce && 13<=value<=20`): `SOFT[value-13][dealerIdx]`; `'Y'→H`, `'M'→S`, else return the letter (H/S/D). Out-of-range → `value>=17?S:H`.
3. **Pair** (`isPair`): row = pairValue==11→0, ==10→1, else `11-pairValue`; `PAIRS[row][dealerIdx]`: `'Y'→P`, `'M'→(DAS? P : H)`, `'N'` falls through to hard/surrender logic.
4. **Surrender** (only 15/16): if `value==15 && SURRENDER[0][d]!=' '` → return it; if `value==16 && SURRENDER[1][d!=' '` → `'R'`.
5. **Hard** (`8<=value<=17`): `HARD[value-8][dealerIdx]`.
6. Default: `value>=17 ? S : H`.

`dealerIdx = dealerUpValue-2`. Soft check precedes pair (a pair of aces = value 12, skipped by soft range → pair branch). Because this returns exactly one letter, **the basic-strategy edge at each node is uniquely that letter among the legal actions.**


---

## 4. Exact rule mirror (legal-action enumeration + application)
Mirror these from `src/Game.cpp`/`src/RoundEngine.cpp`. These define which edges exist and their outcomes.

**Hand predicates (`Hand.cpp`):**
- `canSplit()` = `size()==2 && cards[0].getBlackjackValue()==cards[1].getBlackjackValue()`.
- `canDouble()` = `size()==2 && !doubledDown`. (Game adds extra guards below.)
- `isSoft()`, `getValue()`, `isBust()` per `Hand.cpp` (aces re-counted).

**Action legality & application (Game-level guards):**
- **HIT**: legal whenever hand not finished and can still draw. On split-ace with no more cards allowed → forced stand (finish, log 'S'); else draw one card (`cursor++`), then finish if `isBust()` or `value==21`. Edge code `H`; the drawn card advances the cursor.
- **STAND**: legal whenever hand not finished and value<21. Finish only; no card drawn; edge `S`.
- **DOUBLE**: legal iff `!finished && !isSplitAce && canDouble` (i.e. 2 cards, not a split ace). Draw exactly one forced card (`cursor++`), then the hand is DONE. Edge `D`; terminal leaf (DOUBLE_END).
- **SURRENDER**: legal iff `!finished && canDouble` (2 cards, not finished). Finish immediately; no card drawn; half bet lost. Edge `R`.
  - *Knob default:* offer surrender on any first-two-card hand that is mechanically legal (matches Game mechanics), even when strategy doesn't recommend it — this makes it a "full action tree." The basic-strategy label lands on 'R' only for hard 15/16 vs the SURRENDER-table cards. **Alternative** (flip one flag): offer surrender ONLY where `getRecommendedAction` returns 'R'. Documented so it's a one-line change if the user prefers that.
- **SPLIT**: legal iff `!finished && hands.size()<MAX_SPLIT_HANDS && canSplit && !(isSplit && cards[0].value==10 && !RESPLIT_TENS_ALLOWED)`.
  - Apply per RoundEngine: move one pair card into a new hand (`takeLastCard`), both hands now size 1 carrying the pair value `v`; **deal exactly one forced card to each split hand from the deck in sequence** (consumes `nHands` cards at cursor); flag each `setSplit(true, v)`. For split-aces: after its one card it stands unless re-split is allowed (`RESPLIT_ACES_ALLOWED` and a free slot). Each resulting split hand becomes its OWN decision subtree (can hit/stand/double-if-size2/re-split-if-pair-and-allowed). After all sibling hands resolve, the seat resolves → advance to next seat.
  - Re-split: handled by recursion when `walkActiveSeat` is called on a split hand that is again a pair under the re-split rule.

**Dealer completion (`Game::dealerTurn`, `HandEvaluator::evaluateHands`):** after all seats resolve along a path, the dealer draws while `value<17 || (value==17 && isSoft)` (H17), stands otherwise, then settles per seat via `Player::settle(...)`. This is deterministic per path; represent it as a short deterministic sub-chain ending in a terminal leaf carrying each seat's result (`WIN`/`LOSS`/`PUSH`/`SURRENDER`/`BLACKJACK`).

---

## 5. Data model — `ActionTree.hpp`

```cpp
enum class Outcome { DealRoot, HitContinue, Stand, Bust, TwentyOne, Blackjack,
                     Surrender, DoubleEnd, SplitSiblingStart, DealerResult }; // terminal: DealerResult

struct TreeNode {
    int nodeId = 0;
    int seatIndex = 0;          // -1 for dealer nodes
    bool isDealerNode = false;

    char decision = 'D';        // edge taken to reach this node: D(deal-root)/H/S/D(double)/P/R/X(dealer)
    bool isBasicStrategy = false;   // true => this edge is the prescribed basic-strategy line (the label)

    std::vector<Card> cards;    // cards at this node (dealer: up + hole as known; then draws)
    int handValue = 0;
    bool isSoft = false;

    Outcome outcome = Outcome::HitContinue;
    // For terminal DealerResult leaves, per-seat settlement of THIS round:
    std::vector<int> seatResults;   // index by seatIndex in the chunk's seat list

    int deckCursorAtNode = 0;     // absolute position in the shoe sequence at this node
    std::vector<TreeNode> children;
};

struct SeatInfo { int index; std::string name; std::vector<Card> startCards; };
struct Chunk {
    int roundIndex = 0;
    int deckStartCursor = 0;      // tail position this chunk's initial deal reads from
    std::vector<SeatInfo> seats;  // seat identity + fixed starting pair (unchanged across branches)
    TreeNode root;                // DealRoot node holding all initial hands
};

class ActionTree {   // owns forest, holds the immutable shoe sequence for a run
public:
    explicit ActionTree(std::vector<Card> shoeSequence);
    std::vector<Chunk> buildShoeForest();

    std::string toFlatText() const;      // PRIMARY save format: flat pre-order records
    std::string toAscii(int maxDepth = -1) const;   // human-readable preview (-1 = full)
    std::string toJson() const;          // universal sharing / future viz input
    static ActionTree fromFlatText(const std::string&);

    struct Spine { int roundIndex; std::vector<int> nodeIds; };
    std::vector<Spine> extractBasicStrategySpines() const;

    void summarize(std::size_t& outNodes, std::size_t& outLeaves, int& outMaxDepth) const;
private:
    std::vector<Card> shoe_;              // immutable input sequence for this run
    std::vector<Chunk> chunks_;
};
```
Design notes for the future visualizer: `nodeId`, `decision`, `isBasicStrategy`, `seatIndex`, `outcome` are exactly what a viz needs to color nodes (by seat + outcome), highlight edges (basic-strategy flag), and follow the optimal line (`extractBasicStrategySpines`). No geometry stored.


---

## 6. Generator algorithm (pseudocode — implement faithfully)

A DFS over decision points. A "state" = `(activeSeatIndex, per-seat hand lists, deckPos)`. Each **node** = one seat's decision point; its children = that seat's legal actions. Resolving an edge either continues the same seat (hit/double-card-drawn), advances to the next seat (stand/surrender/terminal double), injects split-sibling subtrees, or — after the last seat — builds the dealer branch.

```
buildShoeForest():
  cursor = 0
  while cursor + INITIAL_DEAL_CARDS <= shoe_.size():
     start = cursor; advance cursor over the initial-deal cards (same order/counts as Game::deal()).
     rootState = makeInitialDeal(shoe_ from start)   // every seat 2 cards; dealer up+down known
     chunk = Chunk{ roundIndex, deckStartCursor=start, seats=..., root = DealRoot(node) }
     cursorAfterRealRound = buildRound(chunk.root, rootState, cursor)   // returns where the REAL (basic-strategy) resolution ends
     chunks_.push_back(chunk)
     cursor = cursorAfterRealRound        // next chunk begins exactly where real play left off

buildRound(rootNode, state, deckPos):
  advanceCursorForRealPlay(state, deckPos)   // single-pass basic-strategy replay used ONLY to compute the
                                             // cursor offset for the NEXT chunk (does not mutate enumerated tree)
  walkActiveSeat(rootNode, state, deckPos)   // enumerates the FULL tree; threads real cursors per path
  return <deckPos after the basic-strategy spine reached dealer finish>

walkActiveSeat(node, state, deckPos):     // node is a decision point for activeSeat's active hand
  seat = state.seats[state.activeSeat]
  actions = legalActions(seat.hand, dealerUpCard)      // {H,S,D,P,R} ∩ legal; emit in canonical order H,S,D,P,R
  for (a in actions):
     child = makeChild(node, seat, a, deckPos)        // applies ONE action; returns child + newHand + newCursor + terminal?
     child.isBasicStrategy = (a == getRecommendedAction(...))
     if a==HIT and hand still active:                 // hit drew a card; same seat continues
         walkActiveSeat(child, state, newCursor)       // recurse SAME seat with bigger hand
     elif a in {STAND,SURRENDER} or DOUBLE-terminal:
         if activeSeat is last seat:
             buildDealerBranch(node=child-or-node, state, newCursor)   // completes this line to a result leaf
         else:
             activeSeat++                              // next player initiates the next branch
             walkActiveSeat(child, state, newCursor)     // child becomes that seat's first decision node
     elif a==SPLIT:
         expandSplitSiblings(node, state, deckPos)     // sibling-hand subtrees under `node`, then resolve seat
  return

expandSplitSiblings(node, state, deckPos):
  h = active hand (pair), v = pair value, p = deckPos
  move one pair card into a new hand; nHands split hands now size 1 with value v
  deal one forced card to each split hand from shoe_ in sequence: cursor += nHands   // RoundEngine semantics
  for each split hand h_i (0..nHands-1):              // played sequentially, sharing the advancing cursor
     child = makeChild(node, h_i, DEAL-as-start)      // sibling subtree root
     if h_i is a split ace and not resplittable: mark leaf (forced stand)
     else: walkActiveSeat(child, stateFor(h_i), cursorAfterSplitCards)  // can re-split / double(size2)/hit/stand
  <after all siblings resolved> advance activeSeat to next seat
```

Correctness notes: children are enumerated in canonical order and the tree rebuilds deterministically from `shoe_`, so output bytes are reproducible for a given input. The dealer branch is deterministic (one chain → one result leaf per path) but still created as nodes for structural uniformity with the viz. Split-hand cursor consumption must match RoundEngine: the forced card to each split hand comes BEFORE those hands act, and sibling hands play in index order drawing sequentially.


---

## 7. Serialization schemas

### 7a. Flat pre-order text — PRIMARY (compact, one-pass parseable)
File layout:
```
FOREST <nChunks>
CHUNK <chunkIndex> <deckStartCursor>
<node record lines in DFS pre-order; each node's nChildren records follow immediately>
CHUNK <...> ...
```
Node record = one line, pipe-delimited, no spaces (fields left→right):
`nodeId|seatIndex|isDealer|decision|isBS|handValue|isSoft|cards|deckCursor|nChildren|outcomeCode|results`
- `decision`: `DEAL|H|S|D|P|R|X` (`X`=dealer nodes; `DEAL`=chunk root).
- `isBS`: `0/1`.
- `cards`: comma-separated bj-values in order, e.g. `2,3`; dealer at root includes up+hole e.g. `7,10`.
- `nChildren`: explicit count — the pre-order delimiter; a parser recurses exactly this many child records.
- `results` (only on `X`/DealerResult leaves): `s0=WIN,s1=LOSS,...` indexed by seat position in the chunk's seat list; empty otherwise.
Optional gzip at rest (`.gz`) — large trees compress very well (repeated decision letters). Provide `.gz` as a save option, not required for correctness.

### 7b. JSON export — universal sharing / future viz input
Nested objects mirroring `TreeNode`; forest = array of chunks:
```json
{ "shoe": {"decks":6},
  "chunks":[ { "roundIndex":0,"deckStartCursor":24,
    "seats":[{"index":0,"name":"William","startCards":[2,3]}],
    "root":{"nodeId":0,"seatIndex":-1,"isDealerNode":false,"decision":"DEAL",
            "cards":[...],"outcome":"DealRoot",
            "children":[ {…edges…, "children":[…]} ] } } ] }
```
Every node carries: `nodeId, seatIndex, isDealerNode, decision, isBasicStrategy, cards, handValue, isSoft, outcome, deckCursorAtNode, children`. Dealer leaves additionally carry `seatResults:[...]`. Keep field names identical to the model so a viz consumes JSON directly.

### 7c. ASCII visualization — human preview (NOT the saved format)
- Indentation by depth with connectors `├─` / `└─` (last child uses `└─`).
- Node label: `[seat i] hand=<cards> (<value><soft?>) | edge=<H|S|D|P|R>* ` where trailing `*` marks the basic-strategy edge; terminal lines append outcome + results, e.g. `=> Dealer 19 — seat0 WIN`.
- Splits shown under a `┃ SPLIT (n hands)` marker line with each sibling as its own indented block.
- `maxDepth` param: when set, print `…(N more lines)` past the depth so big trees preview cheaply while flat/JSON keep full data.
- Deterministic child order = canonical H,S,D,P,R (legal only), matching flat output ordering.


---

## 8. Testing plan (`tests/test_action_tree.cpp`, added to `Blackjack2Tests`)
1. **Unit (no Game):** `legalActions` returns expected sets + correct `isBasicStrategy` letter vs `getRecommendedAction` for representative hard/soft/pair/surrender hands; split fan-out count & `MAX_SPLIT_HANDS` cap; double terminal after exactly one card; surrender only on 2-card hands; flat round-trip parse reproduces the tree byte-identically; JSON round-trip reproduces the tree.
2. **Cross-check vs real Game (locks the rule mirror):** for several injected shoes (`Game::setDeck`), run a headless `Game` with `npcTakeTurn` to obtain actual per-hand action sequences + settlement results; assert my generator's basic-strategy spine reproduces those exact actions, outcomes, and card-consumption. Also assert tree leaves' terminal results match Game's for the prescribed path.
3. **Size measurement:** build a shoe, CHECK node/leaf/max-depth counts are within sane bounds and print via `summarize()` so you can see how large it gets before adding limits.
4. **Determinism:** same input sequence → identical flat output across two runs.

## 9. CMake wiring
Add `src/ActionTree.cpp` to BOTH `target_sources(Blackjack2 PRIVATE …)` and the `Blackjack2Tests` sources list in `CMakeLists.txt`. Headers go in `include/`. No new SFML link required (module is pure logic). The test already links SFML via `Blackjack2Tests`, so it can construct a headless `Game` for §8.2.

## 10. Suggested build order + verification
1. `ActionTree.hpp` — model + serializer declarations (§5, §7).
2. `ActionTree.cpp` — generator/rules-mirror (§4/§6), then flat-text, ASCII, JSON backends, spine extraction, summarize.
3. Add a small demo in the test that prints one chunk's ASCII + JSON so output can be eyeballed.
4. Wire CMake; build; run `ctest`; inspect sample tree output; confirm cross-check passes (this is the fidelity gate).

## 11. Assumptions / what NOT to change
- Do not modify existing game behavior in `Game.cpp`/`RoundEngine.cpp`/`BasicStrategy.cpp` — mirror them, don't refactor.
- `NUM_DECKS=6`, `DEALER_STAND=17` (H17), `MAX_SPLIT_HANDS=4`, `DOUBLE_AFTER_SPLIT_ALLOWED=true`, `RESPLIT_ACES_ALLOWED=true`, `RESPLIT_TENS_ALLOWED=true`.
- The tool is analysis-only; it does not touch bankroll/HUD/timers. Betting and card counting are irrelevant to the tree except that the shoe sequence encodes which cards remain (all the cursor needs).

## 12. Optional future extensions (do NOT build now)
- Replay an actual played shoe via `Deck` records instead of an injected sequence.
- Interactive SFML viewer consuming the JSON/first-child-next-sibling form, highlighting basic-strategy spines per §5.
- Opt-in tree limits: total-leaf cap + sampling, max-path depth, single-round/single-seat mode — driven by numbers from `summarize()` (§8.3).

8. **Output = headless, no graphics.** Flat pre-order text (primary), ASCII preview method, JSON export method. Geometry/positioning stays computed by the future visualizer; never stored in the model.
