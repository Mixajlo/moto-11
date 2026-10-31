# 15. Mainboard v1 is a close-coupled enclosure beside the relay box, mixed SMD/module assembly

- Status: Accepted — frame mount decided 2026-10-31; **enclosure model still open**, see *Open questions*
- Date: 2026-10-31
- Deciders: Miki

## Context
The mainboard schematic is complete and the first layout pass is routed, but the outline is
still the **reference rectangle** — 188 × 101 mm, well over the ~90 × 70 mm target in
[03-design-rules.md](../../hardware/mainboard/03-design-rules.md) — and the board carries **no
mounting holes at all**. [05-layout-todo.md](../../hardware/mainboard/05-layout-todo.md) gates
fab on *"lock the form factor against the real mounting location"*, and that location is now
decided: the owner wants the first board that actually gets **produced** to be **attached to the
relay box**, so the under-seat bay holds one serviceable assembly rather than two loose boxes.

That pulls against two things already settled. [ADR-0004](0004-master-relay-gating-and-failon-headlights.md)
and `electrical-plan.html` §21 decided **two enclosures, not one**: the relay box is ~100 × 70 ×
44 mm with ~28 mm JD1912 relays inside, leaving no room to lid-mount the electronics, and keeping
the switching relays physically away from the sensitive electronics and the short I²C bus is good
practice, not just a space compromise. Meanwhile §05 of the layout todo plans the production board
as **full SMD, JLCPCB-assembled** — which is what would make a box-sized board possible, but is a
full re-layout with a new BOM pass and gives up field-swappable modules.

The board also hosts the IMU, which does crash, theft and tilt detection. §21 is explicit that it
must be **hard-mounted** and calibrated at rest — so whatever "attached" means mechanically, it
has to be stiff.

## Decision
- **Outside the box, own enclosure.** The PCB lives in its **own sealed enclosure**. Mounting it
  *inside* the relay box is **ruled out on inspection** — there is not enough space above the
  ~28 mm relays, and it would put the logic in among the switching relays. The relay box is not
  opened, not re-lidded, and not drilled through into the cavity. §21's electrical separation
  therefore holds: the only wires between the two are the coil-control leads, a shared ground and
  the power feed.
- **Close-coupled, for short leads.** The enclosure sits immediately next to the relay box. The
  requirement is **short logic-to-distribution wiring**, not fastening to the box as such.
- **Rigid path to the frame is the binding constraint, not proximity.** The IMU needs rigidity
  **relative to the bike**, so what matters is that the enclosure has a stiff, fastened path to
  the frame/tray — *not* that it is attached to the relay box. No zip-ties or foam tape anywhere
  in that path: a compliant mount rings and drifts under vibration and produces false crash
  triggers.
- **Mixed assembly, optos excepted.** Passives and the ULN2803 go SMD; **U4/U5 (PC817) stay
  `DIP-4_W7.62mm`** — the DIP body gives 7.62 mm pin-to-pin across the isolation barrier, so the
  **≥4 mm creepage** rule in `03-design-rules.md` is satisfied by the package instead of becoming
  a tight placement constraint. An SOP-4 opto would buy nothing here; 8 pins of hand-soldering is
  trivial on a board that already needs module headers done by hand.
- **Mixed assembly for v1.** Passives and the ULN2803 go **SMD, machine-assembled (JLCPCB PCBA)**;
  the **INA3221, MP1584 and GY-521 stay socketed modules**. The schematic and netlist carry over
  unchanged — only footprints and the outline move.
- **The enclosure drives the outline, not the relay box.** In a piggyback build the PCB has to fit
  the enclosure; the box only drives the bracket. The routed outline is **110 × 70 mm**, plus the
  ESP antenna overhanging one short edge, so the enclosure needs **internal ≥ 118 × 75 × 40 mm**. Procedure and the measurement sets are
  in [06-mechanical-mounting.md](../../hardware/mainboard/06-mechanical-mounting.md).
