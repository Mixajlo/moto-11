<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# Mainboard — KiCad capture & layout, step by step

How to turn [01-schematic-spec.md](01-schematic-spec.md) into a KiCad 10 schematic + routed PCB +
fab files (steps are stable across KiCad 8–10). I (Claude) can't drive the GUI, so this is the hands-on part — but every value,
connection, and rule is pinned down in the spec, BOM, and design rules so you're transcribing,
not deciding. Ping me at any step (DRC errors, footprint questions, a net you're unsure of).

> **Before you start:** install the **current stable KiCad 10.x** (Windows) from
> <https://www.kicad.org/download/>. Then tell me — I'll note it in `hardware/README.md`.

---

## 1. Create the project
1. KiCad → **File → New Project** → save as `hardware/mainboard/mainboard.kicad_pro`.
   (Keep it inside the repo so it's version-controlled. The `hardware/.gitignore` already
   excludes KiCad backup/cache cruft.)
2. Open the **Schematic Editor**.

## 2. Pull in symbol libraries
Most parts are in KiCad's built-in libs. You'll need a few module symbols:
- **ESP32-DevKitC**: built-in `RF_Module:ESP32-DEVKITC-32D` (or `ESP32-WROOM-32`). If you prefer
  the socketed-header approach, you can instead place two `Connector_Generic:Conn_01x19` and
  label nets by GPIO — but the DevKitC symbol is cleaner; just assign a **female-header
  footprint** to it in step 4.
- **ULN2803A**: `Driver_Relay:ULN2803A` (or `Relay:ULN2803A`).
- **INA3221 (module)**: place as `Connector_Generic:Conn_01x04` labelled `VS/GND/SDA/SCL`, plus a
  channel input (CH1) for the sense lead. (A stock symbol may exist, but the module-as-connector
  approach keeps it simple.) Note: INA3221, not INA226 — see ADR-0011.
- **MP1584 buck**: no stock symbol — place `Connector_Generic:Conn_01x04`, label `IN+ IN− OUT+ OUT−`.
- **IMU (LSM6DSO/MPU-6050)**: place `Connector_Generic:Conn_01x05`, label `VCC GND SDA SCL INT`.
- **PC817**: `Isolator:PC817`.
- **Passives / Q1 / TVS / Zener / LED / caps**: `Device` library (R, C, CP, D, D_Zener, D_TVS,
  Q_PMOS_GSD, LED).
- **Terminal blocks / headers**: `Connector` / `Connector_Generic`.

> Treating the breakout boards as connectors (with net labels) is the standard, low-friction way
> to put modules on a carrier — it keeps the schematic readable and the footprint is just a
> header.

## 3. Capture the schematic (from the spec)
Work block by block — the spec is already grouped this way. For each:
1. Place the symbols for that block.
2. Wire them, or (faster for a net-heavy board) use **global labels** with the exact net names
   from the spec (`+12V_PROT`, `COIL_FOG`, `SDA`, `IGN_SENSE`, …). Matching labels = connected.
3. Set values and reference designators to **match the BOM exactly** (`R1`=10k, etc.).

Recommended order (mirrors the spec):
1. **Power in**: J1 → Q1 (reverse-pol) → D1 (TVS) → C1 → A3 buck → +5V → A1 5V/VIN. Add R11/R12/Dz1.
2. **ESP A1**: drop the DevKitC, label its GPIOs per the *pin usage* table.
3. **ULN U2**: in1–5 from GPIO 13/25/26/27/12 with R1–R4 + R23 pulldowns; out1 → J2.2 (master), out2–5 → J3.1–4 (box FOG/GRIP/SPARE/AUX); COM→`+12V_PROT`, also on J2.1; GND. ch6–8 inputs → GND, outputs NC (ADR-0018).
4. **I²C**: SDA/SCL from A1.21/22 to U3 (pin3=SCL, pin4=SDA) + A2, R5/R6 pull-ups, J6. IMU INT→A1.33. INA3221 VS→+3V3, GND→GND; CH1 bus-sense is **off-board** (batt+ lead straight to the module CH1 terminal, D3 clamp at the module) — nothing to route here. Leave U3 pins 5–8 (PV/CRI/WAR/TC) unconnected.
5. **Opto**: U4 (IGN) and U5 (START) per the table — R7/R8 on 12 V side, R9/R10 pull-ups, J4.
6. **Buttons**: J10 (JST-XH 1×9) → R15–R22 (1k series) → GPIO 4/14/16/17/18/19/23/32; C11–C18 (100n) each node→GND; J10.9→GND.
7. **Indicator + test points + decoupling caps** (one 100 nF per IC).
7. Run **Inspect → Electrical Rules Checker (ERC)**. Fix every error (un-driven nets, missing
   power flags — add `PWR_FLAG` on +12V_HOT, +5V, +3V3, GND).

**Cross-check against the spec's net table when ERC is clean** — confirm each net lists the same
pins. Send me the netlist or a screenshot and I'll diff it against the spec.

## 4. Assign footprints
Tools → **Assign Footprints**. Use the footprints column in [02-bom.csv](02-bom.csv):
- A1 → female-header footprint matching your DevKitC (two `PinSocket_1x19_P2.54mm` rows; **measure
  your board's row spacing** — common is 0.9″ or 1.0″).
- A2/U3/A3/J6/J7 → `PinSocket`/`PinHeader` 2.54 mm of the right pin count.
- U2 → `DIP-18_W7.62mm` (+ socket) or `SOIC-18`.
- U4/U5 → `DIP-4`. Q1 → `TO-252`/`TO-220`. Passives → `0805` (or THT `Resistor_THT` if you prefer hand-soldering).
- J1/J2 → `TerminalBlock_5.08mm` 2-pos; J3/J4/J5 → `TerminalBlock_3.5mm` of the right size.

## 5. Lay out the PCB
1. Schematic Editor → **Tools → Update PCB from Schematic** (pushes netlist + footprints).
2. Set board rules from [03-design-rules.md](03-design-rules.md): **File → Board Setup → Design
   Rules / Net Classes**. Make net classes **Power** (1.5 mm), **Rail3V3** (0.8 mm), **Default/
   Signal** (0.30 mm); clearance 0.2 mm; via 0.4/0.8.
3. **Placement** (do this well — it's 80% of a good board):
   - DevKitC along one edge, **antenna end overhanging** (antenna keep-out, no copper).
   - Terminal blocks J1–J5 along the opposite/adjacent **field-wiring edge**.
   - Buck A3 near J1 (power entry); keep its loop tight.
   - ULN U2 between the ESP GPIO side and J3.
   - Opto U4/U5 straddling the 12 V edge (J4) with the **4 mm isolation gap** kept clear.
   - INA226/IMU on the I²C side, sense lead (J5) short.
4. Draw the **board outline** (Edge.Cuts) ~90 × 70 mm; add 4× M3 holes.
5. **Route**: power nets first (1.5 mm), then signals. Pour **GND** on both layers; stitch with
   vias. Keep I²C away from the buck switch node; don't pour under the opto barrier.
6. **DRC** (Inspect → Design Rules Checker) until zero errors. Send me the DRC report if stuck.

## 6. Review against fail-safe rules
Before fab, confirm:
- ULN input pulldowns present → relays **OFF at boot** (fail-OFF). ✔
- No external flyback diodes (ULN COM→`+12V_PROT`, always-hot). ✔
- GPIO34/35 pull-ups present (input-only pins). ✔
- Opto isolation gap intact; 12 V kept off the logic side. ✔
- Antenna keep-out clear. ✔

## 7. Fabrication outputs
- **File → Plot** → Gerbers (all copper, mask, silk, Edge.Cuts) → `gerbers/` (gitignored by
  default; un-ignore if you want to tag a release).
- **File → Fabrication Outputs → Drill Files**.
- Zip the Gerbers + drill → upload to JLCPCB/PCBWay. 2-layer, 1.6 mm, HASL, default rules.
- (Optional) Export **BOM** + **centroid** if you ever want assembly; for hand-build, skip.

## 8. After boards arrive
- Hand-assemble, then run the **[bench relay guide](../../docs/bench-relay-guide.html)** against
  the real board: `selftest` should click the coils through the ULN; verify GPIO 13/25/26/27,
  the 5 V/3V3 rails (TP2/TP3), and opto inversion (12 V on IGN_12V → GPIO34 reads LOW).
- Record any rework as a v1.1 note here, and promote firm changes to an ADR.

---

### Where I can plug in
Send me: the ERC/DRC report, a netlist export, a placement screenshot, or any "which footprint /
is this net right?" question. I'll check it against the spec and the fail-safe rules and tell you
exactly what to change.
