<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# PWM board: schematic specification (draft, 2026-11-12)

**What this board is:** an add-on to the mainboard for **4 PWM-controlled MOSFET outputs, up to
8 A each**. Typical uses are heated grips and a heated seat with proportional heat instead of
on/off. It connects to the mainboard's **J6 I²C header** and adds no GPIO load on the ESP. In
firmware the outputs become **OUT5–OUT8**, alongside the relay outputs OUT1–OUT4 (ADR-0021).

**Assembly:**
- JLC fits every surface-mount part (each symbol carries an `LCSC` field; files from
  `tools/run-jlc-assembly.ps1`).
- The owner hand-solders the through-hole parts.
- See [02-bom.csv](02-bom.csv).

It will be ordered together with mainboard v1. It is **untested hardware**: the owner has no
PCA9685 on hand, so the firmware is proven against a simulated device only.

---

## Design decisions

1. **Output stage: 4 low-side N-MOSFETs** (AOD4184A, 40 V, DPAK) with PWM at about 200 Hz.
   - The heaters are 2-wire floating loads, so low-side switching is fine. Load + stays on the
     fused feed.
   - A MOSFET at 100 % duty is plain ON, so any resistive load can use an output.
2. **PWM source: PCA9685** (16-channel, 12-bit, I²C).
   - LED0–3 drive the outputs; LED4–7 go to a spare 3.3 V logic header (J7).
   - **Address 0x41** (A0 high, A1–A5 low). 0x40 is the INA3221; 0x68 is the MPU-6050.
   - Firmware disables the ALLCALL address (0x70).
3. **Gate drive: 2× TC4427A** (dual non-inverting, 4.5–18 V) supplied from the load feed.
   - The PCA9685's 3.3 V outputs can't fully enhance an 8 A MOSFET; the driver gives
     Vgs ≈ 12–14 V.
   - Driver supply = `+12V_LOAD` → 100 Ω → `VDRV`, clamped by a 15 V zener, so load-dump spikes
     stay under the driver's 18 V limit.
4. **Fail-OFF in every case.** Each of these leaves the outputs off:

   | Event | Why the outputs go off |
   |---|---|
   | Key-off or ESP death | The master relay drops `+12V_LOAD`, so loads and drivers are unpowered. |
   | 3V3 lost | Driver-input pull-downs pull every input low. |
   | PCA9685 power-up | LEDn full-OFF and SLEEP are set at reset. |
   | Driver unpowered | Gate pull-downs hold Vgs = 0. |
   | ESP hung with the PCA still running | The task watchdog resets the ESP, and firmware clears all outputs at boot. This is the only window where a PWM state is held. |

5. **Load power comes from the relay box's master-gated output** (owner decision), so heaters
   are off twice over when the master relay drops.
6. **Per-channel fuse on the board.** PCB mini-blade holders, so a fuse is serviceable without
   opening the harness. Size each fuse for its load wire, e.g. 7.5 A for grips.
