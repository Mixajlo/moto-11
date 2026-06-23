# moto-11 — V-Strom DL650 Smart Cockpit

Modular ESP32-based smart-cockpit, accessory, and security system for a **2013 Suzuki
V-Strom DL650**. Hardware + firmware + documentation monorepo. Built on weekends; the
philosophy is **"fairings off once"** — provision generously now, upgrade in place later.

## Hardware platform
- **Bike-side controller:** ESP32-WROOM-32D — always-hot, deep-sleeps when parked, wakes
  on ignition. **Two physical units**: a *bench twin* for regression testing and the
  *bike unit*. Identical firmware, distinguished only by `DEVICE_NAME`.
- **Dashboard:** Android tablet (Galaxy Tab Active5 target) as a *thin* BLE display **and**
  an Android Auto receiver (Headunit Reloaded / Revived) projecting the phone. The ESP is
  authoritative; the tablet is UI only and never decides.
- **Phone:** navigation, connectivity, and BLE relay client when parked.
- USB-UART on the ESP boards is **CP2102** (needs the Silicon Labs CP210x driver on Windows).

## Architecture & philosophy
- ESP32 = single source of truth for all bike logic. The tablet renders, never decides.
- **Bench-twin-first:** every change is proven on the bench twin before it touches the bike
  unit. OTA with **A/B rollback** so a bad flash can never brick the bike.
- v1 = on/off control via the mechanical relay box; v2 = MOSFET/PWM dimming as a drop-in on
  the same GPIO enable lines.

## Hard constraints — do not violate
- **Colourblind-safe output, always.** Any diagram, chart, UI, or document uses the
  **Okabe-Ito palette** and never encodes meaning by colour alone — always pair colour with
  line pattern + a text label/tag. The owner is colourblind; this is non-negotiable across
  firmware UI, docs, and any tooling.
- **Fail-safe direction matters.** Accessories (fog, grips, tablet) are **fail-OFF** (ESP
  dies → they go dark, which is safe). Headlights are **fail-ON** (ESP dies → they stay lit).
  The ESP must never be the single point that can leave the bike dark.
- **Safety-critical lighting is never solely behind the ESP** — headlights stay *outside* the
  master-gated relay box.

## Locked-in technical decisions
- **Charging:** series R/R (Shindengen SH775 / SH847), not a MOSFET unit (still shunt).
  Stator: **measure with the INA3221 before any upgrade** — do not pre-emptively replace it.
- **Engine-run detection:** `engineRunning = ignitionHIGH && (Vbus >= 13.2 V)`. Resolves the
  charger-vs-running ambiguity (a tender can't fake it because the key is off). **Power model
  (ADR-0012):** the ignition key is the master switch — once powered, accessories stay on until
  key-off; engine-run only gates first power-on and separates `RUNNING` (charging) from `POWERED`
  (engine off, key on — stall/kill/stop keep power). 10-min POWERED backstop saves the battery
  on a forgotten key; charge-marginal warning while running below the 13.4 V healthy threshold
  (clean ladder, no overlap: V_RUN_OFF 12.9 < V_RUN_ON 13.2 < CHARGE_OK 13.4).
- **Relay coil drive:** ULN2803A Darlington array. The relay box's commoned coil **pin 86 is
  reused as the +12 V coil rail**; each white **pin-85 wire is low-side sunk** by one ULN
  channel. ULN **COM (pin 10) → +12 rail** gives internal flyback — **no external 1N4007s**.
  Active-HIGH (GPIO high → relay on). 10 kΩ pull-down on every ULN input for boot safety.
- **Box gating:** an external ESP-driven **master relay** gates the whole combo box on the
  engine-run signal; this also turns the box's 5 A / 30 A direct-out fuses into engine-switched
  feeds. The **ESP is powered from its own always-hot battery tap (inline 2 A fuse), NOT from
  the box** — it must outlive the master relay for the off-delay and parked sentinel.
- **Headlights:** two bulbs; relay coils triggered from the original start-button feed (relieves
  the starter switch, preserves the factory cranking cut). One bulb always-on; the second on a
  **5-pin NC relay** the ESP energises only to shed in a critical low-power moment. Load-shed
  priority: grips → fog → (last, daylight only) one headlight; never all forward light.
- **Sensing:** INA3221 (3-ch; was INA226 — ADR-0011) over I²C for bus voltage (engine-run +
  battery telemetry). **Always sense
  on a dedicated lead to the battery +** (zero current = no IR drop); never on a load-carrying
  wire. PC817 2-channel opto isolates two 12 V inputs: ignition/run-sense and start-button/
  headlight-feed sense (logic inverts — 12 V present pulls the GPIO low).
- **Connectivity (layered):** riding = the tablet's radio; parked (shared underground garage,
  no WiFi) = opportunistic BLE relay through the owner's/wife's phones draining an NVS event
  queue; v2 = dedicated LTE Cat-M/NB-IoT + GPS + backup LiPo.
