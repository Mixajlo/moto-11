<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# Mainboard — PCB layout refine checklist

Tracking list for the layout pass. The first placement is a **guesstimate** (done before all
hardware is in hand) just to validate size/shape. Work through this before generating fab files.
See [03-design-rules.md](03-design-rules.md) for the numbers and [04-kicad-guide.md](04-kicad-guide.md)
for the how-to.

## Board strategy / phasing (decided 2026-06, revised 2026-10-31)
- **Now:** bench-validate the design using through-hole parts / modules / the existing
  ESP + relay box. No PCB ordered yet.
- **Before fab:** lock the **form factor against the real mounting location** — the current
  188 × 101 mm rectangle is a *reference prototype* only.
  **Resolved ([ADR-0015](../../docs/adr/0015-piggyback-enclosure-mixed-assembly.md)):** the board
  goes in its **own sealed enclosure, close-coupled beside the stock relay box** (inside the box is
  ruled out), so the **enclosure** sets the outline — target **~110 × 80 mm**. The exact
  attachment interface (to the box vs. to the tray) is still open; it does not affect the outline. Measurement sets and the ordered work
  queue live in **[06-mechanical-mounting.md](06-mechanical-mounting.md)**; that doc is the fab gate.
- **This board (v1) = hand-assembled (revised 2026-10-31):** no JLCPCB assembly. 1206 passives,
  U2 ULN2803 in a DIP-18 socket, U4/U5 DIP-4, and the INA3221, MP1584 and GY-521 as socketed modules.
- **Next revision:** full **SMD** from the assembler's catalog (JLCPCB/LCSC, prefer "basic" parts),
  no modules — the only route to a board that matches the relay box's 100 × 70 footprint. The
  **schematic/netlist carries over** — only footprints and the outline change.

## Placeholders to replace (put in for the rough pass)
- [ ] **U3 (INA3221)** — currently `PinSocket_1x06` placeholder. Replace with the real custom
  footprint once measured (header pin count + 2.54 mm pitch + CH1 hole-pair spacing). Goes in
  `mainboard.pretty` alongside the MP1584 footprint.
- [x] **Q1 symbol — FIXED 2026-10-31.** Was `Q_PMOS_GSD` (pads 2=S, 3=D) against a real pinout of
  **1=G, 2=D, 3=S**, which put **Drain on the load and Source on the battery** — i.e. reverse-
  polarity protection wired backwards, and *invisible on the bench* because a backwards P-FET still
  conducts normally forward. Swapped to **`Q_PMOS_GDS`**; verified pin 2 = D → `+12V_HOT`,
  pin 3 = S → `+12V_PROT`. No rewiring was needed (both symbols share the same geometry; only the
  pin numbers swap). `02-bom.csv` had the orientation stated backwards too — also corrected.
- [x] **Q1 gate clamp connected — FIXED 2026-10-31.** `Dz1` cathode and `R11` were on a two-pin net
  with no link to `+12V_PROT`, so the gate had no clamp. Now on `+12V_PROT` (confirmed in the
  netlist). The PCB still needs the sync.
- [x] **Schematic values set 2026-10-31:** `R12` 100 → **10k**, `D1` SMBJ24A → **SMBJ16A** (same
  SMB footprint).
- [x] **PCB synced 2026-10-31** (KiCad `F8`): schematic parity 0, all pad nets match. The old
  routing is now invalid around `Q1`, `R12` and `U2`, so DRC fails by design until the re-route.
  **Do not generate fab files from this routing.**
- [x] **Q1 package — decided 2026-10-31: IRF4905, TO-220**, matching the existing footprint. Pad
  order 1=G, 2=D, 3=S, so `Q_PMOS_GDS` stays correct. Check gate drive at 8–11 V against the
  datasheet when buying.
- [ ] **A3 (MP1584)** — custom `MP1584_Module_22.5x17.2mm` footprint done; **verify pad order against the
  physical board** at assembly (pad 1 = IN+, pad 3 = OUT+).

## Decisions still open
- [x] **Opto**: resolved — **bare PC817 DIP-4, integrated** (current design). Buy new chips;
  the 2-channel module stays whole as a bench tool. (Production board uses an SMD opto.)
- [ ] Confirm **MP1584 / INA3221 / IMU** module pin orders vs their silkscreens.

