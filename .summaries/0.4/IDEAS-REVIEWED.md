# IDEAS — reviewed against the tree after T1 / B21 / B9 (2026-11-12)

**What this document is.** `IDEAS.md` is the proposal record and is left untouched. This is a **review layer** over
it: it re-verifies every premise and `file:line` reference against the tree at HEAD `25cd750`+ (suite **345/345**),
records what the T1/B21/B9 work did to each idea, and develops the ideas on the new footing. `IDEAS.md`'s own closing
line asks for exactly this — "re-verify every file:line reference above against the tree at that time."

> **Re-verified 2026-11-12 at HEAD `cfdf718` (suite 395/395).** §F's citations were re-checked against source and
> **eight rows were stale** — the `Player("William")` row was flatly false (B23 removed all four sites), and the
> `GameRecord.hpp` / `Serialization.hpp` / `Game.cpp` / `SessionLifecycle.cpp` / `Deck.cpp` line numbers had all
> drifted under the Daily-Challenge and per-mode-history (MS1) work. §F is now corrected and carries the MS1 rows.
> **MS1 is the change this review had not absorbed:** career `longHistory` is now Regular-only, with Daily/tournament
> shoes in their own capped vectors and `ShoeRecord::mode` as the router key. Read §B.3, §C #6/#7/#9/#10 and §E for
> its effect before picking a feature — the headline is that **any surface reading shoe history now has to say which
> mode it is reading**, because `getLongHistory()` alone silently omits Daily and tournament play.

**Headline: the ideas got cheaper, and one of them is half-built.** B21 shipped the seed plumbing that #4 lists as
its implementation work; #1's hardest component (exact deck reconstruction) now exists **and is tested**; and every
hard dependency the document lists — B1, B2, B4, B5, B8, B13, B14, B15, B16, U3, U5, U7, T2 — is **Resolved**. The
dependency columns are effectively cleared. What still blocks the big features is one unbuilt seam
(`Game::persistsProgress()`) and one new constraint the seed introduced (§B.1).

---

## A. Delta: what IDEAS.md asserts vs what the tree does now

| # | IDEAS.md asserts | Current truth | Effect |
|---|---|---|---|
| — | "There are currently two live copies of `Serialization.hpp` (`src/` and `include/`)" (cross-cutting note) | **False.** `src/Serialization.hpp` does not exist; B15 landed. One copy: `include/Serialization.hpp` | The "any new persisted field must land after B15" gate is **gone**; the note is stale and must not gate anything |
| 1 | "`ShoeRecord::initialDeck` is a verbatim copy of `deck.getCards()`… and it is already serialised" | **Deleted.** Replaced by `shuffleSeed` + `shoeDecks` + `shoeCardCount` (`GameRecord.hpp:178-193`) | #1's premise survives and **improves**: the shoe is reconstructible from ~16 bytes instead of 792 KB, proven card-for-card by #13–#16 |
| 1 | "Do not reuse `reconstructDeckAtRound()`… write the correct one and delete or fix the old one in the same commit" | **Already deleted** (B16, `5f81d48`); zero references remain | Instruction obsolete. The correct helper is now *seed reconstruction*, which exists |
| 2 | "T1 is a hard prerequisite, not a nice-to-have" (per-round disk writes) | T1 **Resolved**, but only cadence + size: `AUTO_SAVE_EVERY_HANDS = 5` (`Constants.hpp:184`) gates the round-end save (`HandEvaluator.cpp:302-308`) | A 50,000-hand run writes **10,000** times, not 50,000. Better, not solved. The structural fix is still `persistsProgress()` |
| 2 | "B8… fix B8 first or the Lab will crash" and "B9 (you want logs when a long run dies)" | Both **Resolved** — `Deck::draw()` guards both empty cases (`Deck.cpp:137-155`); B9 closed 2026-11-12 | Dependencies cleared |
| 4 | "`Deck::shuffle()` reseeds from `std::random_device`… so the recorded seed can be read back but never replayed. That gap… is the whole feature" | **Closed.** `Deck::shuffleWithSeed()` exists (B18) and production now **records** the seed it used (B21) | **Half of #4 is shipped.** What remains is encode/decode + checksum + Daily, not seed plumbing |
| 4 | Option (b): "Encode the deck order itself. `initialDeck` is already recorded" | The array is **gone**; encoding an order now means *reconstruct from the seed, then encode* | The (a)/(b) choice **flips** — (a) is now the natural path. See §B.1: (a) carries a portability hazard (b) does not |
| 6 | "`bankrollAtShoeEnd` defaults to 0 for pre-existing profiles… the one real trap in an otherwise trivial feature" | **Moot.** History was wiped; every shoe on disk is new-schema with a real value | #6 loses its only trap. It is now a clean S |
| 6 | `HUDState::bankrollSeries` / `edgeSeries` are single series | U12 added **scope switching**: `activeBankrollSeries()` / `activeEdgeSeries()` return session **or** per-shoe series (`HUD.hpp:96-100`) | #6 gains a real feature free of charge — a session-spanning career curve already exists as data |
| 9 | "`ShoeRecord` already contains a complete, replayable game" | True, **because of the seed**, not the deck array | Premise survives; the mechanism changed |
| all | "additive, defaults" for every new profile field | The additive contract was **deliberately broken** by the developer on 2026-11-12 | New fields may now be **required**. Freedom gained — and a policy decision (§G.1) |
| — (new since this review) | Nothing in `IDEAS.md` anticipates **mode**: history is one list, and every idea reads it as if it were the whole history | **MS1, 2026-11-12:** career `longHistory` is Regular-only; Daily/tournament shoes live in their own capped vectors (`PlayerProfile.hpp:72-87`), tagged by `ShoeRecord::mode`, routed by `addShoeRecord()` | **Every history-reading idea gains a mandatory mode choice**: #6 (chart), #3 (leak report), #9 (ghost fixture), #7 (challenges over per-mode aggregates), #4 (a shoe code that should name its mode). Reuse `include/HistoryBrowser.hpp` rather than inventing a second source model |
| all | Test IDs cited throughout | Inserting tests renumbers later IDs, **non-uniformly** | Provenance, not addresses. Search by test **name** |

