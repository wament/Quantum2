# On-screen verification harness (Phase 0.4)

The headless-first contract deliberately leaves rendering untested, and that gap is how **U1** survived two audits by
code reading: reading `drawMenuItem` shows a gold row; only a **screenshot** shows that the gold row under the cursor
is not the row Enter will fire. These scripts close that gap. They run the **real binary** under `Xvfb`, drive it with
synthetic X11 input, and capture PNGs.

## Requirements

`Xvfb`, python-**xlib** (`python3 -c "import Xlib"`), ImageMagick `import`. `xdotool`/`scrot` are **not** installed on
this machine — that is why input is raw XTEST via python-xlib. The app itself is unmodified.

## Run

```bash
Xvfb :97 -screen 0 1600x1024x24 >/dev/null 2>&1 &     # if no X server is running
DISPLAY=:97 python3 tools/visual_check/harness.py     # menu, hover, resize, achievements, pause
DISPLAY=:97 python3 tools/visual_check/check_u6.py    # deal -> hit fan -> settled totals
DISPLAY=:97 python3 tools/visual_check/check_menu.py  # hover marker glyph at two window sizes
DISPLAY=:97 python3 tools/visual_check/check_u12_click.py  # per-graph click toggle + 'G' bulk switch (prints PASS/FAIL)
DISPLAY=:97 python3 tools/visual_check/check_mode_history.py  # MS1: per-mode history browser + mode rows (prints PASS/FAIL)
```

Screenshots land in `tools/visual_check/shots/` (git-ignored). Override with `BJ_SHOTS`, `BJ_REPO`, `BJ_BIN`.

## What each script proves

| Script | Proves |
|---|---|
| `harness.py` | **U1** selected ≠ hovered; **U11** hover follows the *drawn* row after a resize (and the old pixel position highlights nothing); **U3** Achievements list vs. footer at 900×560 and 1600×900; **U2** pause panel at both sizes |
| `check_u6.py` | **U6** 3+ card fan is centred on the seat and shows more than a sliver of each card; hand totals sit on dark pills that stay legible over the felt |
| `check_menu.py` | The hover marker glyph actually renders (see the font rule below) at 1600×900 and 900×560 |
| `check_u12_click.py` | **U12 part 1** per-graph view toggle: one click on the Bankroll pane changes **only** that pane's title strip (measured AE 947 / 0 / 0), and `G` changes the two still-per-shoe panes while leaving the already-session pane at 0. Proves the pane hit-test and the per-pane draw selection, not just the flag |
| `check_mode_history.py` | **MS1** per-mode history: the main-menu rows carry their aggregates (`Daily Challenge (streak 5)`, `Tournament Mode (2 runs, best stage 4)`), the history-list title strip names the source, and `Tab` switches **which history the list shows** — measured on the title strip: Career→Daily 4101, Daily→Tournament 4391, Tournament→Career 3626, and a no-key re-capture of one state diffs **0** (the stability control that makes those non-zeros mean something). The third `Tab` lands on a frame pixel-identical to the first Career frame, proving the cycle wraps rather than drifting |

## A fifth trap: a hand-authored profile must write EVERY nested required key

`check_mode_history.py` seeds a profile with career + Daily + tournament shoes. Its first version wrote
round records with only `timestamp` / `runningCount` / `playerHands` / `dealerCards` — and the app opened
on the **name-entry screen**, because `from_json(RoundRecord&)` reads all six of its fields with `j.at()`
(`trueCount` and `cardsRemaining` were missing), the throw propagated to `PlayerProfile::load`, and the
loader's recovery is *"relocate the file, start fresh"*. The profile was gone before the first frame drew,
so every screenshot in the run showed the same unrelated screen and every crop compared **0**.

That is B24's **stated limitation**, live: the `jsonOr*` guards cover the top-level fields; nested reads
(`RoundRecord`, `HandOutcome`) are still strict. Two consequences for anyone writing a scenario:

