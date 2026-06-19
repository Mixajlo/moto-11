# 11. INA3221 (3-channel) in place of the INA226 for bus sensing

- Status: Accepted (amends [ADR-0005](0005-ina226-sensing.md))
- Date: 2026-06-19
- Deciders: Miki

## Context
[ADR-0005](0005-ina226-sensing.md) chose the **INA226** for trustworthy bus-voltage (and
optional current) sensing over I²C, read on a clean dedicated lead to battery + (zero current =
no IR drop). The supplier shipped an **INA3221** instead. Rather than re-order, we evaluated the
INA3221 in its place — it is a related TI part, so the sensing *architecture* (I²C, clean sense
lead, engine-run detection) is unchanged; only the device differs.

## Decision
We will use the **INA3221** as the bus monitor. It is a **3-channel** voltage/current monitor:
- **CH1** = battery bus voltage (the role the INA226 had) — drives engine-run detection and
  battery telemetry.
- **CH2 / CH3** = reserved for future **per-branch current** sensing (e.g. fog and grip
  branches), enabling smarter, targeted load-shedding than a single bus reading allows.

Default I²C address is **0x40** (A0 → GND), the same as the INA226 default, so no address change.

## Consequences
- **Free upgrade path:** two extra measurement channels we didn't have, fitting the
  "provision generously" philosophy at no extra board cost.
- **Lower input ceiling:** INA3221 bus inputs are rated to **26 V** (vs INA226's 36 V). The bike
  bus is ~12–14.4 V, but load-dump transients exceed 26 V, so the sense input gets a **TVS clamp
  (D3, ~20 V)** plus a small series R — captured in the mainboard schematic spec.
- **Firmware driver changes:** the register map differs from the INA226; the (not-yet-written)
  sensing driver targets an INA3221 library. The I²C address and bus wiring are unchanged.
- Keep the sense series resistance **low** (≈100 Ω, not 100 k) to avoid bus-measurement error
  from the device's input behaviour.

## Alternatives considered
- **Re-order an INA226** to match ADR-0005: avoidable delay and cost; the INA3221 is a strict
  superset for our needs (more channels), so no reason to wait.
- **Bare ADC divider** (no INA part): rejected in ADR-0005 already (ESP32 ADC is noisy/non-linear);
  unchanged.
