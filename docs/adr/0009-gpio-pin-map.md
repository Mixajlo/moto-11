# 9. ESP32 GPIO pin map for relay enables, ignition sense, and I²C

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The wiring schematic fixes the *logical* signals — `MASTER_EN`, `FOG_EN`, `GRIP_EN`,
`SPARE` (active-HIGH into the ULN2803A), `IGN_SENSE` (a wake input), and an I²C bus for the
INA226 and IMU — but never bound them to physical ESP32 pins. The firmware needs concrete
GPIO numbers, and the choice is constrained: the ESP32-WROOM-32D has strapping pins
(0, 2, 5, 12, 15) that must hold particular levels at boot, flash pins (6–11) that are
unusable, and input-only pins (34, 35, 36, 39) with no internal pull and no output driver.
Relay enables must additionally sit **LOW at boot** so no coil energises before `setup()`
runs (accessory fail-OFF), and `IGN_SENSE` must be on an **RTC-capable** pin so it can wake
the ESP from deep sleep via `ext0`.

## Decision
We will use this map (defined once in `firmware/src/pins.h`):

| Signal       | GPIO | Notes                                                     |
|--------------|------|-----------------------------------------------------------|
| `MASTER_EN`  | 13   | low at boot; not strapping/flash                          |
| `FOG_EN`     | 25   | low at boot                                               |
| `GRIP_EN`    | 26   | low at boot                                               |
| `SPARE_EN`   | 27   | low at boot                                               |
| `IGN_SENSE`  | 34   | input-only, RTC-capable → `ext0` ignition wake            |
| `I2C_SDA`    | 21   | Arduino-ESP32 default SDA (INA226 + IMU)                  |
| `I2C_SCL`    | 22   | Arduino-ESP32 default SCL                                 |
| `LED`        | 2    | onboard LED — heartbeat / bench relay-activity indicator  |

The ULN2803A inputs 1–4 are wired to GPIO 13/25/26/27 in that channel order. The owner
wires the bench ULN to match this map.

## Consequences
- Relays are guaranteed OFF through boot: the chosen output pins are low at reset and the
  ULN inputs also carry 10 kΩ pull-downs (ADR-0003) — defence in depth for fail-OFF.
- `IGN_SENSE` on GPIO 34 keeps the deep-sleep wake path open without burning a general-
  purpose pin; being input-only is fine because the ignition front-end is a passive divider.
- GPIO 2 is the onboard LED *and* a strapping pin; using it only as an output indicator (not
  driving it during reset) is safe and frees no constraint we need elsewhere.
- Plenty of headroom remains (e.g. 14, 32, 33, 4, 16, 17) for the headlight-shed NC relay,
  the second PC817 sense channel, CAN TWAI, and future loads.

## Alternatives considered
- **Putting a relay enable on GPIO 2/4/5/12/15** (strapping/onboard-LED pins): rejected — a
  HIGH on a strapping pin at reset can change boot mode or flash voltage, and any glitch
  risks a coil twitching at power-up. The relay lines must be boot-quiet.
- **`IGN_SENSE` on a non-RTC GPIO** (e.g. 16/17): rejected — it would forfeit `ext0`
  deep-sleep wake on ignition, which is core to the parked-sentinel design.
- **Non-default I²C pins**: rejected — no reason to deviate from 21/22; staying on the
  Arduino default avoids a `Wire.begin(sda, scl)` footgun later.
