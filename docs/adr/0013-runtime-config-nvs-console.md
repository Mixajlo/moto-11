# 13. Runtime-configurable tunables: NVS-backed, edited via the console (BLE/app later)

- Status: Accepted
- Date: 2026-06-23
- Deciders: Miki

## Context
The supervisor has several tunables — voltage thresholds (`vRunOn`, `vRunOff`,
`chargeWarn`, `chargeClear`) and timers (`runDebounce`, `stallDebounce`, `offDelay`,
`backstop`, `chargeDebounce`) — and more are coming (button→action mappings). The
owner will want to adjust these **without reflashing**, ideally from the tablet
eventually. Today they're compile-time constants in `supervisor.h`. We need
persistence across reboot/OTA and a way to edit them, starting as simply as possible.

## Decision
A small **`Config` module backed by ESP32 NVS** (via the Arduino `Preferences`
library). NVS is a dedicated key/value flash partition (~20 KB), purpose-built for
this; our config is a few dozen bytes, with wear-leveling and survival across reboot
and OTA (separate partition).

- **Defaults live in firmware** (`supervisor.h`) and are the fallback. At boot,
  `Config` loads NVS; if a key is absent or the stored **schema version** doesn't
  match, the compile-time default is used. Loaded values are applied to `Engine`.
- **Edit via the existing serial/Telnet console** first — `config` (dump), `get
  <key>`, `set <key> <value>`, `config reset` (restore defaults). This is the
  simplest interface and needs no new hardware; it also forces us to build the
  validation core that later front-ends reuse.
- **Validate every write**: clamp to sane min/max and **enforce the ladder**
  `vRunOff < vRunOn < chargeWarn < chargeClear`. Reject/clamp invalid input; never
  persist an inconsistent set. A corrupt NVS falls back to safe firmware defaults.
- **Layered access, one backend:** console now → **BLE GATT** characteristics
  (tablet) → a future **Android config app** — all read/write the same `Config`/NVS
  core. (Roadmap.)

## Consequences
- Tune on the bench or bike with no reflash; changes persist and are OTA-safe.
- Fail-safe by construction: firmware defaults are the floor; validation blocks
  footguns (e.g. `offDelay` absurdly large, or thresholds out of order).
- Negligible flash wear (writes only on change).
- The console layer is testable now; BLE/app are later UI on top.
- Adds a tiny module + a schema-version constant to bump when the config shape changes.

## Alternatives considered
- **Hardcoded constants (reflash to change):** what we have — rejected; the owner
  needs field adjustment.
- **SPIFFS/LittleFS JSON file:** heavier than needed for ~10 scalars; NVS is simpler
  and more robust for key/value.
- **Legacy EEPROM emulation:** superseded by NVS on ESP32.
- **BLE/app first:** more work before any payoff; the console backend is the
  prerequisite anyway, so it goes first.
