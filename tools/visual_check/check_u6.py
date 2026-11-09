#!/usr/bin/env python3
"""Focused U6 verification: raise the bet, confirm, hit repeatedly, screenshot the card fan
and the settled hand totals. Reuses the helpers from bj_visual.py."""
import os, sys, time, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness as V
from Xlib import X, display

d = V.d
w = None


def main():
    V.seed_profile()
    env = dict(os.environ)
    env["HOME"] = V.home
    env["DISPLAY"] = V.DISPLAY
    proc = subprocess.Popen([V.BIN], env=env, cwd=V.REPO,
                            stdout=open("/tmp/bj_run6.log", "wb"), stderr=subprocess.STDOUT)
    try:
        w = V.find_window(V.TITLE)
        if not w:
            print("FAIL: no window")
            return 1
        w.set_input_focus(X.InputFocus, X.CurrentTime)
        time.sleep(1.5)

        V.click(w, 800, 220)          # Play Game
        time.sleep(2.0)

        # Raise the wager off 0 (MINIMUM_BET is 10) and confirm it.
        for _ in range(3):
            V.key(w, "Up")
            time.sleep(0.25)
        V.key(w, "Right")             # confirm bet
        time.sleep(1.5)
        V.shot("17a_dealt_2_cards_1600x900")

        # The dealer's up card was an Ace, so the round sits in OfferInsurance and ignores H/S.
        # Esc declines insurance and hands the turn to the player (InputHandler insurance branch).
        V.key(w, "Escape")
        time.sleep(1.0)
        V.shot("17b_after_insurance_1600x900")

        # Escape declines insurance when the round is in OfferInsurance, and PAUSES when it is not
        # (this run the dealer's up card was 6, so there was nothing to decline). Clicking the pause
        # panel's Resume row is safe in both cases: it only acts in Paused, and a left-click in the
        # middle play band is inert during PlayerTurn.
        V.click(w, 800, 360)
        time.sleep(0.8)

        # Hit until the fan is long enough to show the old overlap (3+ cards).
        for i in range(5):
            V.key(w, "h")
            time.sleep(0.6)
            V.shot("18_hit_%d" % (i + 3))

        V.key(w, "s")                 # stand -> dealer plays, totals get drawn
        time.sleep(3.5)
        V.shot("19_settled_totals_1600x900")
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
