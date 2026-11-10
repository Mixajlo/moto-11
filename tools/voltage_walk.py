"""Guided walks on REAL sensors: you turn the bench supply (XY-SK35H) and the IGN
switch, the script watches the firmware and reports PASS/FAIL per step.

Profiles:
  supervisor  the engine-run state machine (ADR-0012): SLEEP, ARMED, RUNNING +
              MARGINAL, recovered, POWERED, RUNNING, OFF_DELAY, SLEEP.
  loads       the load policy (ADR-0019) on the FOG / GRIP LEDs: engine-only grips,
              the grips -> fog shed ladder and its restore. shedStep is set to 10 s
              for the walk and put back afterwards.

Bench wiring this expects:
  charger -> XY VIN only;  XY OUT+ = bench +12 rail (ULN COM, LEDs, IGN/START
  switches, INA3221 CH1);  XY OUT- = bench GND.  XY: OVP 15 V, OCP 0.5 A.
  LEDs on ULN pins 18 MASTER, 17 FOG, 16 GRIP.

  python voltage_walk.py                      # supervisor profile, auto COM port
  python voltage_walk.py --profile loads
  python voltage_walk.py --port COM4

You set what a step says, press Enter, the script waits for the expected result.
Output printed while it waits for Enter is kept. At the end (or Ctrl+C) the ESP is
put back on SIM sensors. Never set the XY above 15 V.
Usually launched through run-voltage-walk.ps1 (own window).
"""
import argparse
import datetime
import os
import re
import sys
import time

from moto_serial import Esp


def step(text, timeout=10, **expect):
    """expect keys: state, master, charge, grip, fog (bool = relay on), logs (list of
    substrings that must all appear), before (console cmds the script sends first)."""
    return {"text": text, "timeout": timeout, **expect}


PROFILES = {
    "supervisor": {
        "title": "SUPERVISOR VOLTAGE WALK (real INA3221 + PC817)",
        "setup": [], "teardown": [],
        "steps": [
            step("IGN switch OFF, XY at 12.40 V", 5, state="SLEEP", master=False),
            step("IGN switch ON  (key on, engine not running)", 5, state="ARMED", master=False),
            step("XY -> 13.30 V  (engine started, but charging weakly)\n"
                 "         expect: RUNNING after ~1 s, MASTER LED ON, then a MARGINAL warning after ~5 s",
                 15, state="RUNNING", master=True, charge="MARGINAL"),
            step("XY -> 13.80 V  (charging healthy)\n"
                 "         expect: 'charging recovered' after ~5 s",
                 15, state="RUNNING", master=True, charge="ok"),
            step("XY -> 12.40 V  (engine stalled / kill switch, key still on)\n"
                 "         expect: POWERED after ~2 s, MASTER LED STAYS ON",
                 10, state="POWERED", master=True),
            step("XY -> 13.80 V  (engine restarted)\n"
                 "         expect: RUNNING after ~1 s, MASTER LED still ON",
                 10, state="RUNNING", master=True),
            step("IGN switch OFF (key off)\n"
                 "         expect: OFF_DELAY, MASTER LED stays ON for the off-delay (30 s)",
                 5, state="OFF_DELAY", master=True),
            step("Just wait - hands off. MASTER LED goes OFF when the off-delay ends.",
                 45, state="SLEEP", master=False),
            step("XY back to 12.40 V (leave the bench at rest)", 0),
        ],
    },
    "loads": {
        "title": "LOAD POLICY WALK (engine-only grips, shed ladder) on the LEDs",
        "setup": ["set shedStep 10000"],
        "teardown": ["loads want grip off", "loads want fog off"],
        "restore_keys": ["shedStep"],
        "steps": [
            step("IGN switch OFF, XY at 12.40 V. The script now asks for GRIP + FOG\n"
                 "         (like pressing the buttons). expect: FOG LED ON, GRIP LED OFF\n"
                 "         (grips are engine-only: they wait for the engine)",
                 5, before=["loads want grip on", "loads want fog on"],
                 state="SLEEP", grip=False, fog=True),
            step("IGN switch ON  (key on, engine not running)\n"
                 "         expect: still FOG ON, GRIP OFF",
                 5, state="ARMED", grip=False, fog=True),
            step("XY -> 13.80 V  (engine running, charging healthy)\n"
                 "         expect: MASTER ON, and the GRIP LED comes ON by itself",
                 10, state="RUNNING", master=True, grip=True, fog=True),
            step("XY -> 13.30 V, press Enter, then HANDS OFF for ~20 s  (charging marginal)\n"
                 "         expect: GRIP LED OFF after ~5 s, then FOG LED OFF ~10 s later",
                 30, state="RUNNING", charge="MARGINAL", grip=False, fog=False,
                 logs=["shedding grip", "shedding fog"]),
            step("XY -> 13.80 V, press Enter, then HANDS OFF for ~30 s  (charging recovered)\n"
                 "         expect: FOG LED back first (~15 s), then GRIP LED (~10 s later)",
                 45, state="RUNNING", charge="ok", grip=True, fog=True,
                 logs=["restoring fog", "restoring grip"]),
            step("XY -> 12.40 V  (engine stalled, key on)\n"
                 "         expect: GRIP LED OFF (engine-only), FOG and MASTER stay ON",
                 10, state="POWERED", master=True, grip=False, fog=True),
            step("XY -> 13.80 V  (engine restarted)\n"
                 "         expect: GRIP LED back ON",
                 10, state="RUNNING", master=True, grip=True, fog=True),
            step("IGN switch OFF, XY back to 12.40 V. The script switches GRIP + FOG off.", 0),
        ],
    },
}

