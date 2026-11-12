#!/usr/bin/env python3
"""Focused U12 part-1 verification: per-graph click toggles ONE side-panel graph between its
per-shoe view and its session view, independently of the other two; 'G' switches all together.

Drives the real Blackjack2 binary under Xvfb (python-xlib), populates the three left-panel graphs
with a handful of settled rounds, then uses ImageMagick `compare` on each pane's **title strip** to prove:
  - clicking the bankroll graph toggles ONLY it (its strip changes; the other two do not);
  - pressing 'G' switches every graph to the session view at once (the two still per-shoe change; the
    already-session bankroll strip does not).

The title strip is the right region to diff: drawChartFrame() renders the pane name there and the view
switch renames it ("Bankroll Curve" -> "Session Bankroll Curve", "N bets" -> "N session bets"), so a
toggle is a guaranteed pixel change while an untouched pane is a guaranteed zero. The plot area is not:
a session curve can coincide with the shoe curve for the first rounds.

Pane rects are DERIVED below from the same Constants::Hud values HUD::leftPanelCharts() uses, not measured
by eye. The first version of this script hard-coded crop rows guessed from a screenshot; two of the three
landed in blank panel background and compared 0 forever, which reads as "unchanged" and makes a genuine
pass indistinguishable from a broken check.
"""
import os, sys, time, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness as V
from Xlib import X, display

d = V.d
w = None

# Mirrors Constants::Hud + HUD::leftPanelCharts (src/HUD.cpp:378) at the 1600x900 world size: the left panel is
# (0,0) of size SIDE_WIDTH x (WINDOW_HEIGHT - BOTTOM_HEIGHT) = 240 x 804, and three charts stack inside it with
# CHART_MARGIN inset and CHART_GAP between. drawChartFrame() draws the title at rect.position + (8,4) at
# CHART_TITLE_FONT (14px), so a pane's title strip is about top+2 .. top+26.
PANEL_W, PANEL_H = 240.0, 900.0 - 96.0
CHART_MARGIN, CHART_GAP, CHART_COUNT = 10.0, 18.0, 3
PER_CHART = (PANEL_H - 2 * CHART_MARGIN - (CHART_COUNT - 1) * CHART_GAP) / CHART_COUNT
PANE_TOP = [CHART_MARGIN + i * (PER_CHART + CHART_GAP) for i in range(CHART_COUNT)]
PANE_CENTRE = [(PANEL_W / 2.0, PANE_TOP[i] + PER_CHART / 2.0) for i in range(CHART_COUNT)]


def title_crop(path, index):
    """Crop pane `index`'s title-strip region (window-local) to `path`."""
    subprocess.run(["import", "-window", str(w.id), "-crop",
                    "228x24+%d+%d" % (6, int(PANE_TOP[index]) + 2), path], check=True)
    return path


def ae(a, b):
    """Absolute-error pixel count between two equal-sized crops; ~0 means visually identical.

    ImageMagick prints the metric to **stderr** and leaves stdout empty, and exits 1 when the images
    differ (so check=True is not an option). Parse stderr.
    """
    out = subprocess.run(["compare", "-metric", "AE", a, b, "/tmp/u12_delta.miff"],
                         capture_output=True, text=True)
    try:
        return int(float(out.stderr.strip().split()[0]))
    except Exception:
        return -1


def main():
    V.seed_profile()
    env = dict(os.environ)
    env["HOME"] = V.home
    env["DISPLAY"] = V.DISPLAY
    proc = subprocess.Popen([V.BIN], env=env, cwd=V.REPO,
                            stdout=open("/tmp/bj_run_u12.log", "wb"), stderr=subprocess.STDOUT)
    try:
        global w
        w = V.find_window(V.TITLE)
        if not w:
            print("FAIL: no window")
            return 1
        w.set_input_focus(X.InputFocus, X.CurrentTime)
        time.sleep(1.5)

        # Enter live play at the default world size (1600x900).
        V.click(w, 800, 220)            # Play Game (pause row 0 at drawn y=220)
        time.sleep(2.0)

        # Populate the graphs: raise off a zero bet (a 0 bet cannot be confirmed), confirm, stand, then press R for the
        # next round. R is required -- standing settles the round but leaves it in Cleanup, so a loop without R runs
        # exactly ONE round and the graphs stay near-empty (the first version of this script did that: 307 of 312 cards
        # left, one bar on the histogram). Varying the stake (3 x Up) also gives the bets axis more than one bucket.
        for _ in range(6):
            for _ in range(3):
                V.key(w, "Up")
                time.sleep(0.2)
            V.key(w, "Right")           # confirm the bet -> deal
            time.sleep(1.2)
            V.key(w, "s")               # stand -> dealer plays out and the round settles
            time.sleep(2.5)             # let the settle animation finish
            V.key(w, "r")               # Cleanup -> next round (PlaceBets)
            time.sleep(1.5)

        # Let it go fully idle so only the toggle (not a settling animation) will move pixels next.
        time.sleep(2.0)
        V.shot("u12a_table_shoe_view_1600x900")

        # Each pane's title strip (window-local), from the derived pane geometry above.
        before = [title_crop("/tmp/u12_p%d_before.png" % i, i) for i in range(3)]

        # U12 part 1: click the top (Bankroll) graph. It must switch ONLY that graph to its session view; the
        # Edge and Bets graphs keep their per-shoe view because each pane is an independent toggle.
        cx, cy = PANE_CENTRE[0]
        V.click(w, int(cx), int(cy))     # centre of the Bankroll pane
        time.sleep(1.4)
        V.shot("u12b_bankroll_clicked_session_1600x900")

        after = [title_crop("/tmp/u12_p%d_after.png" % i, i) for i in range(3)]

        d_bank = ae(before[0], after[0])
        d_edge = ae(before[1], after[1])
        d_bets = ae(before[2], after[2])
        print("compare AE after one Bankroll click: bank=%d  edge=%d  bets=%d" % (d_bank, d_edge, d_bets))

        # 'G' now switches every graph to the session view together. The Bankroll pane is ALREADY session (clicked
        # above), so it must not move; Edge and Bets are per-shoe and must both change. That is the bulk switch.
        V.key(w, "g")
        time.sleep(1.2)
        V.shot("u12c_all_graphs_session_g_1600x900")

        at_g = [title_crop("/tmp/u12_p%d_g.png" % i, i) for i in range(3)]

        g_bank = ae(after[0], at_g[0])
        g_edge = ae(after[1], at_g[1])
        g_bets = ae(after[2], at_g[2])
        print("compare AE after 'G': bank=%d  edge=%d  bets=%d" % (g_bank, g_edge, g_bets))

        ok_click = (d_bank > 0) and (d_edge == 0) and (d_bets == 0)
        ok_bulk = (g_bank == 0) and (g_edge > 0) and (g_bets > 0)
        ok = ok_click and ok_bulk
        print("RESULT:", "PASS" if ok else "FAIL",
              "(click: bankroll changed=%s, edge unchanged=%s, bets unchanged=%s | "
              "G: bankroll unchanged=%s, edge changed=%s, bets changed=%s)" %
              (d_bank > 0, d_edge == 0, d_bets == 0, g_bank == 0, g_edge > 0, g_bets > 0))
        return 0 if ok else 2
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("temp home:", V.home)


if __name__ == "__main__":
    sys.exit(main())
