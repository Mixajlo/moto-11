"""Live readout of the ESP's sense inputs: prints a line every time IGN, START,
the INA3221 CH1 voltage (if fitted), or the supervisor state changes.

  python watch_inputs.py                 # watch for 60 s, sensors stay as they are
  python watch_inputs.py --secs 120
  python watch_inputs.py --real          # also switch the supervisor to real sensors
                                         # ('sim real') and follow its state; back to
                                         # SIM at the end unless --stay-real

Use it while toggling a jumper / the key: IGN should follow it, and with --real
the supervisor should go SLEEP -> ARMED -> SLEEP.
Usually launched through run-watch-inputs.ps1, which opens it in its own window.
"""
import argparse
import datetime
import os
import re
import time

from moto_serial import Esp


def parse(out):
    r = {}
    m = re.search(r"IGN\s+gpio\d+\s+pin (\w+)\s+-> ignition (\w+)", out)
    if m: r["IGN"] = f"{m.group(2).upper():<4} (pin {m.group(1)})"
    m = re.search(r"START\s+gpio\d+\s+pin (\w+)\s+-> start (\w+)", out)
    if m: r["START"] = f"{m.group(2).upper():<8} (pin {m.group(1)})"
    m = re.search(r"CH1\s+bus\s+([\d.]+) V", out)
    if m: r["VBUS"] = f"{float(m.group(1)):.2f} V"
    m = re.search(r"state=(\w+)", out)
    if m: r["STATE"] = m.group(1)
    m = re.search(r"master=(\w+)", out)
    if m: r["MASTER"] = m.group(1).upper()
    return r


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="auto")
    ap.add_argument("--secs", type=float, default=60)
    ap.add_argument("--real", action="store_true", help="'sim real' and follow the supervisor")
    ap.add_argument("--stay-real", action="store_true", help="don't switch back to SIM at the end")
    a = ap.parse_args()

    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    logpath = os.path.join(logdir, datetime.datetime.now().strftime("watch-inputs-%Y%m%d-%H%M%S.log"))
    log = open(logpath, "w", encoding="utf-8")

    def say(msg):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    esp = Esp(a.port)
    esp.read(0.3)
    if a.real:
        esp.send("sim real", 0.5)
    say(f"Watching {esp.port} for {a.secs:.0f} s  ({'REAL sensors' if a.real else 'sensors unchanged'})")
    say("Toggle the key / jumper now. A line prints on every change. Ctrl+C to stop.\n")

    last = {}
    end = time.time() + a.secs
    try:
        while time.time() < end:
            out = esp.send("sensors", 0.35)
            if a.real:
                out += esp.send("state", 0.25)
            now = parse(out)
            changed = {k: v for k, v in now.items() if last.get(k) != v}
            if changed:
                t = datetime.datetime.now().strftime("%H:%M:%S")
                say(f"{t}  " + "   ".join(f"{k}: {v}" for k, v in now.items())
                    + ("" if not last else "     <- " + ", ".join(changed)))
                last = now
            left = int(end - time.time())
            print(f"\r  ... {left:3d} s left ", end="", flush=True)
    except KeyboardInterrupt:
        pass
    print()
    if a.real and not a.stay_real:
        esp.send("sim sim", 0.5)
        say("Supervisor back on SIM sensors.")
    say(f"Done. Log: {logpath}")
    esp.close()
    log.close()


if __name__ == "__main__":
    main()
