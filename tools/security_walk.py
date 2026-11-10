"""Guided bench walk of the IMU security/crash logic (ADR-0020). You move the MPU-6050;
the script watches the ESP's `motion` status and reports PASS/FAIL per step:

  park reference set -> MOVED (tilt ~10 deg) -> re-armed -> BUMP (taps) -> TIPPED
  (on its side) -> UPRIGHT -> CRASH (riding: knock + lie it down) -> auto-cancel

For the crash step the script fakes "engine running" with SIM sensors and lowers
crashG to 1.5 g (a hand knock on a bench is far below a real crash); both are put
back afterwards. Calibrate first if `motion` says calibration=NONE (the script offers).

  python security_walk.py              # auto COM port
  python security_walk.py --port COM4
Logs go to tools/logs/. Usually launched through run-security-walk.ps1.
"""
import argparse
import datetime
import os
import re
import sys
import time

from moto_serial import Esp

COUNTS = re.compile(r"events: bump (\d+)\s+moved (\d+)\s+tipped (\d+)\s+crash pending (\d+) / "
                    r"confirmed (\d+) / cancelled (\d+)")


def status(esp):
    out = esp.send("motion", 0.6)
    st = {"text": out}
    for key, rx in (("park", r"park reference (set|settling)"), ("moved", r"moved=(YES|no)"),
                    ("tipped", r"tipped=(YES|no)"), ("crash", r"crash=(\w+)"),
                    ("cal", r"calibration=(ok|NONE)"), ("ctx", r"context=(\w+)"),
                    ("tilt", r"tilt from upright\s+([\d.]+)"), ("delta", r"delta\s+([\d.]+)")):
        m = re.search(rx, out)
        st[key] = m.group(1) if m else None
    m = COUNTS.search(out)
    if m:
        st.update(zip(("n_bump", "n_moved", "n_tipped", "n_pending", "n_confirmed", "n_cancelled"),
                      (int(x) for x in m.groups())))
    return st


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="auto")
    a = ap.parse_args()

    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    logpath = os.path.join(logdir, f"security-walk-{stamp}.log")
    log = open(logpath, "w", encoding="utf-8")

    def say(msg=""):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    esp = Esp(a.port, log=log)
    esp.read(0.3)
    failures = []
    say("=" * 70)
    say("  moto-11  SECURITY / CRASH WALK (MPU-6050 on the bench)")
    say(f"  port {esp.port}   log {logpath}")
    say("=" * 70)
    say("Keep the IMU's wires plugged in; move the module gently by its edges.")
    input("\nPress Enter to start... ")
    esp.sync()
    s0 = status(esp)
    if s0.get("cal") != "ok":
        input("  Not calibrated. Lay the IMU FLAT and still, then press Enter to calibrate... ")
        esp.send("imu cal", 3.0)
        s0 = status(esp)
        say(f"  calibration: {s0.get('cal')}")
    m = re.search(r"crashG = ([\d.]+)", esp.send("get crashG", 0.6))
    crash_g = m.group(1) if m else "3"

    # (instruction, check(st, base) -> bool, timeout s, console commands first)
    steps = [
        ("Lay it FLAT, hands off ~12 s (it takes a parked reference after 10 s still)",
         lambda s, b: s.get("park") == "set", 25, []),
        ("Tilt it ~10 deg (lift one edge onto a pen) and HOLD it there",
         lambda s, b: s.get("n_moved", 0) > b.get("n_moved", 0), 15, []),
        ("Hands off ~12 s at that angle (it re-arms on the new position)",
         lambda s, b: s.get("park") == "set" and s.get("moved") == "no", 25, []),
        ("Tap the table firmly next to it, a few times",
         lambda s, b: s.get("n_bump", 0) > b.get("n_bump", 0), 15, []),
        ("Turn it ON ITS SIDE (90 deg) and hold ~3 s",
         lambda s, b: s.get("tipped") == "YES", 12, []),
        ("Lay it FLAT again, hold ~3 s",
         lambda s, b: s.get("tipped") == "no", 12, []),
        ("CRASH (engine now faked RUNNING, crashG 1.5 g): knock the table HARD right\n"
         "         next to it, then within 3 s turn it ON ITS SIDE and hold ~5 s",
         lambda s, b: s.get("crash") == "PENDING", 25,
         ["sim ign on", "sim vbus 13.8", "set crashG 1.5"]),
        ("Stand it FLAT again and hold ~3 s (picking the bike up cancels the alert)",
         lambda s, b: s.get("crash") == "none" and s.get("n_cancelled", 0) > b.get("n_cancelled", 0), 15, []),
    ]
    try:
        for i, (text, check, timeout, before) in enumerate(steps, 1):
            say(f"\nSTEP {i}/{len(steps)}: {text}")
            for c in before:
                esp.send(c, 0.5)
            base = status(esp)
            input("         ...press Enter, then do it ")
            esp.sync(1)
            t0, st = time.time(), base
            while time.time() - t0 < timeout:
                st = status(esp)
                if check(st, base):
                    break
                left = int(timeout - (time.time() - t0))
                print(f"\r         waiting... tilt {st.get('tilt')} deg  park delta {st.get('delta')} deg  "
                      f"crash {st.get('crash')}  {left:3d} s ", end="", flush=True)
            print("\r" + " " * 78 + "\r", end="")
            detail = (f"tilt={st.get('tilt')} delta={st.get('delta')} park={st.get('park')} "
                      f"moved={st.get('moved')} tipped={st.get('tipped')} crash={st.get('crash')}")
            if check(st, base):
                say(f"         PASS  {detail}")
            else:
                say(f"         FAIL  {detail}")
                failures.append(f"step {i}")
    except KeyboardInterrupt:
        say("\n\nSTOPPED (Ctrl+C)")
    finally:
        esp.send("crash cancel", 0.5)
        esp.send(f"set crashG {crash_g}", 0.5)
        esp.send("sim vbus 12.4", 0.5)
        esp.send("sim ign off", 0.5)
        say("\n" + "=" * 70)
        say(f"  Restored crashG={crash_g}, SIM engine off.")
        say("  RESULT: " + ("ALL STEPS PASSED" if not failures else "FAILED: " + ", ".join(failures)))
        say("=" * 70)
        esp.close()
        log.close()
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
