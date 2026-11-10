"""Live IMU watcher: polls the ESP's `imu` command and prints one compact line per
reading - accel (g), |a|, roll/pitch from gravity, gyro, temperature - plus the
peak |a| and peak rotation seen so far. Use it while you tilt, tap or shake the
MPU-6050 on the bench.

  python watch_imu.py                 # 60 s, ~4 readings/s, auto COM port
  python watch_imu.py --secs 120 --port COM4
  python watch_imu.py --replay logs\watch-imu-<stamp>.log   # re-parse a saved run

At rest |a| should read ~1.00 g in any orientation, and the gyro should be within a
few deg/s of zero. Logs go to tools/logs/. Usually launched through run-watch-imu.ps1.
"""
import argparse
import datetime
import os
import re
import time

from moto_serial import Esp

NUM = r"\s*([+-]?\d+\.\d+)"        # the firmware pads numbers (%+7.2f), so allow spaces
ACC = re.compile(rf"accel\s+x{NUM}\s+y{NUM}\s+z{NUM} g\s+\|a\|{NUM}")
TILT = re.compile(rf"roll{NUM} deg\s+pitch{NUM} deg")
GYR = re.compile(rf"gyro\s+x{NUM}\s+y{NUM}\s+z{NUM}")
TMP = re.compile(rf"temp{NUM} C")


def parse(block):
    """One `imu` reply -> dict, or None if it isn't a complete reading."""
    acc, tilt, gyr, tmp = ACC.search(block), TILT.search(block), GYR.search(block), TMP.search(block)
    if not (acc and tilt and gyr):
        return None
    ax, ay, az, mag = (float(x) for x in acc.groups())
    roll, pitch = (float(x) for x in tilt.groups())
    gx, gy, gz = (float(x) for x in gyr.groups())
    return dict(ax=ax, ay=ay, az=az, mag=mag, roll=roll, pitch=pitch, gx=gx, gy=gy, gz=gz,
                temp=tmp.group(1) if tmp else "?")


def line(t, r):
    rot = max(abs(r["gx"]), abs(r["gy"]), abs(r["gz"]))
    flag = "  <- jolt" if abs(r["mag"] - 1.0) > 0.3 else ("  <- rotating" if rot > 30 else "")
    return (f"{t:6.1f}s | {r['ax']:+.2f}  {r['ay']:+.2f}  {r['az']:+.2f} | {r['mag']:.2f} | "
            f"{r['roll']:+6.1f} {r['pitch']:+6.1f} | {r['gx']:+7.1f} {r['gy']:+7.1f} {r['gz']:+7.1f} | "
            f"{r['temp']} C{flag}"), rot


def replay(path):
    """Re-parse a saved watch log (e.g. after a parser fix) and print the summary."""
    text = open(path, encoding="utf-8", errors="replace").read()
    blocks = text.split("IMU @0x")[1:]
    rows = [r for r in (parse(b) for b in blocks) if r]
    print(f"replay {path}: {len(rows)} readings")
    peak_a = peak_g = 0.0
    for i, r in enumerate(rows):
        s, rot = line(i * 0.3, r)                 # ~0.3 s per reading in a live run
        peak_a, peak_g = max(peak_a, r["mag"]), max(peak_g, rot)
        print(s)
    print(f"\npeaks: |a| {peak_a:.2f} g, rotation {peak_g:.0f} deg/s")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--secs", type=float, default=60)
    ap.add_argument("--port", default="auto")
    ap.add_argument("--replay", metavar="LOG", help="re-parse a saved watch-imu log instead of reading the ESP")
    a = ap.parse_args()
    if a.replay:
        replay(a.replay)
        return

    logdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
    os.makedirs(logdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    logpath = os.path.join(logdir, f"watch-imu-{stamp}.log")
    log = open(logpath, "w", encoding="utf-8")

    def say(msg):
        print(msg, flush=True)
        log.write(msg + "\n")
        log.flush()

    esp = Esp(a.port, log=log)
    esp.read(0.3)
    esp.sync()
    first = esp.send("imu", 0.4)
    if "NOT found" in first:
        say("IMU not found - check VCC=3V3, GND, SDA=21, SCL=22, then run 'imu probe' (or reset the ESP).")
        esp.close()
        return
    say(f"moto-11 IMU watch  port {esp.port}  log {logpath}  ({a.secs:.0f} s, Ctrl+C to stop)")
    say("   time |   ax     ay     az  |  |a|  | roll   pitch |   gx      gy      gz   | temp")
    peak_a, peak_g, t0 = 0.0, 0.0, time.time()
    try:
        while time.time() - t0 < a.secs:
            r = parse(esp.send("imu", 0.25))
            if not r:
                continue
            s, rot = line(time.time() - t0, r)
            peak_a, peak_g = max(peak_a, r["mag"]), max(peak_g, rot)
            say(s)
    except KeyboardInterrupt:
        pass
    say(f"\npeaks: |a| {peak_a:.2f} g, rotation {peak_g:.0f} deg/s")
    esp.close()
    log.close()


if __name__ == "__main__":
    main()