## Board setup (Board Setup → Design Rules / Net Classes)
- [x] Net classes exist and match spec — **Power** 1.5 mm, **Rail3V3** 0.8 mm, **Default** 0.30 mm,
  all with via 0.8/0.4. Note the 3V3 net is actually named **`+3.3V`** (not `+3V3`), and the
  `Rail3V3` pattern matches it correctly.
- [x] **Coil outputs assigned to Power — FIXED 2026-10-31.** The only coil pattern was
  `*COIL_RAIL`, so `/COIL_MASTER`, `/COIL_FOG`, `/COIL_GRIP` and `/COIL_SPARE` were falling through
  to **Default (0.30 mm)** despite `03-design-rules.md` putting coil outputs in **Power (1.5 mm)**.
  All four now have explicit `netclass_patterns` entries.
- [x] Clearance **0.2 mm**; hole-to-hole 0.25 mm; via 0.4/0.8 mm — verified in the project file.
- [ ] **Reload the project in KiCad** after the above (netclasses live in `mainboard.kicad_pro`; an
  already-open KiCad holds its own copy and will overwrite them on save).

## Placement / mechanical
- [ ] DevKitC (J8/J9) along one edge, **antenna end overhanging** — copper keep-out under the
  WROOM antenna, any layer.
- [ ] **USB end of the DevKitC reachable** for reflashing (orient to a board edge).
- [ ] Terminal blocks J1–J4 on the **field-wiring edge**, one side; J10 (button JST) on an edge the handlebar cable reaches.
- [ ] Buck loop tight, away from the I²C pair. (INA CH1 sense is off-board — no on-board sense network to place.)
- [ ] INA3221 module (U3) socketed near the I²C side; leave clearance for its body (~25×19 mm) per the custom footprint keep-out.
- [ ] **Opto isolation gap ≥ 4 mm** across U4/U5; **no ground pour under the opto barrier**.
- [ ] 4× **M3 mounting holes** near the board corners; the enclosure floor gets drilled to match
      and the board sits on spacers (ADR-0015). The board currently has **zero** mounting holes.
- [x] **Layout redone 2026-10-31:** Edge.Cuts is **110 × 70 mm**, re-placed, re-routed and stitched
      (GND pour both layers, antenna notch). DRC: 0 errors, 0 unconnected, parity 0.
- [x] **5-channel relay drive, re-routed 2026-11-05** ([ADR-0018](../../docs/adr/0018-five-channel-relay-drive.md)):
      `COIL_RAIL` removed (ULN COM on `+12V_PROT`), J2 = master relay, J3 = four box relays, ULN ch5
      = `AUX_EN` on GPIO 12 with R23, `/COIL_AUX` in Power. Re-routed from a blank slate in
      Freerouting; ~23 GND stitching vias join the pour fragments that the coil and button tracks
      split. DRC: 0 unconnected; only the H5–H7 NPTH-in-U3-courtyard errors remain (accepted).
      `fab-v1` gerber zip regenerated.
- [x] **ESP32 row spacing confirmed 25.40 mm** from the module drawing (`wroom-32D-dim.jpg`).
- [ ] **Before ordering, check against the real modules:** MP1584, INA3221 and GY-521 pin order, and
      the GY-521 body side relative to its socket.

## Routing
- [ ] Route **power nets first** (1.5 mm), then signals (0.3 mm).
- [ ] **GND pour both layers**, stitched with vias (~10 mm grid + around the ESP/RF area).
- [ ] Keep I²C as a pair, away from the buck.

## Verify before fab
- [ ] **DRC** clean (Inspect → Design Rules Checker).
- [ ] Re-check fail-safe: ULN input pull-downs, no external flyback (COM→`+12V_PROT`), GPIO34/35
  pull-ups, antenna keep-out, opto barrier intact.
- [ ] **Plot Gerbers** + drill (`File → Plot`, `File → Fabrication Outputs`) → zip → JLCPCB
  (2-layer, 1.6 mm, HASL). Outputs go to `gerbers/` (gitignored).

## After boards arrive
- [ ] Hand-assemble, then run the [bench relay guide](../../docs/bench-relay-guide.html) against
  the real board (relay clicks via ULN, 5 V/3V3 rails at test points, opto inversion).
- [ ] Fold any rework into a v1.1 note here; promote firm changes to an ADR.