1. Seed complete nested records, or the app silently starts from an empty profile.
2. **Check the run log for `Corrupt profile ... relocated` before trusting a black/blank/unexpected
   screenshot.** The symptom here was a plausible screen, not a crash — the same shape as README trap 2.

## Measuring pixels, not just looking at them

`check_u12_click.py` diffs a region with `compare -metric AE`. Three traps it hit while being written, each of which
makes a broken check look like a passing one:

1. **`compare` prints the metric on stderr and exits 1 when the images differ.** Parsing stdout yields nothing, and
   `check=True` throws on a legitimate difference. Parse `stderr`, do not check the return code.
2. **Derive crop regions from the layout constants; never guess them from a screenshot.** Rows guessed by eye put
   pane 0's crop on the bottom edge of its title (so it did change) and panes 1 and 2 into **blank panel background**,
   where they compared **0 forever** — "unchanged", the exact result the assertion wants. The rows are now computed
   from the same `Constants::Hud` values `HUD::leftPanelCharts()` uses.
3. **Prove a crop can go non-zero before trusting that it goes to zero.** A check that has only ever reported 0 is
   indistinguishable from a check pointed at empty space.

Also: standing settles a round but leaves it in `Cleanup` — **`R` is required to reach the next round.** A round loop
without `R` runs exactly one round, so the graphs stay near-empty and a data-dependent assertion is silently vacuous.

## Four lessons that cost time — read before writing a new scenario

1. **Navigate by clicking the row the renderer draws, not by pressing arrows.** `Up`/`Down` **wrap** through the menu,
   so "press Up 20 times" is not deterministic navigation — it landed on Game Rules, then opened it, and the pause-menu
   screenshot came out as the main menu. Clicking also exercises the U11 hit-test on every transition, which is a bonus.
2. **`Escape` at the main menu closes the window.** A script that presses Escape "to go back" kills the app and the next
   screenshot is pure black. Escape at the table *pauses*; Escape during `OfferInsurance` *declines insurance*.
3. **A 0 bet cannot be confirmed, and `OfferInsurance` swallows H/S.** Raise the wager (`Up`) before `Right`, and if the
   dealer's up card is an Ace the round sits in `OfferInsurance` — every `h` press is ignored until it is declined.
   Clicking the pause panel's Resume row is safe in both cases: it only acts in `Paused`, and a left-click in the
   middle play band is inert during `PlayerTurn`.
4. **A string reaching `sf::Text` must be ASCII, or built as `sf::String(U"…")`.** The first U1 build drew `›`
   (U+203A) as the hover marker and the screenshot showed **tofu boxes** — plural, which is the tell. The cause is
   **not** a missing glyph: `assets/fonts/noto/NotoSans-Regular.ttf` has U+203A (`guilsinglright`, verified with
   fontTools). SFML 3 decodes `std::string`/`const char*` as **ANSI through `std::locale`**
   (`SFML/include/SFML/System/String.hpp:173`), so the UTF-8 bytes `E2 80 BA` become three code points and the
   U+0080 in the middle has no glyph. Fixed as **U13** (ASCII `>` for the marker; the `↑ ↓ ← →` in
   `Game::bettingControlsHint()` and the `•` in the name-entry hint replaced — the arrows *are* genuinely absent from
   the Noto cmap). Locked by test **#326** (`isSfmlSafeAscii`). **If you need a non-ASCII label, pass
   `sf::String(U"…")` / `std::u32string`, never a UTF-8 `const char*`.**
5. **A screenshot shows *what* is broken, not *why*.** The "font lacks the glyph" explanation above was inferred from
   a screenshot and was wrong; it sat in the docs until the cmap was actually checked. Test a suspected cause against
   the artifact (font cmap, library header) before writing it down.

The harness runs under a **temp `$HOME`** seeded with a named profile — B17's hermeticity rule applies to manual runs
too, so the developer's real `~/.local/share/blackjack/saves/player_profile.json` is never touched. The seeded name
also skips the U7 name-entry prompt, which synthetic keys cannot drive (SFML under Xvfb cannot create an X input
context, so `TextEntered` never returns unicode).
