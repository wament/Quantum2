#!/usr/bin/env python3
"""Progress-tab on-screen verification (U12/IDEAS #6).

The headless suite locks the geometry (ChartPrimitives: scaling, downsampling, disjoint bands) and the
state machine (source vs. scope axes). It cannot construct a RenderWindow, so it cannot prove the tab
DRAWS. This script drives the real binary under Xvfb and measures pixels:

  1. the Progress tab renders two charts, and its title strips name the axis each one shows;
  2. Up/Down changes the ACROSS-SHOES chart's title (the Source axis) and nothing else;
  3. 'S' changes the LIVE chart's title (the Scope axis) and leaves the Source title untouched --
     the two axes are independent controls, which is the whole design decision;
  4. the two chart bands stay DISJOINT at 1600x900 and at 900x560 (the U3 defect class: layout that
     only fits one size) -- measured as a pure-background strip between them;
  5. four tab labels fit inside the window at both sizes (the tab bar width is derived from the count).

Every crop is DERIVED from the draw site in StatsDashboard.cpp, never guessed from a screenshot
(README trap 2). The profile seed writes COMPLETE nested records (README trap 5: a partial round
record throws in from_json, and the loader's recovery is "relocate the profile, start fresh", which
presents as a plausible screen and makes every diff 0).
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

KEY = {"Left": 0xFF51, "Up": 0xFF52, "Right": 0xFF53, "Down": 0xFF54,
       "Return": 0xFF0D, "Escape": 0xFF1B, "s": 0x0073}

# Statistics is the LAST main-menu row: {MenuOption::Statistics, 580.f} in include/GameRenderer.hpp,
# scaled by worldH/900 (mainMenuRowYAt). Clicking the drawn row is the rule; arrow keys wrap.
STATS_ROW_DESIGN_Y = 580.0
DESIGN_H = 900.0

os.makedirs(OUT, exist_ok=True)
home = tempfile.mkdtemp(prefix="bj_home_progress_")


def shoe(seed, decks, card_count, bankroll, rounds, mode):
    """A recorded shoe with COMPLETE round records -- see the module docstring's README trap 5."""
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
    """All three histories populated, with bankrolls that give the charts visibly different shapes:
    career climbs, Daily dips, tournament climbs higher. An empty profile.name fires the U7 name-entry
    prompt, which synthetic keys cannot drive, so the name is committed here."""
    saves = os.path.join(home, ".local/share/blackjack/saves")
    os.makedirs(saves, exist_ok=True)
    profile = {
        "name": "Tester",
        "bankroll": 5000,
        "longHistory": [shoe(0xC0FFEE, 6, 313, 5200, 3, 0),
                        shoe(0xC0FFEF, 6, 313, 5600, 4, 0),
                        shoe(0xC0FF10, 6, 313, 6100, 5, 0)],
        "dailyShoes": [shoe(0xBEEF01, 6, 313, 1400, 4, 1), shoe(0xBEEF02, 6, 313, 900, 2, 1)],
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


def key(w, name):
    kc = d.keysym_to_keycode(KEY[name])
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


def resize(w, width, height):
    for size in ((width, height), (width - 1, height), (width, height)):
        w.configure(width=size[0], height=size[1])
        d.sync()
        time.sleep(0.5)


def shot(tag):
    path = os.path.join(OUT, tag + ".png")
    subprocess.run(["import", "-window", "root", path], check=True)
    return path


def crop(src, region, dest):
    subprocess.run(["convert", src, "-crop", region, "+repage", dest], check=True)
    return dest


def ae(a, b):
    """`compare -metric AE` prints the metric on STDERR and exits 1 when images differ (README trap 1)."""
    p = subprocess.run(["compare", "-metric", "AE", a, b, "/dev/null"],
                       capture_output=True, text=True)
    return int(p.stderr.strip().split()[0]) if p.stderr.strip() else -1


def colors(path):
    """Number of unique colours in a crop: 1 means the region is flat background."""
    p = subprocess.run(["convert", path, "-format", "%k", "info:"],
                       capture_output=True, text=True)
    return int(p.stdout.strip()) if p.stdout.strip().isdigit() else -1


# --- crops derived from the draw sites in StatsDashboard.cpp -------------------
# topFrame    = frame(world, .12, .88, .28, .55);  bottomFrame = frame(world, .12, .88, .66, .90)
# titles sit at x = w*0.5 - w*0.34 = 0.16w, y = frame.top - 26; the tab bar sits at y = 0.16h.
#
# The dashboard lays out in WORLD units (see StatsDashboard::worldDrawSize), and Game::gameView is a
# FIXED 1600x900 rect letterboxed into the OS window by resizeView. So the regions below are constants
# in world space and only the world->pixel transform varies. A first version cropped in window pixels,
# which coincides with world units at exactly 1600x900 and measured the wrong band at every other size
# -- it "failed" 900x560 by measuring the panel's own bottom edge, and its 1280x720 passes were vacuous.
WORLD_W, WORLD_H = 1600.0, 900.0


def view_rect(Wp, Hp):
    """resizeView's letterbox: (offset_x, offset_y, width, height) in PIXELS."""
    wr, vr = float(Wp) / float(Hp), WORLD_W / WORLD_H
    if wr > vr:
        s = vr / wr
        return ((1.0 - s) / 2.0 * Wp, 0.0, s * Wp, 1.0 * Hp)
    s = wr / vr
    return (0.0, (1.0 - s) / 2.0 * Hp, 1.0 * Wp, s * Hp)


def region(Wp, Hp, wx, wy, ww, wh):
    """A world-space rect -> an ImageMagick crop geometry in window pixels."""
    ox, oy, sw, sh = view_rect(Wp, Hp)
    sx, sy = sw / WORLD_W, sh / WORLD_H
    return "%dx%d+%d+%d" % (int(round(ww * sx)), int(round(wh * sy)),
                            int(round(ox + wx * sx)), int(round(oy + wy * sy)))


def top_title(W, H):
    return region(W, H, 0.16 * WORLD_W, 0.28 * WORLD_H - 26.0, 700.0, 24.0)


def bot_title(W, H):
    return region(W, H, 0.16 * WORLD_W, 0.66 * WORLD_H - 26.0, 700.0, 24.0)


def gap_strip(W, H):
    """The band between the top chart's x-axis label (0.55h + 6, 13pt tall) and the bottom chart's
    title (0.66h - 26, 17pt). If the two charts ever overlap, this stops being flat background.

    Margins are arithmetic, not eyeballed: it starts 30px under the plot floor (clearing the 13pt axis
    label) and stops 4px above the bottom title's y -- 525..564 against a title at 568 in world units."""
    return region(W, H, 0.12 * WORLD_W, 0.55 * WORLD_H + 30.0,
                  0.76 * WORLD_W, 0.11 * WORLD_H - 64.0)


def tab_strip(W, H):
    return region(W, H, 0.0, 0.16 * WORLD_H, WORLD_W, 34.0)


def right_edge(W, H):
    """The 14px column (world) at the right edge of the tab bar's LABEL band: a fourth label pushed
    off-screen would put glyph pixels here.

    Deliberately only the label band: the active tab's underline is (tabWidth - 12) wide with a 6px
    inset, so it reaches within 6px of the right edge BY DESIGN at every size (1594 of 1600). Measuring
    the underline, as a first version did, reports 2 colours and reads as an overflow that is not there.
    """
    return region(W, H, WORLD_W - 14.0, 0.16 * WORLD_H, 14.0, 24.0)


def hint_row(W, H):
    """The footer hint, drawn at world (cx, h - 24) and centred on x. This is the coordinate-basis
    probe: h is 900 in world units and 560 in window pixels, so the hint sits at world y=876 (pixel
    518 at 900x560) under the world basis and at world y=536 (pixel 327) under the pixel basis. A crop
    at the world position is flat backdrop in the second case, so this check FAILS before
    StatsDashboard::worldDrawSize exists and passes after.

    It is used instead of the tab underline because the underline's y is tabTop + text bounds height,
    which depends on font metrics -- and guessing font metrics is exactly the eyeballed-crop trap.
    """
    return region(W, H, 0.25 * WORLD_W, WORLD_H - 26.0, 0.5 * WORLD_W, 20.0)


def main():
    seed_profile()
    env = dict(os.environ)
    env["HOME"] = home
    env["DISPLAY"] = DISPLAY
    proc = subprocess.Popen([BIN], env=env, cwd=REPO,
                            stdout=open("/tmp/bj_progress_run.log", "wb"), stderr=subprocess.STDOUT)
    failures = []

    def check(label, ok, detail=""):
        print(("  PASS  " if ok else "  FAIL  ") + label + ((" (%s)" % detail) if detail else ""))
        if not ok:
            failures.append(label)

    try:
        w = find_window(TITLE)
        if not w:
            print("FAIL: game window not found (see /tmp/bj_progress_run.log)")
            return 1
        W, H = w.get_geometry().width, w.get_geometry().height
        print("window %d at %dx%d" % (w.id, W, H))
        w.set_input_focus(X.InputFocus, X.CurrentTime)
        time.sleep(1.5)

        # README trap 5: the symptom of a rejected seed is a PLAUSIBLE screen, not a crash.
        log = open("/tmp/bj_progress_run.log").read()
        check("profile loaded (no 'Corrupt profile' relocation in the run log)",
              "Corrupt profile" not in log)

        # ---- 1. Statistics -> Progress tab -------------------------------------
        click(w, W // 2, int(STATS_ROW_DESIGN_Y * H / DESIGN_H))
        time.sleep(1.0)
        for _ in range(3):                      # Overview -> Hands -> Profits -> Progress
            key(w, "Right")
        time.sleep(0.8)
        career = shot("30a_progress_career")

        top0 = crop(career, top_title(W, H), os.path.join(OUT, "30b_top_title.png"))
        bot0 = crop(career, bot_title(W, H), os.path.join(OUT, "30c_live_title.png"))
        check("the across-shoes chart title strip has content", colors(top0) > 1)
        check("the live chart title strip has content", colors(bot0) > 1)

        # Stability control: a re-capture of the SAME state must diff 0, or every non-zero below is noise.
        recap = shot("30d_progress_career_recap")
        top0b = crop(recap, top_title(W, H), os.path.join(OUT, "30e_top_title_recap.png"))
        bot0b = crop(recap, bot_title(W, H), os.path.join(OUT, "30i_live_title_recap.png"))
        check("stability: re-capturing one state diffs 0", ae(top0, top0b) == 0)

        # ---- 2. Source axis: Up/Down re-labels the ACROSS-SHOES chart ----------
        key(w, "Down")                          # Career -> Daily
        time.sleep(0.6)
        daily = shot("30f_progress_daily")
        top1 = crop(daily, top_title(W, H), os.path.join(OUT, "30g_top_title_daily.png"))
        bot1 = crop(daily, bot_title(W, H), os.path.join(OUT, "30h_live_title_daily.png"))
        check("Down re-labels the across-shoes chart (the Source axis moves)", ae(top0b, top1) > 0)
        check("Down leaves the live chart's label alone (the two axes stay independent)",
              ae(bot0b, bot1) == 0)

        # ---- 3. Scope axis: 'S' re-labels the LIVE chart only ------------------
        key(w, "s")                             # shoe -> session
        time.sleep(0.6)
        sess = shot("30j_progress_session")
        top2 = crop(sess, top_title(W, H), os.path.join(OUT, "30k_top_title_session.png"))
        bot2 = crop(sess, bot_title(W, H), os.path.join(OUT, "30l_live_title_session.png"))
        check("'S' re-labels the live chart (the Scope axis moves)", ae(bot1, bot2) > 0)
        check("'S' leaves the across-shoes chart's label alone (Scope is not a Source option)",
              ae(top1, top2) == 0)

        # ---- 4. Disjoint bands and a fitting tab bar, at this size -------------
        flat = colors(crop(sess, gap_strip(W, H), os.path.join(OUT, "30m_gap.png")))
        check("the two chart bands are disjoint at %dx%d (gap strip is flat background)" % (W, H),
              flat == 1, "unique colours=%d" % flat)
        edge = colors(crop(sess, right_edge(W, H), os.path.join(OUT, "30n_tab_edge.png")))
        check("four tab labels fit inside the window at %dx%d" % (W, H), edge == 1,
              "unique colours=%d" % edge)
        bars = colors(crop(sess, tab_strip(W, H), os.path.join(OUT, "30o_tab_strip.png")))
        check("the tab bar draws its four labels/underlines", bars > 2, "unique colours=%d" % bars)

        # ---- 5. Smaller windows: the U3 defect class ---------------------------
        for (W2, H2, tag) in ((1280, 720, "1280x720"), (900, 560, "900x560")):
            resize(w, W2, H2)
            time.sleep(0.6)
            small = shot("30p_progress_" + tag)
            flat2 = colors(crop(small, gap_strip(W2, H2), os.path.join(OUT, "30q_gap_%s.png" % tag)))
            check("the two chart bands are disjoint at %s" % tag, flat2 == 1,
                  "unique colours=%d" % flat2)
            edge2 = colors(crop(small, right_edge(W2, H2), os.path.join(OUT, "30r_tab_edge_%s.png" % tag)))
            check("four tab labels fit inside the window at %s" % tag, edge2 == 1,
                  "unique colours=%d" % edge2)
            # The coordinate-basis check: the footer hint must land at world y = h - 24.
            hint = colors(crop(small, hint_row(W2, H2), os.path.join(OUT, "30u_hint_row_%s.png" % tag)))
            check("the panel lays out in world units at %s (footer hint lands at world h-24)" % tag,
                  hint > 1, "unique colours=%d" % hint)
            top_small = crop(small, top_title(W2, H2), os.path.join(OUT, "30s_top_title_%s.png" % tag))
            check("the across-shoes chart still draws at %s" % tag, colors(top_small) > 1)
            bot_small = crop(small, bot_title(W2, H2), os.path.join(OUT, "30t_live_title_%s.png" % tag))
            check("the live chart's label still draws at %s" % tag, colors(bot_small) > 1)

        key(w, "Escape")
        time.sleep(0.5)
        return 1 if failures else 0
    finally:
        proc.terminate()
        proc.wait(timeout=5)
        print("\n%d failure(s): %s" % (len(failures), failures if failures else "none"))


if __name__ == "__main__":
    sys.exit(main())

