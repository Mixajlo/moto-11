"""Shared serial/Telnet helpers for the moto-11 console tools.

Talks to the ESP's text console (firmware/src/console.cpp) over:
  - USB serial:  COM4, or "auto" = first Silicon Labs CP210x port found
  - Telnet:      socket://vstrom-bike.local:23  (WiFi; no reset possible)

Run these tools with PlatformIO's Python, which already ships pyserial:
  %USERPROFILE%\\.platformio\\penv\\Scripts\\python.exe
"""
import sys
import time

import serial
import serial.tools.list_ports


def find_port(port="auto"):
    if port != "auto":
        return port
    for p in serial.tools.list_ports.comports():
        if "CP210" in (p.description or "") or "Silicon Labs" in (p.manufacturer or ""):
            return p.device
    sys.exit("No ESP found (no CP210x COM port). Is USB plugged in and the CP210x driver installed?")


class Esp:
    """One console connection. Never touches DTR (IO0), so it can't enter download mode."""

    def __init__(self, port="auto", log=None):
        self.port = find_port(port)
        self.is_serial = not self.port.startswith("socket://")
        self.log = log
        if self.is_serial:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = self.port, 115200, 0.1
            s.dtr = False          # DTR -> IO0: keep high (normal boot)
            s.rts = False          # RTS -> EN:  keep high (not in reset)
            try:
                s.open()
            except serial.SerialException as e:
                sys.exit(f"Can't open {self.port}: {e}\n"
                         "Another program (e.g. 'pio device monitor') may be holding it - close it.")
            self.s = s
        else:
            self.s = serial.serial_for_url(self.port, timeout=0.1)

    def read(self, secs):
        """Collect everything the ESP prints for `secs` seconds."""
        end, out = time.time() + secs, ""
        while time.time() < end:
            d = self.s.read(4096)
            if d:
                out += d.decode("utf-8", "replace")
        if self.log:
            self.log.write(out)
            self.log.flush()
        return out

    def send(self, cmd, wait=1.5):
        self.s.write((cmd + "\r\n").encode())
        return self.read(wait)

    def reset(self, boot_secs=6.0):
        """Pulse EN via RTS and capture the boot log. USB serial only."""
        if not self.is_serial:
            raise RuntimeError("reset needs USB serial (not possible over Telnet)")
        self.s.rts = True
        time.sleep(0.2)
        self.s.rts = False
        return self.read(boot_secs)

    def master_is_on(self):
        out = self.send("state", 1.0)
        return "master=ON" in out

    def close(self):
        self.s.close()


def boot_is_clean(boot_log):
    """Exactly one reset, no brownout/panic, console came up."""
    return (boot_log.count("rst:") == 1
            and "rownout" not in boot_log
            and "Guru" not in boot_log
            and "live on USB" in boot_log)


def all_off(status_out):
    return " ON" not in status_out.split("state", 1)[-1]
