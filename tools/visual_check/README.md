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
```

Screenshots land in `tools/visual_check/shots/` (git-ignored). Override with `BJ_SHOTS`, `BJ_REPO`, `BJ_BIN`.

## What each script proves

| Script | Proves |
|---|---|
| `harness.py` | **U1** selected ≠ hovered; **U11** hover follows the *drawn* row after a resize (and the old pixel position highlights nothing); **U3** Achievements list vs. footer at 900×560 and 1600×900; **U2** pause panel at both sizes |
| `check_u6.py` | **U6** 3+ card fan is centred on the seat and shows more than a sliver of each card; hand totals sit on dark pills that stay legible over the felt |
| `check_menu.py` | The hover marker glyph actually renders (see the font rule below) at 1600×900 and 900×560 |

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