- **CAN:** the 2013 DL650 has **no CAN** — its data is **K-line SDS** (6-pin diagnostic port).
  CAN is reserved for the owner's *own* module network (ESP32 TWAI + SN65HVD230 transceiver,
  twisted pair, 120 Ω at both ends).
- **Security/telemetry:** multi-wake (ignition / IMU motion / 24 h timer). 6-axis IMU for
  crash + theft + tilt. Crash → corroborate (orientation + was-moving) → 30 s cancel window →
  SMS via the tablet's LTE.

## Repository layout
- `firmware/` — PlatformIO project. **This is the PlatformIO root — open it directly in VS Code.** (MIT)
- `hardware/` — KiCad (esp-box / future PCB) and relay-box as-built notes. (CERN-OHL-P)
- `harness/`  — WireViz source + rendered output.
- `docs/`     — `electrical-plan.html` and `wiring-schematic.html` (authoritative design
                references), `bench-relay-guide.html` (hands-on bench walkthrough), `adr/`
                decision records, build log. Published via GitHub Pages from `/docs`
                (`.nojekyll` → HTML served as-is, so Pages-facing docs are HTML; markdown like
                ADRs is read on GitHub). (CC-BY-4.0)
- `app/`      — future tablet / Android Auto app.

## Firmware
- PlatformIO: `platform = espressif32`, `board = esp32dev`, `framework = arduino`.
- Environments: `bench` / `bench_ota` / `bike` / `bike_ota`. Shared `src/`; `DEVICE_NAME` is
  set per environment via a build flag.
- Credentials live in `firmware/include/secrets.h` (**gitignored**; `secrets.h.example` is the
  committed template). The OTA password in `secrets.h` must match `--auth` in `platformio.ini`.
- GPIO pin map lives in `src/pins.h` (ADR-0009): relay enables `MASTER_EN`/`FOG_EN`/`GRIP_EN`/
  `SPARE` = GPIO 13/25/26/27 (active-HIGH into the ULN), `IGN_SENSE` = GPIO 34 (RTC wake),
  I²C = 21/22, onboard LED = 2.
- `RelayController` (`src/relays.{h,cpp}`) owns the four coil enables (fail-OFF at boot). A
  serial/Telnet **bench console** (`src/console.{h,cpp}`) drives them by hand: `status`, `on/off
  <ch|all>`, `toggle <ch>`, `selftest`. Onboard LED = heartbeat when all off, solid when any on.
- Engine-run supervisor (`src/supervisor.{h,cpp}`, ADR-0012): `SLEEP→ARMED→RUNNING⇄POWERED` +
  `OFF_DELAY`, drives the master relay; reads via `src/sensors.{h,cpp}` (bench SIM until the
  INA3221/opto are wired — console `sim ign`/`sim vbus`). Leveled logging (`src/log.h`): `LOGD`
  compiles out unless `MOTO_DEBUG` (bench only).
- Current milestone: engine-run supervisor proven on the bench (simulated inputs). **Next:** wire
  the real INA3221 (driver) + PC817 ignition-sense and flip `sim real`; then `manageLoads()`
  load-shedding and the IMU/security layer.

## Dev environment
- **Windows only, PowerShell only.** All shell commands must be PowerShell (or plain `pio`/git
  invocations that work as-is) — never generate bash/Linux-isms (`cp`, `rm`, `&&` chains,
  `/dev/null`, forward-slash-only paths). Use PowerShell equivalents.

## Build / flash / OTA — run from `firmware/`
- Build:          `pio run -e bench`
- USB flash:      `pio run -e bench -t upload`
- Serial monitor: `pio device monitor` (115200)
- OTA flash:      `pio run -e bench_ota -t upload`
  (device must be on the same WiFi; if `vstrom-bench.local` won't resolve on Windows, set the
  device's IP as `upload_port`)
- Bike unit: swap `bench` → `bike`.

## Working agreement
- **Claude Code does:** edit firmware, build, run USB/OTA uploads, read serial output, write and
  run tests, manage git (clear commit messages, push), keep `docs/` and ADRs in sync with code.
- **The owner does (physical, manual):** plug/unplug the ESPs, confirm real-world behaviour
  (LED, relay clicks, multimeter voltages), and all bike wiring/hardware.
- **Never assume:** that the bike bus is CAN (it's K-line SDS); that an upload "worked" without
  serial/multimeter confirmation; that colour alone is acceptable in any output.
- When a real architectural decision is made, record it as a new ADR in `docs/adr/`.

## Where the detail lives
`docs/electrical-plan.html` (23 sections — full electrical design) and
`docs/wiring-schematic.html` (single-sheet wiring + the ULN2803 driver detail). Read those for
the reasoning behind any decision above before changing related code or hardware.