**Found during this review — two defects, both fixed today (suite 345/345):**

- **A regression the seed schema introduced.** `reshuffleWithRemaining()` shuffles the full deck **minus every card
  already dealt**, so a continuation shoe is *smaller* than `shoeDecks*52+1`. Rebuilding it with
  `create(shoeDecks) + shuffleWithSeed(seed)` yields a full 313-card shoe — wrong size, and Replay's
  `cardsRemaining`/`trueCount` were inflated for every continuation shoe. The pre-B21 array captured the real shoe and
  got this right for free. Fixed by storing `shoeCardCount`; locked by **#17** `[Continuation]`.
- **B22, a second zombie field of the same class as B21.** `ShoeRecord::isContinuation` was declared, set once
  (`Game.cpp:1547`), **never read anywhere, and not serialized at all** — a saved continuation shoe came back marked
  as an ordinary one. Now serialized both ways. It matters because it is the *only* marker of the reconstruction
  boundary that #1, #4 and #9 all need.

---

## B. Three constraints the seed creates — every idea that touches a shoe must respect these

### B.1 A seed is only portable inside one standard library — and the window to fix that is open *now*

`Deck` shuffles with `std::shuffle(cards, rng)` at `Deck.cpp:55` and `:111`, drawing from `std::mt19937`. The swap
sequence is specified by the standard, but the distribution it draws indices through
(`std::uniform_int_distribution`) is **implementation-defined** — libstdc++, libc++ and MSVC are not obliged to agree.
So a shoe code is reproducible across builds of the *same* standard library, and not guaranteed across different ones.

`IDEAS.md` flagged this in one sentence under #4 and moved on. It is now load-bearing: the seed is the shipped
mechanism, and #4, #9 and #1's shareable puzzles all assume a code means the same shoe on two machines.

**DONE 2026-11-12, same day.** The two `std::shuffle` calls were replaced with a local `shuffleShoe()` in `Deck.cpp`:
hand-written Fisher-Yates drawing indices straight from `std::mt19937` through `uniformIndex()`, which does its own
**rejection sampling** so the index range is specified by the program rather than by a library distribution. mt19937's
output is fixed by the standard, so a seed now means the same shoe on every platform. The window was still open when
this landed — history had been wiped hours earlier, so no existing shoe order needed preserving. Frozen by **#18**
`[Golden]`, which pins the first eight cards and the cut-card index for seed `0xC0FFEE`; a future change to the order
fails loudly instead of silently redefining every seed ever written.

**Why now:** shoe order only has to be preserved for shoes already on disk. History was wiped today, so there is
**nothing to invalidate**. Change the shuffle after the player has accumulated a hundred shoes and every pre-change
seed silently describes a different shoe — the same class of silent corruption as B21, discovered much later. This is a
now-or-never window that costs almost nothing today.

