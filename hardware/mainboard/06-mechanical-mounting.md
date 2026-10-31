<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# Mainboard — mechanical: enclosure, board outline & relay-box mounting

How the mainboard PCB gets a **final outline** and how the finished unit **attaches to the
relay box**. This is the gate on [05-layout-todo.md](05-layout-todo.md)'s *"lock the form factor
against the real mounting location"* item — nothing gets ordered until the numbers below are
filled in.

Decided 2026-10-31, recorded as [ADR-0015](../../docs/adr/0015-piggyback-enclosure-mixed-assembly.md):

- **Outside the box, own enclosure, close-coupled.** Inside the relay box is **ruled out** — not
  enough space above the ~28 mm relays, and it would put the logic in among the switching relays.
  The PCB lives in its **own sealed enclosure** sitting immediately beside the **stock, sealed**
  relay box, which keeps [electrical-plan.html](../../docs/electrical-plan.html) §21's *two
  enclosures, not one* reasoning intact and the coil/power/GND leads short.
- **Frame mount decided 2026-10-31: the two M6 seat-latch screws** on the under-seat cross-member
  (opposite corners of ~40 × 20 mm, ~45 mm diagonal). The enclosure hangs from these. The PCB does
  not use them.
- **PCB holes are free.** The owner drills the enclosure floor to match the PCB and uses spacers,
  so measurements B4 and B5 no longer apply and do not gate the order. The PCB only has to fit the
  enclosure's internal envelope.
- **Hand-assembled (revised 2026-10-31):** no JLCPCB assembly. 1206 passives, **U2 ULN2803 in a
  DIP-18 socket**, U4/U5 DIP-4 (the 7.62 mm body delivers the isolation creepage), and the INA3221,
  MP1584 and GY-521 as socketed modules.

## Read this first: what blocks what

These are two different critical paths, and it is easy to conflate them.

| To finish… | You need… | Blocks the PCB order? |
|---|---|---|
| **Board outline + mounting holes** | the **enclosure's** internal dimensions and standoff pattern | **YES** |
| **The bracket** between the two boxes | the **relay box's** external dimensions | No — bracket is printed/cut after |
| Coil-lead / power / GND run length | relay box pigtail free length | No, but sets how far apart they can sit |

Measuring the relay box does **not** unblock the PCB — the enclosure does. Measure both in one
session (you will have the box in hand anyway), but **choose the enclosure first**: in a piggyback
build the PCB outline is a function of the enclosure, not the box.

## Board size budget (mixed assembly)

Component-area estimate for the chosen mixed build, from the 52 placed footprints. Parts that sit
**raised on sockets** (DevKitC, INA3221, GY-521) let 1206 passives tuck underneath, which is the
main lever for shrinking.

| Group | Parts | Area |
|---|---|---|
| A1 ESP32-DevKitC on 2× 1×19 sockets | 1 | ~1595 mm² (raised ~8.5 mm — passives fit under) |
| Terminal blocks J1–J4 (MX126-5.0, 11 positions) | 4 | ~605 mm², **55 mm of board edge** |
| U3 INA3221 module (socketed) | 1 | ~475 mm² (raised) |
| 1206 passives (20× R, 12× C) | 32 | ~768 mm², ~half can tuck under raised parts |
| A3 MP1584 module | 1 | ~371 mm² |
| A2 GY-521 IMU on 1×08 socket | 1 | ~336 mm² (raised) |
| J10/J11 JST-XH 1×05 | 2 | ~220 mm², **28 mm of board edge** |
| U4/U5 PC817 SMD + **≥4 mm isolation barrier** | 2 | ~200 mm² |
| Q1 TO-220 P-FET | 1 | ~168 mm² |
| U2 ULN2803 → SOIC-18W | 1 | ~87 mm² (was ~230 mm² as a DIP-18 socket) |
| C1/C2 electrolytics, D1, Dz1, J6 | 5 | ~141 mm² |
| **Component area** | | **≈ 5 000 mm²** |
| **× 1.7–2.0** routing, GND pour, M3 keepouts, minus tucking | | **≈ 8 000–9 500 mm²** |

**As routed (2026-10-31): 110 × 70 mm**, about the relay box footprint. The table above is the
early estimate and undersized the INA3221 module, which is really 38 × 32 mm; the outline only fits
because low parts sit under the raised ESP and IMU.

> **Consequence to accept:** keeping the modules means the board will **not** shrink to the relay
> box's 100 × 70 footprint, so the stacked pair is **not** a flush brick — the ESP enclosure
> overhangs the box by roughly 10 mm per axis, or mounts crosswise. Shrinking to 100 × 70 requires
> dropping the modules for discrete SMD (the full-PCBA option), which is a larger redesign.

Resulting **enclosure requirement: internal ≥ 118 × 75 × 40 mm**, where the length includes the
ESP antenna overhanging the short edge by ~6–8 mm. The depth comes from spacer ~10 + PCB 1.6 +
tallest part ~16 mm (DevKitC on sockets, or the TO-220 upright) + ~10 mm of wire bend radius.

## Measurement set A — the relay box (for the bracket)

Box is **~100 × 70 × 44 mm**, 4× JD1912 40 A relays, pre-wired and sealed. Confirm the rough
figures and fill the rest in. All in mm; note where you measured.

