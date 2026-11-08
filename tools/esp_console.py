"""Send console commands to the ESP and print the replies (non-interactive).

Each argument is a console command, optionally followed by ':<seconds>' to listen
afterwards (default 1.5 s).

  python esp_console.py status
  python esp_console.py "on fog:3" "off fog"
  python esp_console.py --reset --boot 6 status        # EN reset first, show boot log
  python esp_console.py --port COM5 selftest:4
  python esp_console.py --port socket://vstrom-bench.local:23 status   # over WiFi
"""
import argparse
import sys

from moto_serial import Esp

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--port", default="auto", help="COMx, socket://host:23, or auto (default)")
ap.add_argument("--reset", action="store_true", help="pulse EN (USB only) and print the boot log first")
ap.add_argument("--boot", type=float, default=6.0, help="seconds to capture after --reset")
ap.add_argument("commands", nargs="*")
a = ap.parse_args()

esp = Esp(a.port)
if a.reset:
    print(">>> EN reset")
    sys.stdout.write(esp.reset(a.boot))
else:
    esp.read(0.3)
for c in a.commands:
    cmd, _, w = c.partition(":")
    print(f"\n>>> {cmd}")
    sys.stdout.write(esp.send(cmd, float(w) if w else 1.5))
    sys.stdout.flush()
esp.close()
