<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# Mainboard — PCB layout refine checklist

Tracking list for the layout pass. The first placement is a **guesstimate** (done before all
hardware is in hand) just to validate size/shape. Work through this before generating fab files.
See [03-design-rules.md](03-design-rules.md) for the numbers and [04-kicad-guide.md](04-kicad-guide.md)
for the how-to.

## Board strategy / phasing (decided 2026-06)
- **Now:** bench-validate the design using through-hole parts / modules / the existing
  ESP + relay box. No PCB ordered yet.
- **Before fab:** lock the **form factor against the real mounting location** — the current
  rectangular outline is a *reference prototype* only; final shape is TBD (likely non-rectangular).
- **Production board:** redesign as **SMD**, with parts chosen from the assembler's catalog
  (JLCPCB/LCSC, prefer "basic" parts), and **manufacturer-assembled (PCBA)** so there are no
  parts to hand-source/solder. The **schematic/netlist carries over** — only footprints
  (THT→SMD) and the outline change.

## Placeholders to replace (put in for the rough pass)
- [ ] **U3 (INA3221)** — currently `PinSocket_1x06` placeholder. Replace with the real custom
  footprint once measured (header pin count + 2.54 mm pitch + CH1 hole-pair spacing). Goes in
  `mainboard.pretty` alongside `MP1584_Module`.
- [ ] **Q1 (P-FET)** — currently `TO-220-3_Vertical` placeholder. Confirm once the FET is ordered
  (suggested **IRF4905**). Its pinout is **G-D-S**, so switch the symbol to `Q_PMOS_GDS` and
  re-verify pad mapping (D→+12V_HOT, S→+12V_PROT).
- [ ] **A3 (MP1584)** — custom `MP1584_Module` footprint done; **verify pad order against the
  physical board** at assembly (pad 1 = IN+, pad 3 = OUT+).

## Decisions still open
- [x] **Opto**: resolved — **bare PC817 DIP-4, integrated** (current design). Buy new chips;
  the 2-channel module stays whole as a bench tool. (Production board uses an SMD opto.)
- [ ] Confirm **MP1584 / INA3221 / IMU** module pin orders vs their silkscreens.

## Board setup (Board Setup → Design Rules / Net Classes)
- [ ] Net classes: **Power** (`+12V_HOT/PROT`, `+5V`, `COIL_*`, `COIL_RAIL`) = **1.5 mm**;
  **Rail3V3** (`+3V3`) = **0.8 mm**; **Default/signal** = **0.30 mm**.
- [ ] Clearance **0.2 mm**; via 0.4/0.8 mm.

## Placement / mechanical
- [ ] DevKitC (J8/J9) along one edge, **antenna end overhanging** — copper keep-out under the
  WROOM antenna, any layer.
- [ ] **USB end of the DevKitC reachable** for reflashing (orient to a board edge).
- [ ] Terminal blocks J1–J5 on the **field-wiring edge**, one side.
- [ ] Buck loop tight; INA3221 sense (`R14`/`C10`/`D3`) short, away from the buck switch node.
- [ ] **Opto isolation gap ≥ 4 mm** across U4/U5; **no ground pour under the opto barrier**.
- [ ] 4× **M3 mounting holes**, ~4 mm from corners.
- [ ] Resize **Edge.Cuts** to the real outline once placement settles (~90 × 70 mm guess).

## Routing
- [ ] Route **power nets first** (1.5 mm), then signals (0.3 mm).
- [ ] **GND pour both layers**, stitched with vias (~10 mm grid + around the ESP/RF area).
- [ ] Keep I²C as a pair, away from the buck.

## Verify before fab
- [ ] **DRC** clean (Inspect → Design Rules Checker).
- [ ] Re-check fail-safe: ULN input pull-downs, no external flyback (COM→COIL_RAIL), GPIO34/35
  pull-ups, antenna keep-out, opto barrier intact.
- [ ] **Plot Gerbers** + drill (`File → Plot`, `File → Fabrication Outputs`) → zip → JLCPCB
  (2-layer, 1.6 mm, HASL). Outputs go to `gerbers/` (gitignored).

## After boards arrive
- [ ] Hand-assemble, then run the [bench relay guide](../../docs/bench-relay-guide.html) against
  the real board (relay clicks via ULN, 5 V/3V3 rails at test points, opto inversion).
- [ ] Fold any rework into a v1.1 note here; promote firm changes to an ADR.