Test surface: a fixed seed reproduces a hard-coded expected card order, so a standard-library change can no longer move
a shoe order silently.

### B.2 The seed describes a *fresh* shoe. A continuation shoe is a different object.

For `isContinuation == true`, `create(shoeDecks) + shuffleWithSeed(seed)` rebuilds a shoe that was **never dealt** —
#17 asserts the reconstructed order differs from the real one. Consequences, stated once so each idea does not
rediscover them:

- **Replay** is correct: it takes `totalCards` from `shoeCardCount`, not from the reconstruction.
- **#1 forks** must refuse to fork past a reshuffle. The doc already recommends refusing; the seed makes it
  *necessary*, not merely tidy.
- **#4 codes** must refuse to encode continuation shoes in v1 (same).
- **#9 ghosts** must not race on a continuation shoe.
- The full fix, when a feature needs exact order past a reshuffle: add `reshuffleWithRemainingWithSeed(played, seed)`,
  mirroring the B18 `shuffle`/`shuffleWithSeed` seam. The dealt set is derivable from the record's own rounds, so a
  continuation shoe *is* faithfully reconstructible — it needs that one seam. Do not build it speculatively; build it
  with the first feature that needs it.

### B.3 Five features need "a game that does not write to the profile." That seam does not exist.

`grep -rn persistsProgress include src` → **zero hits**. `IDEAS.md`'s closing section is right that this is the pattern
worth noticing: #1, #2, #4, #8 and #10 all require it, and `HandEvaluator` still saves on a cadence (`:302-308`). The
only non-persisting gate today is B1's tournament fix, and it is specific to tournaments.

This is the highest-leverage prerequisite in the document and it is small: one predicate on `Game`, consulted by the
save path, the achievements path and the stats path. Build it once, before the first of those five, and all five
inherit the hardest correctness rule for free. Every one of those five features should carry the same test: **a run
leaves the saved profile byte-identical.**

**MS1 (2026-11-12) shipped the sibling seam, and it is the half of this the codebase now has.** Two mechanisms exist
and neither is the predicate:
- **`setPersistable(false)`** on the seat — a *blanket* gate on `saveProfile()`. It is what makes the Daily and
  tournament safe, and it is coarse: it turns off ALL writes, so a session that wants to persist *one* thing cannot
  use it alone.
- **The narrow commit** — `commitDailyResult()` / `commitTournamentShoe()` (`SessionLifecycle.cpp:400-437`): read the
  career file, touch one slice, write it back. Deliberately not a seat save. This is the "record exactly one thing and
  nothing else" pattern, and it is now proven in both directions by #314 and the `[Daily]` suite.

