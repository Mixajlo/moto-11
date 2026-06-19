<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# moto-11 — hardware

KiCad designs and as-built notes for the bike electronics. Licensed **CERN-OHL-P-2.0**
(permissive open hardware) — full text in [`../LICENSES/`](../LICENSES/).

> **Hardware safety:** this drives a motorcycle's 12 V electrical system. Nothing here is
> proven until it's bench-tested per the [bench relay guide](../docs/bench-relay-guide.html).
> Fail-safe direction is non-negotiable: accessories fail **OFF**, headlights fail **ON**.

## Boards

| Board | What it is | Status |
|-------|-----------|--------|
| [`mainboard/`](mainboard/) | ESP32-DevKitC carrier + ULN2803 coil driver + sensing front-end (INA226, PC817 opto, IMU) + connectors. Drives the **existing** pre-wired relay/fuse box; it does **not** carry load current. | 📐 design package (pre-KiCad) |

The pre-wired 4-relay / 6-fuse box and the external master relay stay as discrete parts — the
mainboard is the brain that drives their coils and senses the bus. High load current (fog,
grips, tablet) never flows through this PCB.

## How this is organised

Per the chosen workflow, the design is captured as a **text design package** first, then drawn
in KiCad from it:

- [`mainboard/01-schematic-spec.md`](mainboard/01-schematic-spec.md) — the schematic as a
  component list + net-by-net connection table. This is the source of truth for capture.
- [`mainboard/02-bom.csv`](mainboard/02-bom.csv) — bill of materials with part numbers.
- [`mainboard/03-design-rules.md`](mainboard/03-design-rules.md) — board stackup, trace widths
  (sized from real currents), clearances, connectors, mechanical.
- [`mainboard/04-kicad-guide.md`](mainboard/04-kicad-guide.md) — step-by-step to turn the spec
  into a KiCad schematic + PCB, with DRC and fab outputs.

KiCad project files (`*.kicad_pro/_sch/_pcb`) land in `mainboard/` once capture begins.

## Toolchain

**KiCad 10.0.3** is installed on the build machine. Start with
[`04-kicad-guide.md`](mainboard/04-kicad-guide.md). The capture/layout workflow is stable across
KiCad 8–10, so the guide applies regardless of minor version.

## Cross-references

- GPIO pin map: [ADR-0009](../docs/adr/0009-gpio-pin-map.md) (this board adds two pins — see the
  schematic spec's *open decisions*).
- Coil driver rationale: [ADR-0003](../docs/adr/0003-uln2803-coil-driver.md).
- Master-relay gating / fail-safe: [ADR-0004](../docs/adr/0004-master-relay-gating-and-failon-headlights.md).
- Sensing: [ADR-0005](../docs/adr/0005-ina226-sensing.md).
- Authoritative electrical design: [`../docs/electrical-plan.html`](../docs/electrical-plan.html)
  and [`../docs/wiring-schematic.html`](../docs/wiring-schematic.html).
