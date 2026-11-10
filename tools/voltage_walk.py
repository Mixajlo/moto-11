"""Guided supervisor walk on REAL sensors: you turn the bench supply (XY-SK35H)
and the IGN switch, the script watches the engine-run state machine (ADR-0012).

Bench wiring this expects:
  charger -> XY VIN only;  XY OUT+ = bench +12 rail (ULN COM, LEDs, IGN/START
  switches, INA3221 CH1);  XY OUT- = bench GND.  XY: OVP 15 V, OCP 0.5 A.
  MASTER LED on ULN pin 18 shows the master relay.

  python voltage_walk.py               # auto COM port
  python voltage_walk.py --port COM4

Each step: you set what it says, press Enter, the script waits for the expected
state (or log line) and reports PASS/FAIL. At the end (or Ctrl+C) the ESP is
switched back to SIM sensors. Never set the XY above 15 V.
Usually launched through run-voltage-walk.ps1 (own window).
"""
import argparse
import datetime
import os
import re
import sys
import time

from moto_serial import Esp

# (instruction, expected state, expected master, expected charge= or None, timeout s)
STEPS = [
    ("IGN switch OFF, XY at 12.40 V",
     "SLEEP", False, None, 5),
    ("IGN switch ON  (key on, engine not running)",
     "ARMED", False, None, 5),
    ("XY -> 13.30 V  (engine started, but charging weakly)\n"
     "         expect: RUNNING after ~1 s, MASTER LED ON, then a MARGINAL warning after ~5 s",
     "RUNNING", True, "MARGINAL", 15),
    ("XY -> 13.80 V  (charging healthy)\n"
     "         expect: 'charging recovered' after ~5 s",
     "RUNNING", True, "ok", 15),
    ("XY -> 12.40 V  (engine stalled / kill switch, key still on)\n"
     "         expect: POWERED after ~2 s, MASTER LED STAYS ON",
     "POWERED", True, None, 10),
    ("XY -> 13.80 V  (engine restarted)\n"
     "         expect: RUNNING after ~1 s, MASTER LED still ON",
     "RUNNING", True, None, 10),
    ("IGN switch OFF (key off)\n"
     "         expect: OFF_DELAY, MASTER LED stays ON for the off-delay (30 s)",
     "OFF_DELAY", True, None, 5),
    ("Just wait - hands off. MASTER LED goes OFF when the off-delay ends.",
     "SLEEP", False, None, 45),
    ("XY back to 12.40 V (leave the bench at rest)",
     None, None, None, 0),
]

STATE_RE = re.compile(r"state=(\w+).*?master=(\w+)(?:\s+charge=(\S+))?")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="auto", help="COMx or auto (default)")
    a = ap.parse_args()

    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    logpath = os.path.join(logdir, f"voltage-walk-{stamp}.log")
    log = open(logpath, "w", encoding="utf-8")

    def say(msg=""):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    esp = Esp(a.port, log=log)
    esp.read(0.3)
    failures = []

    def snapshot():
        out = esp.send("state", 0.6)
        m = STATE_RE.search(out)
        return out, (m.group(1), m.group(2) == "ON", m.group(3)) if m else None

    say("=" * 66)
    say("  moto-11  SUPERVISOR VOLTAGE WALK (real INA3221 + PC817)")
    say(f"  port {esp.port}   log {logpath}")
    say("=" * 66)
    say("Before starting:")
    say("  - XY output ON at 12.40 V, OVP 15 V, OCP 0.5 A, encoder UNLOCKED")
    say("  - IGN switch OFF, START switch released")
    say("  - Never above 15 V. Ctrl+C = stop (ESP goes back to SIM sensors).")
    input("\nPress Enter to start... ")
    if not esp.sync():
        say("  !! console isn't answering cleanly - check USB; continuing anyway")
    out = esp.send("sim real", 1.0)
    if "real" not in out:
        say("  !! 'sim real' not confirmed - aborting")
        sys.exit(1)
    st = esp.send("sensors", 1.5)
    m = re.search(r"CH1\s+bus\s+([\d.]+) V", st)
    say(f"  real sensors on. INA CH1 reads {m.group(1) if m else '??'} V "
        "(compare with the XY display / your meter)")

    try:
        for i, (what, want_state, want_master, want_charge, timeout) in enumerate(STEPS, 1):
            say(f"\nSTEP {i}/{len(STEPS)}: {what}")
            input("         ...done? press Enter ")
            # Whatever the ESP printed while we waited for Enter is in the OS buffer:
            # keep it (it goes to the log) - the change may already have happened.
            seen_log = esp.read(0.2)
            esp.sync(1)
            if want_state is None:
                continue
            got = None

            def matches(g):
                return (g and g[0] == want_state and g[1] == want_master
                        and (want_charge is None or g[2] == want_charge))
            t0 = time.time()
            while time.time() - t0 < timeout:
                out, got = snapshot()
                seen_log += out
                if matches(got):
                    break
                left = int(timeout - (time.time() - t0))
                now = f"{got[0]} master={'ON' if got[1] else 'off'}" if got else "?"
                print(f"\r         waiting... now {now:24s} {left:3d} s ", end="", flush=True)
            print("\r" + " " * 66 + "\r", end="")
            vb = re.findall(r"vbus=([\d.]+)", seen_log)
            detail = f"state={got[0] if got else '?'} master={'ON' if got and got[1] else 'off'}"
            if want_charge:
                detail += f" charge={got[2] if got else '?'}"
            if vb:
                detail += f" vbus={vb[-1]}"
            if matches(got):
                say(f"         PASS  {detail}")
            else:
                want = f"{want_state}, master {'ON' if want_master else 'off'}"
                if want_charge:
                    want += f", charge {want_charge}"
                say(f"         FAIL  {detail}  (wanted {want})")
                if want_state == "OFF_DELAY" and got and got[0] in ("RUNNING", "POWERED"):
                    say("               (IGN came back on - switch/jumper bounce or flipped back?)")
                failures.append(f"step {i}")
    except KeyboardInterrupt:
        say("\n\nSTOPPED (Ctrl+C)")
    finally:
        esp.send("sim sim", 1.0)
        say("\n" + "=" * 66)
        say("  ESP back on SIM sensors.")
        say("  RESULT: " + ("ALL STEPS PASSED" if not failures else "FAILED: " + ", ".join(failures)))
        say("  Also check with your eyes: MASTER LED matched every 'master=' above.")
        say("=" * 66)
        esp.close()
        log.close()
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
