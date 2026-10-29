# ACTION PLAN — Blackjack2 feature work

Grounded execution of `.summaries/0.3/FEATURES.md`. Order sequenced by effort + dependency. Each feature: logic-first (headless-tested) THEN UI. Statuses in `CONTEXT.md` §5 are the live tracker.

## Shared setup before features 1-6 (one-time)
- Confirm §6 decisions #1 (counting approach) and #2 (custom-rules scope).
- Decide deliverable shape (§6 #3): new files under `include/`+`src/` with mirrored `tests/*.cpp`, existing module naming.

---

## 7. Sound Effects & Music — Low (~1d)
- Add: `include/SoundManager.{hpp,cpp}`; add sound/music assets under `assets/`.
- Modify: profile volume settings (`PlayerProfile`), thin hooks in `Game`/`GameRenderer`.
- Test: logic layer (queue state) via headless game; one graphics-only test for asset loading.

## 1. Side Bets — Med (2-3d)
- Add fields to `HandOutcome` (`SideBetType`, side-bet wager/payout/result); extend `PlayerStats` counters.
- Modify: `Hand`/`HandEvaluator` evaluation; `Game` betting phase + resolve BEFORE main-hand settlement; `HUDState`; `GameRenderer` HUD.
- Test: headless `setDeck` -> assert exact Perfect-Pairs (10/25/30:1) and 21+3 paytable outcomes. Highest unit-test ROI feature.

## 2. Strategy Trainer Mode — Med (2-3d)
- Add `AppMode::StrategyTrainer`; new `include/StrategyTrainer.{hpp,cpp}` state machine; menu entry.
- Reuse `Game::giveBasicStrategyAdvice()` for grading; generate hands via existing `Deck`/`Hand`.
- Test: feed known dealer-up + hand -> assert correct/incorrect, accuracy %, streaks, timed-session logic.

## 5. Achievement System — Med (2-3d)
- Add: `include/Achievement.{hpp,cpp}` (`Achievement` struct + `AchievementManager`, conditions + progress).
- Modify: hook checks after each hand/shoe in `Game`; persist unlocked set in profile; popup UI via `HUDState`; list view.
- Test: drive headless shoe to trigger "Natural 21"/"Blackjack Streak" etc. -> assert unlock state.

## 3. Custom Game Rules — Med-High (3-4d)
- Add: `include/GameRules.{hpp,cpp}` struct; rules-config UI in `GameRenderer`.
- Modify: `Deck::create()` deck count; `dealerTurn()` soft-17; split/double DAS + resplit aces; surrender types. Thread narrowly-scoped `const GameRules&` into advisor+UI (§6 #2). Save in profile.
- Test: headless — dedicated tests for soft-17 hit/stand, DAS, resplit-aces, mirroring existing `test_split_aces.cpp`/`test_resplit_tens.cpp`.

## 4. Statistics Dashboard — Med-High (3-4d)
- Add: `AppMode::StatsDashboard`; `include/StatsDashboard.{hpp,cpp}`; CSV/JSON export via `Serialization`.
- Read from existing `PlayerStats` + `ShoeAnalytics`; render charts with SFML shapes; tabbed interface.
- Test: feed synthetic round history -> assert computed win-rates, bust/bj frequency, chart data arrays.

## 6. Additional Counting (KO/WongHalves/Omega II) — Med-High (3-4d)
- Extend `BasicStrategy::CountingStyle`; add per-system value tables + true-count handling (KO unbalanced = no TC). (§6 #1: extend dispatch.)
- Modify: `Game`, count display/selector in `HUDState`/`GameRenderer`, profile save. Reuse the existing two-buffer toggle model.
- Test: each system's running/true count on a fixed deck sequence; assert KO stays unbalanced, others balanced.

## 8. Animated Card Dealing — Low-Med (1-2d)
- Add tween helpers in `GameRenderer` only (presentation). No new logic classes; visual smoke check.

## 9. Tournament Mode — Med (2-3d)
- Add: `TournamentConfig` struct + mode; reuse `RoundEngine`; override betting to blind-bet, track chip totals for winner.
- Test: headless shoe with fixed hands -> assert winner = highest chips.

## 10. Multiplayer Networking — High (5-7d)
- Use SFML Network (already linked). Split: network message protocol/state machine (headless-testable) BEFORE netcode UI; host/join lobby, chat, spectator.
- Test protocol logic in the Graphics-only test binary first.

---

## Guardrails for every feature
- Register new tests in `CMakeLists.txt` `Blackjack2Tests`; build+run `ctest` before marking done.
- Keep JSON schema additive; verify an old profile still loads after persistence changes.
- Keep pure logic SFML-Graphics-only; gate Audio/Network behind headless-safe paths.
