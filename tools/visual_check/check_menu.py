#!/usr/bin/env python3
"""Menu-only capture: confirm the hover marker glyph renders (no tofu) and that the keyboard-selected
row (gold) stays distinct from the hovered row (pill + marker)."""
import os, sys, time, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness as V
from Xlib import X, display


def main():
    V.seed_profile()
    env = dict(os.environ)
    env["HOME"] = V.home
    env["DISPLAY"] = V.DISPLAY
    proc = subprocess.Popen([V.BIN], env=env, cwd=V.REPO,
                            stdout=open("/tmp/bj_run_menu.log", "wb"), stderr=subprocess.STDOUT)
    try:
        w = V.find_window(V.TITLE)
        if not w:
            print("FAIL: no window")
            return 1
        w.set_input_focus(X.InputFocus, X.CurrentTime)
        time.sleep(1.5)
        # Move the selection off row 0 with the keyboard, then hover a DIFFERENT row.
        for _ in range(3):
            V.key(w, "Down")
            time.sleep(0.2)
        V.motion(w, 800, 460)         # hover "Achievements" (row 6)
        time.sleep(0.6)
        V.shot("20a_menu_hover_marker_1600x900")
        V.resize(w, 900, 560)
        V.motion(w, 450, 286)         # drawn Achievements row at world 560
        time.sleep(0.6)
        V.shot("20b_menu_hover_marker_900x560")
        return 0
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("temp home:", V.home)


if __name__ == "__main__":
    sys.exit(main())
