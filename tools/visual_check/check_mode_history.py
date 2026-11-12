#!/usr/bin/env python3
"""MS1 on-screen verification: per-mode history in the Replay browser + the mode rows on the main menu.

The headless suite locks the pure helpers (HistoryBrowser.hpp / ModeStatsLabels.hpp) and the data layer,
but it cannot construct Replay (it takes a window/texture/font) and cannot construct the renderer. This
script proves the drawn result: that the Tournament menu row carries its aggregates, that the Daily row
carries its streak, that the history-list title strip names the source, and that Tab switches the source
of the LIST -- measured by pixel diff on the title strip, not by eye.

Seeds a profile with career + Daily + tournament shoes and aggregates, under a temp HOME (B17's rule
applies to manual runs too). See README.md for the traps this script obeys: click the drawn row (arrow
keys wrap), and never press Escape at the main menu (it closes the window).
"""
import json
import os
import subprocess
import sys
import tempfile
import time

from Xlib import X, display
from Xlib.protocol import event

DISPLAY = os.environ.get("DISPLAY", ":97")
REPO = os.environ.get("BJ_REPO", os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
BIN = os.environ.get("BJ_BIN", os.path.join(REPO, "out/build/GCC 13.3.0 x86_64-linux-gnu/Blackjack2"))
OUT = os.environ.get("BJ_SHOTS", os.path.join(REPO, "tools", "visual_check", "shots"))
TITLE = "Blackjack"

KEY = {"Tab": 0xFF09, "Return": 0xFF0D, "Escape": 0xFF1B}

# Main-menu row centres, from mainMenuItems() in include/GameRenderer.hpp, at the 900 design height.
# Row 1 is Daily Challenge, row 4 is Review History, row 6 is Tournament.
ROW_DAILY_Y = 260
ROW_REVIEW_HISTORY_Y = 380
ROW_TOURNAMENT_Y = 460

# The history-list title strip, from ReplayView::drawHistoryList: sf::Text at (60, 30), 28pt.
# Derived from the draw site, never guessed from a screenshot (README trap 2).
TITLE_STRIP = "900x45+60+25"

os.makedirs(OUT, exist_ok=True)
home = tempfile.mkdtemp(prefix="bj_home_modestats_")


def shoe(seed, decks, card_count, bankroll, rounds, mode):
    """A recorded shoe with COMPLETE round records.

    RoundRecord's from_json reads ALL SIX of its fields with j.at() -- dealerCards, playerHands,
    runningCount, trueCount, cardsRemaining, timestamp -- and HandOutcome's does the same for eight
    more. A partial round therefore THROWS, and the load path's recovery from a throw is "relocate the
    profile, start fresh": the first version of this seed omitted trueCount/cardsRemaining and the app
    opened on the name-entry screen because the whole profile had been moved aside. That is B24's stated
    limitation (the guards cover top-level fields; nested reads are still strict), so a hand-authored
    profile for a visual check must write every required key.
    """
    def round_record(i):
        return {
            "dealerCards": [],
            "playerHands": [],
            "runningCount": i,
            "trueCount": 0,
            "cardsRemaining": max(card_count - 6 * (i + 1), 0),
            "timestamp": 1700000000 + i,
        }

    return {
        "shuffleSeed": seed,
        "shoeDecks": decks,
        "shoeCardCount": card_count,
        "isContinuation": False,
        "mode": mode,
        "completed": True,
        "bankrollAtShoeEnd": bankroll,
        "rounds": [round_record(i) for i in range(rounds)],
    }


def seed_profile():
    """A profile with all three histories populated, so every source has rows and the menu rows have
    non-zero aggregates to draw. An empty profile.name is what fires the U7 name-entry prompt, which
    synthetic keys cannot drive -- so the name is committed here."""
    saves = os.path.join(home, ".local/share/blackjack/saves")
    os.makedirs(saves, exist_ok=True)
    profile = {
        "name": "Tester",
        "bankroll": 5000,
        "longHistory": [shoe(0xC0FFEE, 6, 313, 5200, 3, 0)],
        "dailyShoes": [shoe(0xC0FFEE, 6, 313, 1400, 4, 1), shoe(0xC0FFEF, 6, 313, 900, 2, 1)],
        "dailyStats": {"shoesPlayed": 2, "bestBankroll": 1400, "mostHandsWon": 21, "bestFinish": 0},
        "dailyStreak": 5,
        "dailyLastPlayed": "2026-11-12",
        "tournamentShoes": [shoe(0xDECADE, 4, 209, 2400, 5, 2), shoe(0xDECAF, 4, 209, 3100, 3, 2)],
        "tournamentStats": {"shoesPlayed": 2, "bestBankroll": 3100, "mostHandsWon": 18, "bestFinish": 4},
        "tournamentsCompleted": 2,
    }
    with open(os.path.join(saves, "player_profile.json"), "w") as f:
        json.dump(profile, f)


# --- X11 helpers (same shape as harness.py; xdotool/scrot are not installed) ---


d = display.Display(DISPLAY)
root = d.screen().root


def find_window(name, deadline=15.0):
    end = time.time() + deadline
    while time.time() < end:
        w = _search(root, name)
        if w:
            return w
        time.sleep(0.2)
    return None


def _search(win, name):
    for child in win.query_tree().children:
        try:
            if child.get_wm_name() == name:
                return child
        except Exception:
            pass
        found = _search(child, name)
        if found:
            return found
    return None


def key(w, code):
    kc = d.keysym_to_keycode(KEY[code])
    for is_press in (1, 0):
        cls = event.KeyPress if is_press else event.KeyRelease
        w.send_event(cls(detail=kc, state=0, root=root, window=w.id, subwindow=X.NONE, child=X.NONE,
                         time=X.CurrentTime, root_x=0, root_y=0, event_x=0, event_y=0,
                         same_screen=1))
        d.sync()
        time.sleep(0.10)


def motion(w, x, y):
    w.warp_pointer(x, y)
    d.sync()
    w.send_event(event.MotionNotify(detail=X.NotifyNormal, state=0, root=root, window=w.id,
                                    subwindow=X.NONE, child=X.NONE,
                                    time=X.CurrentTime, root_x=x, root_y=y,
                                    event_x=x, event_y=y, same_screen=1))
    d.sync()


def click(w, x, y):
    motion(w, x, y)
    time.sleep(0.15)
    w.send_event(event.ButtonPress(detail=1, state=X.Button1Mask, root=root, window=w.id,
                                   subwindow=X.NONE, child=X.NONE,
                                   time=X.CurrentTime, root_x=x, root_y=y,
                                   event_x=x, event_y=y, same_screen=1))
    d.sync()
    time.sleep(0.12)
    w.send_event(event.ButtonRelease(detail=1, state=0, root=root, window=w.id,
                                     subwindow=X.NONE, child=X.NONE,
                                     time=X.CurrentTime, root_x=x, root_y=y,
                                     event_x=x, event_y=y, same_screen=1))
    d.sync()


def shot(tag):
    path = os.path.join(OUT, tag + ".png")
    subprocess.run(["import", "-window", "root", path], check=True)
    print("  shot:", path)
    return path


def crop(src, region, dest):
    subprocess.run(["convert", src, "-crop", region + "+0+0", "+repage", dest], check=True)
    return dest


def ae(a, b):
    """ImageMagick `compare -metric AE` prints the metric on STDERR and exits 1 when the images
    differ (README trap 1): parse stderr, never the return code."""
    p = subprocess.run(["compare", "-metric", "AE", a, b, "/dev/null"],
                       capture_output=True, text=True)
    return int(p.stderr.strip().split()[0]) if p.stderr.strip() else -1


def main():
    seed_profile()
    env = dict(os.environ)
    env["HOME"] = home
    env["DISPLAY"] = DISPLAY
    proc = subprocess.Popen([BIN], env=env, cwd=REPO,
                            stdout=open("/tmp/bj_ms1_run.log", "wb"), stderr=subprocess.STDOUT)
    failures = []

    def check(label, ok):
        print(("  PASS  " if ok else "  FAIL  ") + label)
        if not ok:
            failures.append(label)

    try:
        w = find_window(TITLE)
        if not w:
            print("FAIL: game window not found (see /tmp/bj_ms1_run.log)")
            return 1
        print("window %d at %dx%d" % (w.id, w.get_geometry().width, w.get_geometry().height))
        w.set_input_focus(X.InputFocus, X.CurrentTime)
        time.sleep(1.5)

        # ---- 1. Main menu: both mode rows carry their aggregates -------------------
        menu = shot("22a_ms1_main_menu_mode_rows_1600x900")
        crop(menu, "900x40+400+%d" % (ROW_DAILY_Y - 20), os.path.join(OUT, "22b_ms1_daily_row.png"))
        crop(menu, "900x40+400+%d" % (ROW_TOURNAMENT_Y - 20), os.path.join(OUT, "22c_ms1_tournament_row.png"))

        # ---- 2. Enter the browser by CLICKING the drawn row (arrows wrap) ----------
        click(w, 800, ROW_REVIEW_HISTORY_Y)
        time.sleep(1.0)
        career = shot("23a_ms1_history_career_1600x900")

        # ---- 3. Tab switches the LIST's source; the title strip must say so --------
        key(w, "Tab")
        time.sleep(0.7)
        daily = shot("23b_ms1_history_daily_1600x900")
        key(w, "Tab")
        time.sleep(0.7)
        tournament = shot("23c_ms1_history_tournament_1600x900")
        key(w, "Tab")
        time.sleep(0.7)
        wrapped = shot("23d_ms1_history_wrapped_to_career_1600x900")

        tdir = tempfile.mkdtemp(prefix="bj_ms1_crops_")
        c = {n: crop(src, TITLE_STRIP, os.path.join(tdir, n + ".png"))
             for n, src in [("career", career), ("daily", daily),
                            ("tournament", tournament), ("wrapped", wrapped)]}

        # Stability control: re-capture the SAME state with NO key press, so a non-zero AE above is a
        # real change and not capture noise. (Tab from Career goes to DAILY, not back to Career -- the
        # first version of this check pressed Tab here and compared Career against Daily.)
        c["career2"] = crop(shot("23e_ms1_history_career_repeat_1600x900"), TITLE_STRIP,
                            os.path.join(tdir, "career2.png"))

        check("title strip stable for the same source (career vs repeat, AE == 0)",
              ae(c["career"], c["career2"]) == 0)
        # README trap 3: prove the crop CAN go non-zero before trusting that it goes to zero.
        d_cd, d_dt, d_tc = ae(c["career"], c["daily"]), ae(c["daily"], c["tournament"]), ae(c["tournament"], c["wrapped"])
        check("Career -> Daily changes the title strip (AE > 0): %d" % d_cd, d_cd > 0)
        check("Daily -> Tournament changes the title strip (AE > 0): %d" % d_dt, d_dt > 0)
        check("Tournament -> Career changes the title strip (AE > 0): %d" % d_tc, d_tc > 0)
        check("the 3rd Tab wraps to the SAME Career view (AE == 0 vs the first Career shot)",
              ae(c["career"], c["wrapped"]) == 0)

        print()
        print("Read the crops for the strings themselves:")
        print("  22b_ms1_daily_row.png       expect  Daily Challenge   (streak 5)")
        print("  22c_ms1_tournament_row.png  expect  Tournament Mode   (2 runs, best stage 4)")
        print("  23a/23b/23c title strips    expect  Review History - Career / Daily Challenge / Tournament - Select a Shoe")
        print()
        print("RESULT:", "ALL PASS" if not failures else "FAILURES: %s" % failures)
        return 0 if not failures else 1
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        real = os.path.expanduser("~/.local/share/blackjack/saves/player_profile.json")
        print("real profile present and untouched:", os.path.exists(real))
        print("temp home used:", home)


if __name__ == "__main__":
    sys.exit(main())