- **The frame mount uses the seat-latch screws (decided 2026-10-31).** The enclosure hangs from the
  two M6 screws of the seat-latch plate, on the under-seat cross-member. They sit at opposite
  corners of a ~40 × 20 mm rectangle, so ~45 mm apart on the diagonal. The PCB is **not** fastened
  to the M6 screws.
- **The PCB sets its own M3 holes.** The owner drills the enclosure floor to match the PCB and
  fits spacers, so the enclosure needs no standoff pattern; only its internal envelope constrains
  the board.

## Consequences
- The relay box stays sealed and warranty-intact, and its **fuse lid must remain serviceable** —
  nothing may be mounted over it, whichever attachment interface is chosen.
- Keeping the modules means the board **will not reach the relay box's 100 × 70 footprint**. The
  stack is not a flush brick: the ESP enclosure overhangs by roughly 10 mm per axis or mounts
  crosswise. Accepted in exchange for swappable modules and a much smaller redesign.
- Socketed modules stay raised ~8.5 mm, so 1206 passives tuck underneath — this is the main lever
  that gets ~5 000 mm² of parts into a ~110 × 80 mm board.
- The existing routing (256 segments, 13 vias, 2 GND zones) **does not survive the resize** and
  will be re-routed. The committed `DRC.rpt` (2026-06-26) is already stale — it predates the 1206
  and J10/J11 changes — so DRC must be re-run from scratch before fab.
- PCBA means U2 (ULN2803) must come from the assembler's catalogue as **SOIC-18W**, which gives up
  the current DIP socket and so the field-swap of a blown coil driver. Accepted: the ULN has
  **4 unused channels**, so a dead channel is re-assignable in firmware + one wire rather than a
  chip swap.
- Keeping U4/U5 as DIP-4 means the **≥4 mm isolation barrier stays a property of the package**
  rather than a placement rule that each re-layout could silently break.
- `02-bom.csv` gains mechanical lines it has never had: enclosure, gland(s), bracket, standoffs,
  M3 hardware.
- A future full-SMD revision remains open and is now cheaper to justify: it is the way to get a
  genuinely box-sized board, and the netlist carries over again.

## Alternatives considered
- **Inside the relay box** (the original intent, and §21's taller-3D-printed-lid variant): one
  unit, one gland, tidiest result — **ruled out by inspection**: there is not enough usable space
  above the ~28 mm relays, and it would put the logic in among the switching relays. Even with a
  printed taller lid it would mean re-lidding and re-sealing a working sealed box, cap the outline
  at the box cavity, and add a new waterproofing risk directly above the relays.
- **Bare PCB on a bracket, no enclosure of its own:** fewest parts, but an exposed board under the
  seat sees water, salt and vibration. Rejected on reliability.
- **Full SMD PCBA now:** the only route to a ~100 × 70 board that matches the relay-box footprint,
  and nothing to hand-solder — rejected *for v1* as a full re-layout plus a new BOM pass, and it
  gives up field-swappable modules on a board that is still being bench-proven. Kept as the
  documented next revision.
- **Keep THT and the 188 × 101 outline, hand-assembled:** fastest to order and reuses the existing
  routing — rejected because the board is far too large to sit beside a 100 × 70 box in any
  reasonable enclosure.

## Open questions
- **Rigidity of a two-screw mount.** Two screws ~45 mm apart carry an enclosure wider than that
  span, so the overhang can flex. Check for this before trusting the IMU; see
  [06-mechanical-mounting.md](../../hardware/mainboard/06-mechanical-mounting.md).
- **Fallback if the mechanics get awkward:** the GY-521 is socketed, so the **IMU could be remoted**
  to a rigid frame point on a short I²C lead, decoupling enclosure placement from crash-detection
  rigidity entirely. Costs a connector and a second sealed penetration; keeps §21's "short I²C"
  intent only if the lead stays short.
- **Which enclosure.** Still unchosen, and it is the gate on the board outline — see
  [06-mechanical-mounting.md](../../hardware/mainboard/06-mechanical-mounting.md), measurement set B.
