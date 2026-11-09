"""Guided relay walk-through: prints what you should SEE/HEAR just BEFORE each step,
then drives the ESP console and checks it answered.

Profiles:
  bench  - breadboard: ULN2803 + relay box, yellow LED on master. Clicks only.
  bike   - on the motorcycle: master relay powers the box, then fog lights,
           heated grips (held long enough to feel warmth), spare, aux.

  python relay_test.py                      # bench, auto-detect the COM port
  python relay_test.py --profile bike
  python relay_test.py --profile bike --port socket://vstrom-bike.local:23   # WiFi, no reset test
  python relay_test.py --no-reset           # skip the EN-reset fail-OFF test

Ctrl+C at any time switches every accessory off and puts master back how it was.
Usually launched through run-relay-test.ps1, which opens it in its own window.
"""
import argparse
import datetime
import os
import sys
import time

from moto_serial import Esp, all_off, boot_is_clean

# A step is (console command, what the owner should notice, seconds to hold).
# hold >= 15 shows a live countdown (used for the grips).
PROFILES = {
    "bench": {
        "title": "BENCH relay walk-through (ULN2803 + relay box + yellow LED)",
        "precheck": [
            "USB plugged in first, THEN the 12 V supply.",
            "Nothing from the +12 rail touches the ESP's 3V3/5V/GPIO pins.",
        ],
        "steps": [
            ("on fog",     "RELAY 1 (fog)    clicks ON", 3.5),
            ("off fog",    "RELAY 1 (fog)    clicks OFF", 3.5),
            ("on grip",    "RELAY 2 (grip)   clicks ON", 3.5),
            ("off grip",   "RELAY 2 (grip)   clicks OFF", 3.5),
            ("on spare",   "RELAY 3 (spare)  clicks ON", 3.5),
            ("off spare",  "RELAY 3 (spare)  clicks OFF", 3.5),
            ("on aux",     "RELAY 4 (aux)    clicks ON", 3.5),
            ("off aux",    "RELAY 4 (aux)    clicks OFF", 3.5),
            ("on master",  "YELLOW LED (master) turns ON   (no click)", 3.5),
            ("off master", "YELLOW LED (master) turns OFF  (no click)", 3.5),
            ("on all",     "ALL 4 RELAYS click ON together + yellow LED ON", 3.5),
            ("off all",    "ALL 4 RELAYS click OFF together + yellow LED OFF", 3.5),
            ("selftest",   "SELFTEST fast run, 0.3 s each: yellow LED -> relay 1 -> 2 -> 3 -> 4 -> all off", 5),
        ],
        # fail-OFF at reset: this channel is switched on, then the ESP is reset
        "reset_channel": ("aux", "RELAY 4 (aux)"),
    },
    "bike": {
        "title": "BIKE load walk-through (real fogs, grips, spare, aux)",
        "precheck": [
            "Bike on its stand, in neutral, nothing touching the fog lights.",
            "Engine RUNNING (recommended): fogs + grips draw ~13 A. On battery only,",
            "  keep it to one run - this test takes about 2 minutes.",
            "Headlights are NOT part of this test (they are not on the ESP, by design).",
            "If the supervisor changes state mid-test it may switch MASTER itself;",
            "  the script checks MASTER and tells you.",
        ],
        "steps": [
            ("on master",  "MASTER relay clunks ON -> relay box is now powered", 3),
            ("on fog",     "FOG LIGHTS turn ON (relay 1 clicks)", 6),
            ("off fog",    "FOG LIGHTS turn OFF", 3),
            ("on grip",    "HEATED GRIPS ON (relay 2) - hold both grips, warmth after ~30 s", 45),
            ("off grip",   "HEATED GRIPS OFF", 3),
            ("on spare",   "SPARE load ON (whatever is on relay 3; just a click if nothing)", 5),
            ("off spare",  "SPARE load OFF", 3),
            ("on aux",     "AUX load ON (whatever is on relay 4; just a click if nothing)", 5),
            ("off aux",    "AUX load OFF", 3),
        ],
        # fogs are the visible one: they must go dark at reset and stay dark
        "reset_channel": ("fog", "FOG LIGHTS"),
    },
}
PROFILES["leds"] = {
    "title": "BENCH LED walk-through (ULN2803 -> one LED per channel, no relay box)",
    "precheck": [
        "USB plugged in first, THEN the 12 V supply.",
        "Each LED: +12 -> 15 kOhm -> LED long leg; short leg -> ULN pin.",
        "LED positions: pin 18 MASTER, 17 FOG, 16 GRIP, 15 SPARE, 14 AUX (label them).",
    ],
    "steps": [
        ("on fog",     "FOG LED    (ULN pin 17) turns ON", 3),
        ("off fog",    "FOG LED    turns OFF", 2.5),
        ("on grip",    "GRIP LED   (ULN pin 16) turns ON", 3),
        ("off grip",   "GRIP LED   turns OFF", 2.5),
        ("on spare",   "SPARE LED  (ULN pin 15) turns ON", 3),
        ("off spare",  "SPARE LED  turns OFF", 2.5),
        ("on aux",     "AUX LED    (ULN pin 14) turns ON", 3),
        ("off aux",    "AUX LED    turns OFF", 2.5),
        ("on master",  "MASTER LED (ULN pin 18) turns ON", 3),
        ("off master", "MASTER LED turns OFF", 2.5),
        ("on all",     "ALL 5 LEDs turn ON together", 3.5),
        ("off all",    "ALL 5 LEDs turn OFF together", 3),
        ("selftest",   "SELFTEST fast run, 0.3 s each: MASTER -> FOG -> GRIP -> SPARE -> AUX -> all off", 5),
    ],
    "reset_channel": ("aux", "AUX LED (pin 14)"),
}
ACCESSORIES = ("fog", "grip", "spare", "aux")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", choices=PROFILES, default="bench")
    ap.add_argument("--port", default="auto", help="COMx, socket://host:23, or auto (default)")
    ap.add_argument("--no-reset", action="store_true", help="skip the EN-reset fail-OFF test")
    ap.add_argument("--yes", action="store_true", help="don't wait for Enter before starting")
    a = ap.parse_args()
    prof = PROFILES[a.profile]

    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    logpath = os.path.join(logdir, f"relay-test-{a.profile}-{stamp}.log")
    log = open(logpath, "w", encoding="utf-8")

    def say(msg=""):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    esp = Esp(a.port, log=log)
    do_reset = esp.is_serial and not a.no_reset
    esp.read(0.3)
    master_was_on = esp.master_is_on()
    failures = []

    def cmd(c, hold):
        # Listen for the whole step (selftest alone takes ~3 s); only long holds
        # (the grips) check after 1.5 s and then show a countdown.
        first = min(hold, 1.5) if hold >= 15 else hold
        out = esp.send(c, first)
        if "selftest" in c:
            ok = "selftest done" in out
        else:
            ok = "->" in out
        if not ok and "�" in out:
            say("         [ESP answered: NO REPLY - garbled bytes on the USB line (noise?)]")
        else:
            say(f"         [ESP answered: {'yes' if ok else 'NO REPLY'}]")
        if not ok:
            failures.append(c)
        if hold >= 15:                       # long hold: live countdown
            for left in range(int(hold - first), 0, -1):
                print(f"\r         {left:3d} s left ", end="", flush=True)
                time.sleep(1)
            print("\r" + " " * 30 + "\r", end="")
        if c == "on master" and not esp.master_is_on():
            say("         !! MASTER is not on - the supervisor may have switched it. Box loads won't work.")
            failures.append("master held")

    def safe_off():
        for ch in ACCESSORIES:
            esp.send(f"off {ch}", 0.3)
        esp.send("on master" if master_was_on else "off master", 0.5)

    say("=" * 66)
    say(f"  moto-11  {prof['title']}")
    say(f"  port {esp.port}   log {logpath}")
    say("=" * 66)
    say("Before starting, check:")
    for line in prof["precheck"]:
        say(f"  - {line}")
    say("Each line below appears just BEFORE it happens. Ctrl+C = stop, all accessories off.")
    if not a.yes:
        input("\nPress Enter to start... ")
    for n in (3, 2, 1):
        say(f"  starting in {n}...")
        time.sleep(1)

    steps = prof["steps"]
    total = len(steps) + (1 if do_reset else 0)
    try:
        for i, (c, what, hold) in enumerate(steps, 1):
            say(f"\nSTEP {i:2}/{total}: {what}")
            cmd(c, hold)

        if do_reset:
            ch, label = prof["reset_channel"]
            say(f"\nSTEP {total:2}/{total}: RESET TEST (fail-OFF)")
            if a.profile == "bike":
                say("         MASTER stays on so the box has power...")
                esp.send("on master", 0.5)
            say(f"         {label} turns ON and stays on...")
            time.sleep(1.5)
            cmd(f"on {ch}", 3)
            say(f"         ...now the ESP resets: {label} goes OFF once")
            if a.profile == "bike":
                say("         (MASTER clunks off too),")
            say("         then must stay OFF for the whole boot (about 5 s).")
            time.sleep(1.5)
            boot = esp.reset(6)
            st = esp.send("status", 1.5)
            clean, off = boot_is_clean(boot), all_off(st)
            say(f"         [boot clean: {'yes' if clean else 'NO'}   all off after boot: {'yes' if off else 'NO'}]")
            if not (clean and off):
                failures.append("reset test")
            say("         After the reset the supervisor owns MASTER again.")
        else:
            safe_off()
            say("\nAll accessories off; MASTER put back to how it was "
                f"({'on' if master_was_on else 'off'}).")
    except KeyboardInterrupt:
        say("\n\nSTOPPED (Ctrl+C) - switching all accessories off...")
        safe_off()
        say(f"Done. MASTER put back to {'on' if master_was_on else 'off'}.")
    finally:
        say("\n" + "=" * 66)
        say("  ESP side: " + ("ALL STEPS ANSWERED" if not failures else "PROBLEMS: " + ", ".join(failures)))
        say("  The ESP can't hear clicks or see lights - YOUR eyes/ears are the real result.")
        say("=" * 66)
        esp.close()
        log.close()
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
