# 5. INA226 for engine-run detection and battery telemetry; sense on a clean lead

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The controller must (a) tell *running* from *key-on, engine-off*, without being fooled by a
battery tender; (b) judge battery health; and (c) drive voltage-based load-shedding. A panel
voltmeter recently fitted to the bike read ~1 V low because it was spliced into a
load-carrying ignition wire.

## Decision
Use an **INA226 over I²C** for bus voltage. Engine-run is
`engineRunning = ignitionHIGH && (Vbus >= 13.2 V)`. Battery telemetry uses the **rate of
change** — a fast drop after parking means a load left on; a slow sag over days means it just
sat — sampled when settled (a 24 h timer firing in the early morning). **Always sense on a
dedicated lead to the battery +** (only microamps flow, so no IR drop); never on a
load-carrying wire. A **PC817 2-channel opto** isolates two 12 V inputs into the ESP:
ignition/run-sense and start-button/headlight-feed sense (logic inverts).

## Consequences
- Engine-run detection is unambiguous — a charger can't fake it because the key is off.
- Voltage readings are true (no IR drop); the tablet readout becomes the accurate "voltmeter,"
  superseding the panel meter.
- Battery alerts are trend-based rather than single-threshold.
- The cheap INA226 module's default 0.1 Ω shunt maxes at ~0.8 A — fine for voltage-only; for
  current measurement use a low-value (~2 mΩ) shunt.

## Alternatives considered
- **Ignition-only detection**: rejected — can't distinguish running from key-on.
- **Voltage-only detection**: rejected — a tender fools it.
- **Panel voltmeter on a switched accessory wire**: rejected — the IR drop that caused the ~1 V
  error; sense must be on a current-free lead.
