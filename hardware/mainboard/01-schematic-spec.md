<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# Mainboard — schematic specification

The complete schematic as text: functional blocks, a component list with reference
designators, and a **net-by-net connection table**. This is the source of truth for KiCad
capture ([04-kicad-guide.md](04-kicad-guide.md)).

**What this board is:** the brain. It sockets an ESP32-DevKitC, drives the existing relay-box
coils through a ULN2803, senses the 12 V bus (INA3221) and two 12 V logic inputs (PC817 opto),
carries a 6-axis IMU, takes 8 handlebar aux buttons, and makes its own 5 V from the always-hot
battery feed. It carries only control and coil current — **never** load current.

---

## Design decisions (resolved)

1. **ESP mounting = socketed DevKitC** (2× female headers), not a bare WROOM module. Reuses
   your existing boards, keeps USB programming, matches the docs' "USB = programming-only,
   power from buck 5 V" note. *(Bare-module alternative = adds USB-UART + auto-reset + LDO; more
   parts, more risk.)*
2. **12 V sense = PC817 opto — decided.** Chosen over the divider+Zener (electrical-plan Fig 5.1)
   for transient robustness on a noisy bike bus and to keep the input-only GPIOs current-driven.
   Logic inverts (12 V present → GPIO LOW). Recorded in
   [ADR-0010](../../docs/adr/0010-twelve-volt-sense-opto.md). *(The divider+Zener stays a
   documented drop-in if you ever want fewer parts.)*
3. **Two pins added (accepted):** `START_SENSE` = **GPIO35** (input-only, RTC) = 2nd opto channel
   (start-button / headlight-feed sense); `IMU_INT` = **GPIO33** (RTC) = IMU wake-on-motion
   interrupt (deep-sleep wake source). Reflected in `firmware/src/pins.h` and
   [ADR-0009](../../docs/adr/0009-gpio-pin-map.md).
