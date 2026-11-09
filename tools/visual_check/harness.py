#!/usr/bin/env python3
"""Phase 0.4 vision-critical on-screen verification harness (U1, U2, U3, U6, U11).

Launches the real Blackjack2 binary under Xvfb, drives it with synthetic X11 input
(python-xlib -- xdotool/scrot are not installed on this machine), and captures screenshots
with ImageMagick `import`. HOME is redirected to a temp dir so the developer's real
~/.local/share/blackjack/saves/player_profile.json is never touched.
"""
import os, subprocess, sys, time, tempfile

from Xlib import X, display
from Xlib.protocol import event

DISPLAY = os.environ.get("DISPLAY", ":97")
REPO = os.environ.get("BJ_REPO", os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
BIN = os.environ.get("BJ_BIN", os.path.join(REPO, "out/build/GCC 13.3.0 x86_64-linux-gnu/Blackjack2"))
OUT = os.environ.get("BJ_SHOTS", os.path.join(REPO, "tools", "visual_check", "shots"))
TITLE = "Blackjack"

KEY = {
    "Escape": 0xff1b, "Return": 0xff0d, "Up": 0xff52, "Down": 0xff54,
    "Left": 0xff51, "Right": 0xff53, "space": 0x020,
    "h": 0x68, "s": 0x73, "d": 0x64, "r": 0x72, "p": 0x70, "y": 0x79,
    "g": 0x67, "1": 0x31, "2": 0x32, "3": 0x33,
}

os.makedirs(OUT, exist_ok=True)
home = tempfile.mkdtemp(prefix="bj_home_")


def seed_profile():
    """SFML under Xvfb cannot create an X input context, so TextEntered never returns unicode and the
    U7 name-entry screen cannot be driven with synthetic keys. Seed a named profile instead: an empty
    profile.name is exactly what makes Game::needsNameEntry() fire, so a committed name skips the prompt
    and lands on the main menu. Only `name` matters; every other field is optional by the additive-JSON rule."""
    import json
    saves = os.path.join(home, ".local/share/blackjack/saves")
    os.makedirs(saves, exist_ok=True)
    with open(os.path.join(saves, "player_profile.json"), "w") as f:
        json.dump({"name": "Tester", "bankroll": 5000}, f)

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
    for _ in range(1):
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
    w.configure(width=width, height=height)
    d.sync()
    time.sleep(0.5)
    w.configure(width=width - 1, height=height)
    d.sync()
    time.sleep(0.2)
    w.configure(width=width, height=height)
    d.sync()
    time.sleep(0.6)


def shot(tag):
    path = os.path.join(OUT, tag + ".png")
    subprocess.run(["import", "-window", "root", path], check=True)
    print("  shot:", path)
    return path


def main():
    seed_profile()
    env = dict(os.environ)
    env["HOME"] = home
    env["DISPLAY"] = DISPLAY
    proc = subprocess.Popen([BIN], env=env, cwd=REPO,
                            stdout=open("/tmp/bj_run.log", "wb"),
                            stderr=subprocess.STDOUT)
    try:
        w = find_window(TITLE)
        if not w:
            print("FAIL: game window not found (see /tmp/bj_run.log)")
            return 1
        print("window %d at %dx%d" % (w.id, w.get_geometry().width, w.get_geometry().height))
        w.set_input_focus(X.InputFocus, X.CurrentTime)
        time.sleep(1.5)

        # Navigation is CLICK-driven: keyboard Up/Down wraps, so a fixed number of presses is not
        # deterministic. Clicking a row activates exactly the row drawn at that y, which also exercises
        # the U11 hit-test on every transition.
        def menuRowY(index, worldH):
            return 460.0 * worldH / 900.0 if index == 6 else None

        # ============ U1: keyboard-selected row and hovered row on screen together ============
        shot("10a_menu_no_hover_1600x900")
        motion(w, 800, 460)          # hover "Achievements" (row 6, drawn y=460 at world 900)
        time.sleep(0.5)
        shot("10b_menu_hover_achievements_1600x900")

        # ============ U11: hover must follow the DRAWN row after a resize ==================
        resize(w, 900, 560)
        motion(w, 450, 286)          # drawn Achievements row = 460*560/900 = 285.6
        time.sleep(0.5)
        shot("11a_menu_hover_after_resize_900x560")
        motion(w, 450, 460)          # where the OLD absolute-pixel row sat: must highlight nothing
        time.sleep(0.4)
        shot("11b_menu_hover_old_basis_900x560")

        # ============ U3: Achievements at the squashed world (the overlap case) =============
        click(w, 450, 286)
        time.sleep(0.9)
        shot("12a_achievements_900x560")

        # ============ U3: Achievements at the default world =================================
        key(w, "Escape")
        time.sleep(0.5)
        resize(w, 1600, 900)
        time.sleep(0.5)
        shot("12b_menu_1600x900")
        click(w, 800, 460)           # Achievements
        time.sleep(0.9)
        shot("13b_achievements_1600x900")

        # ============ U2: pause panel over the live table ===================================
        key(w, "Escape")
        time.sleep(0.5)
        click(w, 800, 220)           # Play Game (row 0, drawn y=220)
        time.sleep(2.0)
        shot("14a_table_1600x900")
        key(w, "Escape")
        time.sleep(0.8)
        shot("14b_pause_panel_1600x900")

        # ============ U2: pause panel at the squashed world =================================
        resize(w, 900, 560)
        time.sleep(0.6)
        shot("15_pause_panel_900x560")
        resize(w, 1600, 900)
        time.sleep(0.6)
        click(w, 800, 360)           # Resume (pause row 0 = 0.40*900)
        time.sleep(0.8)
        shot("15b_resumed_table_1600x900")

        # ============ U6: long hand -> card fan overlap + hand-total pill ==================
        key(w, "Right")              # confirm the bet
        time.sleep(1.2)
        for _ in range(5):
            key(w, "h")
            time.sleep(0.5)
        shot("16a_hand_fan_1600x900")
        key(w, "s")                  # stand -> dealer plays, totals drawn on their pills
        time.sleep(3.0)
        shot("16b_settled_totals_1600x900")

        return 0
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