STATE_RE = re.compile(r"state=(\w+).*?master=(\w+)(?:\s+charge=(\S+))?")


def load_row(text, name):
    m = list(re.finditer(rf"^\s+{name}\s+(yes|no)\s+(ON|off)", text, re.M))
    return (m[-1].group(2) == "ON") if m else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", choices=PROFILES, default="supervisor")
    ap.add_argument("--port", default="auto", help="COMx or auto (default)")
    a = ap.parse_args()
    prof = PROFILES[a.profile]

    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    logpath = os.path.join(logdir, f"voltage-walk-{a.profile}-{stamp}.log")
    log = open(logpath, "w", encoding="utf-8")

    def say(msg=""):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    esp = Esp(a.port, log=log)
    esp.read(0.3)
    failures = []
    saved = {}

    def snapshot(need_loads):
        out = esp.send("state", 0.6)
        if need_loads:
            out += esp.send("loads", 0.7)
        m = list(STATE_RE.finditer(out))
        g = {"text": out}
        if m:
            g.update(state=m[-1].group(1), master=m[-1].group(2) == "ON", charge=m[-1].group(3))
        g["grip"] = load_row(out, "grip")
        g["fog"] = load_row(out, "fog")
        return g

    def matches(st, got, seen):
        for k in ("state", "master", "charge", "grip", "fog"):
            if k in st and got.get(k) != st[k]:
                return False
        return all(s in seen for s in st.get("logs", []))

    def describe(st, got):
        parts = []
        for k in ("state", "master", "charge", "grip", "fog"):
            if k in st or k in ("state", "master"):
                v = got.get(k)
                parts.append(f"{k}={'ON' if v is True else 'off' if v is False else v}")
        return " ".join(parts)

    say("=" * 70)
    say(f"  moto-11  {prof['title']}")
    say(f"  port {esp.port}   log {logpath}")
    say("=" * 70)
    say("Before starting:")
    say("  - XY output ON at 12.40 V, OVP 15 V, OCP 0.5 A, encoder UNLOCKED")
    say("  - IGN switch OFF, START switch released")
    say("  - Never above 15 V. Ctrl+C = stop (ESP goes back to SIM sensors).")
    input("\nPress Enter to start... ")
    if not esp.sync():
        say("  !! console isn't answering cleanly - check USB; continuing anyway")
    for k in prof.get("restore_keys", []):
        m = re.search(rf"{k} = ([\d.]+)", esp.send(f"get {k}", 0.6))
        if m:
            saved[k] = m.group(1)
    for c in prof["setup"]:
        esp.send(c, 0.6)
    if "real" not in esp.send("sim real", 1.0):
        say("  !! 'sim real' not confirmed - aborting")
        sys.exit(1)
    st = esp.send("sensors", 1.5)
    m = re.search(r"CH1\s+bus\s+([\d.]+) V", st)
    say(f"  real sensors on. INA CH1 reads {m.group(1) if m else '??'} V "
        "(compare with the XY display / your meter)")

    steps = prof["steps"]
    try:
        for i, s in enumerate(steps, 1):
            say(f"\nSTEP {i}/{len(steps)}: {s['text']}")
            input("         ...done? press Enter ")
            # Whatever the ESP printed while we waited for Enter is in the OS buffer:
            # keep it - the change may already have happened.
            seen = esp.read(0.2)
            esp.sync(1)
            seen += esp.sync_text              # a log line can land during the wake-up too
            for c in s.get("before", []):
                seen += esp.send(c, 0.5)
            if "state" not in s:
                continue
            need_loads = "grip" in s or "fog" in s
            got = {}
            t0 = time.time()
            while time.time() - t0 < s["timeout"]:
                got = snapshot(need_loads)
                seen += got["text"]
                if matches(s, got, seen):
                    break
                left = int(s["timeout"] - (time.time() - t0))
                print(f"\r         waiting... now {describe(s, got)[:48]:48s} {left:3d} s ",
                      end="", flush=True)
            print("\r" + " " * 70 + "\r", end="")
            detail = describe(s, got)
            vb = re.findall(r"vbus=([\d.]+)", seen)
            if vb:
                detail += f" vbus={vb[-1]}"
            missing = [x for x in s.get("logs", []) if x not in seen]
            if missing:
                detail += "  not seen: " + ", ".join(f"'{x}'" for x in missing)
            if matches(s, got, seen):
                say(f"         PASS  {detail}")
            else:
                want = " ".join(f"{k}={'ON' if s[k] is True else 'off' if s[k] is False else s[k]}"
                                for k in ("state", "master", "charge", "grip", "fog") if k in s)
                say(f"         FAIL  {detail}\n               (wanted {want})")
                if s.get("state") == "OFF_DELAY" and got.get("state") in ("RUNNING", "POWERED"):
                    say("               (IGN came back on - switch/jumper bounce or flipped back?)")
                failures.append(f"step {i}")
    except KeyboardInterrupt:
        say("\n\nSTOPPED (Ctrl+C)")
    finally:
        for c in prof["teardown"]:
            esp.send(c, 0.5)
        for k, v in saved.items():
            esp.send(f"set {k} {v}", 0.5)
        esp.send("sim sim", 1.0)
        say("\n" + "=" * 70)
        say("  ESP back on SIM sensors" + (f"; restored {', '.join(f'{k}={v}' for k, v in saved.items())}."
                                            if saved else "."))
        say("  RESULT: " + ("ALL STEPS PASSED" if not failures else "FAILED: " + ", ".join(failures)))
        say("  Also check with your eyes: the LEDs matched every step above.")
        say("=" * 70)
        esp.close()
        log.close()
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