`persistsProgress()` is the generalisation of the first, and the features that need it are the ones running on the
**live** `Game` (in-game drills, sim, fork, tutorial) — sessions that build no table of their own, so an ephemeral
seat is not available to them. That is why the Daily did not need it and why it is still the top prerequisite. When it
is built, the byte-identical test is the one to write first, and note the trap MS1 exposed: the profile's own
`currentHistory` is the *career's* in-progress shoe, so a non-career session must never let it into its records
(that is why `addModeShoe` takes the caller's rounds verbatim while `addCareerShoe` overwrites them).

---

## C. Per-idea impact, and where each one should go now

### #1 Decision Fork — **materially cheaper, and its hardest part is done**

The doc's whole pitch rested on "the app already writes the entire shoe order to disk." That sentence is now false and
the feature is *better* off: the shoe is reconstructible from `shoeDecks` + `shuffleSeed`, and #13–#16 prove the
reconstruction is exact, cut card included. The `deckStateAtDeal(shoe, n)` helper the sketch calls for is now
"reconstruct, then drop the first *n* from the **back**" — and the back is right, because `Deck::draw()` pops
`cards.back()` (`Deck.cpp:140-141`).

Three things the sketch must add, all discovered by reading the engine this time:
- **The cut card consumes two cards.** When `draw()` turns the cut card it immediately pops a second card
  (`Deck.cpp:143-152`). A fork reconstruction that replays one card per deal lands one card out and prints a
  confidently wrong verdict. This is the doc's own worst gotcha, and it is confirmed in the code.
- **Refuse past a reshuffle** (§B.2), gated on `isContinuation` — which is only trustworthy now that B22 persists it.
- **`persistsProgress()` is a prerequisite, not a nicety** (§B.3). The doc asks for the "never authoritative" rule to
  be structural and locked by a test; that is exactly the shared seam, and it should be built here if #1 goes first.

Effort stays **L**, but the riskiest third of it is already shipped and tested.

### #2 Simulation Lab — unblocked except one seam; the write problem is 5× better, not gone

B8 and B9 are Resolved. Determinism is available today: `shuffleWithSeed` exists and B18 already uses it to pin
sessions. The remaining blocker is `persistsProgress()` — with `AUTO_SAVE_EVERY_HANDS = 5` a 50k-hand run still writes
the profile 10,000 times, which is a stall, not a crash.

Two developments worth adding to the design:
- **The counterfactual is now nearly free.** "Replay your own career under flat betting" needs, per shoe, exactly
  `(shoeDecks, shuffleSeed, shoeCardCount, isContinuation)` — 16 bytes, already stored. The doc calls it "a three-hour
  feature once the Lab exists"; the seed is why it is genuinely cheap, and it is the most persuasive screen in the
  whole list.
- **Seed the Lab with the app's own RNG seam, not a parallel one.** Reuse `shuffleWithSeed`; do not introduce a second
  engine, or the Lab measures a shoe the game cannot produce.

Effort stays **L**. Build `persistsProgress()` first and it becomes a thin wrapper, as advertised.

### #3 Leak Report — unchanged, and now the best value-to-effort on the list

Nothing in the T1/B21 work touches it. Verified premises: `DeviationRecord` fields exist, `recentDeviations` is capped
at **10** and the cap is enforced by erasing the front (`Game.cpp:1108-1110`), so the doc's warning that a career leak
ledger cannot be built from it is **correct and current**. Its recommendation stands: derive the career ledger from
`longHistory` at read time.

One sharpening: `HandEvaluator.cpp:194` **clears** `recentDeviations` at a boundary — so the last-ten list is not even
stable across shoes. Another reason to derive from the record rather than read the live list.

Effort **M**. Unblocked.

### #4 Shoe Codes + Daily — **the Daily half shipped 2026-11-12; the code half is what is left**

The doc's central observation ("a seed that is stored but not usable — that gap is the whole feature") is closed.
`shuffleWithSeed` exists; production records the seed; the record round-trips it; and the shuffle is now
program-defined, so a seed means the same shoe on every machine (§B.1).

**Shipped 2026-11-12 — the Daily.** `include/DailyChallenge.hpp` / `src/DailyChallenge.cpp`: FNV-1a 64-bit
date→seed written out in full (the salt `Daily::kSeedSalt` is the bump point), civil-date arithmetic written out
in full, the fixed rule set, and the one-attempt gate as a pure `Daily::gateFor`. `PlayerProfile` carries seven
additive Daily fields plus the streak state machine; `SessionLifecycle::startDailySession()` builds the seeded
shoe on an **ephemeral seat**; `Game::finishDailyShoe()` books the run and deals no further shoe. On screen: a
**Daily Challenge** main-menu row showing the streak, and a banner over the play screen. The doc's cut-card
gotcha is satisfied by construction, and continuation shoes are **recorded, not disqualifying** — a Daily result
carries `isContinuation` so comparisons stay honest, which is the better call for a solo app where the shoe
running out is not the player's fault.

What is left, and it is smaller than the doc estimated:
- `ShoeCode` encode/decode + checksum. A code is now tiny: deck count + a 64-bit seed, base32, plus a checksum —
  roughly 12–14 characters, not "a few hundred bytes." **A Daily needs no code at all: the date *is* the seed.**
- **The Daily's comparison half.** "Your result vs the original result" has no server to rank against, so it means
  a ghost (#9) racing the same shoe. What shipped is the shoe, the gate, the streak and practice — not the
  comparison. This is the part that remains a product decision, and the doc's own honesty about it ("the weakest
  justification here") still holds.
- **Forks and deck-preview must be disabled inside a Daily.** The app can reconstruct any shoe from a seed, so the
  Daily is a solved puzzle to anyone who knows that. Label it study. §B.2's continuation guard applies to #1, #4
  and #9, and the Daily records the flag rather than refusing.

Effort for the remaining code half stays **S**. `persistsProgress()` (§B.3) was **not** needed for the Daily — the
ephemeral seat covers it — and is still owed to the features that run on the *live* `Game`.

### #5 Bet Spread Designer — unchanged, still gated on #2

Verified: `calculateKellyBet`, `calculateRekoBet` and `estimateEdgePercent` all exist as the doc says. The B4 count-scale
dependency is **Resolved**, so the doc's warning that the axis scale is "off by up to 19" no longer applies — the axis
can be labelled honestly today. The `BettingHandler` → `BetPolicy` refactor is still the real work, and it is the piece
#2, #5 and #9 all share.

Effort stays **L**, dependency on #2 unchanged.

### #6 Bankroll Chart tab — **now the cleanest first task on the list**

Verified: the series exist (`HUD.hpp:175-176`), and `ComputedStats` already computes peak/max-drawdown. The doc's
"one real trap" — `bankrollAtShoeEnd` reading 0 for pre-existing profiles and rendering a crash-to-zero — is **moot**:
history was wiped, so every stored shoe carries a real value. The trap should stay documented as a *forward* hazard for
any future profile-import feature, not as a live one.

Two upgrades the doc could not have known about:
- **U12 shipped session/shoe scoping.** `activeBankrollSeries()` and `activeEdgeSeries()` already switch between a
  per-shoe and a session-spanning series (`HUD.hpp:96-97`). The career curve #6 wants is partly built; the tab should
  inherit the toggle rather than invent a third scope concept.
- **`bankrollSeries` resets at shoe boundaries** — the doc's sawtooth warning is still exactly right, and U12's
  session series is the existing answer to it.
- **MS1 added a third axis: mode.** Career, Daily and tournament shoes now live in **separate vectors**
  (`PlayerProfile.hpp:72-87`), so a chart over `getLongHistory()` alone silently shows only ordinary play. A mode
  selector is now required, not optional. Do not invent one: `include/HistoryBrowser.hpp` is the pure, tested
  source-switch model (Career / Daily / Tournament, empty-skipping, labelled) and `Replay` already consumes it —
  reuse it and the chart and the browser cannot drift apart. Decide explicitly whether Daily shoes join the career
  curve (they are excluded from career stats by construction, so a merged curve needs its own derivation).

Effort **S**, risk now near zero, and the chart primitives are still the shared win for #2/#5/#9. **Build this first.**

### #7 Achievements → Challenges — unchanged; B2 is Resolved, which *raises* the stakes

B2 (toast never clears) is **Resolved**, so the toast timer exists — the doc's warning that the challenge queue and
B2's timer must be designed together is now partly moot, and the queue is the remaining work. The id-stability rule
("never re-order the existing 12; ids persist through `Achievement::toInt/fromInt`") is **more** important than when
the doc was written, because the project has just demonstrated with B21/B22 how a persisted field that disagrees with
its consumers behaves: silently wrong, discovered late.

The additive-schema note in this idea is superseded (§A, last row): new profile fields may now be required.

**MS1 just created a batch of challenge-able facts that did not exist as data a day ago**, all persisted and read:
per-mode `ModeStats` (`shoesPlayed`, `bestBankroll`, `mostHandsWon`, `bestFinish`), `tournamentsCompleted`, and
`handsWonInShoe(shoe)` as a record-derived per-shoe measure. That is the raw material for "win N hands in one shoe",
"finish a tournament", "reach stage N", "play N Daily shoes" — and the streak (`dailyStreak`) is already a working
streak implementation to model challenges on. Two constraints MS1 also demonstrates: the per-mode vectors are **capped**
(30 / 20 / 20), so a challenge measured over history must be evaluated at the moment the shoe is booked, not scanned
from the window later; and the aggregates are maintained **incrementally** precisely so a peak survives eviction.

### #8 Count-Only Drill Mode — unchanged, and B4 is Resolved

The doc's dependency ("the seed must be right or the drill teaches wrong") is satisfied — B4 is Resolved. The mask-over-
`HUDState` design is confirmed sound: every displayed number is a separate field. Still **S**, still the cheapest real
feature, and it needs `persistsProgress()` (§B.3) for the "drills never touch career stats" rule.

### #9 Ghost Rival — premise survives; add the continuation guard, and the Daily now supplies the fixture

"ShoeRecord already contains a complete, replayable game" is now true via the seed. Add: refuse continuation shoes
(§B.2), and note that a ghost shared with a friend inherits §B.1's portability question. `playerBankrolls` exists as
the doc says. Effort **M**, dependencies (#4, #2's policies, #6's primitives) unchanged.

**MS1 changes the fixture story, and closes half of #4's "comparison" gap.** A Daily shoe is now *recorded* —
`dailyShoes` keeps the full round records, mode-tagged, with the date-derived `shuffleSeed` — so "race the recorded
result of today's Daily" now has stored data behind it. What §11 called the Daily's missing comparison half needed a
ghost to race; the ghost is the remaining half, not the recording. Note the cap: `dailyShoes` holds 30 shoes
(~27 KB each with rounds), so a ghost fixture must be captured, not assumed to still be on disk a year later.

### #10 Onboarding Campaign + name entry — **the defect it closed is now CLOSED; the campaign is what remains**

**This row was stale and is corrected 2026-11-12.** It used to read "Verified: `Player(\"William\")` is hard-coded at
**four** sites … the player has never had a name entry path." Both halves are now done: **U7** shipped the name-entry
screen, and **B23** removed the hard-coding — `grep -rn 'Player("William")' src include` returns **zero** matches, and
the five construction sites pass `Constants::DEFAULT_PLAYER_NAME` with the loading constructor adopting the persisted
name. The "split out the name-entry half as an S" advice is therefore spent: that S is spent, not available.

What remains is the campaign itself. `StrategyTrainer::startSession(deckCount, secondsPerSession)` exists with a
deterministic variant, and `Deck::setCards()` installs an exact order — both premises still confirmed. B4 and U5 being
settled removes the doc's "teaching a number that's about to change" risk. Effort **L**, and it now needs
`persistsProgress()` (§B.3): a tutorial that runs on the live `Game` and writes to the career profile is the exact
failure B1 was, and a scripted tutorial will fire the autosave cadence repeatedly.

---

## D. New ideas the seed makes possible (these did not exist before today)

1. **Every shoe is addressable in ~16 bytes.** A shoe code is deck count + seed + checksum. That is the whole
   foundation of #4, and it makes "send a friend the shoe you just played" a data-formatting task, not a feature.
2. **"Play the shoe that got away."** Pick any recorded shoe and play it again with a different strategy or bet size.
   The app can now re-deal its own history on demand. This is #1's teaching value applied to a whole shoe, and it is
   much cheaper than #1.
3. **Career counterfactual under flat betting** (see #2) — the single most persuasive screen available, and it needs
   only fields that are already stored.
4. **A determinism harness for the whole suite.** B18 pinned one trainer session; the seed seam now generalises it, so
   any test that needs a known shoe can pin it without an OS-RNG flake. Given B18/B20's history of shuffle-related
   flakes, this is worth doing as infrastructure, not as part of a feature.
5. **A new honesty requirement, stated once:** the app can now reproduce any past shoe exactly, and a fork reveals the
   dealer's hole card. That is a **cheating lens**, and `IDEAS.md` says so for #1 only. It now applies to #1, #4, #9 and
   the new items above. Any of them that ships should be labelled as study, and none should be usable in the Daily.

---

## E. Re-ranked build order given today's tree

`IDEAS.md` recommends **#6 → #3 → #1**. That still holds, with two small infrastructure items inserted ahead of the
second feature, because both are cheap today and expensive tomorrow:

| Order | Item | Size | Why here |
|---|---|---|---|
| 1 | **#6 Bankroll Chart** | S | Cheapest real feature; its only trap is gone; the chart primitives are reused by #2, #5 and #9 |
| 2 | **`Game::persistsProgress()`** | S | Not in the list, and it unblocks #1, #2, #4, #8, #10 (§B.3). Build it before the first of them. **The Daily did not need it** — an ephemeral seat covers a session that builds its own table; the predicate is owed to features that run on the *live* `Game` |
| 3 | ~~**Fixed Fisher-Yates in `Deck`**~~ **DONE 2026-11-12** | S | §B.1 shipped the same day, while history was empty. Locked by #18 `[Golden]` |
| 4 | ~~**#4 Daily: date→seed, streak, gate, menu entry**~~ **DONE 2026-11-12** | S–M | Shipped with the main-menu row. Found **B24** (one wrong-typed save field destroyed the whole profile) and made menu navigation index-driven. **MS1 (2026-11-12) then shipped the recording half**: Daily shoes are stored, mode-tagged, with their rounds and the date seed, and are browsable in Replay. Left: `ShoeCode` encode/decode + checksum, and the comparison half (needs #9 — and #9's fixture now exists) |
| 5 | **#4 Shoe Codes** | S | Encode/decode + checksum only. A Daily needs no code — the date *is* the seed. Note: `mode` is now part of a shoe's identity, so a code that claims to describe a shoe should say which mode produced it (a Daily shoe and an ordinary shoe can share a seed) |
| 6 | **#3 Leak Report** | M | Unblocked, self-contained, makes existing tracking legible |
| 7 | **#1 Decision Fork** | L | The riskiest third is already built and tested |
| 8 | **#2 Sim Lab** → **#5**, **#9**, **#8**, **#7**, **#10** | | **#10's name-entry half is DONE** (U7 + B23) — that lift-out is spent, not available. **#8** is the remaining S lift-out, and it is the cheapest test of `persistsProgress()` |

**MS1's net effect on this table:** it does not reorder anything, but it *lowers* #9 (the ghost's fixture is now
recorded data), *raises* #7 (per-mode `ModeStats` + `tournamentsCompleted` are new challenge-able facts), and makes a
mode selector **mandatory** in #6 and any other history-reading surface — a view over `getLongHistory()` alone now
silently omits Daily and tournament play. `persistsProgress()` stays at 2 and is still the only unbuilt prerequisite.

---

## F. Verified reference table (the doc asks for exactly this)

| `IDEAS.md` cites | Current tree |
|---|---|
| `ShoeRecord::initialDeck` (`GameRecord.hpp:166`) | **Removed.** Seed fields at `GameRecord.hpp:202-210` (`shuffleSeed` / `shoeDecks` / `shoeCardCount`); `mode` at `:197`, `isContinuation` at `:191`, `completed` at `:199`, `bankrollAtShoeEnd` at `:200` |
| — (new since this review) | `enum class ShoeMode { Regular, DailyChallenge, Tournament, Multiplayer }` at `GameRecord.hpp:171-185`; `ShoeRecord::mode` is the **router key** — `PlayerProfile::addShoeRecord()` dispatches on it (`src/PlayerProfile.cpp:95-112`), career goes to `addCareerShoe` (`:116-130`), every other mode to `addModeShoe` (`:140-153`) |
| — (new) | Per-mode history + aggregates: `PlayerProfile.hpp:72-87` (`dailyShoes` / `tournamentShoes` / `multiplayerShoes`, `dailyStats` / `tournamentStats` / `multiplayerStats`, `tournamentsCompleted`); `ModeStats` at `GameRecord.hpp` (shoesPlayed / bestBankroll / mostHandsWon / bestFinish); caps `Constants::Daily::MAX_HISTORY_SHOES` = 30, `TournamentHistory`/`MultiplayerHistory` = 20 |
| `Serialization.hpp:170 / 174 / 185` (shoe record) | `include/Serialization.hpp:169-210` (shoe record, now including `"mode"`); per-mode keys written at `:363-372`, read at `:420-430` |
| `src/Game.cpp:276`, `:1349` (deck capture) | Five shoe-creation sites, each stamping seed + deck count + card count + **mode**: `Game.cpp:456` (ordinary), `Game.cpp:1609` (continuation), `SessionLifecycle.cpp:67` (tournament), `:231` (new game), `:368` (Daily) |
| `src/SessionLifecycle.cpp:55`, `:198` | `SessionLifecycle.cpp:67` and `:231` (see above); the Daily/tournament narrow commits are at `:400-417` (`commitDailyResult`) and `:420-437` (`commitTournamentShoe`) |
| `src/Deck.cpp:38-41` (reseed) | `Deck.cpp:83-85` — `lastShuffleSeed = seed` then the fixed `shuffleShoe()` (`:30-35`), inside the shared shoe builder at `:72-86`; `Deck::shuffleWithSeed` at `:67` |
| `src/Deck.cpp:94-97` (continuation reseed) | `Deck.cpp:139-141` — `lastShuffleSeed = std::random_device{}()` inside `reshuffleWithRemaining()` (`:101`). **Confirms §B.2: a continuation shoe's order is not described by the recorded seed.** |
| `src/Deck.cpp:126-132` (cut card consumes an extra card) | `Deck.cpp:145-152` (continuation) and `:90-96` (fresh shoe) — **confirmed real** |
| `HandEvaluator.cpp:297` (`saveProfile()` every round) | `HandEvaluator.cpp:302-308` — now **gated** by `AUTO_SAVE_EVERY_HANDS` |
| `Constants.hpp:184` | unchanged |
| `recentDeviations` = last 10 | `GameRecord.hpp:56`; cap enforced `Game.cpp:1108-1110`; **cleared** at `HandEvaluator.cpp:194` |
| `Player("William")` hard-coded — "four sites" | **STALE — zero sites remain.** B23 closed it 2026-11-12: the construction sites pass `Constants::DEFAULT_PLAYER_NAME` (`SessionLifecycle.cpp:44`, `:214`, `:359`; `Game.cpp:112`, `:834`), and the loading constructor adopts the persisted name. Verified by `grep -rn 'Player("William")' src include` → **no matches** |
| `StrategyTrainer::startSession(deck, seconds)` | `StrategyTrainer.hpp:51`, plus a deterministic variant |
| `calculateKellyBet` / `calculateRekoBet` / `estimateEdgePercent` | `BasicStrategy.hpp:22` / `:39` / `:53` |
| `HUDState::bankrollSeries` / `edgeSeries` | `HUD.hpp:175-176`; **scoped** accessors at `:96-97` |
| `Tab` = Overview / Hands / Profits | `StatsDashboard.hpp:19-23`. **Note:** `Tab` is now ALSO the history-browser source switch inside `Replay`'s history list (`src/Replay.cpp`, `HistoryBrowser.hpp`) — the two do not collide (different AppModes), but a new `Tab` binding must check both |
| `reconstructDeckAtRound()` (B16, "delete it") | **Already deleted** — zero references |
| "two live copies of `Serialization.hpp`" | **False** — `src/` copy deleted by B15 |
| `Game::persistsProgress()` (the shared seam) | **Still does not exist** — re-verified 2026-11-12: `grep -rn persistsProgress include src` → **0 hits**. MS1 did NOT build it (the Daily and tournament are covered by the ephemeral seat + a narrow commit) |

---

## G. Decisions this review hands back

1. ~~**Schema policy going forward.**~~ **ANSWERED 2026-11-12**: future features *will* add profile fields. Policy
   recorded in `CONTEXT.md` §5 — write unconditionally, read with `j.value(key, default)` (a partial-write guard, not
   legacy support), never keep a reader for a field that is no longer written, and bump-and-wipe when a field's
   **meaning** changes rather than adding a translation branch. Every idea that adds a field (#4, #5, #7, #8, #10) can
   proceed on that basis.
2. ~~**Do the shuffle-portability fix now?**~~ **DONE 2026-11-12**, same day — see §B.1 and #18 `[Golden]`.
3. ~~**Is the Daily wanted at all?**~~ **ANSWERED 2026-11-12 — it shipped, and it earned its keep.** The Daily landed
   the same day with the seeded shoe, the gate, the streak and the menu row, and it immediately paid for itself as a
   *test* surface: writing it found **B24** (one wrong-typed save field destroyed the whole profile) and **B25** (a
   session-kind flag set on entry and cleared by nothing). Its recording half — the part this review called missing —
   shipped with MS1. The remaining Daily work is the shoe code and the comparison half, both of which are now cheaper.
4. ~~**Lift name entry out of #10?**~~ **DONE 2026-11-12, and it was a defect, not a feature.** The name-entry screen
   already existed (U7) and worked; the four hard-coded `"William"` sites were what suppressed the identity on screen.
   Removing them exposed **B23** — the loading constructor never synced the seat's display name from the profile, so
   the entered name was persisted but never shown. Fixed; #10 is now purely the scripted tutorial.
5. **Which feature is the phase 0.5 flagship?** — **STILL OPEN, and it is the decision the next round should start
   with.** The list's own answer (#1, the fork) is better positioned than when it was written, because the hardest
   part of it shipped as a side effect of closing a storage bug. The two cheap openers are unchanged: **#6 Bankroll
   Chart** (S, and MS1 made its mode selector mandatory — reuse `HistoryBrowser.hpp`) and
   **`persistsProgress()`** (S, unblocks #1/#2/#4/#8/#10). #3 Leak Report remains the best value-to-effort that needs
   no prerequisite.
6. **Does a "session" mean the same thing in the chart and the history browser?** — **NEW, opened by MS1.** U12's
   session view is *one open play window* (§10 #9a); MS1's per-mode history is *a capped list across windows*. They
   are different axes and both are now in the product. A chart tab that offers "session" and "Daily" as sibling
   options is offering two incommensurable things. Decide the vocabulary before building #6.

---

*This document reviews `IDEAS.md`; it does not schedule anything. If an idea is picked up, give it its own section in
the next phase's `NEXT_STEPS.md` with the same fix/test/done-when structure the defects use, and re-verify the
references in §F again at that time — they were re-checked against source on **2026-11-12 at HEAD `cfdf718`, suite
395/395**, and eight of them had drifted since the original 345/345 pass. The drift rate is the lesson: **a `file:line`
citation in these documents has a half-life of one feature**, so re-verify rather than trust, and prefer naming the
function over the line.*

