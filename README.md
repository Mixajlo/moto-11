# moto-11

A modular **ESP32 smart-cockpit, accessory, and security system** for a 2013 Suzuki
V-Strom DL650 — hardware, firmware, harness, and documentation in one repository.

The guiding philosophy is **"fairings off once"**: provision generously now, upgrade in
place later. The ESP32 owns all bike logic; a tablet is a thin display; the phone handles
navigation and connectivity.

---

## What it is

`moto-11` turns the bike into a programmable platform without making any consumer device a
single point of failure:

- **Bike-side controller** — an ESP32-WROOM-32D that is always-hot, deep-sleeps when parked,
  and wakes on ignition. It is the single source of truth for relay control, sensing,
  load management, security, and the parked sentinel.
- **Dashboard** — an Android tablet acting as a *thin* BLE display and an Android Auto
  receiver. UI only; it never decides anything.
- **Phone** — navigation, connectivity, and the BLE relay that drains the bike's event queue
  when it's parked out of network range.

It also folds in the unglamorous-but-important electrical work: a series R/R charging upgrade,
relieving the notorious headlight-through-the-starter-switch wiring, accessory switching via a
relay box driven by a ULN2803, and battery/charging telemetry from an INA226.

## Status

**Early development.** Basic OTA firmware (WiFi + ArduinoOTA + heartbeat) is proven on the
bench across both ESP units. Next firmware milestone: A/B rollback validation, then the cockpit
logic (ignition/run-sense → relay enables → INA226 voltage read). Hardware is at the
bench-prototyping stage. See `docs/adr/` for the decisions made so far.

## Architecture

```
        ┌─────────────┐    BLE     ┌──────────────────────┐
        │   ESP32     │◄──────────►│  Tablet (thin UI +   │
        │ authoritative│            │  Android Auto recv)  │
        │ controller   │            └──────────────────────┘
        └──────┬───────┘                       ▲ projects
   I²C / GPIO / │ relays                        │ Android Auto
  ┌─────────────┼───────────────┐        ┌──────┴──────┐
  │ INA226  IMU  ULN2803  opto   │        │   Phone     │
  │ relay box   master relay     │        │ nav + relay │
  └──────────────────────────────┘        └─────────────┘
```

The ESP runs the bike; the tablet renders; the phone connects. A tablet reboot or removal
degrades only the UI, never the bike's function. Full reasoning is in
[`docs/adr/0001`](docs/adr/0001-esp32-authoritative-controller.md).

## Repository layout

```
moto-11/
├── CLAUDE.md          # project context for Claude Code (read first)
├── firmware/          # PlatformIO ESP32 project        (MIT)
│   ├── platformio.ini
│   ├── src/main.cpp
│   └── include/secrets.h.example
├── hardware/          # KiCad / relay-box as-built       (CERN-OHL-P)
├── harness/           # WireViz source + rendered output
├── docs/              # design docs + decision records    (CC-BY-4.0)
│   ├── electrical-plan.html
│   ├── wiring-schematic.html
│   └── adr/
└── app/               # future tablet / Android Auto app
```

## Hardware

| Area            | Choice                                                              |
|-----------------|--------------------------------------------------------------------|
| Controller      | ESP32-WROOM-32D (×2: bench twin + bike unit)                        |
| Charging        | Series R/R (Shindengen SH775 / SH847); stock stator, measured first |
| Accessory switch| Pre-wired 4-relay / 6-fuse box, gated by an external master relay   |
| Coil driver     | ULN2803A (low-side, internal flyback)                              |
| Sensing         | INA226 (bus voltage) + PC817 opto-isolated 12 V inputs + 6-axis IMU |
| Dashboard       | Android tablet (Galaxy Tab Active5 target) + phone                 |

The authoritative electrical design is in `docs/electrical-plan.html` (23 sections) and
`docs/wiring-schematic.html` (single-sheet wiring + ULN2803 driver detail).

