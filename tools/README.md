# tools/ — PC-side helpers for the ESP console

Small scripts that talk to the firmware's text console (`firmware/src/console.cpp`) over
USB serial or Telnet. Every reusable script written while working on the project lives
here. Licensed MIT, like `firmware/`.

They run on **PlatformIO's own Python**, which already includes `pyserial`, so there is
nothing extra to install:

```powershell
$py = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
```

Only one program can hold the COM port at a time. **Close `pio device monitor`
(`Ctrl+C`) before running any of these.**

## Guided relay test — `run-relay-test.ps1`

Opens a window that prints what you should **see or hear** just *before* each step, then
drives the ESP and checks that it answered. It ends with a **reset test**: one channel
is switched on, the ESP is reset, and the channel must turn off and stay off through
the boot (accessories fail OFF).

```powershell
.\tools\run-relay-test.ps1                 # bench: relay box + yellow LED on master
.\tools\run-relay-test.ps1 -Mode bike      # on the motorcycle: fogs, grips, spare, aux
.\tools\run-relay-test.ps1 -Mode bike -Port socket://vstrom-bike.local:23   # over WiFi
.\tools\run-relay-test.ps1 -NoReset        # skip the reset test
```

| Mode | What happens |
|---|---|
| `bench` | Relays 1→4 on/off one at a time, yellow LED (master), all together, `selftest`, reset test with **aux** on. |
| `leds` | Same steps with **one LED per ULN output** instead of the relay box: pin 18 master, 17 fog, 16 grip, 15 spare, 14 aux (each LED: +12 → 15 kΩ → LED → ULN pin). Reset test uses the AUX LED. |
| `bike` | **MASTER** on (the box gets power), **fog lights** on/off, **heated grips** on for 45 s (a countdown runs; hold the grips), spare, aux. The reset test runs with the **fog lights** on: they must go dark and stay dark. |

Bike mode notes:
- Run it with the **engine running** if you can: fogs and grips draw about 13 A together.
- Headlights are not part of the test. They aren't on the ESP, by design (ADR-0004/0018).
- It never runs `on all` or `selftest` on the bike, so no lamp inrush all at once.
- `Ctrl+C` at any point switches every accessory off and puts MASTER back how it was.
- Over Telnet there is no reset test, because a reset needs the USB RTS line.

Every run writes a log to `tools/logs/` (gitignored).

To add a load or change hold times, edit the `PROFILES` table at the top of
`relay_test.py`. Each step is `(console command, what you should notice, seconds)`.

## Live sense inputs: `run-watch-inputs.ps1`

Opens a window that prints a line every time **IGN**, **START**, the INA3221 **CH1
voltage** (if fitted), or the supervisor **state** changes. Use it while you toggle the
key, or the bench key wire.

```powershell
.\tools\run-watch-inputs.ps1                  # 60 s, sensors left as they are
.\tools\run-watch-inputs.ps1 -Real -Secs 90   # 'sim real': the supervisor follows the real key
```

With `-Real`, the supervisor goes back to SIM at the end. Expected on the bench with no
INA fitted: key in, so `IGN: ON (pin LOW)` and `STATE: ARMED`; key out, so `SLEEP`.
MASTER stays off, because 0.0 V means "engine not running". Logs go to `tools/logs/`.

## Live IMU: `run-watch-imu.ps1`

A window that prints one line per MPU-6050 reading: accel (g), |a|, roll/pitch from
gravity, gyro (deg/s) and temperature. Lines are tagged `<- jolt` or `<- rotating`, and the
peaks are shown at the end. Use it while you tilt, tap or shake the module. At rest |a| is
about 1.00 g in any orientation, and the gyro reads near 0.

```powershell
.\tools\run-watch-imu.ps1 -Secs 120
```

## Security / crash walk: `run-security-walk.ps1`

