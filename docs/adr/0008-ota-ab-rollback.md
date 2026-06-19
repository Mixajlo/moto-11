# 8. OTA with A/B rollback; bench-twin-first

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The bike-side ESP will be sealed and awkward to reach, so a bad firmware flash must never be
able to brick it. Two identical ESP32 units are on hand, which makes a disciplined test path
possible.

## Decision
Update over the air (**ArduinoOTA over WiFi**), with **A/B partition rollback** as the safety
layer: a new image self-validates after boot and the bootloader reverts to the previous slot if
it fails. Apply **bench-twin-first** discipline — every change is proven on the bench twin
before it touches the bike unit. One codebase builds both via two PlatformIO environments
(`bench` / `bike`), distinguished only by `DEVICE_NAME`.

## Consequences
- Wireless updates with no disassembly; identical, individually-addressable firmware on both units.
- A bad flash self-reverts instead of stranding a sealed controller.
- Regressions are caught on the bench before the bike ever sees them.
- On the bike, OTA happens when in WiFi range or via the tablet's hotspot.
- Requires the two-app-partition scheme (the default ESP32 table provides it) and the
  validate-or-revert logic (the next firmware milestone after basic OTA).

## Notes
Basic ArduinoOTA is proven working on the bench (USB-first flash, then wireless push). The
automatic rollback layer is the next milestone. A known Windows gotcha: OTA's reverse
connection fails behind extra network adapters (WSL/Hyper-V/VMware/VPN) and behind a
third-party AV firewall — disable the virtual adapters and the AV's network shield for the push.

## Alternatives considered
- **USB-only flashing**: rejected — the bike ESP is sealed and hard to reach.
- **OTA without rollback**: rejected — a single bad flash would brick a hard-to-reach unit.