4. **Sensing IC = INA3221 (3-channel), not INA226** — the shipped part changed. Bonus: 3 channels
   → CH1 = battery bus voltage now; CH2/CH3 reserved for per-branch current sensing later (smarter
   load-shed). Watch the **26 V bus-input limit** (vs INA226's 36 V): the sense input gets a clamp
   (D3). Needs an INA3221 firmware driver (different register map). Recorded in
   [ADR-0011](../../docs/adr/0011-ina3221-replaces-ina226.md).
5. **INA3221, IMU, and buck are breakout modules on headers**, not raw chips — keeps this
   hand-solderable. *(All-SMD alternative is denser but needs reflow/hot-air.)*
6. **ULN2803 has 8 channels; only 4 are used.** The other 4 inputs/outputs are broken out to a
   header + terminals for future relays (e.g. the headlight-shed NC relay). "Fairings off once."

---

## Functional blocks

```
 +12V always-hot (fused 2A, external)
        │
   [reverse-polarity P-FET]──[TVS + bulk cap]──┬─────────────► +12V_PROT (board 12V)
        │                                      │
        │                              [MP1584 buck module A3] ──► +5V ──► ESP 5V/VIN
        │                                                                    │
        │                                                          ESP onboard LDO ──► +3V3
        │
 COIL_RAIL +12 (box pin 86, external) ──► ULN2803 COM (pin10, flyback)

 ESP GPIO 13/25/26/27 ─► ULN2803 in1..4 ─► out1..4 ─► coil 85 wires (screw terminals)
 ESP I2C 21/22 ─────────► INA3221 module + IMU module (shared bus, 4k7 pull-ups)
 12V IGN  ─► PC817 U4 ─► GPIO34 (pull-up 3V3)   [LOW when 12V present]
 12V START─► PC817 U5 ─► GPIO35 (pull-up 3V3)   [LOW when 12V present]
 IMU INT  ─────────────► GPIO33 (wake)
 BTN1..8  ─► J8 (JST-XH) ─► GPIO 4/14/16/17/18/19/23/32 (INPUT_PULLUP, active-LOW, RC)
```

---

## Power rails / nets (naming)

| Net | Meaning |
|-----|---------|
| `+12V_HOT` | Always-hot battery feed, after the external 2 A inline fuse (board input). |
| `+12V_PROT` | After reverse-polarity FET + TVS — the protected board 12 V. Feeds the buck. |
| `+5V` | Buck output → ESP 5V/VIN pin. |
| `+3V3` | From the ESP DevKitC onboard regulator → powers INA3221, IMU, opto + button pull-ups. |
| `COIL_RAIL` | +12 V coil common (the box's commoned pin 86). Feeds ULN COM. Externally sourced. |
| `GND` | Single board ground; ties to the star ground externally. |

---

## Component list

| Ref | Part | Value / type | Footprint | Notes |
|-----|------|--------------|-----------|-------|
| A1 | ESP32-DevKitC | ESP32-WROOM-32D module | 2× 1×19 (or 1×15) female header, 2.54 mm, 0.9″/1.0″ rows | Socketed; verify your board's pin count/row spacing. |
| A2 | IMU module | LSM6DSO (pref.) / MPU-6050 breakout | 1×5 (or 1×8) female header, 2.54 mm | 6-axis, I²C, INT out. |
| A3 | Buck module | MP1584EN mini buck | 4-pad (IN+ IN− OUT+ OUT−), 2.54 mm | Pre-set to 5.0 V **and verify** before fitting. |
| U2 | ULN2803A | Darlington array, 8-ch | DIP-18 (socketed) or SOIC-18 | Active-HIGH; COM→COIL_RAIL for flyback. |
| U3 | INA3221 module | CJMCU-3221 3-ch breakout (+ shunts) | `INA3221_Module` (custom, `mainboard.pretty`) — 1×5 labeled edge row | CH1 = bus voltage now; CH2/CH3 reserved for branch current. **26 V input max** → clamp D3. Socketed via the labeled VS/GND/SCL/SDA/A0 row; **confirm count/order** against the silk. |
| U4, U5 | PC817 | Optocoupler, single | DIP-4 | One per 12 V sense channel (IGN, START). |
| Q1 | P-MOSFET | AOD4185 / DMP3017SFG / IRF4905 | DPAK / TO-220 | Reverse-polarity (ideal-diode orientation). |
| D1 | TVS | SMBJ24A (uni, 24 V standoff) | SMB / DO-214AA | Load-dump clamp on +12V_PROT. |
| D2 | LED | green, 3 mm | THT | +3V3 power indicator (optional). |
| D3 | TVS | SMBJ20A (uni, 20 V standoff) | SMB / DO-214AA | Clamp on INA3221 sense input (26 V limit). |
| C1 | Cap | 470 µF / 35 V electrolytic | radial 8 mm | Bulk on +12V_PROT. |
| C2 | Cap | 100 µF / 16 V electrolytic | radial 6.3 mm | Bulk on +5V. |
| C3–C8 | Cap | 100 nF X7R | 0805 / THT | Decoupling: ESP, ULN, INA3221, opto, IMU. |
| C9 | Cap | 10 µF X7R | 0805 / THT | +3V3 bulk near ESP. |
| C10 | Cap | 1 µF | 0805 | RC filter on INA3221 VBUS sense. |
| R1–R4 | Res | 10 kΩ | 0805 / THT | ULN input pull-downs (boot-safe OFF). |
| R5, R6 | Res | 4.7 kΩ | 0805 | I²C SDA/SCL pull-ups to +3V3. |
| R7, R8 | Res | 2.2 kΩ, 0.25 W | THT/1206 | PC817 LED series (12 V side), ~5 mA. |
| R9, R10 | Res | 10 kΩ | 0805 | Pull-ups on GPIO34/35 (opto outputs; input-only pins). |
| R11 | Res | 100 kΩ | 0805 | Q1 gate–source. |
| R12 | Res | 100 Ω | 0805 | Q1 gate series (optional, slew). |
| Dz1 | Zener | 18 V, 0.5 W | SOD-123 / THT | Q1 gate–source clamp. >14.4 V so idle while charging; <20 V Vgs(max). |
| R13 | Res | 220 Ω | 0805 | D2 LED series. |
| R14 | Res | 100 Ω | 0805 | INA3221 sense series (light RC with C10; keep low to avoid bus-measure error). |
| J1 | Term block | 2-pos, 5.08 mm | THT | `+12V_HOT`, `GND` (power in). |
| J2 | Term block | 2-pos, 5.08 mm | THT | `COIL_RAIL` (+12 to box pin86 / ULN COM), `GND`. |
| J3 | Term block | 4-pos, 3.5 mm | THT | Coil outputs → 85 wires: MASTER, FOG, GRIP, SPARE. |
| J4 | Term block | 3-pos, 3.5 mm | THT | `IGN_12V`, `START_12V`, `SENSE_GND`. |
| J5 | Term block | 2-pos, 3.5 mm | THT | INA3221 `VBUS_SENSE` (clean lead to batt +), `GND`. |
| J6 | Header | 1×4, 2.54 mm | THT | I²C expansion: `+3V3`, `SDA`, `SCL`, `GND`. |
| J7 | Header | 2×4, 2.54 mm | THT | ULN spare ch5–8 in/out break-out (future relays). |
| J8 | Connector | JST-XH, 1×9, 2.50 mm | `Connector_JST:JST_XH_B9B-XH-A_1x09_P2.50mm_Vertical` | Handlebar aux buttons: BTN1..8 + common GND (ADR-0014). |
| R15–R22 | Res | 1 kΩ | 0805 / THT | Button series (RC with C11–C18); bench can omit, fit for the bike (noise). |
| C11–C18 | Cap | 100 nF | 0805 / THT | Button RC to GND (debounce/noise); pairs with R15–R22. |
| TP1–TP4 | Test point | — | THT loop | `+12V_PROT`, `+5V`, `+3V3`, `GND`. |

---

## ESP32-DevKitC pin usage (A1)

| GPIO | Net | Direction | Notes |
|------|-----|-----------|-------|
| 13 | `MASTER_EN` | out | → ULN in1 |
| 25 | `FOG_EN` | out | → ULN in2 |
| 26 | `GRIP_EN` | out | → ULN in3 |
| 27 | `SPARE_EN` | out | → ULN in4 |
| 34 | `IGN_SENSE` | in | ← opto U4, pull-up R9. Input-only/RTC (ext0 wake). |
| 35 | `START_SENSE` | in | ← opto U5, pull-up R10. Input-only/RTC. *(added; ADR-0009)* |
| 33 | `IMU_INT` | in | ← IMU INT. RTC (wake-on-motion). *(added; ADR-0009)* |
| 21 | `SDA` | I/O | I²C; pull-up R5. |
| 22 | `SCL` | I/O | I²C; pull-up R6. |
| 4 | `BTN1` | in | ← `J8.1`; INPUT_PULLUP + RC (R15/C11). Active-LOW. *(ADR-0014)* |
| 14 | `BTN2` | in | ← `J8.2`; INPUT_PULLUP + RC (R16/C12). |
| 16 | `BTN3` | in | ← `J8.3`; INPUT_PULLUP + RC (R17/C13). |
| 17 | `BTN4` | in | ← `J8.4`; INPUT_PULLUP + RC (R18/C14). |
| 18 | `BTN5` | in | ← `J8.5`; INPUT_PULLUP + RC (R19/C15). |
| 19 | `BTN6` | in | ← `J8.6`; INPUT_PULLUP + RC (R20/C16). |
| 23 | `BTN7` | in | ← `J8.7`; INPUT_PULLUP + RC (R21/C17). |
| 32 | `BTN8` | in | ← `J8.8`; INPUT_PULLUP + RC (R22/C18). Spare. |
| 5V (VIN) | `+5V` | pwr in | from buck A3. |
| 3V3 | `+3V3` | pwr out | from ESP LDO → board 3V3 rail. |
| GND (×) | `GND` | pwr | tie all DevKitC GND pins. |

> Do **not** feed +5V to VIN while USB is also connected (see electrical-plan §05). On the
> bench, power via USB *or* the buck, not both.

---

## Net-by-net connection table

Every electrical node, listed by net. `A1.13` = DevKitC GPIO13; `U2.1` = ULN2803 pin 1; etc.
Decoupling caps: place one 100 nF physically next to each IC's V+/GND.

### Power & ground
| Net | Connects |
|-----|----------|
| `+12V_HOT` | `J1.1`, Q1 **drain**. External 2 A fuse upstream. |
| `+12V_PROT` | Q1 **source**, `D1` cathode (TVS across PROT→GND), `C1+`, `A3.IN+`, `TP1`. |
| `+5V` | `A3.OUT+`, `C2+`, `A1.5V/VIN`, `TP2`. |
| `+3V3` | `A1.3V3`, `U3.VS`/`U3.VCC`, `A2.VCC`, `R5`, `R6`, `R9`, `R10`, `D2`(via R13), `C9+`, `TP3`. |
| `GND` | `J1.2`, `J2.2`, `A3.IN−`, `A3.OUT−`, `A1.GND`, `U2.9`, `U3.GND`, `U4.gnd side`, `U5.gnd side`, `A2.GND`, `J4.SENSE_GND`, `J5.2`, `J6.4`, all cap −, `TP4`. |
| `COIL_RAIL` | `J2.1`, `U2.10 (COM)`. (Externally wired to box pin 86.) |

### Reverse-polarity + TVS (input protection)
| Net | Connects |
|-----|----------|
| Q1 gate | → `GND` via `R12` (100 Ω series); `R11` (100 k) gate→source; `Dz1` zener gate→source clamp. |
| `D1` (TVS) | cathode → `+12V_PROT`, anode → `GND` (unidirectional). |

*P-FET reverse-polarity (high-side): **drain = +12V_HOT (battery in), source = +12V_PROT (load)**,
gate to GND via R12. Correct polarity → body diode forward (HOT→PROT), then Vgs ≈ −12 V turns the
channel fully on (tiny drop). Reverse polarity → body diode reverse-biased → blocked. R11 keeps a
defined off-state if the gate floats; Dz1 clamps Vgs against load-dump.*

### Relay coil driver (ULN2803 U2)
| Net | Connects |
|-----|----------|
| `MASTER_EN` | `A1.13` → `U2.1`; `R1` (10k) `U2.1`→GND. |
| `FOG_EN` | `A1.25` → `U2.2`; `R2` `U2.2`→GND. |
| `GRIP_EN` | `A1.26` → `U2.3`; `R3` `U2.3`→GND. |
| `SPARE_EN` | `A1.27` → `U2.4`; `R4` `U2.4`→GND. |
| `COIL_MASTER` | `U2.18` → `J3.1` (master relay coil 85). |
| `COIL_FOG` | `U2.17` → `J3.2`. |
| `COIL_GRIP` | `U2.16` → `J3.3`. |
| `COIL_SPARE` | `U2.15` → `J3.4`. |
| ULN COM | `U2.10` → `COIL_RAIL` (internal flyback clamps to +12). |
| ULN GND | `U2.9` → `GND`. |
| spare ch5–8 | `U2.5..8` → `J7` inputs (with their own pull-downs if used); `U2.14..11` → `J7` outputs. |

### I²C bus (INA3221 U3 + IMU A2)
| Net | Connects |
|-----|----------|
| `SDA` | `A1.21`, `U3.SDA`, `A2.SDA`, `R5`→+3V3, `J6.2`. |
| `SCL` | `A1.22`, `U3.SCL`, `A2.SCL`, `R6`→+3V3, `J6.3`. |
| `IMU_INT` | `A2.INT` → `A1.33`. |
| INA3221 sense | `J5.1` (`VBUS_SENSE`) → `R14` (100 Ω) → INA3221 **CH1** input; `C10` CH1→GND (light filter); `D3` (TVS) CH1→GND (26 V clamp). Tie CH1 VIN+/VIN− to the sense node for a zero-current voltage read. |
| INA3221 CH2/CH3 | Reserved — leave on the module terminals for future per-branch current sensing. |

> INA3221 measures bus voltage on a **clean dedicated lead to battery +** (zero current = no IR
> drop), per ADR-0005/0011. Default I²C address **0x40** (A0→GND) — same as the old INA226, so no
> firmware address change, but the **register map / driver differs**. Verify CH wiring against
> your specific breakout's silkscreen. Populate shunts only when you want branch current.

**INA3221 module header pin map (U3) — socketed via the LABELED edge row; CONFIRM count/order
against the board silk.** The custom `INA3221_Module` footprint is a 1×5 row matching that
labeled edge. Pad names live in the schematic symbol, so if your silk order differs just remap
the symbol pins (or rotate 180°). `A0→GND` sets I²C address **0x40**. The alert pins
(PV/WARN/CRIT/TC) and CH2/CH3 terminals are left unconnected here — we read everything over I²C.

| Pad | Silk label (assumed) | Net |
|-----|----------------------|-----|
| 1 | VS  | `+3V3` |
| 2 | GND | `GND` |
| 3 | SCL | `SCL` |
| 4 | SDA | `SDA` |
| 5 | A0  | `GND` (address 0x40) |

### Aux buttons — handlebar (J8, ADR-0014)
Dry contacts, **active-LOW**: each button shorts its GPIO to GND when pressed; the ESP's
internal pull-up idles it HIGH. Debounced in firmware (`src/buttons.cpp`). The per-line RC
(series R15–R22 + C11–C18 to GND) filters bus noise on the bike run — optional on the bench.

| Net | Connects |
|-----|----------|
| `BTN1` | `J8.1` → `R15` (1k) → `A1.4`; `C11` (100n) node→GND. |
| `BTN2` | `J8.2` → `R16` → `A1.14`; `C12`→GND. |
| `BTN3` | `J8.3` → `R17` → `A1.16`; `C13`→GND. |
| `BTN4` | `J8.4` → `R18` → `A1.17`; `C14`→GND. |
| `BTN5` | `J8.5` → `R19` → `A1.18`; `C15`→GND. |
| `BTN6` | `J8.6` → `R20` → `A1.19`; `C16`→GND. |
| `BTN7` | `J8.7` → `R21` → `A1.23`; `C17`→GND. |
| `BTN8` | `J8.8` → `R22` → `A1.32`; `C18`→GND. |
| button GND | `J8.9` → `GND` (common return for all 8 buttons). |

> Pull-ups are the ESP's internal ones (`INPUT_PULLUP`), so no external pull-up resistors are
> needed. Keep the RC light (1 kΩ / 100 nF ≈ 0.1 ms) — far faster than the 25 ms firmware
> debounce, so it only knocks down spikes. Past 8 buttons → swap to an MCP23017 on I²C (ADR-0014).

### 12 V sense — opto-isolated (PC817 U4 = IGN, U5 = START)
PC817 pins: 1 = anode (LED+), 2 = cathode (LED−), 3 = emitter, 4 = collector.

| Net | Connects |
|-----|----------|
| `IGN_12V` | `J4.1` → `R7` (2k2) → `U4.1`; `U4.2` → `SENSE_GND` (`J4.3`). |
| IGN out | `U4.4` (collector) → `+3V3` via `R9` (10k) **and** → `A1.34`; `U4.3` (emitter) → `GND`. |
| `START_12V` | `J4.2` → `R8` (2k2) → `U5.1`; `U5.2` → `SENSE_GND`. |
| START out | `U5.4` → `+3V3` via `R10` **and** → `A1.35`; `U5.3` → `GND`. |

> Inverted logic: 12 V present → opto LED on → transistor pulls the GPIO **LOW**. Firmware reads
> `engineRunning`/start with this inversion in mind. `SENSE_GND` ties to board `GND`.

### Indicator
| Net | Connects |
|-----|----------|
| pwr LED | `+3V3` → `R13` (220Ω) → `D2.anode`; `D2.cathode` → `GND`. |

---

## Sanity checks baked in
- **Boot-safe:** R1–R4 hold ULN inputs low while the ESP boots → all relays OFF (fail-OFF).
- **No external flyback diodes:** ULN COM→COIL_RAIL uses the chip's internal clamps (ADR-0003).
- **Input-only pins fed:** GPIO34/35 have no internal pull-ups, so R9/R10 are mandatory.
- **One ground:** single `GND` net, star-tied off-board; sense grounds returned here.
- **Decoupling:** a 100 nF within ~5 mm of every IC power pin; bulk where rails enter.

Next: [02-bom.csv](02-bom.csv) · [03-design-rules.md](03-design-rules.md) · [04-kicad-guide.md](04-kicad-guide.md)