A guided test of the IMU event logic (ADR-0020). You move the module and the script
watches `motion`:
- it takes a parked reference after 10 s still;
- a tilt of about 10° gives **MOVED**, and it re-arms on the new position;
- taps give **BUMP**;
- on its side gives **TIPPED**, and flat again gives UPRIGHT;
- a knock and then lying on its side gives **CRASH PENDING**, which is auto-cancelled when
  it stands upright again.

For the crash step it fakes "engine running" (SIM) and lowers `crashG` to 1.5 g, then
restores both. It offers `imu cal` if the IMU isn't calibrated.

```powershell
.\tools\run-security-walk.ps1
```

## Supervisor voltage walk: `run-voltage-walk.ps1`

A guided test of the engine-run state machine (ADR-0012) on **real sensors**. You turn the
bench supply (XY-SK35H) and the IGN switch; the script tells you each setting, waits for
the expected state, and prints PASS or FAIL. The steps are:
SLEEP → ARMED → 13.3 V RUNNING (MASTER on) + MARGINAL warning → 13.8 V recovered →
12.4 V POWERED (MASTER held) → 13.8 V RUNNING → key off OFF_DELAY → SLEEP (MASTER off).

```powershell
.\tools\run-voltage-walk.ps1                 # supervisor state machine
.\tools\run-voltage-walk.ps1 -Mode loads     # load policy on the FOG (pin 17) / GRIP (pin 16) LEDs
```

`-Mode loads` (ADR-0019) asks for GRIP and FOG the way the buttons would, then checks five
things on the LEDs:
- the grips wait for the engine;
- at 13.3 V (marginal) the grips are shed, then the fog;
- at 13.8 V the fog comes back first, then the grips;
- on a stall the grips go off but the fog stays on;
- on a restart the grips come back on.

For the walk it sets `shedStep` to 10 s, and restores it at the end.

Bench wiring: the charger feeds **only** the XY input. XY OUT+ is the bench +12 rail (ULN
COM, LEDs, IGN/START switches, INA CH1) and XY OUT− is the bench GND. On the XY, set OVP to
15 V and OCP to 0.5 A, and never go above 15 V. At the end, or on Ctrl+C, the script puts
the ESP back on SIM. Logs go to `tools/logs/`.

## One-off commands — `esp_console.py`

```powershell
& $py tools\esp_console.py status
& $py tools\esp_console.py "on fog:3" "off fog"            # ':3' = listen 3 s afterwards
& $py tools\esp_console.py --reset --boot 6 status         # EN reset, print the boot log
& $py tools\esp_console.py --port COM5 selftest:4
```

## JLCPCB assembly files — `run-jlc-assembly.ps1`

This one is for the KiCad boards, not the ESP. It writes the **BOM** and **pick-and-place
(CPL)** files that JLC's assembly service needs, for both boards, into each board's
`fab-v1/assembly/` folder. Those folders are not committed.

```powershell
.\tools\run-jlc-assembly.ps1                   # mainboard + pwm-board
.\tools\run-jlc-assembly.ps1 -Board pwm-board
```

- **Which parts:** only symbols with an `LCSC` field are listed. Those are the ones JLC
  fits; through-hole and DNP parts are left for hand soldering.
- **What it runs:** `jlc_assembly.py`, which uses `kicad-cli` (KiCad 10) and plain Python.
- **Before running:** save the schematic and the board first.
- **After uploading:** always check the part rotations in JLC's assembly preview.
- **Rotation corrections:** when a footprint needs one, add it to `ROTATION_FIX` in
  `jlc_assembly.py`, so the next export is right first time. So far: SOIC-8 +270°
  (TC4427A) and LED 1206 +180° (XL-3216UGC).
  - Confirm a correction by **re-uploading the CPL**, not from the angle JLC's viewer
    shows: the viewer and the CPL file count rotation in opposite directions.
- **Duplicate lines:** the BOM groups by LCSC part number, because JLC rejects one part on
  two lines (e.g. "100n" and "100nF").

## Shared code — `moto_serial.py`

Port auto-detection (first CP210x), open without touching DTR/IO0 (so the ESP never drops
into download mode), RTS-pulse reset, and the boot-clean / all-off checks.