| # | Measurement | Value | Why it matters |
|---|---|---|---|
| A1 | Outer footprint at the **base** — L × W | ____ × ____ | Bracket plate size |
| A2 | Outer footprint at the **lid** (if it overhangs the base) | ____ × ____ | A lid lip stops a flat plate from seating |
| A3 | Total height, base to top of lid | ____ | Stack height under the seat |
| A4 | Height of any cable boot / strain relief above the lid | ____ | Often the real tallest point |
| A5 | Mounting ears: **count**, hole Ø, ear thickness | ____ / Ø____ / ____ | Whether the box mounts to the tray at all |
| A6 | Ear hole **centre-to-centre**, each axis | ____ × ____ | Bracket can reuse the existing holes |
| A7 | Which faces are **flat and free** (no pigtail, no lid, no label) | ____ | Candidate mounting face |
| A8 | Flatness of the chosen face (rock a straight edge — gap) | ____ | > ~0.5 mm means a compliant pad or 3 feet, not 4 |
| A9 | **Fuse lid**: which face + swing clearance to open it | ____ / ____ | **Must stay serviceable** — do not bracket over it |
| A10 | Lid screw heads: count, Ø, protrusion above the face | ____ / Ø____ / ____ | A plate on that face must clear or clamp them |
| A11 | **Pigtail exit** face, bundle Ø, **free lead length** | ____ / Ø____ / ____ | Sets max separation of the two boxes |
| A12 | Wall thickness at the chosen face | ____ | Tapped insert vs. a strap that wraps the box |
| A13 | Ribs / curvature / draft on the chosen face | ____ | Printed bracket may need a matching relief |

**Rigidity is a functional requirement, not a nicety.** The IMU lives on this PCB and does crash,
theft and tilt detection (§21: *hard-mount the box and calibrate its resting orientation*). What it
needs is rigidity **relative to the bike** — so the test is the stiffness of the whole path from
PCB to frame, not whether the enclosure touches the relay box. Zip-ties or foam tape anywhere in
that path will ring and drift and give false crash triggers; use **metal or a stiff print with
mechanical fasteners** on at least 3 points.

So add one measurement to set A if you are considering fastening to the box:

| # | Measurement | Value | Why it matters |
|---|---|---|---|
| A14 | Is the **relay box's own mount** to the tray rigid, or on rubber/compliant mounts? | ____ | Compliant → fastening to the box puts **two** soft joints between the IMU and the frame, which disqualifies it. Tray-mounting beside the box is then the only option. |

## Measurement set B — the enclosure (this is what unblocks the order)

Not yet chosen — there is **no enclosure line in [02-bom.csv](02-bom.csv)**. Buy to the
*internal ≥ 115 × 85 × 35 mm* requirement above (sealed ABS/PC, IP65+, with internal PCB
standoffs and at least one gland position), then measure:

| # | Measurement | Value | Why it matters |
|---|---|---|---|
| B1 | Internal floor, wall to wall — L × W | ____ × ____ | Hard ceiling on the outline |
| B2 | Same at the **PCB standoff plane** (walls are usually drafted) | ____ × ____ | The number the outline must actually clear |
| B3 | Internal cavity **corner radius** | R____ | PCB corners get a matching chamfer/radius |
| B4 | Standoff/boss pattern: hole **centre-to-centre**, each axis | ____ × ____ | **Sets the M3 hole positions exactly** |
| B5 | Boss Ø, boss height, screw size | Ø____ / ____ / M____ | Hole Ø + annular keepout |
| B6 | Lid-screw boss intrusion into the cavity (corner keepouts) | ____ | Usually forces notched PCB corners |
| B7 | Clear height, PCB top face → lid underside (incl. lid ribs) | ____ | Must exceed ~26 mm (tallest part + wire bend) |
| B8 | Gland / knockout positions + Ø | ____ / Ø____ | Which edge the terminal blocks face |
| B9 | Gasket channel position relative to the cavity | ____ | Nothing may foul the seal |

## PCB work queue, once B is filled in

In order. **KiCad must be running** — PCB edits go over Konnect's IPC API, and it is currently
not reachable; schematic/netlist is untouched by all of this.

1. **U2 stays DIP-18 in a socket** and U4/U5 stay DIP-4. The >=4 mm barrier and the no-GND-pour
   rule under it still apply as placement rules.
2. **Resize Edge.Cuts** from 188 × 101 to the B1/B2 outline, with the B3 corner radius and the B6
   corner notches.
3. **Place 4× M3 mounting holes** near the board corners; the enclosure is drilled to match them.
   There are currently **zero** mounting holes on the board. Add the keepout + ground-stitch ring per [03-design-rules.md](03-design-rules.md)
   (default isolated, chassis tie via an 0 Ω / ferrite option).
4. **Re-place** to the smaller outline: terminal blocks on the B8 gland edge; DevKitC USB facing
   the service-pigtail gland; antenna end overhanging with a copper keepout; buck loop tight and
   away from the I²C pair; passives tucked under the raised modules.
5. **Re-route** — existing 256 segments / 13 vias / 2 GND zones will not survive the resize.
   Freeroute per the DSN/SES round-trip, then re-pour and stitch.
6. **Fix the outstanding unconnected pad** — `C18` pad 2 (GND) vs. the B.Cu GND zone, open in the
   last DRC run.
7. **Re-run DRC** — the committed [DRC.rpt](mainboard/DRC.rpt) is from **2026-06-26** and predates
   the 1206 and J10/J11 changes, so its "0 violations" no longer means anything.
8. **Fab outputs** → `production/`, then the fab + PCBA order (2-layer, 1.6 mm, HASL).

## Open items

- [ ] Enclosure chosen and measured (set B) — **the order gate**
- [ ] Relay box measured (set A)
- [ ] Bracket designed: material, fastener points, frame/tray tie for IMU rigidity
- [ ] Coil/power/GND inter-box lead length fixed from A11
- [ ] `02-bom.csv` gains the mechanical lines: enclosure, gland(s), bracket, M3 hardware, standoffs
- [ ] Confirm MP1584 / INA3221 / GY-521 pad order against their silkscreens before PCBA
      (carried over from [05-layout-todo.md](05-layout-todo.md))