7. **Flyback diode per channel: SS34** (3 A Schottky, SMA, cathode → OUTn+), ordered from LCSC.
   - Heaters are resistive, so the diode only clamps harness inductance at PWM edges (about
     0.1 mJ at 8 A, which the MOSFET's avalanche rating also covers). It may be left unpopulated
     on purely resistive loads.
   - **Not a 1N4148:** it would carry the full load current for about 1 µs on every turn-off,
     which is far beyond its rating.
8. **No I²C pull-ups on this board.** The mainboard's 4.7 kΩ R5/R6 serve the whole bus.
   Footprints R18/R19 are DNP, in case the bus ever runs without the mainboard.
9. **Ground.** `PGND` (heavy wire to battery −) is the board ground and is also J1.4 (I²C
   reference).
   - Land PGND at the **same point as the mainboard's GND**, so heater return current never
     shares the thin I²C ground.
   - Keep the I²C cable **under about 30 cm**; mount the board next to the mainboard.
10. **Copper:** 2 layers, 1 oz, with power on wide pours on both layers, stitched. 8 A on a
    short, wide pour needs no 2 oz.

---

## Blocks

```
 Mainboard J6 ──4w──► J1 (3V3, SDA, SCL, GND)
                        │
                        ▼
                    U1 PCA9685 @0x41 ── LED4..7 ──► J7 spare PWM header (3.3 V logic)
                        │ LED0..3
                        ▼ (10k pull-down each)
                    U2/U3 TC4427A ×2  (VDD = VDRV, 15 V-clamped from +12V_LOAD)
                        │ via Rg 10 Ω (10k gate pull-down)
                        ▼
  +12V_LOAD ─► Fn ─► OUTn+ ─► [load] ─► OUTn− ─► Qn drain   (Qn source → PGND)
       ▲                      (SS34: anode OUTn−, cathode OUTn+)
  J2 (from box master-gated output; TVS D10 + bulk C6 across J2)
```

---

## Component list

| Ref | Part | Value / type | Footprint | Notes |
|---|---|---|---|---|
| U1 | PWM controller | PCA9685PW | TSSOP-28 (0.65 mm) | VDD = 3V3 from J1. OE → GND, EXTCLK → GND. |
| U2, U3 | Gate driver | TC4427A (dual, non-inverting) | SOIC-8 | U2 = ch1/2, U3 = ch3/4. |
| Q1–Q4 | N-MOSFET | AOD4184A (40 V, about 7 mΩ) | TO-252 (DPAK) | Thermal pour on the drain tab. Owner orders an AOD N/P kit; this board uses only the N-channel. |
| D1–D4 | Schottky | SS34 | SMA | Flyback; anode OUTn−, cathode OUTn+. |
| D5–D8 | LED | any single colour, 1206 | 1206 | Channel activity (brightness follows the duty). Silk label `OUT5`–`OUT8` next to each; meaning never by colour. |
| D9 | Zener | 15 V, BZT52C15 | SOD-123 | VDRV clamp. |
| D10 | TVS | SMBJ16A | SMB | Across `+12V_LOAD` → PGND (same as the mainboard D1). |
| F1–F4 | Fuse holder | Mini blade, PCB mount, **Keystone 3568** (exact part, since the footprint is specific) | `Fuse:Fuseholder_Blade_Mini_Keystone_3568` | One per channel; order with the LCSC parts. |
| R1–R4 | Res | 10 Ω | 1206 | Gate series. |
| R5–R8 | Res | 10 kΩ | 1206 | Gate → PGND pull-down. |
| R9–R12 | Res | 10 kΩ | 1206 | Driver input (PCA LED0–3) → GND pull-down. |
| R13–R16 | Res | 2 kΩ | 1206 | LED series, from the gate. About 4.5 mA at 12 V. |
| R17 | Res | 100 Ω, 0.25 W | 1206 | `+12V_LOAD` → VDRV. The drivers draw < 1 mA, so the drop is negligible. |
| R18, R19 | Res | 4.7 kΩ, **DNP** | 1206 | Optional I²C pull-ups. |
| R20 | Res | 10 kΩ | 1206 | A0 → 3V3 (sets 0x41). |
| C1 | Cap | 100 nF | 1206 | U1 VDD. |
| C2 | Cap | 10 µF | 1206 | 3V3 bulk at J1. |
| C3 | Cap | 1 µF, 50 V | 1206 | VDRV bulk. |
| C4, C5 | Cap | 100 nF, 25 V | 1206 | U2/U3 VDD decoupling. |
| C6 | Cap | 470 µF / 35 V | Radial 8 mm | `+12V_LOAD` bulk (PWM ripple). |
| J1 | Connector | XH-style 1×4 (owner's kit) | JST-XH B4B-XH-A vertical | 1 = 3V3, 2 = SDA, 3 = SCL, 4 = GND (same order as mainboard J6). Signal only. |
| J2 | Terminal | 4-pos 5.0 mm screw (MaiXu MX126, as on the mainboard) | `TerminalBlock_MaiXu_MX126-5.0-04P` | 1+2 = `+12V_LOAD`, 3+4 = `PGND`. Two pins paralleled per polarity for the combined heater current. |
| J3–J6 | Terminal | 2-pos 5.0 mm screw (MaiXu MX126) | `TerminalBlock_MaiXu_MX126-5.0-02P` | Per channel: 1 = OUTn+ (fused +12), 2 = OUTn− (switched). Check the per-pin rating (≥ 10 A) when ordering. |
| J7 | Connector | XH-style 1×6 (owner's kit) | JST-XH B6B-XH-A vertical | LED4, LED5, LED6, LED7, 3V3, GND. Spare PWM, 3.3 V logic only. |
| H1–H4 | Mounting | M3 | — | Corners. |

---

## Nets

| Net | Connects |
|---|---|
| `3V3` | J1.1, U1.VDD, C1, C2, R20, R18/R19 (DNP), J7.5 |
| `SDA` | J1.2, U1.SDA, R18 (DNP) |
| `SCL` | J1.3, U1.SCL, R19 (DNP) |
| `GND` = `PGND` | J1.4, J2.3, J2.4, U1.VSS, U1.A1–A5, U1.OE, U1.EXTCLK, U2/U3.GND, Q1–Q4 source, R5–R12 low ends, D9 anode, D10 anode, C1–C6 −, J7.6 |
| `A0` | U1.A0 → R20 → 3V3 |
| `PWMn` (n = 1–4) | U1.LED(n−1), R(8+n) pull-down, driver input |
| `GDRVn` | driver output → Rn (10 Ω) |
| `GATEn` | Rn → Qn gate, R(4+n) pull-down, R(12+n) → LED D(4+n) → GND |
| `+12V_LOAD` | J2.1, J2.2, D10 cathode, C6 +, F1–F4 input, R17 |
| `VDRV` | R17, D9 cathode, C3, C4, C5, U2/U3.VDD |
| `OUTn+` | Fn output, J(2+n).1, Dn cathode |
| `OUTn−` | J(2+n).2, Qn drain, Dn anode |
| `SPWM4`–`SPWM7` | U1.LED4–7 → J7.1–4 |

Unused PCA9685 outputs LED8–15 are left NC.

---

## Firmware (planned; tested only in simulation)

- `src/pca9685.{h,cpp}`: library-free driver like the INA/MPU ones.
  - Probe at 0x41.
  - MODE1 set to auto-increment with ALLCALL off; MODE2 set to totem-pole.
  - Prescale for about 200 Hz.
  - `setDuty(ch, 0..4095)`; full-OFF and full-ON bits for 0 % / 100 %.
  - Missing chip → outputs reported absent, never "on".
- ADR-0021 outputs: OUT5–OUT8 map to PCA channels 0–3. Each gets the per-output config plus a
  `pwm` duty (%). The load manager treats them like OUT1–4 (engine-only, shed order, crank hold).
- Boot: clear all PCA outputs before anything else, which covers the "ESP reset, PCA still
  running" case.
- Host tests against a fake I²C register map; console `pwm` / `pwm set <out> <0-100>`.