## Getting started (firmware)

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the **PlatformIO IDE** extension
- **Silicon Labs CP210x USB-to-UART driver** (the ESP boards use a CP2102 — without it Windows
  shows no COM port)
- Git

### Clone and open
```bash
git clone https://github.com/<you>/moto-11.git
```
Open the **`firmware/`** subfolder in VS Code (`File → Open Folder → moto-11/firmware`).
PlatformIO expects `platformio.ini` at the root of the opened folder, so open `firmware/`
directly — not the repo root. (Git still tracks the whole repo.) On the first build,
PlatformIO downloads the ESP32 toolchain and framework; let it finish.

### Configure secrets
Copy the template and fill it in — it is gitignored, so your credentials never reach the repo:
```bash
cp include/secrets.h.example include/secrets.h
```
Set `WIFI_SSID`, `WIFI_PASS`, and an `OTA_PASS`. The `OTA_PASS` must match the `--auth=` value
in `platformio.ini`. Avoid `%` and `$` in the OTA password (the config parser treats them
specially).

### Build, flash, monitor
One codebase builds two devices via separate environments (`bench` / `bike`), distinguished by
`DEVICE_NAME`. First flash of each device is over USB:
```bash
pio run -e bench                 # build
pio run -e bench -t upload       # flash over USB
pio device monitor               # serial logs @ 115200
```
Swap `bench` → `bike` for the second unit. (In VS Code, the PlatformIO **Project Tasks** panel
exposes Build / Upload / Monitor per environment — use those rather than the status-bar buttons,
which act on whatever environment is selected.)

### OTA updates
After the first USB flash, update wirelessly:
```bash
pio run -e bench_ota -t upload
```
Set the device's IP (from the serial log) as `upload_port` in the `*_ota` environment if the
`.local` mDNS name doesn't resolve on your OS.

### Windows OTA gotcha
ArduinoOTA needs the ESP to open a connection *back* to your PC. Two things block this on
Windows:
- **Extra network adapters** (WSL/Hyper-V `vEthernet`, VMware, VirtualBox, VPN) — espota can
  advertise the wrong host IP. Disable them so only your real LAN/Wi-Fi adapter is active.
- **A third-party antivirus firewall** — independent of the Windows firewall; pause its network
  shield for the push.

Symptom is `Authenticating…OK` followed by `Waiting for device… No response`. See
[`docs/adr/0008`](docs/adr/0008-ota-ab-rollback.md).

## Documentation

- **Design docs** (`docs/*.html`) — enable **GitHub Pages** on `/docs` to read them as web
  pages at `https://<you>.github.io/moto-11/electrical-plan.html`.
- **Decision records** (`docs/adr/`) — the *why* behind every major choice, Nygard-format.
- **`CLAUDE.md`** — the condensed project brief; also what Claude Code reads on startup.

## Design principles

- **Colourblind-safe output, always.** Every diagram, chart, and UI uses the Okabe-Ito palette
  and never encodes meaning by colour alone — colour is always paired with line pattern and a
  text label. The maintainer is colourblind; this is a hard constraint.
- **Fail-safe by direction.** Accessories fail **OFF** (safe when the ESP dies); headlights fail
  **ON**. Safety-critical lighting is never solely behind the controller.
- **Bench-twin-first.** Every change is proven on the bench twin before it reaches the bike.

## Licensing

This repo is multi-licensed by content type:

| Path         | License        |
|--------------|----------------|
| `firmware/`  | MIT            |
| `hardware/`  | CERN-OHL-P-2.0 |
| `harness/`   | CERN-OHL-P-2.0 |
| `docs/`      | CC-BY-4.0      |

Full texts live in `LICENSES/`. SPDX identifiers in file headers make this machine-readable;
the [REUSE](https://reuse.software) tool lints it.

## Disclaimer

This project modifies a motorcycle's electrical, charging, and lighting systems. Do this only
if you understand the risks — incorrect wiring can damage the bike, the battery, or yourself,
and some changes (e.g. lighting behaviour) may be subject to local road regulations. Use at your
own risk. Not affiliated with or endorsed by Suzuki.
